#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/math/optimization/boundedgn.hpp>
#include <dal/model/gsr.hpp>
#include <dal/model/gsrbondoption.hpp>
#include <dal/script/calibrationmc.hpp>

namespace Dal {
    namespace {
        using namespace CalibrationInternal;
        using namespace BoundedGaussNewton;

        void Settings(const GSRCalibrationSettings_& s) {
            const String_ code = "InvalidCalibrationPlan";
            Check(s.maxIterations_ > 0, code, "/settings/max_iterations", "positive iteration limit");
            for (double v : {s.gradientTolerance_, s.stepTolerance_, s.finiteDifferenceStep_, s.parameterScale_})
                Check(std::isfinite(v) && v > 0., code, "/settings", "finite positive tolerances, FD step and parameter scale");
            for (double v : {s.priorWeight_, s.smoothingWeight_})
                Check(std::isfinite(v) && v >= 0., code, "/settings", "finite nonnegative penalty weights");
            Check(std::isfinite(s.numericalErrorFraction_) && s.numericalErrorFraction_ > 0. && s.numericalErrorFraction_ < 1., code,
                  "/settings/numerical_error_fraction", "fraction strictly between zero and one");
            Check(s.pricing_.quadratureOrder_ >= 2 && s.pricing_.quadratureOrder_ <= 32, code, "/settings/pricing/quadrature_order", "2 through 32");
        }
        const MultiFactorGSRModelData_& Initial(const CalibrationPreparationContext_& c) {
            Check(c.model_ && typeid(*c.model_) == typeid(MultiFactorGSRModelData_), "UnsupportedFastEvaluator", "/context/model",
                  "native MultiFactor Gaussian model for selected-g calibration");
            return static_cast<const MultiFactorGSRModelData_&>(*c.model_);
        }
        void Rows(const Vector_<Handle_<CalibrationObservation_>>& rows) {
            Check(!rows.empty(), "InvalidCalibrationPlan", "/observations", "nonempty BondOption observations");
            for (const auto& row : rows) {
                CalibrationDiagnostic_ d;
                d.observationId_ = row->Id();
                d.instrumentId_ = row->Instrument()->Id();
                d.quoteId_ = row->Quote()->Id();
                d.measureId_ = row->MeasureId();
                Check(row->Quote()->Value() >= 0., "InvalidCalibrationQuote", "/observations/quote/value", "nonnegative option Price amount", "", d);
            }
        }
        void Parameter(const GSRCalibrationParameter_& p, double initial, double scale) {
            Check(std::isfinite(p.lower_) && std::isfinite(p.upper_) && p.lower_ >= 0. && p.upper_ > p.lower_ && std::isfinite(p.upper_ / scale),
                  "InvalidCalibrationPlan", "/parameters/bounds", "finite resolvable bounds with 0 <= lower < upper");
            Check(initial >= p.lower_ && initial <= p.upper_, "InvalidCalibrationPlan", "/parameters/initial", "initial g inside bounds");
        }
        class Problem_ {
            const CalibrationPreparationContext_& context_;
            const MultiFactorGSRModelData_& initial_;
            const Vector_<Handle_<CalibrationObservation_>>& rows_;
            const Vector_<GSRCalibrationParameter_>& parameters_;
            const GSRCalibrationSettings_& settings_;
            GaussianBondOptionCosts_* costs_;
            Vector_<> guess_;
            Vector_<std::pair<int, int>> smoothPairs_;

        public:
            // Calibration counters are explicit output state; pricing models remain immutable.
            Problem_(const CalibrationPreparationContext_& c,
                     const Vector_<Handle_<CalibrationObservation_>>& rows,
                     const Vector_<GSRCalibrationParameter_>& parameters,
                     const GSRCalibrationSettings_& settings,
                     GaussianBondOptionCosts_* costs)
                : context_(c), initial_(Initial(c)), rows_(rows), parameters_(parameters), settings_(settings), costs_(costs) {
                Check(!parameters.empty(), "InvalidCalibrationPlan", "/parameters", "nonempty selected g coordinates");
                static_cast<void>(CalibrationMcGLabels(c.model_, parameters));
                for (const auto& p : parameters) {
                    const double g = initial_.vol_->gValues_(p.factor_, p.knot_);
                    Parameter(p, g, settings.parameterScale_);
                    guess_.push_back(g / settings.parameterScale_);
                }
                if (settings.smoothingWeight_ > 0.)
                    for (int f = 0; f < initial_.vol_->gValues_.Rows(); ++f)
                        for (int k = 1; k < initial_.vol_->gValues_.Cols(); ++k)
                            if (std::any_of(parameters.begin(), parameters.end(),
                                            [&](const auto& p) { return p.factor_ == f && (p.knot_ == k || p.knot_ == k - 1); }))
                                smoothPairs_.push_back({f, k});
            }
            [[nodiscard]] const Vector_<>& Guess() const { return guess_; }
            [[nodiscard]] double Bound(size_t i, bool upper) const {
                return (upper ? parameters_[i].upper_ : parameters_[i].lower_) / settings_.parameterScale_;
            }
            [[nodiscard]] Handle_<MultiFactorGSRModelData_> Model(const Vector_<>& x) const {
                const auto& v = *initial_.vol_;
                auto g = v.gValues_;
                for (size_t i = 0; i < x.size(); ++i)
                    g(parameters_[i].factor_, parameters_[i].knot_) = x[i] * settings_.parameterScale_;
                Handle_<MultiFactorGSRVolData_> vol(
                    new MultiFactorGSRVolData_(v.name_, v.factorNames_, v.gKnotDates_, g, v.hKnotDates_, v.hValues_, v.correlations_));
                ++costs_->models_;
                return Handle_<MultiFactorGSRModelData_>(new MultiFactorGSRModelData_(initial_.name_, initial_.curve_, vol));
            }
            [[nodiscard]] CalibrationPreparationContext_ Context(const Handle_<MultiFactorGSRModelData_>& model) const {
                auto c = context_;
                c.model_ = Handle_<ModelData_>(model);
                return c;
            }
            [[nodiscard]] PreparedCalibrationInstrument_ Plan(const CalibrationObservation_& row, const CalibrationPreparationContext_& c) const {
                CalibrationEvaluationOptions_ options;
                options.evaluation_ = CalibrationEvaluation_::Value_::RequireFast;
                auto plan = PrepareCalibrationInstrument(row.Instrument(), row.MeasureId(), c, options);
                Check(plan.Capability().backend_ == CalibrationBackend_::Value_::FastGaussian, "UnsupportedFastEvaluator", "/observations",
                      "only complete Gaussian BondOption Price rows");
                ++costs_->preparations_;
                return plan;
            }
            [[nodiscard]] GSRPriceResult_
            Price(const CalibrationObservation_& row, const PreparedCalibrationInstrument_& plan, const CalibrationPreparationContext_& c) const {
                ++costs_->fastPrices_;
                return PriceGaussianBondOption(plan, row.Instrument(), c);
            }
            [[nodiscard]] Vector_<> Residuals(const Vector_<>& x) const {
                const auto c = Context(Model(x));
                Vector_<> result;
                for (const auto& row : rows_) {
                    const auto plan = Plan(*row, c);
                    result.push_back((Price(*row, plan, c).price_ - row->Quote()->Value()) / row->ErrorScale());
                }
                if (settings_.priorWeight_ > 0.)
                    for (size_t i = 0; i < x.size(); ++i)
                        result.push_back(std::sqrt(settings_.priorWeight_) * (x[i] - guess_[i]));
                const auto& g = static_cast<const MultiFactorGSRModelData_&>(*c.model_).vol_->gValues_;
                for (const auto& pair : smoothPairs_) {
                    const auto [f, k] = pair;
                    const double gap = (initial_.vol_->gKnotDates_[k] - initial_.vol_->gKnotDates_[k - 1]) / DAYS_PER_YEAR;
                    const double change = (g(f, k) - initial_.vol_->gValues_(f, k)) - (g(f, k - 1) - initial_.vol_->gValues_(f, k - 1));
                    result.push_back(std::sqrt(settings_.smoothingWeight_ / gap) * change / settings_.parameterScale_);
                }
                return result;
            }
            [[nodiscard]] int JacobianEvaluations(size_t columns) const { return 2 * static_cast<int>(columns); }
            [[nodiscard]] Matrix_<> Jacobian(const Vector_<>& x, size_t rows) const {
                return DifferenceJacobian(*this, x, rows, settings_.finiteDifferenceStep_);
            }
        };

        bool Nondegenerate(const CalibrationBondOptionDescriptor_& option, const CalibrationPreparationContext_& c) {
            const AAD::GSR_<double> gaussian(static_cast<const MultiFactorGSRModelData_&>(*c.model_));
            const double expiry = (option.expiry_ - *c.valuation_.evaluationDate_) / DAYS_PER_YEAR;
            const double maturity = (option.maturity_ - *c.valuation_.evaluationDate_) / DAYS_PER_YEAR;
            const auto loading = gaussian.GaussianBondLoading(expiry, maturity);
            const auto covariance = gaussian.GaussianVariance(expiry);
            double variance = 0.;
            for (int i = 0; i < covariance.Rows(); ++i)
                for (int j = 0; j < covariance.Cols(); ++j)
                    variance += loading[i] * covariance(i, j) * loading[j];
            return std::isfinite(variance) && variance > 0.;
        }

        // Calibration assembly uses explicit output pointers to avoid copying result matrices.
        void FinalParameters(const Problem_& problem,
                             const Vector_<>& x,
                             const GSRCalibrationSettings_& settings,
                             GaussianBondOptionCalibrationResult_* r) {
            for (size_t i = 0; i < x.size(); ++i) {
                r->parameters_.push_back(x[i] * settings.parameterScale_);
                r->activeBounds_.push_back(x[i] == problem.Bound(i, false) || x[i] == problem.Bound(i, true));
                const double h = settings.finiteDifferenceStep_ * std::max(1., std::abs(x[i]));
                r->oneSidedColumns_.push_back(x[i] - h < problem.Bound(i, false) || x[i] + h > problem.Bound(i, true));
            }
            r->nativeJacobianAvailable_ = std::all_of(x.begin(), x.end(), [](double g) { return g > 0.; });
        }
        void FinalPrices(const Problem_& problem,
                         const CalibrationPreparationContext_& c,
                         const GSRCalibrationSettings_& settings,
                         GaussianBondOptionCalibrationResult_* r) {
            r->fitWithinTolerance_ = r->numericalValidationPassed_ = true;
            for (const auto& row : r->axes_.observations_) {
                const auto plan = problem.Plan(*row, c);
                const auto price = problem.Price(*row, plan, c);
                r->nativeJacobianAvailable_ =
                    Nondegenerate(std::get<CalibrationBondOptionDescriptor_>(plan.Descriptor()), c) && r->nativeJacobianAvailable_;
                r->planIdentities_.push_back(plan.PlanIdentity());
                r->modelPrices_.push_back(price.price_);
                r->residuals_.push_back(price.price_ - row->Quote()->Value());
                r->scaledResiduals_.push_back(r->residuals_.back() / row->ErrorScale());
                r->numericalErrors_.push_back(price.numericalError_);
                r->fitWithinTolerance_ = r->fitWithinTolerance_ && std::abs(r->scaledResiduals_.back()) <= 1.;
                r->numericalValidationPassed_ =
                    r->numericalValidationPassed_ && price.numericalError_ <= settings.numericalErrorFraction_ * row->ErrorScale();
            }
        }
        void
        FinalJacobian(const Problem_& problem, const Vector_<>& x, const GSRCalibrationSettings_& settings, GaussianBondOptionCalibrationResult_* r) {
            const auto residuals = problem.Residuals(x);
            const auto jacobian = problem.Jacobian(x, residuals.size());
            r->evaluations_ += 1 + problem.JacobianEvaluations(x.size());
            const double gradient = ProjectedGradient(problem, x, Gradient(jacobian, residuals));
            const double tolerance = settings.gradientTolerance_ * GradientScale(jacobian) * std::max(1., std::sqrt(NormSquared(residuals)));
            if (r->terminationReason_ == "StepStalled" && gradient <= tolerance) {
                r->converged_ = true;
                r->terminationReason_ = "ProjectedGradient";
            }
            if (!r->nativeJacobianAvailable_)
                return;
            r->quoteJacobian_ = Matrix_<>(r->axes_.observations_.size(), x.size());
            for (int row = 0; row < r->quoteJacobian_.Rows(); ++row)
                for (int col = 0; col < r->quoteJacobian_.Cols(); ++col)
                    r->quoteJacobian_(row, col) = jacobian(row, col);
            RankDiagnostics(r->quoteJacobian_, r);
            for (int row = 0; row < r->quoteJacobian_.Rows(); ++row)
                for (int col = 0; col < r->quoteJacobian_.Cols(); ++col)
                    r->quoteJacobian_(row, col) *= r->axes_.observations_[row]->ErrorScale() / settings.parameterScale_;
        }

        GaussianBondOptionCalibrationResult_ Calibrate(const CalibrationPreparationContext_& context,
                                                       const CalibrationObservationAxes_& axes,
                                                       const Vector_<GSRCalibrationParameter_>& parameters,
                                                       const GSRCalibrationSettings_& settings) {
            Settings(settings);
            Rows(axes.observations_);
            GaussianBondOptionCalibrationResult_ r;
            r.axes_ = axes;
            const Problem_ problem(context, axes.observations_, parameters, settings, &r.costs_);
            r.parameterLabels_ = CalibrationMcGLabels(context.model_, parameters);
            auto x = problem.Guess();
            Fit(problem, {settings.maxIterations_, settings.gradientTolerance_, settings.stepTolerance_}, &x, &r);
            r.model_ = problem.Model(x);
            const auto c = problem.Context(r.model_);
            FinalParameters(problem, x, settings, &r);
            FinalPrices(problem, c, settings, &r);
            FinalJacobian(problem, x, settings, &r);
            r.report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Validate, axes));
            return r;
        }
        GaussianBondOptionCalibrationResult_ Run(const CalibrationPreparationContext_& context,
                                                 const CalibrationObservationAxes_& axes,
                                                 const Vector_<GSRCalibrationParameter_>& parameters,
                                                 const GSRCalibrationSettings_& settings) {
            try {
                return Calibrate(context, axes, parameters, settings);
            } catch (const CalibrationError_&) {
                throw;
            } catch (const Exception_& error) {
                CalibrationDiagnostic_ d;
                d.stage_ = CalibrationStage_::Value_::Solve;
                Fail("CalibrationNumericalFailure", "/solver", "finite, resolvable Gaussian calibration state", String_(error.what()), d);
            }
        }
    } // namespace

    GaussianBondOptionCalibrationResult_ CalibrateGaussianBondOptions(const CalibrationPreparationContext_& context,
                                                                      const Vector_<Handle_<CalibrationObservation_>>& observations,
                                                                      const Vector_<GSRCalibrationParameter_>& parameters,
                                                                      const GSRCalibrationSettings_& settings) {
        const auto axes = BuildCalibrationObservationAxes(observations);
        try {
            return Run(context, axes, parameters, settings);
        } catch (const CalibrationError_& error) {
            auto failure = error.Failure();
            failure.report_.reset(new CalibrationReport_(failure.diagnostic_.stage_, axes, {failure.diagnostic_}));
            throw CalibrationError_(__FILE__, __LINE__, __func__, std::make_shared<const CalibrationFailure_>(std::move(failure)));
        }
    }
} // namespace Dal
