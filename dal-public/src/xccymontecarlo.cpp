#include <dal/platform/platform.hpp>

#include <dal-public/src/xccymontecarlo.hpp>
#include <dal/calibration/validation_internal.hpp>

namespace Dal {
    namespace {
        using namespace CalibrationInternal;
        CalibrationPreparationContext_ Context(const Handle_<XccyGaussianModelData_>& model, const Handle_<MarketFixingSnapshot_>& fixings) {
            Check(bool(model), "InvalidCalibrationPlan", "/model", "nonnull typed XCCY Gaussian model");
            const auto& market = *model->market_;
            XccyScriptContext_ xccy;
            xccy.valuationTime_ = market.valuationTime_;
            xccy.pair_ = market.pair_;
            xccy.collateralCurrency_ = market.collateralCurrency_;
            xccy.discountRoute_ = market.discountRoute_;
            xccy.fixings_ = fixings;
            CalibrationPreparationContext_ result;
            result.xccy_ = std::make_shared<const XccyScriptContext_>(xccy);
            return result;
        }
        XccyMcSettings_ Settings(const XccyMonteCarloRequest_& request) {
            const auto& sampling = request.sampling_;
            Check(Bytes(sampling.mode_) == "PseudoRandom", "InvalidCalibrationPlan", "/sampling/mode", "PseudoRandom", sampling.mode_);
            const auto& simulation = request.simulation_;
            Check(!simulation.useBb_ && simulation.smooth_ == 0. && Bytes(simulation.normalPrecision_) == "Precise", "InvalidCalibrationPlan",
                  "/simulation", "zero smoothing, Precise normals and no Brownian bridge");
            Check(request.derivativeMethod_ == CalibrationDerivative_::Value_::None ||
                      request.derivativeMethod_ == CalibrationDerivative_::Value_::AAD,
                  "DerivativeUnavailable", "/derivative_method", "explicit None or AAD with ordered native labels");
            XccyMcSettings_ result;
            result.phase_ = request.phase_;
            result.batches_ = sampling.batches_;
            result.pathsPerBatch_ = sampling.pathsPerBatch_;
            result.seed_ = sampling.seed_;
            result.streamVersion_ = sampling.streamVersion_;
            result.compiled_ = simulation.compiled_.value_or(Script::DEFAULT_COMPILED);
            result.aad_ = request.derivativeMethod_ == CalibrationDerivative_::Value_::AAD;
            result.parameterLabels_ = request.parameterLabels_;
            return result;
        }
        [[noreturn]] void Own(const Handle_<CalibrationInstrument_>& instrument, const String_& measure, const CalibrationError_& error) {
            auto failure = std::make_shared<CalibrationFailure_>(error.Failure());
            failure->diagnostic_.measureId_ = measure;
            if (instrument)
                failure->diagnostic_.instrumentId_ = instrument->Id();
            auto snapshot = failure->report_->Data();
            if (instrument && snapshot.observations_.empty()) {
                CalibrationReportObservation_ row;
                row.instrument_ = instrument;
                row.measureId_ = measure;
                snapshot.observations_.push_back(row);
            }
            failure->report_.reset(new CalibrationReport_(failure->diagnostic_.stage_, {}, {failure->diagnostic_}, snapshot));
            throw CalibrationError_(__FILE__, __LINE__, __func__, failure);
        }
    } // namespace

    Handle_<XccyGaussianModelData_> NewXccyGaussianModel(const String_& name, const XccyGaussianModelSpec_& spec) {
        return Handle_<XccyGaussianModelData_>(new XccyGaussianModelData_(name, spec));
    }
    XccyMonteCarloResult_ ValueXccyInstrumentByMonteCarlo(const Handle_<CalibrationInstrument_>& instrument,
                                                          const Handle_<XccyGaussianModelData_>& model,
                                                          const XccyMonteCarloRequest_& request) try {
        const auto settings = Settings(request);
        auto details = std::make_shared<const XccyMcResult_>(
            PreparedXccyMc_(instrument, request.measureId_, Context(model, request.fixings_), model, settings).Evaluate());
        XccyMonteCarloResult_ result;
        result.mean_ = details->estimate_.mean_;
        result.standardError_ = details->estimate_.standardError_;
        result.report_ = details->report_;
        result.details_ = std::move(details);
        return result;
    } catch (const CalibrationError_& error) {
        Own(instrument, request.measureId_, error);
    }
    XccyGaussianValue_ ValueXccyInstrumentByGaussian(const Handle_<CalibrationInstrument_>& instrument,
                                                     const Handle_<XccyGaussianModelData_>& model,
                                                     const String_& measure,
                                                     const Handle_<MarketFixingSnapshot_>& fixings) try {
        const auto prepared = PrepareCalibrationInstrument(instrument, measure, Context(model, fixings));
        const auto value = EvaluateXccyGaussianMoments<double>(prepared, *model);
        const double scalar = value.Value(prepared);
        CalibrationReportSnapshot_ snapshot;
        snapshot.summary_.domain_ = "XCCYGaussian";
        snapshot.summary_.status_ = "Evaluated";
        snapshot.summary_.contextIdentity_ = Fingerprint(Obj({{"schema", Str("dal.xccy-gaussian-public-context/1")},
                                                              {"source_context", Str(prepared.ContextIdentity())},
                                                              {"model", Str(model->Identity())}}));
        CalibrationReportObservation_ row;
        row.instrument_ = instrument;
        row.measureId_ = measure;
        row.unit_ = prepared.Measure().unit_.Token();
        row.backend_ = "FastGaussian";
        row.derivativeMethod_ = "None";
        row.modelValue_ = scalar;
        row.planIdentity_ = prepared.PlanIdentity();
        snapshot.observations_.push_back(row);
        CalibrationReportPlan_ plan;
        plan.measureId_ = measure;
        plan.ruleId_ = "dal.xccy-gaussian-moments/1";
        plan.backend_ = row.backend_;
        plan.derivativeMethod_ = row.derivativeMethod_;
        plan.sourceIdentity_ = instrument->Definition()->SourceIdentity();
        plan.definitionIdentity_ = instrument->Definition()->DefinitionIdentity();
        plan.bindingIdentity_ = instrument->BindingIdentity();
        plan.planIdentity_ = prepared.PlanIdentity();
        plan.compilerIdentity_ = prepared.CompilerIdentity();
        plan.contextIdentity_ = *snapshot.summary_.contextIdentity_;
        snapshot.plans_.push_back(plan);
        snapshot.execution_.modelIdentity_ = model->Identity();
        snapshot.execution_.valuationDate_ = model->market_->valuationTime_.Date();
        snapshot.execution_.modelTimeBasis_ = "ACT365F";
        snapshot.execution_.numeraireConvention_ = "dal.domestic-collateral-bank/1";
        return {value.a_, value.b_, scalar,
                Handle_<CalibrationReport_>(new CalibrationReport_(CalibrationStage_::Value_::Evaluate, {}, {}, snapshot))};
    } catch (const CalibrationError_& error) {
        Own(instrument, measure, error);
    }
    const XccyMcResult_& XccyMonteCarloDetails(const XccyMonteCarloResult_& result) {
        Check(bool(result.details_), "CalibrationCandidateUnavailable", "/result/xccy", "owning XCCY MC result extension");
        return *result.details_;
    }
    String_ DumpXccyMonteCarloResultJson(const XccyMonteCarloResult_& result) {
        const auto& value = XccyMonteCarloDetails(result);
        const auto& estimate = value.estimate_;
        const auto& ledger = value.reservation_;
        return String_(Jcs(Obj({{"schema", Str("dal.xccy-gaussian-public-result/1")},
                                {"core_schema", Str(value.version_)},
                                {"model_identity", Str(value.modelIdentity_)},
                                {"mapping_identity", Str(value.mapping_.identity_)},
                                {"transition_layout", Str("dal.xccy-gaussian-exact/1")},
                                {"normal_coordinates", Array(value.mapping_.normalCoordinates_, [](const auto& x) { return Str(x); })},
                                {"economic_factors", Num(3)},
                                {"dimension", Str(OrdinalText(value.mapping_.dimension_))},
                                {"phase", Str(ledger.phase_.String())},
                                {"range_layout", Str(ledger.version_)},
                                {"range_identity", Str(ledger.identity_)},
                                {"ranges", Array(ledger.ranges_,
                                                 [](const auto& x) {
                                                     return Obj({{"first_path", Str(OrdinalText(x.firstPath_))},
                                                                 {"path_end", Str(OrdinalText(x.pathEnd_))},
                                                                 {"first_coordinate", Str(OrdinalText(x.firstCoordinate_))},
                                                                 {"coordinate_end", Str(OrdinalText(x.coordinateEnd_))}});
                                                 })},
                                {"validation", Bool(estimate.validation_)},
                                {"held_out", Bool(estimate.heldOut_)},
                                {"consumed_coordinates", Str(OrdinalText(value.consumedCoordinates_))},
                                {"mean", Num(estimate.mean_)},
                                {"standard_error", Num(estimate.standardError_)},
                                {"a", Num(value.coefficients_.a_)},
                                {"b", Num(value.coefficients_.b_)},
                                {"da", Array(value.coefficients_.da_, [](double x) { return Num(x); })},
                                {"db", Array(value.coefficients_.db_, [](double x) { return Num(x); })},
                                {"coefficient_covariance",
                                 [&] {
                                     auto rows = Json_::Array();
                                     for (int i = 0; i < value.coefficientCovariance_.Rows(); ++i) {
                                         auto row = Json_::Array();
                                         for (int j = 0; j < value.coefficientCovariance_.Cols(); ++j)
                                             row.array_.push_back(Num(value.coefficientCovariance_(i, j)));
                                         rows.array_.push_back(row);
                                     }
                                     return rows;
                                 }()},
                                {"parameter_labels", Array(estimate.parameterLabels_, [](const auto& x) { return Str(x); })},
                                {"gradient", Array(estimate.gradient_, [](double x) { return Num(x); })},
                                {"gradient_standard_error", Array(estimate.gradientStandardError_, [](double x) { return Num(x); })},
                                {"quote_risk_available", Bool(false)}})));
    }
    XccyMcValidation_
    ValidateXccyMonteCarlo(const XccyMonteCarloResult_& result, const CalibrationMcReference_& reference, const CalibrationMcPrecision_& precision) {
        return ValidateXccyMc(XccyMonteCarloDetails(result), reference, precision);
    }
    Handle_<CalibrationReport_> XccyGaussianInstrumentCapabilities(const Handle_<CalibrationInstrument_>& instrument,
                                                                   const Handle_<XccyGaussianModelData_>& model,
                                                                   const XccyMonteCarloRequest_& request) {
        CalibrationReportSnapshot_ snapshot;
        snapshot.summary_.domain_ = "XCCYGaussian";
        snapshot.summary_.status_ = "Capabilities";
        for (const bool mc : {false, true}) {
            CalibrationReportPlan_ row;
            row.measureId_ = request.measureId_;
            row.backend_ = mc ? "ScriptMC" : "FastGaussian";
            row.derivativeMethod_ = mc && request.derivativeMethod_ == CalibrationDerivative_::Value_::AAD ? "AAD" : "None";
            try {
                const auto context = Context(model, request.fixings_);
                const auto prepared = PrepareCalibrationInstrument(instrument, request.measureId_, context);
                row.sourceIdentity_ = instrument->Definition()->SourceIdentity();
                row.definitionIdentity_ = instrument->Definition()->DefinitionIdentity();
                row.bindingIdentity_ = instrument->BindingIdentity();
                row.contextIdentity_ = prepared.ContextIdentity();
                row.compilerIdentity_ = prepared.CompilerIdentity();
                if (mc) {
                    const PreparedXccyMc_ plan(instrument, request.measureId_, context, model, Settings(request));
                    row.ruleId_ = "dal.xccy-gaussian-script-mc/1";
                    row.planIdentity_ = plan.PlanIdentity();
                } else {
                    static_cast<void>(LowerXccyGaussianMoments(prepared));
                    row.ruleId_ = "dal.xccy-gaussian-moments/1";
                    row.planIdentity_ = prepared.PlanIdentity();
                }
            } catch (const CalibrationError_& error) {
                row.admitted_ = false;
                row.reasonCode_ = error.Failure().diagnostic_.code_;
                row.reason_ = error.Failure().diagnostic_.message_;
            }
            snapshot.plans_.push_back(row);
        }
        return Handle_<CalibrationReport_>(new CalibrationReport_(CalibrationStage_::Value_::Prepare, {}, {}, snapshot));
    }
    const Handle_<XccyGaussianModelData_>& XccyInstrumentResultGaussianModel(const XccyInstrumentCalibrationResult_& result) {
        Check(bool(result.gaussianModel_), "CalibrationCandidateUnavailable", "/result/gaussian_model", "validated stochastic staged model");
        return result.gaussianModel_;
    }
} // namespace Dal
