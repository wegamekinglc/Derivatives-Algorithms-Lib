#include <dal/platform/platform.hpp>

#include <climits>
#include <cstdint>
#include <dal-public/src/gsr.hpp>

namespace Dal {
    namespace {
        bool Exact(const String_& s, const char* token) { return std::string(s.data(), s.size()) == token; }
        void Check(bool condition, const String_& field, const String_& expected, const String_& code = "InvalidCalibrationPlan") {
            if (!condition)
                CalibrationInputFailure(code, field, expected);
        }
        int64_t Sampling(const CalibrationMonteCarloSampling_& s, const String_& field) {
            Check(Exact(s.mode_, "PseudoRandom"), field + "/mode", "PseudoRandom");
            Check(Exact(s.streamVersion_, "dal.mrg32-normal-paths/1"), field + "/stream_version", "dal.mrg32-normal-paths/1");
            Check(s.pathsPerBatch_ >= 2, field + "/paths_per_batch", "at least two paths per batch");
            Check(s.batches_ >= 2, field + "/batches", "at least two batches");
            Check(s.seed_ >= 0, field + "/seed", "nonnegative int32 seed");
            return int64_t(s.pathsPerBatch_) * s.batches_;
        }
        void CommonStream(const CalibrationMonteCarloSampling_& fit, const CalibrationMonteCarloSampling_& s, const String_& field) {
            Check(s.seed_ == fit.seed_, field + "/seed", "same seed as fit_sampling");
            Check(s.mode_ == fit.mode_, field + "/mode", "same mode as fit_sampling");
            Check(s.streamVersion_ == fit.streamVersion_, field + "/stream_version", "same stream version as fit_sampling");
        }
        void FastSettings(const GSRInstrumentCalibrationSettings_& s) {
            Check(!s.fitSampling_, "/settings/fit_sampling", "absent for fast calibration");
            Check(!s.validationSampling_, "/settings/validation_sampling", "absent for fast calibration");
            Check(!s.heldOutSampling_, "/settings/held_out_sampling", "absent for fast calibration");
            Check(s.heldOutObservations_.empty(), "/settings/held_out_observations", "empty for fast calibration");
            Check(!s.evaluation_.sampling_, "/settings/evaluation/sampling", "absent for fast calibration");
            Check(s.maxPathsPerView_ == 0, "/settings/max_paths_per_view", "zero for fast calibration");
            const GSRInstrumentCalibrationSettings_ defaults;
            Check(s.maxBasketEvaluations_ == defaults.maxBasketEvaluations_, "/settings/max_basket_evaluations", "default for fast calibration");
            Check(s.validationSigma_ == defaults.validationSigma_, "/settings/validation_sigma", "default for fast calibration");
            Check(s.epsilonValue_ == defaults.epsilonValue_, "/settings/epsilon_value", "default for fast calibration");
            Check(s.epsilonGradient_ == defaults.epsilonGradient_, "/settings/epsilon_gradient", "default for fast calibration");
            Check(s.compiled_ == defaults.compiled_, "/settings/compiled", "default for fast calibration");
        }
        void McSettings(const GSRInstrumentCalibrationSettings_& s) {
            Check(s.evaluation_.allowMonteCarloCalibration_, "/settings/evaluation/allow_monte_carlo_calibration", "explicit MC opt-in");
            Check(bool(s.fitSampling_), "/settings/fit_sampling", "explicit fit sampling");
            Check(bool(s.validationSampling_), "/settings/validation_sampling", "explicit validation sampling");
            Check(s.maxPathsPerView_ > 0, "/settings/max_paths_per_view", "positive path budget");
            const int64_t f = Sampling(*s.fitSampling_, "/settings/fit_sampling");
            const int64_t v = Sampling(*s.validationSampling_, "/settings/validation_sampling");
            const int64_t h = s.heldOutSampling_ ? Sampling(*s.heldOutSampling_, "/settings/held_out_sampling") : 0;
            Check(v > f, "/settings/validation_sampling", "more validation than fit paths", "CalibrationBudgetExceeded");
            // Each product of two int32 values fits int64; compare reserves without summing three products.
            int64_t remaining = s.maxPathsPerView_;
            for (const auto count : {f, v, h}) {
                Check(count <= remaining, "/settings/max_paths_per_view", "fit + validation + held-out paths within budget",
                      "CalibrationBudgetExceeded");
                remaining -= count;
            }
            CommonStream(*s.fitSampling_, *s.validationSampling_, "/settings/validation_sampling");
            if (s.heldOutSampling_)
                CommonStream(*s.fitSampling_, *s.heldOutSampling_, "/settings/held_out_sampling");
            if (s.evaluation_.sampling_) {
                const auto& aggregate = *s.evaluation_.sampling_;
                Check(aggregate.paths_ == f, "/settings/evaluation/sampling/paths", "fit total path count");
                Check(aggregate.seed_ == s.fitSampling_->seed_, "/settings/evaluation/sampling/seed", "fit seed");
                Check(Exact(aggregate.generator_, "mrg32"), "/settings/evaluation/sampling/generator", "mrg32");
            }
            const GSRPricingSettings_ defaults;
            Check(s.solver_.pricing_.quadratureOrder_ == defaults.quadratureOrder_ && s.solver_.pricing_.estimateError_ == defaults.estimateError_,
                  "/settings/solver/pricing", "default native quadrature settings for MC");
        }
        CalibrationPreparationContext_ Context(const Handle_<ModelData_>& initial,
                                               const Vector_<Handle_<CalibrationObservation_>>& rows,
                                               const GSRInstrumentCalibrationSettings_& s) {
            Check(!rows.empty(), "/observations", "nonempty observation basket");
            const auto axes = BuildCalibrationObservationAxes(rows);
            const auto* model = dynamic_cast<const MultiFactorGSRModelData_*>(initial.get());
            Check(model && typeid(*model) == typeid(MultiFactorGSRModelData_), "/initial", "native MultiFactorGSRModelData",
                  "UnsupportedFastEvaluator");
            const auto& header = axes.observations_.front()->Instrument()->Definition()->Spec().header_;
            return {s.valuation_, header.payCurrency_, header.discountRoute_, model->curve_, initial};
        }
        PreparedCalibrationInstrument_ FastAdmission(const CalibrationPreparationContext_& c,
                                                     const CalibrationObservationAxes_& axes,
                                                     const Handle_<CalibrationObservation_>& row,
                                                     const CalibrationEvaluationOptions_& options) try {
            const auto plan = PrepareCalibrationInstrument(row->Instrument(), row->MeasureId(), c, options);
            Check(plan.Capability().backend_ == CalibrationBackend_::Value_::FastGaussian &&
                      std::holds_alternative<CalibrationBondOptionDescriptor_>(plan.Descriptor()),
                  "/observations", "existing Gaussian BondOption calibration driver", "UnsupportedFastEvaluator");
            return plan;
        } catch (const CalibrationError_& error) {
            auto failure = std::make_shared<CalibrationFailure_>(error.Failure());
            auto& d = failure->diagnostic_;
            d.observationId_ = row->Id();
            d.quoteId_ = row->Quote()->Id();
            d.instrumentId_ = row->Instrument()->Id();
            d.measureId_ = row->MeasureId();
            failure->report_.reset(new CalibrationReport_(d.stage_, axes, {d}, failure->report_->Data()));
            throw CalibrationError_(__FILE__, __LINE__, __func__, failure);
        }
        bool AllowMonteCarloFallback(const GSRInstrumentCalibrationSettings_& s, const CalibrationError_& error) {
            return s.evaluation_.evaluation_ == CalibrationEvaluation_::Value_::AutoFast && s.evaluation_.allowMonteCarloCalibration_ &&
                   (Exact(error.Failure().diagnostic_.code_, "UnsupportedFastEvaluator") ||
                    Exact(error.Failure().diagnostic_.code_, "UnsupportedCalibrationProduct"));
        }
        bool Fast(const CalibrationPreparationContext_& c,
                  const Vector_<Handle_<CalibrationObservation_>>& rows,
                  const GSRInstrumentCalibrationSettings_& s) {
            if (s.evaluation_.evaluation_ == CalibrationEvaluation_::Value_::ScriptMC)
                return false;
            Check(s.evaluation_.evaluation_ == CalibrationEvaluation_::Value_::AutoFast ||
                      s.evaluation_.evaluation_ == CalibrationEvaluation_::Value_::RequireFast,
                  "/settings/evaluation/evaluation", "AutoFast, RequireFast or ScriptMC", "UnsupportedFastEvaluator");
            CalibrationEvaluationOptions_ probe;
            probe.evaluation_ = CalibrationEvaluation_::Value_::RequireFast;
            probe.derivativeMethod_ = CalibrationDerivative_::Value_::FiniteDifference;
            const auto axes = BuildCalibrationObservationAxes(rows);
            try {
                for (const auto& row : rows)
                    static_cast<void>(FastAdmission(c, axes, row, probe));
                return true;
            } catch (const CalibrationError_& error) {
                if (!AllowMonteCarloFallback(s, error))
                    throw;
                return false;
            }
        }
        CalibrationDerivative_ Derivative(const GSRInstrumentCalibrationSettings_& s, bool fast) {
            auto d = s.evaluation_.derivativeMethod_;
            if (d == CalibrationDerivative_::Value_::Auto)
                d = fast ? CalibrationDerivative_::Value_::FiniteDifference : CalibrationDerivative_::Value_::AAD;
            Check(d == CalibrationDerivative_::Value_::FiniteDifference || (!fast && d == CalibrationDerivative_::Value_::AAD),
                  "/settings/evaluation/derivative_method", fast ? "FiniteDifference for fast calibration" : "AAD or FiniteDifference for MC",
                  "DerivativeUnavailable");
            return d;
        }
        GSRScriptCalibrationSettings_ CoreSettings(const GSRInstrumentCalibrationSettings_& s, CalibrationDerivative_ derivative) {
            GSRScriptCalibrationSettings_ core;
            core.optimizer_ = s.solver_;
            core.derivativeMethod_ = derivative;
            core.maxBasketEvaluations_ = static_cast<size_t>(s.maxBasketEvaluations_);
            core.validationSigma_ = s.validationSigma_;
            core.epsilonValue_ = s.epsilonValue_;
            core.epsilonGradient_ = s.epsilonGradient_;
            auto& mc = core.sampling_;
            mc.allowMonteCarloCalibration_ = true;
            mc.compiled_ = s.compiled_;
            mc.seed_ = s.fitSampling_->seed_;
            mc.fitBatches_ = s.fitSampling_->batches_;
            mc.fitPathsPerBatch_ = s.fitSampling_->pathsPerBatch_;
            mc.validationBatches_ = s.validationSampling_->batches_;
            mc.validationPathsPerBatch_ = s.validationSampling_->pathsPerBatch_;
            mc.maxPathsPerView_ = static_cast<size_t>(s.maxPathsPerView_);
            if (s.heldOutSampling_) {
                mc.heldOutBatches_ = s.heldOutSampling_->batches_;
                mc.heldOutPathsPerBatch_ = s.heldOutSampling_->pathsPerBatch_;
            }
            return core;
        }
        CalibrationReportObservation_ Observation(const Handle_<CalibrationObservation_>& row, int quoteIndex) {
            CalibrationReportObservation_ out;
            out.instrument_ = row->Instrument();
            out.observationId_ = row->Id();
            out.quoteId_ = row->Quote()->Id();
            out.quoteIndex_ = quoteIndex;
            out.measureId_ = row->MeasureId();
            out.unit_ = row->Quote()->Unit().Token();
            out.representation_ = "Price";
            out.target_ = row->Quote()->Value();
            out.errorScale_ = row->ErrorScale();
            return out;
        }
        void Rows(CalibrationReportSnapshot_* data, const CalibrationObservationAxes_& axes) {
            for (size_t i = 0; i < axes.observations_.size(); ++i)
                data->observations_.push_back(Observation(axes.observations_[i], axes.quoteIndex_[i]));
        }
        std::optional<double> Finite(double x) { return std::isfinite(x) ? std::optional<double>(x) : std::nullopt; }
        void Execution(CalibrationReportSnapshot_* data, const GSRInstrumentCalibrationSettings_& s) {
            data->execution_.valuationDate_ = s.valuation_.evaluationDate_;
            data->execution_.todayFixingPolicy_ = s.valuation_.todayFixingPolicy_.String();
            data->execution_.modelTimeBasis_ = "ACT/365";
            data->execution_.numeraireConvention_ = "dal.native-gsr/1";
        }
        CalibrationReportSampling_ ReportSampling(const CalibrationMonteCarloSampling_& s, const CalibrationMcResult_& value) {
            CalibrationReportSampling_ out{s.mode_, s.streamVersion_, value.mappingIdentity_, "mrg32", "Precise",
                                           s.seed_, s.batches_,       s.pathsPerBatch_};
            for (const auto& range : value.ranges_) {
                out.firstPaths_.push_back(static_cast<double>(range.firstPath_));
                out.pathCounts_.push_back(static_cast<double>(range.pathCount_));
            }
            return out;
        }
        void Parameters(CalibrationReportSnapshot_* data,
                        const MultiFactorGSRModelData_& initial,
                        const Vector_<GSRCalibrationParameter_>& coordinates,
                        const GSRInstrumentCalibrationSettings_& s,
                        const GSRInstrumentCalibrationResult_& r) {
            for (size_t i = 0; i < coordinates.size(); ++i) {
                const auto& p = coordinates[i];
                CalibrationReportParameter_ row;
                row.id_ = r.parameterLabels_[i];
                row.unit_ = "Rate";
                row.initial_ = initial.vol_->gValues_(p.factor_, p.knot_);
                row.value_ = Finite(r.parameters_[i]);
                row.scale_ = s.solver_.parameterScale_;
                row.lower_ = p.lower_;
                row.upper_ = p.upper_;
                row.activeBound_ = r.activeBounds_[i] ? (r.parameters_[i] <= p.lower_ ? "Lower" : "Upper") : "None";
                data->parameters_.push_back(row);
                data->objective_.theta0_.push_back(*row.initial_);
                data->objective_.parameterScales_.push_back(*row.scale_);
                data->objective_.nativeLayout_.push_back(row.id_);
            }
        }
        CalibrationReportPlan_ Plan(const CalibrationReportObservation_& row, const std::optional<String_>& context) {
            CalibrationReportPlan_ plan;
            plan.observationId_ = row.observationId_;
            plan.measureId_ = row.measureId_;
            plan.backend_ = row.backend_;
            plan.derivativeMethod_ = row.derivativeMethod_;
            plan.sourceIdentity_ = row.instrument_->Definition()->SourceIdentity();
            plan.definitionIdentity_ = row.instrument_->Definition()->DefinitionIdentity();
            plan.bindingIdentity_ = row.instrument_->BindingIdentity();
            plan.planIdentity_ = row.planIdentity_;
            plan.contextIdentity_ = context;
            return plan;
        }
        void Validation(CalibrationReportSnapshot_* data,
                        const Handle_<CalibrationObservation_>& row,
                        const CalibrationMcResult_& value,
                        const char* edge,
                        const GSRInstrumentCalibrationSettings_& s,
                        bool accepted) {
            CalibrationReportValidation_ record;
            record.observationId_ = row->Id();
            record.edge_ = edge;
            record.quantity_ = "Price";
            record.reason_ = accepted ? "Accepted" : "Rejected";
            record.value_ = value.mean_;
            record.difference_ = Finite(value.mean_ - row->Quote()->Value());
            record.standardError_ = value.standardError_;
            record.absoluteTolerance_ = row->ErrorScale();
            record.sigmaMultiplier_ = s.validationSigma_;
            record.accepted_ = accepted;
            data->validation_.push_back(record);
        }
        bool Accepted(const GSRInstrumentCalibrationResult_& r) {
            return r.converged_ && r.fitWithinTolerance_ && r.numericalValidationPassed_ && (!r.heldOutRequested_ || r.heldOutPassed_);
        }
        CalibrationReportSummary_ Summary(const GSRInstrumentCalibrationResult_& r) {
            auto summary = r.report_->Data().summary_;
            summary.domain_ = "GSR";
            summary.mode_ = r.backend_;
            summary.status_ = Accepted(r) ? "Succeeded" : "Candidate";
            summary.converged_ = r.converged_;
            summary.fitWithinTolerance_ = r.fitWithinTolerance_;
            summary.numericalValidationPassed_ = r.numericalValidationPassed_;
            summary.terminationReason_ = r.terminationReason_;
            summary.objective_ = Finite(r.objective_);
            summary.iterations_ = r.iterations_;
            if (!Exact(r.backend_, "ScriptMC")) {
                summary.evaluations_ = r.evaluations_;
                summary.jacobianRank_ = r.jacobianRank_;
                summary.jacobianConditionEstimate_ = Finite(r.jacobianConditionEstimate_);
            }
            return summary;
        }
        void ObservationValues(CalibrationReportSnapshot_* data,
                               GSRInstrumentCalibrationResult_& r,
                               const GSRInstrumentCalibrationSettings_& s,
                               const Vector_<String_>& fastIdentities) {
            const bool mc = Exact(r.backend_, "ScriptMC");
            for (size_t i = 0; i < data->observations_.size(); ++i) {
                auto& row = data->observations_[i];
                row.backend_ = r.backend_;
                row.derivativeMethod_ = r.derivativeMethod_.String();
                row.planIdentity_ = mc ? r.fit_[i].planIdentity_ : fastIdentities[i];
                row.modelValue_ = Finite(r.modelPrices_[i]);
                row.rawResidual_ = Finite(r.residuals_[i]);
                row.scaledResidual_ = Finite(r.scaledResiduals_[i]);
                row.fitRowPassed_ = std::abs(r.residuals_[i]) <= r.axes_.observations_[i]->ErrorScale();
                // Publish existing final identities; unavailable driver metadata stays null without another admission.
                data->plans_.push_back(Plan(row, mc ? std::optional<String_>(r.fit_[i].contextIdentity_) : std::nullopt));
                if (mc) {
                    row.standardError_ = r.fit_[i].standardError_;
                    row.validationValue_ = r.validation_[i].mean_;
                    row.validationError_ = Finite(r.validationIndicators_[i].errorIndicator_);
                    Validation(data, r.axes_.observations_[i], r.validation_[i], "Validation", s,
                               r.validationIndicators_[i].numericalValidationPassed_);
                    auto& validation = data->validation_.back();
                    validation.difference_ = Finite(r.validation_[i].mean_ - r.fit_[i].mean_);
                    validation.absoluteTolerance_ = s.solver_.numericalErrorFraction_ * r.axes_.observations_[i]->ErrorScale();
                    validation.reason_ = r.validationIndicators_[i].terminationReason_;
                } else {
                    row.validationError_ = Finite(r.numericalErrors_[i]);
                }
            }
        }
        Handle_<CalibrationReport_> Report(GSRInstrumentCalibrationResult_& r,
                                           const MultiFactorGSRModelData_& initial,
                                           const Vector_<GSRCalibrationParameter_>& parameters,
                                           const GSRInstrumentCalibrationSettings_& s,
                                           const Vector_<String_>& fastIdentities) {
            // Calibration snapshots accumulate owning rows without changing driver work or numerical results.
            auto data = r.report_->Data();
            data.summary_ = Summary(r);
            Execution(&data, s);
            Parameters(&data, initial, parameters, s, r);
            Rows(&data, r.axes_);
            ObservationValues(&data, r, s, fastIdentities);
            if (r.nativeJacobianAvailable_)
                data.quoteJacobian_ = r.quoteJacobian_;
            if (!r.fit_.empty()) {
                data.execution_.sampling_ = ReportSampling(*s.fitSampling_, r.fit_.front());
                data.execution_.validationSampling_ = ReportSampling(*s.validationSampling_, r.validation_.front());
                data.execution_.simulation_ = CalibrationReportSimulation_{false, s.compiled_, 0., "Precise"};
                data.summary_.contextIdentity_ = r.fit_.front().contextIdentity_;
            }
            for (size_t i = 0; i < r.heldOut_.size(); ++i) {
                const auto& row = r.heldOutAxes_.observations_[i];
                const auto& value = r.heldOut_[i];
                Validation(&data, row, value, "HeldOut", s,
                           std::abs(value.mean_ - row->Quote()->Value()) + s.validationSigma_ * value.standardError_ <= row->ErrorScale());
            }
            return Handle_<CalibrationReport_>(new CalibrationReport_(r.report_->Stage(), r.axes_, {}, data));
        }
        void SamplingInputs(const GSRInstrumentCalibrationSettings_& s) {
            Check(s.heldOutObservations_.empty() || s.heldOutSampling_, "/settings/held_out_sampling", "sampling for requested held-out rows");
            Check(!s.heldOutObservations_.empty() || !s.heldOutSampling_, "/settings/held_out_observations", "rows for requested held-out sampling");
            if (s.fitSampling_)
                Sampling(*s.fitSampling_, "/settings/fit_sampling");
            if (s.validationSampling_)
                Sampling(*s.validationSampling_, "/settings/validation_sampling");
            if (s.heldOutSampling_)
                Sampling(*s.heldOutSampling_, "/settings/held_out_sampling");
        }
        void DerivativeSettings(const CalibrationEvaluationOptions_& options) {
            Check(options.derivativeMethod_ == CalibrationDerivative_::Value_::Auto ||
                      options.derivativeMethod_ == CalibrationDerivative_::Value_::FiniteDifference ||
                      options.derivativeMethod_ == CalibrationDerivative_::Value_::AAD,
                  "/settings/evaluation/derivative_method", "Auto, FiniteDifference or driver-supported AAD", "DerivativeUnavailable");
        }
    } // namespace

    void ValidateGSRInstrumentSolverSettings(const GSRCalibrationSettings_& s) {
        Check(s.maxIterations_ > 0, "/settings/solver/max_iterations", "positive iteration limit");
        for (const auto& entry : std::initializer_list<std::pair<const char*, double>>{{"gradient_tolerance", s.gradientTolerance_},
                                                                                       {"step_tolerance", s.stepTolerance_},
                                                                                       {"finite_difference_step", s.finiteDifferenceStep_},
                                                                                       {"parameter_scale", s.parameterScale_}})
            Check(std::isfinite(entry.second) && entry.second > 0., String_("/settings/solver/") + entry.first, "finite positive value");
        for (const auto& entry :
             std::initializer_list<std::pair<const char*, double>>{{"prior_weight", s.priorWeight_}, {"smoothing_weight", s.smoothingWeight_}})
            Check(std::isfinite(entry.second) && entry.second >= 0., String_("/settings/solver/") + entry.first, "finite nonnegative weight");
        Check(std::isfinite(s.numericalErrorFraction_) && s.numericalErrorFraction_ > 0. && s.numericalErrorFraction_ < 1.,
              "/settings/solver/numerical_error_fraction", "fraction strictly between zero and one");
        Check(s.pricing_.quadratureOrder_ >= 2 && s.pricing_.quadratureOrder_ <= 32, "/settings/solver/pricing/quadrature_order", "2 through 32");
    }

    void ValidateGSRInstrumentCalibrationSettings(const GSRInstrumentCalibrationSettings_& s) {
        ValidateGSRInstrumentSolverSettings(s.solver_);
        Check(s.valuation_.evaluationDate_ && s.valuation_.evaluationDate_->IsValid(), "/settings/valuation/evaluation_date", "explicit valid date");
        Check(bool(s.valuation_.fixings_), "/settings/valuation/fixings", "explicit fixing snapshot, including empty");
        Check(s.maxPathsPerView_ >= 0, "/settings/max_paths_per_view", "nonnegative int32 budget");
        Check(s.maxBasketEvaluations_ > 0, "/settings/max_basket_evaluations", "positive int32 budget");
        Check(std::isfinite(s.validationSigma_) && s.validationSigma_ > 0., "/settings/validation_sigma", "finite positive sigma");
        Check(std::isfinite(s.epsilonValue_) && s.epsilonValue_ >= 0., "/settings/epsilon_value", "finite nonnegative epsilon");
        Check(std::isfinite(s.epsilonGradient_) && s.epsilonGradient_ >= 0., "/settings/epsilon_gradient", "finite nonnegative epsilon");
        SamplingInputs(s);
        DerivativeSettings(s.evaluation_);
        if (s.evaluation_.evaluation_ == CalibrationEvaluation_::Value_::ScriptMC)
            McSettings(s);
        if (s.evaluation_.evaluation_ == CalibrationEvaluation_::Value_::RequireFast)
            FastSettings(s);
    }

    GSRInstrumentCalibrationResult_ CalibrateGSRInstruments(const Handle_<ModelData_>& initial,
                                                            const Vector_<Handle_<CalibrationObservation_>>& observations,
                                                            const Vector_<GSRCalibrationParameter_>& parameters,
                                                            const GSRInstrumentCalibrationSettings_& settings) try {
        ValidateGSRInstrumentCalibrationSettings(settings);
        const auto context = Context(initial, observations, settings);
        const bool fast = Fast(context, observations, settings);
        const auto derivative = Derivative(settings, fast);
        GSRInstrumentCalibrationResult_ result;
        Vector_<String_> identities;
        if (fast) {
            FastSettings(settings);
            const auto native = CalibrateGaussianBondOptions(context, observations, parameters, settings.solver_);
            static_cast<GSRCalibrationResult_&>(result) = native;
            result.axes_ = native.axes_;
            result.report_ = native.report_;
            result.parameterLabels_ = native.parameterLabels_;
            result.scaledResiduals_ = native.scaledResiduals_;
            result.oneSidedColumns_ = native.oneSidedColumns_;
            result.nativeJacobianAvailable_ = native.nativeJacobianAvailable_;
            result.fastCosts_ = native.costs_;
            identities = native.planIdentities_;
            result.backend_ = "FastGaussian";
        } else {
            McSettings(settings);
            static_cast<GSRScriptCalibrationResult_&>(result) =
                CalibrateGSRScriptPrices(context, observations, parameters, CoreSettings(settings, derivative), settings.heldOutObservations_);
            result.backend_ = "ScriptMC";
            result.nativeJacobianAvailable_ = true;
        }
        result.derivativeMethod_ = derivative;
        result.report_ = Report(result, *static_cast<const MultiFactorGSRModelData_*>(initial.get()), parameters, settings, identities);
        return result;
    } catch (const CalibrationError_& error) {
        auto failure = std::make_shared<CalibrationFailure_>(error.Failure());
        auto data = failure->report_->Data();
        data.summary_.domain_ = "GSR";
        data.summary_.status_ = "Failed";
        if (data.observations_.empty())
            Rows(&data, failure->report_->Axes());
        failure->report_.reset(new CalibrationReport_(failure->report_->Stage(), failure->report_->Axes(), failure->report_->Errors(), data));
        throw CalibrationError_(__FILE__, __LINE__, __func__, failure);
    }
} // namespace Dal
