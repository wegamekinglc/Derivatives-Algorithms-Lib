#include <exception>

#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/math/optimization/boundedgn.hpp>
#include <dal/model/gsrscriptcalibration.hpp>

namespace Dal {
    namespace {
        using namespace CalibrationInternal;
        using namespace BoundedGaussNewton;

        void Tolerances(const GSRScriptCalibrationSettings_& s) {
            const auto& o = s.optimizer_;
            for (double v : {o.gradientTolerance_, o.stepTolerance_, o.finiteDifferenceStep_, o.parameterScale_, s.validationSigma_})
                Check(std::isfinite(v) && v > 0., "InvalidCalibrationPlan", "/settings", "finite positive tolerances, scales and sigma");
            for (double v : {o.priorWeight_, o.smoothingWeight_, s.epsilonValue_, s.epsilonGradient_})
                Check(std::isfinite(v) && v >= 0., "InvalidCalibrationPlan", "/settings", "finite nonnegative weights and floors");
        }
        void Settings(const GSRScriptCalibrationSettings_& s) {
            const auto& o = s.optimizer_;
            Check(o.maxIterations_ > 0 && s.maxBasketEvaluations_ > 0 && s.maxBasketEvaluations_ <= INT_MAX, "InvalidCalibrationPlan",
                  "/settings/budget", "positive iteration and exact basket limits");
            Tolerances(s);
            Check(std::isfinite(o.numericalErrorFraction_) && o.numericalErrorFraction_ > 0. && o.numericalErrorFraction_ < 1.,
                  "InvalidCalibrationPlan", "/settings/numerical_error_fraction", "fraction strictly between zero and one");
            Check(s.derivativeMethod_ == CalibrationDerivative_::Value_::AAD ||
                      s.derivativeMethod_ == CalibrationDerivative_::Value_::FiniteDifference,
                  "DerivativeUnavailable", "/settings/derivative_method", "AAD rows or basket finite differences");
        }
        CalibrationDiagnostic_ RowDiagnostic(const CalibrationObservation_& row, CalibrationDiagnostic_ d = {}) {
            d.stage_ = CalibrationStage_::Value_::Solve;
            d.observationId_ = row.Id();
            d.instrumentId_ = row.Instrument()->Id();
            d.quoteId_ = row.Quote()->Id();
            d.measureId_ = row.MeasureId();
            return d;
        }
        [[noreturn]] void RowFailure(const std::exception& error,
                                     const CalibrationObservation_& row,
                                     const CalibrationObservationAxes_& axes,
                                     CalibrationStage_ stage,
                                     bool heldOut) {
            auto d = RowDiagnostic(row);
            if (const auto* calibration = dynamic_cast<const CalibrationError_*>(&error))
                d = RowDiagnostic(row, calibration->Failure().diagnostic_);
            else {
                d.code_ = "CalibrationNumericalFailure";
                d.field_ = "/solver";
                d.message_ = error.what();
            }
            d.stage_ = stage;
            d.message_ = d.code_ + (heldOut ? ": held-out; " : ": fit; ") + d.message_;
            CalibrationFailure_ failure{d, Handle_<CalibrationReport_>(new CalibrationReport_(stage, axes, {d})), {}};
            throw CalibrationError_(__FILE__, __LINE__, __func__, std::make_shared<const CalibrationFailure_>(std::move(failure)));
        }
        template <class F_>
        auto InRow(const CalibrationObservation_& row,
                   const CalibrationObservationAxes_& axes,
                   CalibrationStage_ stage,
                   const F_& action,
                   bool heldOut = false) -> decltype(action()) {
            try {
                return action();
            } catch (const std::exception& error) {
                RowFailure(error, row, axes, stage, heldOut);
            }
        }
        void PriceMeasure(const CalibrationObservation_& row) {
            const auto& measures = row.Instrument()->Definition()->Spec().measures_;
            const auto measure =
                std::find_if(measures.begin(), measures.end(), [&](const auto& m) { return Bytes(m.id_) == Bytes(row.MeasureId()); });
            Check(measure != measures.end() && measure->kind_ == CalibrationMeasureKind_::Value_::Price, "UnsupportedCalibrationMeasure",
                  "/observations", "native-unit Price observations", "", RowDiagnostic(row));
        }
        void PriceTarget(const CalibrationObservation_& row, const CalibrationPreparationCapability_& capability) {
            const auto rule = Bytes(capability.ruleId_);
            const bool vanilla = rule == "dal.gaussian-bond-option/1" || rule == "dal.gaussian-caplet/1" || rule == "dal.gaussian-swaption/1";
            Check(!vanilla || row.Quote()->Value() >= 0., "InvalidCalibrationQuote", "/observations/quote/value",
                  "nonnegative vanilla option Price target", "", RowDiagnostic(row));
        }
        PreparedCalibrationMc_ PreparedRow(const CalibrationObservation_& row,
                                           const CalibrationObservationAxes_& axes,
                                           const CalibrationPreparationContext_& context,
                                           const CalibrationMcSettings_& sampling,
                                           bool heldOut = false) {
            return InRow(
                row, axes, CalibrationStage_::Value_::Prepare,
                [&] {
                    PriceMeasure(row);
                    auto plan = PrepareCalibrationMc(row.Instrument(), row.MeasureId(), context, sampling);
                    PriceTarget(row, plan.Capability());
                    return plan;
                },
                heldOut);
        }
        void RequireDrawStride(const Vector_<PreparedCalibrationMc_>& plans,
                               const CalibrationObservationAxes_& axes,
                               size_t dimension,
                               bool heldOut = false) {
            for (size_t i = 0; i < plans.size(); ++i)
                InRow(
                    *axes.observations_[i], axes, CalibrationStage_::Value_::Prepare,
                    [&] {
                        const auto& mapping = plans[i].Mapping();
                        Check(mapping.dimension_ == dimension && mapping.normalCoordinates_.size() == dimension, "InvalidCalibrationPlan",
                              "/mc/mapping/dimension", "common normal draws per path across fit and held-out plans: " + OrdinalText(dimension),
                              OrdinalText(mapping.dimension_), RowDiagnostic(*axes.observations_[i]));
                    },
                    heldOut);
        }
        Vector_<PreparedCalibrationMc_> HeldPlans(const CalibrationPreparationContext_& c,
                                                  const CalibrationObservationAxes_& axes,
                                                  const GSRScriptCalibrationSettings_& settings,
                                                  GSRScriptCalibrationCosts_* costs) {
            auto sampling = settings.sampling_;
            sampling.aad_ = false;
            sampling.parameterLabels_.clear();
            sampling.retainPaths_ = false;
            if (!axes.observations_.empty())
                InRow(
                    *axes.observations_.front(), axes, CalibrationStage_::Value_::Prepare,
                    [&] {
                        Check(sampling.heldOutBatches_ >= 2, "InvalidCalibrationPlan", "/mc/held_out", "explicit held-out batches when requested");
                    },
                    true);
            Vector_<PreparedCalibrationMc_> result;
            for (const auto& row : axes.observations_) {
                result.push_back(PreparedRow(*row, axes, c, sampling, true));
                ++costs->preparations_;
            }
            return result;
        }
        const MultiFactorGSRModelData_& Initial(const CalibrationPreparationContext_& c) {
            Check(c.model_ && typeid(*c.model_) == typeid(MultiFactorGSRModelData_), "UnsupportedFastEvaluator", "/context/model",
                  "native MultiFactor Gaussian selected-g model");
            return static_cast<const MultiFactorGSRModelData_&>(*c.model_);
        }
        double NormalizedInitial(const GSRCalibrationParameter_& p, double g, double scale) {
            Check(std::isfinite(p.lower_) && std::isfinite(p.upper_) && p.lower_ >= 0. && p.upper_ > p.lower_ && std::isfinite(p.upper_ / scale) &&
                      g >= p.lower_ && g <= p.upper_,
                  "InvalidCalibrationPlan", "/parameters", "finite resolvable bounds containing initial g");
            return g / scale;
        }
        class Problem_ {
            const CalibrationPreparationContext_& context_;
            const CalibrationObservationAxes_& axes_;
            const Vector_<Handle_<CalibrationObservation_>>& rows_;
            const Vector_<GSRCalibrationParameter_>& parameters_;
            const GSRScriptCalibrationSettings_& settings_;
            GSRScriptCalibrationCosts_* costs_;
            Handle_<Storable_>* lastCandidate_;
            Vector_<> guess_;
            Vector_<PreparedCalibrationMc_> valuePlans_, aadPlans_;
            Matrix_<> penalties_;

            [[nodiscard]] Vector_<PreparedCalibrationMc_> Plans(bool aad) const {
                auto sampling = settings_.sampling_;
                sampling.aad_ = aad;
                sampling.retainPaths_ = false;
                sampling.parameterLabels_ = aad ? CalibrationMcGLabels(context_.model_, parameters_) : Vector_<String_>{};
                Vector_<PreparedCalibrationMc_> plans;
                for (const auto& row : rows_) {
                    Check(settings_.epsilonValue_ < settings_.optimizer_.numericalErrorFraction_ * row->ErrorScale() &&
                              settings_.epsilonGradient_ <
                                  settings_.optimizer_.numericalErrorFraction_ * row->ErrorScale() / settings_.optimizer_.parameterScale_,
                          "InvalidCalibrationPlan", "/settings/floors", "floors below each frozen numerical budget", "", RowDiagnostic(*row));
                    plans.push_back(PreparedRow(*row, axes_, context_, sampling));
                    ++costs_->preparations_;
                }
                return plans;
            }
            [[nodiscard]] Vector_<> SmoothingRow(int factor, int knot) const {
                const auto& v = *Initial(context_).vol_;
                const double weight =
                    std::sqrt(settings_.optimizer_.smoothingWeight_ / ((v.gKnotDates_[knot] - v.gKnotDates_[knot - 1]) / DAYS_PER_YEAR));
                Vector_<> row(parameters_.size(), 0.);
                for (size_t i = 0; i < parameters_.size(); ++i)
                    if (parameters_[i].factor_ == factor && (parameters_[i].knot_ == knot || parameters_[i].knot_ == knot - 1))
                        row[i] = parameters_[i].knot_ == knot ? weight : -weight;
                return row;
            }
            void BuildPenalties() {
                Vector_<Vector_<>> rows;
                const auto& o = settings_.optimizer_;
                if (o.priorWeight_ > 0.)
                    for (size_t i = 0; i < parameters_.size(); ++i) {
                        Vector_<> row(parameters_.size(), 0.);
                        row[i] = std::sqrt(o.priorWeight_);
                        rows.push_back(row);
                    }
                const auto& v = *Initial(context_).vol_;
                if (o.smoothingWeight_ > 0.)
                    for (int f = 0; f < v.gValues_.Rows(); ++f)
                        for (int k = 1; k < v.gValues_.Cols(); ++k) {
                            const auto row = SmoothingRow(f, k);
                            if (std::any_of(row.begin(), row.end(), [](double value) { return value != 0.; }))
                                rows.push_back(row);
                        }
                penalties_ = Matrix_<>(rows.size(), parameters_.size(), 0.);
                for (size_t i = 0; i < rows.size(); ++i)
                    std::copy(rows[i].begin(), rows[i].end(), penalties_.Row(i).begin());
            }

        public:
            // Explicit counters belong to this calibration, while plans and models are immutable.
            Problem_(const CalibrationPreparationContext_& c,
                     const CalibrationObservationAxes_& axes,
                     const Vector_<GSRCalibrationParameter_>& parameters,
                     const GSRScriptCalibrationSettings_& settings,
                     GSRScriptCalibrationCosts_* costs,
                     Handle_<Storable_>* lastCandidate)
                : context_(c), axes_(axes), rows_(axes.observations_), parameters_(parameters), settings_(settings), costs_(costs),
                  lastCandidate_(lastCandidate) {
                Check(!rows_.empty() && !parameters.empty(), "InvalidCalibrationPlan", "/observations", "nonempty Price rows and g coordinates");
                static_cast<void>(CalibrationMcGLabels(c.model_, parameters));
                const auto& g = Initial(c).vol_->gValues_;
                for (const auto& p : parameters)
                    guess_.push_back(NormalizedInitial(p, g(p.factor_, p.knot_), settings.optimizer_.parameterScale_));
                BuildPenalties();
                valuePlans_ = Plans(false);
                if (settings.derivativeMethod_ == CalibrationDerivative_::Value_::AAD)
                    aadPlans_ = Plans(true);
            }
            void RequireStreams(const Vector_<PreparedCalibrationMc_>& heldPlans, const CalibrationObservationAxes_& heldAxes) const {
                const auto dimension = valuePlans_.front().Mapping().dimension_;
                RequireDrawStride(valuePlans_, axes_, dimension);
                RequireDrawStride(aadPlans_, axes_, dimension);
                RequireDrawStride(heldPlans, heldAxes, dimension, true);
            }
            [[nodiscard]] const Vector_<>& Guess() const { return guess_; }
            [[nodiscard]] double Bound(size_t i, bool upper) const {
                return (upper ? parameters_[i].upper_ : parameters_[i].lower_) / settings_.optimizer_.parameterScale_;
            }
            [[nodiscard]] double Native(size_t i, double x) const {
                const auto& parameter = parameters_[i];
                if (x <= Bound(i, false))
                    return parameter.lower_;
                if (x >= Bound(i, true))
                    return parameter.upper_;
                return std::clamp(x * settings_.optimizer_.parameterScale_, parameter.lower_, parameter.upper_);
            }
            [[nodiscard]] Handle_<MultiFactorGSRModelData_> Model(const Vector_<>& x) const {
                const auto& initial = Initial(context_);
                const auto& v = *initial.vol_;
                auto g = v.gValues_;
                for (size_t i = 0; i < x.size(); ++i)
                    g(parameters_[i].factor_, parameters_[i].knot_) = Native(i, x[i]);
                Handle_<MultiFactorGSRVolData_> vol(
                    new MultiFactorGSRVolData_(v.name_, v.factorNames_, v.gKnotDates_, g, v.hKnotDates_, v.hValues_, v.correlations_));
                return Handle_<MultiFactorGSRModelData_>(new MultiFactorGSRModelData_(initial.name_, initial.curve_, vol));
            }
            [[nodiscard]] CalibrationPreparationContext_ Context(const Vector_<>& x) const {
                auto c = context_;
                c.model_ = Handle_<ModelData_>(Model(x));
                return c;
            }
            [[nodiscard]] Vector_<CalibrationMcResult_> Basket(const Vector_<>& x, bool aad, bool validation = false) const {
                CalibrationDiagnostic_ diagnostic;
                diagnostic.stage_ = CalibrationStage_::Value_::Solve;
                Check(costs_->basketEvaluations_ < settings_.maxBasketEvaluations_, "CalibrationBudgetExceeded", "/solver/basket_evaluations",
                      "candidate and final diagnostics within frozen basket budget", OrdinalText(costs_->basketEvaluations_), diagnostic);
                ++costs_->basketEvaluations_;
                const auto c = Context(x);
                const auto& plans = aad ? aadPlans_ : valuePlans_;
                Vector_<CalibrationMcResult_> result;
                for (size_t i = 0; i < rows_.size(); ++i) {
                    result.push_back(
                        InRow(*rows_[i], axes_, validation ? CalibrationStage_::Value_::Validate : CalibrationStage_::Value_::Solve, [&] {
                            const auto plan = plans[i].RefreshG(c);
                            ++costs_->refreshes_;
                            return plan.Evaluate(rows_[i]->Instrument(), c, validation);
                        }));
                    Count(result.back());
                }
                *lastCandidate_ = Handle_<Storable_>(c.model_);
                return result;
            }
            void Count(const CalibrationMcResult_& value) const {
                if (value.parameterLabels_.empty())
                    ++costs_->doubleRows_;
                else
                    ++costs_->aadRows_;
                costs_->batchRuns_ += value.engineRuns_;
                costs_->paths_ += value.pathsPerView_;
                costs_->peakTapeNodes_ = std::max(costs_->peakTapeNodes_, value.peakTapeNodes_);
                costs_->peakTapeBytes_ = std::max(costs_->peakTapeBytes_, value.peakTapeBytes_);
            }
            [[nodiscard]] Vector_<> Penalties(const Vector_<>& x) const {
                Vector_<> result(penalties_.Rows(), 0.);
                for (int i = 0; i < penalties_.Rows(); ++i)
                    for (size_t j = 0; j < x.size(); ++j)
                        result[i] += penalties_(i, j) * (x[j] - guess_[j]);
                return result;
            }
            [[nodiscard]] Vector_<> Residuals(const Vector_<>& x) const {
                const auto values = Basket(x, false);
                Vector_<> result;
                for (size_t i = 0; i < rows_.size(); ++i)
                    result.push_back((values[i].mean_ - rows_[i]->Quote()->Value()) / rows_[i]->ErrorScale());
                for (double penalty : Penalties(x))
                    result.push_back(penalty);
                return result;
            }
            [[nodiscard]] int JacobianEvaluations(size_t columns) const {
                return settings_.derivativeMethod_ == CalibrationDerivative_::Value_::AAD ? 0 : 2 * static_cast<int>(columns);
            }
            [[nodiscard]] Matrix_<> RawJacobian(const Vector_<>& x) const {
                ++costs_->jacobians_;
                Matrix_<> result(rows_.size(), x.size(), 0.);
                if (settings_.derivativeMethod_ == CalibrationDerivative_::Value_::AAD) {
                    const auto values = Basket(x, true);
                    for (size_t i = 0; i < values.size(); ++i)
                        for (size_t j = 0; j < x.size(); ++j)
                            result(i, j) = values[i].gradient_[j];
                } else {
                    for (size_t j = 0; j < x.size(); ++j) {
                        auto up = x, down = x;
                        const double step = settings_.optimizer_.finiteDifferenceStep_ * std::max(1., std::abs(x[j]));
                        up[j] = std::min(Bound(j, true), x[j] + step);
                        down[j] = std::max(Bound(j, false), x[j] - step);
                        const double distance = Native(j, up[j]) - Native(j, down[j]);
                        if (!std::isfinite(distance) || distance <= 0.) {
                            auto d = RowDiagnostic(*rows_[0]);
                            d.parameterKey_ = CalibrationMcGLabels(context_.model_, parameters_)[j];
                            Fail("DerivativeUnavailable", "/solver/finite_difference_step", "resolvable feasible native step", "", d);
                        }
                        const auto high = Basket(up, false), low = Basket(down, false);
                        for (size_t i = 0; i < rows_.size(); ++i)
                            result(i, j) = (high[i].mean_ - low[i].mean_) / distance;
                    }
                }
                return result;
            }
            [[nodiscard]] Matrix_<> Jacobian(const Vector_<>& x, size_t rows) const {
                const auto raw = RawJacobian(x);
                Matrix_<> result(rows, x.size(), 0.);
                for (size_t i = 0; i < rows_.size(); ++i)
                    for (size_t j = 0; j < x.size(); ++j) {
                        const double value = raw(i, j) * (settings_.optimizer_.parameterScale_ / rows_[i]->ErrorScale());
                        Check(std::isfinite(value) && (raw(i, j) == 0. || value != 0.), "CalibrationNumericalFailure", "/solver/jacobian",
                              "representable scaled partial", "", RowDiagnostic(*rows_[i]));
                        result(i, j) = value;
                    }
                for (int i = 0; i < penalties_.Rows(); ++i)
                    for (size_t j = 0; j < x.size(); ++j)
                        result(rows_.size() + i, j) = penalties_(i, j);
                return result;
            }
        };

        CalibrationMcPrecision_ Precision(const CalibrationObservation_& row, const GSRScriptCalibrationSettings_& s, size_t n) {
            CalibrationMcPrecision_ p;
            p.errorScale_ = row.ErrorScale();
            p.sigma_ = s.validationSigma_;
            p.numericalErrorFraction_ = p.gradientErrorFraction_ = s.optimizer_.numericalErrorFraction_;
            p.epsilonValue_ = s.epsilonValue_;
            p.parameterScales_ = Vector_<>(n, s.optimizer_.parameterScale_);
            p.epsilonGradient_ = Vector_<>(n, s.epsilonGradient_);
            return p;
        }
        void Finalize(const Problem_& problem, const Vector_<>& x, const GSRScriptCalibrationSettings_& s, GSRScriptCalibrationResult_* r) {
            const bool aad = s.derivativeMethod_ == CalibrationDerivative_::Value_::AAD;
            r->model_ = problem.Model(x);
            r->fit_ = problem.Basket(x, aad);
            r->validation_ = problem.Basket(x, aad, true);
            r->quoteJacobian_ = problem.RawJacobian(x);
            RankDiagnostics(r->quoteJacobian_, r);
            r->fitWithinTolerance_ = r->numericalValidationPassed_ = true;
            for (size_t i = 0; i < r->axes_.observations_.size(); ++i) {
                const auto& row = *r->axes_.observations_[i];
                r->modelPrices_.push_back(r->fit_[i].mean_);
                r->residuals_.push_back(r->fit_[i].mean_ - row.Quote()->Value());
                r->scaledResiduals_.push_back(r->residuals_.back() / row.ErrorScale());
                r->fitWithinTolerance_ &= std::abs(r->scaledResiduals_.back()) <= 1.;
                r->validationIndicators_.push_back(InRow(row, r->axes_, CalibrationStage_::Value_::Validate, [&] {
                    return ValidateCalibrationMc(r->fit_[i], r->validation_[i], Precision(row, s, aad ? x.size() : 0));
                }));
                r->numericalErrors_.push_back(r->validationIndicators_.back().errorIndicator_);
                r->numericalValidationPassed_ &= r->validationIndicators_.back().numericalValidationPassed_;
            }
            r->penaltyResiduals_ = problem.Penalties(x);
            r->dataObjective_ = NormSquared(r->scaledResiduals_);
            r->penaltyObjective_ = NormSquared(r->penaltyResiduals_);
            r->objective_ = r->dataObjective_ + r->penaltyObjective_;
            for (size_t j = 0; j < x.size(); ++j) {
                r->parameters_.push_back(problem.Native(j, x[j]));
                r->activeBounds_.push_back(x[j] == problem.Bound(j, false) || x[j] == problem.Bound(j, true));
                const double step = s.optimizer_.finiteDifferenceStep_ * std::max(1., std::abs(x[j]));
                r->oneSidedColumns_.push_back(!aad && (x[j] - step < problem.Bound(j, false) || x[j] + step > problem.Bound(j, true)));
            }
        }

        std::optional<double> ReportNumber(double value) { return std::isfinite(value) ? std::optional<double>(value) : std::nullopt; }

        CalibrationReportSummary_ CompletedSummary(const GSRScriptCalibrationResult_& result) {
            CalibrationReportSummary_ summary;
            summary.converged_ = result.converged_;
            summary.fitWithinTolerance_ = result.fitWithinTolerance_;
            summary.numericalValidationPassed_ = result.numericalValidationPassed_;
            summary.terminationReason_ = result.terminationReason_;
            summary.objective_ = ReportNumber(result.objective_);
            summary.dataObjective_ = ReportNumber(result.dataObjective_);
            summary.penaltyObjective_ = ReportNumber(result.penaltyObjective_);
            summary.jacobianRank_ = result.jacobianRank_;
            summary.jacobianConditionEstimate_ = ReportNumber(result.jacobianConditionEstimate_);
            return summary;
        }

        bool Accepted(const GSRScriptCalibrationResult_& result) {
            return result.converged_ && result.fitWithinTolerance_ && result.numericalValidationPassed_ &&
                   (!result.heldOutRequested_ || result.heldOutPassed_);
        }

        CalibrationReportSnapshot_ WorkSnapshot(const GSRScriptCalibrationResult_& result, bool finalized, bool failed) {
            CalibrationReportSnapshot_ data;
            auto& summary = data.summary_;
            summary = finalized ? CompletedSummary(result) : CalibrationReportSummary_{};
            summary.domain_ = "GSR";
            summary.status_ = failed ? "Failed" : (Accepted(result) ? "Succeeded" : "Candidate");
            summary.iterations_ = result.iterations_;
            if (result.costs_.basketEvaluations_ <= INT_MAX)
                summary.evaluations_ = static_cast<int>(result.costs_.basketEvaluations_);
            if (result.costs_.jacobians_ <= INT_MAX)
                summary.jacobianEvaluations_ = static_cast<int>(result.costs_.jacobians_);
            return data;
        }
    } // namespace

    GSRScriptCalibrationResult_ CalibrateGSRScriptPrices(const CalibrationPreparationContext_& context,
                                                         const Vector_<Handle_<CalibrationObservation_>>& observations,
                                                         const Vector_<GSRCalibrationParameter_>& parameters,
                                                         const GSRScriptCalibrationSettings_& settings,
                                                         const Vector_<Handle_<CalibrationObservation_>>& heldOut) {
        const auto axes = BuildCalibrationObservationAxes(observations);
        Handle_<Storable_> lastCandidate;
        GSRScriptCalibrationResult_ r;
        r.axes_ = axes;
        bool finalized = false;
        try {
            Settings(settings);
            r.heldOutAxes_ = BuildCalibrationObservationAxes(heldOut);
            r.parameterLabels_ = CalibrationMcGLabels(context.model_, parameters);
            r.derivativeMethod_ = settings.derivativeMethod_;
            const auto heldPlans = HeldPlans(context, r.heldOutAxes_, settings, &r.costs_);
            const Problem_ problem(context, axes, parameters, settings, &r.costs_, &lastCandidate);
            problem.RequireStreams(heldPlans, r.heldOutAxes_);
            auto x = problem.Guess();
            const auto& o = settings.optimizer_;
            Fit(problem, {o.maxIterations_, o.gradientTolerance_, o.stepTolerance_}, &x, &r);
            Finalize(problem, x, settings, &r);
            finalized = true;
            r.heldOutRequested_ = !heldOut.empty();
            r.heldOutPassed_ = r.heldOutRequested_;
            const auto c = problem.Context(x);
            for (size_t i = 0; i < heldOut.size(); ++i) {
                const auto& row = heldOut[i];
                r.heldOut_.push_back(InRow(
                    *row, r.heldOutAxes_, CalibrationStage_::Value_::Validate,
                    [&] {
                        const auto plan = heldPlans[i].RefreshG(c);
                        ++r.costs_.refreshes_;
                        return plan.EvaluateHeldOut(row->Instrument(), c);
                    },
                    true));
                problem.Count(r.heldOut_.back());
                const auto& value = r.heldOut_.back();
                r.heldOutPassed_ &=
                    std::abs(value.mean_ - row->Quote()->Value()) + settings.validationSigma_ * value.standardError_ <= row->ErrorScale();
            }
            r.evaluations_ = static_cast<int>(r.costs_.basketEvaluations_);
            r.report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Validate, axes, {}, WorkSnapshot(r, finalized, false)));
            return r;
        } catch (const Exception_& error) {
            CalibrationDiagnostic_ d;
            auto failureAxes = axes;
            if (const auto* calibration = dynamic_cast<const CalibrationError_*>(&error)) {
                d = calibration->Failure().diagnostic_;
                if (!calibration->Failure().report_->Axes().observations_.empty())
                    failureAxes = calibration->Failure().report_->Axes();
            } else {
                d.stage_ = CalibrationStage_::Value_::Solve;
                d.code_ = "CalibrationNumericalFailure";
                d.field_ = "/solver";
                d.message_ = error.what();
            }
            CalibrationFailure_ failure{
                d, Handle_<CalibrationReport_>(new CalibrationReport_(d.stage_, std::move(failureAxes), {d}, WorkSnapshot(r, finalized, true))),
                lastCandidate};
            throw CalibrationError_(__FILE__, __LINE__, __func__, std::make_shared<const CalibrationFailure_>(std::move(failure)));
        }
    }
} // namespace Dal
