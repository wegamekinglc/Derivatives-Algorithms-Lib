//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>

#include <dal/math/matrix/covariance.hpp>
#include <dal/model/gsrcalibration.hpp>

namespace Dal {
    namespace {
        double NormSquared(const Vector_<>& values) {
            const double norm = std::inner_product(values.begin(), values.end(), values.begin(), 0.0);
            REQUIRE(std::isfinite(norm), "InvalidGSRCalibration: residual or derivative norm overflow");
            return norm;
        }

        void ValidateSettings(const GSRCalibrationSettings_& settings) {
            REQUIRE(settings.maxIterations_ > 0, "InvalidGSRCalibration: max iterations must be positive");
            for (const auto value : {settings.gradientTolerance_, settings.stepTolerance_, settings.finiteDifferenceStep_, settings.parameterScale_})
                REQUIRE(std::isfinite(value) && value > 0.0,
                        "InvalidGSRCalibration: tolerances, difference step and parameter scale must be finite and positive");
            for (const auto value : {settings.priorWeight_, settings.smoothingWeight_, settings.numericalErrorFraction_})
                REQUIRE(std::isfinite(value) && value >= 0.0,
                        "InvalidGSRCalibration: penalty weights and numerical error fraction must be finite and nonnegative");
            REQUIRE(settings.pricing_.quadratureOrder_ >= 2 && settings.pricing_.quadratureOrder_ <= 32,
                    "InvalidGSRCalibration: pricing quadrature order must be between 2 and 32");
        }

        void ValidateQuotes(const Vector_<GSRCalibrationQuote_>& quotes) {
            REQUIRE(!quotes.empty(), "InvalidGSRCalibration: quotes must be nonempty");
            Vector_<String_> names;
            for (const auto& quote : quotes) {
                REQUIRE(!quote.name_.empty() && std::find(names.begin(), names.end(), quote.name_) == names.end(),
                        "InvalidGSRCalibration: quote names must be nonempty and unique");
                REQUIRE(std::isfinite(quote.price_) && quote.price_ >= 0.0 && std::isfinite(quote.priceScale_) && quote.priceScale_ > 0.0,
                        "InvalidGSRCalibration: quote prices must be finite and nonnegative and price scales finite and positive");
                names.push_back(quote.name_);
            }
        }

        void ValidateParameter(const GSRCalibrationParameter_& parameter, const Matrix_<>& values) {
            REQUIRE(parameter.factor_ >= 0 && parameter.factor_ < values.Rows() && parameter.knot_ >= 0 && parameter.knot_ < values.Cols(),
                    "InvalidGSRCalibration: parameter factor or knot index is out of range");
            REQUIRE(std::isfinite(parameter.lower_) && std::isfinite(parameter.upper_) && parameter.lower_ >= 0.0 &&
                        parameter.upper_ > parameter.lower_,
                    "InvalidGSRCalibration: bounds must be finite with 0 <= lower < upper");
        }

        Vector_<std::pair<int, int>> SmoothingPairs(const Vector_<std::pair<int, int>>& selected, int factors, int knots) {
            Vector_<std::pair<int, int>> pairs;
            for (int factor = 0; factor < factors; ++factor)
                for (int knot = 1; knot < knots; ++knot)
                    if (std::find(selected.begin(), selected.end(), std::make_pair(factor, knot)) != selected.end() ||
                        std::find(selected.begin(), selected.end(), std::make_pair(factor, knot - 1)) != selected.end())
                        pairs.push_back({factor, knot});
            return pairs;
        }

        class Problem_ {
            const MultiFactorGSRModelData_& initial_;
            const Vector_<GSRCalibrationQuote_>& quotes_;
            const Vector_<GSRCalibrationParameter_>& parameters_;
            const GSRCalibrationSettings_& settings_;
            Vector_<> guess_;
            Vector_<std::pair<int, int>> smoothPairs_;

        public:
            Problem_(const MultiFactorGSRModelData_& initial,
                     const Vector_<GSRCalibrationQuote_>& quotes,
                     const Vector_<GSRCalibrationParameter_>& parameters,
                     const GSRCalibrationSettings_& settings)
                : initial_(initial), quotes_(quotes), parameters_(parameters), settings_(settings) {
                REQUIRE(!parameters.empty(), "InvalidGSRCalibration: selected parameters must be nonempty");
                Vector_<std::pair<int, int>> used;
                for (const auto& parameter : parameters) {
                    ValidateParameter(parameter, initial.vol_->gValues_);
                    const std::pair<int, int> key{parameter.factor_, parameter.knot_};
                    REQUIRE(std::find(used.begin(), used.end(), key) == used.end(), "InvalidGSRCalibration: duplicate parameter");
                    used.push_back(key);
                    REQUIRE(std::isfinite(parameter.upper_ / settings.parameterScale_), "InvalidGSRCalibration: normalized bound overflow");
                    const double value = initial.vol_->gValues_(parameter.factor_, parameter.knot_);
                    REQUIRE(value >= parameter.lower_ && value <= parameter.upper_, "InvalidGSRCalibration: initial g lies outside parameter bounds");
                    guess_.push_back(value / settings.parameterScale_);
                }
                if (settings.smoothingWeight_ > 0.0)
                    smoothPairs_ = SmoothingPairs(used, initial.vol_->gValues_.Rows(), initial.vol_->gValues_.Cols());
            }

            [[nodiscard]] const Vector_<>& Guess() const { return guess_; }

            [[nodiscard]] double Bound(size_t i, bool upper) const {
                return (upper ? parameters_[i].upper_ : parameters_[i].lower_) / settings_.parameterScale_;
            }

            [[nodiscard]] Handle_<MultiFactorGSRModelData_> Model(const Vector_<>& x) const {
                MultiFactorGSRVolSettings_ vol;
                vol.factorNames_ = initial_.vol_->factorNames_;
                vol.gKnotDates_ = initial_.vol_->gKnotDates_;
                vol.gValues_ = initial_.vol_->gValues_;
                vol.hKnotDates_ = initial_.vol_->hKnotDates_;
                vol.hValues_ = initial_.vol_->hValues_;
                vol.correlations_ = initial_.vol_->correlations_;
                for (size_t i = 0; i < x.size(); ++i)
                    vol.gValues_(parameters_[i].factor_, parameters_[i].knot_) = x[i] * settings_.parameterScale_;
                return Handle_<MultiFactorGSRModelData_>(new MultiFactorGSRModelData_(
                    initial_.name_, initial_.curve_, Handle_<MultiFactorGSRVolData_>(new MultiFactorGSRVolData_(initial_.vol_->name_, vol))));
            }

            [[nodiscard]] Vector_<> Residuals(const Vector_<>& x) const {
                const auto model = Model(x);
                auto pricing = settings_.pricing_;
                pricing.estimateError_ = false;
                Vector_<> result;
                for (const auto& quote : quotes_)
                    result.push_back((PriceGSREuropeanOption(*model, quote.option_, pricing).price_ - quote.price_) / quote.priceScale_);
                if (settings_.priorWeight_ > 0.0)
                    for (size_t i = 0; i < x.size(); ++i)
                        result.push_back(std::sqrt(settings_.priorWeight_) * (x[i] - guess_[i]));
                for (const auto& pair : smoothPairs_) {
                    const auto [factor, knot] = pair;
                    const double gap = (model->vol_->gKnotDates_[knot] - model->vol_->gKnotDates_[knot - 1]) / DAYS_PER_YEAR;
                    const double change = model->vol_->gValues_(factor, knot) - initial_.vol_->gValues_(factor, knot);
                    const double previous = model->vol_->gValues_(factor, knot - 1) - initial_.vol_->gValues_(factor, knot - 1);
                    result.push_back(std::sqrt(settings_.smoothingWeight_ / gap) * (change - previous) / settings_.parameterScale_);
                }
                return result;
            }

            [[nodiscard]] Matrix_<> Jacobian(const Vector_<>& x, size_t rows) const {
                Matrix_<> result(static_cast<int>(rows), static_cast<int>(x.size()), 0.0);
                for (size_t col = 0; col < x.size(); ++col) {
                    auto up = x, down = x;
                    const double step = settings_.finiteDifferenceStep_ * std::max(1.0, std::abs(x[col]));
                    up[col] = std::min(Bound(col, true), x[col] + step);
                    down[col] = std::max(Bound(col, false), x[col] - step);
                    REQUIRE(up[col] > down[col], "InvalidGSRCalibration: finite difference step is below floating-point resolution");
                    const auto high = Residuals(up), low = Residuals(down);
                    for (size_t row = 0; row < rows; ++row)
                        result(static_cast<int>(row), static_cast<int>(col)) = (high[row] - low[row]) / (up[col] - down[col]);
                }
                return result;
            }
        };

        Vector_<> Gradient(const Matrix_<>& jacobian, const Vector_<>& residuals) {
            Vector_<> gradient(jacobian.Cols(), 0.0);
            for (int col = 0; col < jacobian.Cols(); ++col)
                for (int row = 0; row < jacobian.Rows(); ++row)
                    gradient[col] += jacobian(row, col) * residuals[row];
            return gradient;
        }

        double GradientScale(const Matrix_<>& jacobian) {
            double scale = 1.0;
            for (int col = 0; col < jacobian.Cols(); ++col) {
                double norm = 0.0;
                for (int row = 0; row < jacobian.Rows(); ++row)
                    norm += jacobian(row, col) * jacobian(row, col);
                scale = std::max(scale, std::sqrt(norm));
            }
            return scale;
        }

        double ProjectedGradient(const Problem_& problem, const Vector_<>& x, const Vector_<>& gradient) {
            double norm = 0.0;
            for (size_t i = 0; i < x.size(); ++i) {
                const bool blocked = (x[i] <= problem.Bound(i, false) && gradient[i] > 0.0) || (x[i] >= problem.Bound(i, true) && gradient[i] < 0.0);
                if (!blocked)
                    norm = std::max(norm, std::abs(gradient[i]));
            }
            return norm;
        }

        Vector_<> DampedStep(const Matrix_<>& jacobian, const Vector_<>& gradient, double damping) {
            const int n = jacobian.Cols();
            Matrix_<> hessian(n, n, 0.0);
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j)
                    for (int row = 0; row < jacobian.Rows(); ++row)
                        hessian(i, j) += jacobian(row, i) * jacobian(row, j);
                hessian(i, i) += damping * std::max(1.0, hessian(i, i));
            }
            const auto lower = AAD::CovarianceFactor(hessian);
            Vector_<> step(n, 0.0);
            for (int i = 0; i < n; ++i) {
                double value = -gradient[i];
                for (int j = 0; j < i; ++j)
                    value -= lower(i, j) * step[j];
                step[i] = value / lower(i, i);
            }
            for (int i = n; i-- > 0;) {
                for (int j = i + 1; j < n; ++j)
                    step[i] -= lower(j, i) * step[j];
                step[i] /= lower(i, i);
            }
            return step;
        }

        void Orthogonalize(const Vector_<>& direction, Vector_<Vector_<>>* columns, size_t start) {
            for (size_t col = start; col < columns->size(); ++col)
                for (int pass = 0; pass < 2; ++pass) {
                    const double dot = std::inner_product(direction.begin(), direction.end(), (*columns)[col].begin(), 0.0);
                    for (size_t row = 0; row < (*columns)[col].size(); ++row)
                        (*columns)[col][row] -= dot * direction[row];
                }
        }

        void RankDiagnostics(const Matrix_<>& jacobian, GSRCalibrationResult_* result) {
            Vector_<Vector_<>> columns;
            double largest = 0.0, smallest = std::numeric_limits<double>::infinity();
            for (int col = 0; col < jacobian.Cols(); ++col) {
                Vector_<> column(jacobian.Rows(), 0.0);
                for (int row = 0; row < jacobian.Rows(); ++row)
                    column[row] = jacobian(row, col);
                largest = std::max(largest, std::sqrt(NormSquared(column)));
                columns.push_back(column);
            }
            for (size_t step = 0; step < columns.size(); ++step) {
                const auto best = std::max_element(columns.begin() + step, columns.end(),
                                                   [](const auto& a, const auto& b) { return NormSquared(a) < NormSquared(b); });
                std::swap(columns[step], *best);
                const double norm = std::sqrt(NormSquared(columns[step]));
                if (norm <= largest * 1e-8)
                    break;
                ++result->jacobianRank_;
                smallest = std::min(smallest, norm);
                for (auto& value : columns[step])
                    value /= norm;
                Orthogonalize(columns[step], &columns, step + 1);
            }
            result->jacobianConditionEstimate_ =
                result->jacobianRank_ == jacobian.Cols() ? largest / smallest : std::numeric_limits<double>::infinity();
        }

        void Fit(const Problem_& problem, const GSRCalibrationSettings_& settings, Vector_<>* x, GSRCalibrationResult_* result) {
            auto residuals = problem.Residuals(*x);
            ++result->evaluations_;
            double objective = NormSquared(residuals), damping = 1e-3;
            result->terminationReason_ = "MaxIterations";
            for (int iteration = 0; iteration < settings.maxIterations_; ++iteration) {
                result->iterations_ = iteration + 1;
                const auto jacobian = problem.Jacobian(*x, residuals.size());
                result->evaluations_ += 2 * static_cast<int>(x->size());
                const auto gradient = Gradient(jacobian, residuals);
                if (ProjectedGradient(problem, *x, gradient) <=
                    settings.gradientTolerance_ * GradientScale(jacobian) * std::max(1.0, std::sqrt(objective))) {
                    result->converged_ = true;
                    result->terminationReason_ = "ProjectedGradient";
                    break;
                }
                bool accepted = false;
                for (int trial = 0; trial < 16; ++trial) {
                    const auto step = DampedStep(jacobian, gradient, damping);
                    auto candidate = *x;
                    double distance = 0.0;
                    for (size_t i = 0; i < x->size(); ++i) {
                        candidate[i] = std::clamp((*x)[i] + step[i], problem.Bound(i, false), problem.Bound(i, true));
                        distance = std::max(distance, std::abs(candidate[i] - (*x)[i]));
                    }
                    const auto nextResiduals = problem.Residuals(candidate);
                    ++result->evaluations_;
                    const double nextObjective = NormSquared(nextResiduals);
                    if (nextObjective < objective) {
                        *x = std::move(candidate);
                        residuals = nextResiduals;
                        objective = nextObjective;
                        damping = std::max(damping / 3.0, 1e-12);
                        accepted = true;
                        if (distance <= settings.stepTolerance_) {
                            result->terminationReason_ = "StepStalled";
                            result->objective_ = objective;
                            return;
                        }
                        break;
                    }
                    damping *= 10.0;
                }
                if (!accepted) {
                    result->terminationReason_ = "NoDescent";
                    break;
                }
            }
            result->objective_ = objective;
        }
    } // namespace

    GSRCalibrationResult_ CalibrateGSRVolatility(const MultiFactorGSRModelData_& initial,
                                                 const Vector_<GSRCalibrationQuote_>& quotes,
                                                 const Vector_<GSRCalibrationParameter_>& parameters,
                                                 const GSRCalibrationSettings_& settings) {
        ValidateSettings(settings);
        ValidateQuotes(quotes);
        const Problem_ problem(initial, quotes, parameters, settings);
        GSRCalibrationResult_ result;
        auto x = problem.Guess();
        Fit(problem, settings, &x, &result);
        result.model_ = problem.Model(x);
        result.fitWithinTolerance_ = result.numericalValidationPassed_ = true;
        for (size_t i = 0; i < x.size(); ++i) {
            result.parameters_.push_back(x[i] * settings.parameterScale_);
            result.activeBounds_.push_back(x[i] == problem.Bound(i, false) || x[i] == problem.Bound(i, true));
        }
        auto pricing = settings.pricing_;
        pricing.quadratureOrder_ *= 2;
        pricing.estimateError_ = true;
        for (const auto& quote : quotes) {
            const auto refined = PriceGSREuropeanOption(*result.model_, quote.option_, pricing);
            auto fittedSettings = settings.pricing_;
            fittedSettings.estimateError_ = false;
            const double fitted = PriceGSREuropeanOption(*result.model_, quote.option_, fittedSettings).price_;
            const double error = std::max(refined.numericalError_, std::abs(refined.price_ - fitted));
            result.modelPrices_.push_back(refined.price_);
            result.residuals_.push_back(refined.price_ - quote.price_);
            result.numericalErrors_.push_back(error);
            result.fitWithinTolerance_ = result.fitWithinTolerance_ && std::abs(result.residuals_.back()) <= quote.priceScale_;
            result.numericalValidationPassed_ = result.numericalValidationPassed_ && error <= settings.numericalErrorFraction_ * quote.priceScale_;
        }
        const auto jacobian = problem.Jacobian(x, quotes.size());
        result.evaluations_ += 2 * static_cast<int>(x.size());
        RankDiagnostics(jacobian, &result);
        result.quoteJacobian_ = jacobian;
        for (int row = 0; row < jacobian.Rows(); ++row)
            for (int col = 0; col < jacobian.Cols(); ++col)
                result.quoteJacobian_(row, col) *= quotes[row].priceScale_ / settings.parameterScale_;
        return result;
    }
} // namespace Dal
