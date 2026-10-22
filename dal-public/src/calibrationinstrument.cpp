#include <dal/platform/platform.hpp>

#include <climits>

#include <dal/calibration/validation_internal.hpp>

#include <dal/model/factory.hpp>
#include <dal/model/gsrmultidata.hpp>

#include <dal-public/src/calibrationinstrument.hpp>

namespace Dal {
    [[noreturn]] void CalibrationInputFailure(const String_& code, const String_& field, const String_& expected, const String_& actual) {
        CalibrationInternal::Fail(code, field, expected, actual);
    }
    namespace {
        void CheckInput(bool condition, const String_& code, const String_& field, const String_& expected) {
            if (!condition)
                CalibrationInputFailure(code, field, expected);
        }
        bool Exact(const String_& value, const char* token) { return std::string(value.data(), value.size()) == token; }
        Handle_<CalibrationInstrument_> ImportPillar(const Handle_<CalibrationInstrument_>& instrument, const std::optional<Date_>& pillar) {
            if (!pillar)
                return instrument;
            return NewScriptCalibrationInstrument({instrument->Id(), instrument->Definition(), {}, pillar});
        }
        template <class N_, class F_>
        Handle_<CalibrationInstrument_>
        ImportCurve(const Handle_<YCInstrument_>& input, const ScriptInstrumentImportSettings_& settings, F_ exportNative) {
            const auto* native = dynamic_cast<const N_*>(input.get());
            CheckInput(native && settings.minAbsAnnuity_, "UnsupportedCalibrationProduct", "/native_instrument",
                       "matching native curve instrument and explicit annuity floor");
            const CurveScriptImportSettings_ resolved{settings.instrumentId_, settings.currency_, settings.discountRoute_, settings.notional_,
                                                      *settings.minAbsAnnuity_};
            return ImportPillar(exportNative(*native, resolved), settings.pillar_);
        }
        Handle_<CalibrationInstrument_> ImportOption(const BondOption_& option, const BondOptionScriptImportSettings_& settings) {
            return NewBondOptionScriptInstrument(option, settings);
        }
        Handle_<CalibrationInstrument_> ImportOption(const Caplet_& option, const BondOptionScriptImportSettings_& settings) {
            return NewCapletScriptInstrument(option, settings);
        }
        Handle_<CalibrationInstrument_> ImportOption(const Swaption_& option, const BondOptionScriptImportSettings_& settings) {
            return NewSwaptionScriptInstrument(option, settings);
        }
        CalibrationPreparationContext_
        McContext(const Handle_<CalibrationInstrument_>& instrument, const Handle_<ModelData_>& model, const CalibrationMonteCarloRequest_& request) {
            CheckInput(bool(instrument), "InvalidCalibrationPlan", "/instrument", "nonnull instrument");
            CalibrationPreparationContext_ context;
            context.valuation_ = request.valuation_;
            context.currency_ = instrument->Definition()->Spec().header_.payCurrency_;
            context.discountRoute_ = instrument->Definition()->Spec().header_.discountRoute_;
            context.model_ = model;
            context.smoothing_ = request.simulation_.smooth_;
            if (const auto* one = dynamic_cast<const GSRModelData_*>(model.get()))
                context.curve_ = one->curve_;
            if (const auto* multi = dynamic_cast<const MultiFactorGSRModelData_*>(model.get()))
                context.curve_ = multi->curve_;
            return context;
        }
        CalibrationMcSettings_ McSettings(const CalibrationMonteCarloRequest_& request) {
            const auto& s = request.sampling_;
            CheckInput(Exact(s.mode_, "PseudoRandom"), "InvalidCalibrationPlan", "/sampling/mode", "PseudoRandom");
            CheckInput(Exact(s.streamVersion_, "dal.mrg32-normal-paths/1"), "InvalidCalibrationPlan", "/sampling/stream_version",
                       "dal.mrg32-normal-paths/1");
            CheckInput(!request.simulation_.useBb_ && Exact(request.simulation_.normalPrecision_, "Precise"), "InvalidCalibrationPlan", "/simulation",
                       "no Brownian bridge and Precise normal precision");
            CheckInput(request.derivativeMethod_ == CalibrationDerivative_::Value_::None ||
                           request.derivativeMethod_ == CalibrationDerivative_::Value_::AAD,
                       "DerivativeUnavailable", "/derivative_method", "None or AAD");
            CheckInput(s.pathsPerBatch_ >= 2 && s.batches_ >= 2 && s.seed_ >= 0, "InvalidCalibrationPlan", "/sampling",
                       "at least two batches of at least two paths and a nonnegative seed");
            const size_t reserved = static_cast<size_t>(s.batches_) * (2 * static_cast<size_t>(s.pathsPerBatch_) + 1);
            CheckInput(reserved <= INT_MAX, "CalibrationBudgetExceeded", "/sampling",
                       "requested stream and internal unused validation reserve within 2^31-1 paths");
            CalibrationMcSettings_ result;
            result.allowMonteCarloCalibration_ = true;
            result.aad_ = request.derivativeMethod_ == CalibrationDerivative_::Value_::AAD;
            result.compiled_ = request.simulation_.compiled_.value_or(Script::DEFAULT_COMPILED);
            result.seed_ = s.seed_;
            result.fitBatches_ = result.validationBatches_ = s.batches_;
            result.fitPathsPerBatch_ = s.pathsPerBatch_;
            result.validationPathsPerBatch_ = s.pathsPerBatch_ + 1;
            result.maxPathsPerView_ = reserved;
            return result;
        }
        Vector_<CalibrationReportParameter_> McParameters(const Handle_<ModelData_>& model) {
            const auto runtime = CreateModel<double>(model);
            const auto labels = runtime->ParameterLabels();
            const auto parameters = runtime->Parameters();
            Vector_<CalibrationReportParameter_> rows;
            for (size_t i = 0; i < labels.size(); ++i) {
                CalibrationReportParameter_ row;
                row.id_ = labels[i];
                row.unit_ = row.id_.find("g:") == 0 ? "Rate" : "Dimensionless";
                row.value_ = *parameters[i];
                rows.push_back(row);
            }
            return rows;
        }
        CalibrationReportObservation_ McObservation(const Handle_<CalibrationInstrument_>& instrument, const CalibrationMonteCarloRequest_& request) {
            CalibrationReportObservation_ row;
            row.instrument_ = instrument;
            row.measureId_ = request.measureId_;
            const auto& measures = instrument->Definition()->Spec().measures_;
            const auto found = std::find_if(measures.begin(), measures.end(), [&](const auto& m) {
                return std::string(m.id_.data(), m.id_.size()) == std::string(request.measureId_.data(), request.measureId_.size());
            });
            if (found != measures.end()) {
                row.unit_ = found->unit_.Token();
                row.representation_ = found->kind_ == CalibrationMeasureKind_::Value_::Price ? "Price" : "Scalar";
            }
            return row;
        }
        CalibrationReportSnapshot_ McReport(const Handle_<CalibrationInstrument_>& instrument,
                                            const CalibrationMonteCarloRequest_& request,
                                            const PreparedCalibrationMc_& plan,
                                            const CalibrationMcResult_& value,
                                            const Vector_<CalibrationReportParameter_>& parameters) {
            CalibrationReportSnapshot_ data;
            auto& summary = data.summary_;
            summary.domain_ = "MonteCarlo";
            summary.status_ = "Succeeded";
            summary.contextIdentity_ = value.contextIdentity_;
            summary.evaluations_ = static_cast<int>(value.engineRuns_);
            summary.jacobianEvaluations_ = request.derivativeMethod_ == CalibrationDerivative_::Value_::AAD ? summary.evaluations_ : 0;
            auto row = McObservation(instrument, request);
            row.modelValue_ = value.mean_;
            row.standardError_ = value.standardError_;
            row.backend_ = plan.Capability().backend_.String();
            row.derivativeMethod_ = plan.Capability().derivativeMethod_.String();
            row.planIdentity_ = value.planIdentity_;
            data.observations_.push_back(row);
            data.parameters_ = parameters;
            if (!value.gradient_.empty())
                data.quoteJacobian_ = value.quoteJacobian_;
            data.execution_.valuationDate_ = request.valuation_.evaluationDate_;
            data.execution_.todayFixingPolicy_ = request.valuation_.todayFixingPolicy_.String();
            data.execution_.numeraireConvention_ = plan.Mapping().numeraire_;
            data.execution_.modelTimeBasis_ = "ACT/365";
            const auto& s = request.sampling_;
            data.execution_.sampling_ = CalibrationReportSampling_{s.mode_, s.streamVersion_, value.mappingIdentity_, "mrg32", "Precise",
                                                                   s.seed_, s.batches_,       s.pathsPerBatch_};
            for (const auto& range : value.ranges_) {
                data.execution_.sampling_->firstPaths_.push_back(static_cast<double>(range.firstPath_));
                data.execution_.sampling_->pathCounts_.push_back(static_cast<double>(range.pathCount_));
            }
            const auto& simulation = request.simulation_;
            data.execution_.simulation_ = CalibrationReportSimulation_{simulation.useBb_, simulation.compiled_.value_or(Script::DEFAULT_COMPILED),
                                                                       simulation.smooth_, simulation.normalPrecision_};
            return data;
        }
    } // namespace
    Handle_<CalibrationInstrument_> NewDepositScriptInstrument(const Handle_<YCInstrument_>& deposit,
                                                               const ScriptInstrumentImportSettings_& settings) {
        const auto* native = dynamic_cast<const Deposit_*>(deposit.get());
        CheckInput(native && settings.minAbsAnnuity_, "UnsupportedCalibrationProduct", "/deposit", "native Deposit and explicit annuity floor");
        return ImportPillar(NewDepositScriptInstrument(*native, {settings.instrumentId_, settings.currency_, settings.discountRoute_,
                                                                 settings.notional_, *settings.minAbsAnnuity_}),
                            settings.pillar_);
    }
    Handle_<CalibrationInstrument_> NewEuropeanRateOptionScriptInstrument(const EuropeanRateOption_& option,
                                                                          const ScriptInstrumentImportSettings_& settings) {
        CheckInput(!settings.minAbsAnnuity_, "InvalidScriptInstrumentDefinition", "/import_settings/min_abs_annuity", "null for rate options");
        const BondOptionScriptImportSettings_ native{settings.instrumentId_, settings.currency_, settings.discountRoute_, settings.notional_};
        const auto instrument = std::visit([&](const auto& value) { return ImportOption(value, native); }, option);
        return ImportPillar(instrument, settings.pillar_);
    }
    Handle_<CalibrationInstrument_>
    NewFraScriptInstrument(const Handle_<YCInstrument_>& native, const Date_& payment, const ScriptInstrumentImportSettings_& settings) {
        return ImportCurve<FRA_>(native, settings, [&](const FRA_& value, const CurveScriptImportSettings_& resolved) {
            return NewFraScriptInstrument(value, payment, resolved);
        });
    }
    Handle_<CalibrationInstrument_> NewSwapScriptInstrument(const Handle_<YCInstrument_>& native, const ScriptInstrumentImportSettings_& settings) {
        return ImportCurve<Swap_>(native, settings, [](const Swap_& value, const CurveScriptImportSettings_& resolved) {
            return NewSwapScriptInstrument(value, resolved);
        });
    }
    Handle_<CalibrationInstrument_> NewPeriodForwardOisScriptInstrument(const Handle_<YCInstrument_>& native,
                                                                        const ScriptInstrumentImportSettings_& settings) {
        return ImportCurve<OISSwap_>(native, settings, [](const OISSwap_& value, const CurveScriptImportSettings_& resolved) {
            return NewPeriodForwardOisScriptInstrument(value, resolved);
        });
    }
    Handle_<CalibrationInstrument_> NewBasisSwapScriptInstrument(const Handle_<YCInstrument_>& native,
                                                                 const ScriptInstrumentImportSettings_& settings) {
        return ImportCurve<BasisSwap_>(native, settings, [](const BasisSwap_& value, const CurveScriptImportSettings_& resolved) {
            return NewBasisSwapScriptInstrument(value, resolved);
        });
    }
    CalibrationMonteCarloResult_ ValueCalibrationInstrumentByMonteCarlo(const Handle_<CalibrationInstrument_>& instrument,
                                                                        const Handle_<ModelData_>& model,
                                                                        const CalibrationMonteCarloRequest_& request) try {
        const auto context = McContext(instrument, model, request);
        auto settings = McSettings(request);
        Vector_<CalibrationReportParameter_> parameters;
        if (settings.aad_) {
            CheckInput(bool(context.curve_) && bool(model), "UnsupportedFastEvaluator", "/model", "matched Gaussian model");
            parameters = McParameters(model);
            for (const auto& parameter : parameters)
                settings.parameterLabels_.push_back(parameter.id_);
        }
        const auto plan = PrepareCalibrationMc(instrument, request.measureId_, context, settings);
        const auto value = plan.Evaluate(instrument, context, false);
        const auto data = McReport(instrument, request, plan, value, parameters);
        return {value.mean_, value.standardError_,
                Handle_<CalibrationReport_>(new CalibrationReport_(CalibrationStage_::Value_::Evaluate, {}, {}, data))};
    } catch (const CalibrationError_& error) {
        auto failure = std::make_shared<CalibrationFailure_>(error.Failure());
        auto& diagnostic = failure->diagnostic_;
        diagnostic.measureId_ = request.measureId_;
        CalibrationReportSnapshot_ data;
        data.summary_.domain_ = "MonteCarlo";
        data.summary_.status_ = "Failed";
        if (instrument) {
            diagnostic.instrumentId_ = instrument->Id();
            data.observations_.push_back(McObservation(instrument, request));
        }
        failure->report_.reset(new CalibrationReport_(diagnostic.stage_, error.Failure().report_->Axes(), {diagnostic}, data));
        throw CalibrationError_(__FILE__, __LINE__, __func__, failure);
    }
    Matrix_<> CalibrationReportQuoteJacobian(const Handle_<CalibrationReport_>& report) {
        CheckInput(report && report->Data().quoteJacobian_, "DerivativeUnavailable", "/quote_jacobian", "available raw model-measure Jacobian");
        return *report->Data().quoteJacobian_;
    }
} // namespace Dal
