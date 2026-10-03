//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <set>

#include <dal/math/optimization/boundedgn.hpp>
#include <dal/model/gsrslvcalibrationinternal.hpp>
#include <dal/model/gsrslvpricinginternal.hpp>

namespace Dal {
    namespace {
        using namespace BoundedGaussNewton;

        Vector_<EuropeanRateOption_> Options(const Vector_<CalibrationQuote_>& quotes) {
            Vector_<EuropeanRateOption_> result;
            for (const auto& quote : quotes)
                result.push_back(quote.option_);
            return result;
        }

        void ValidateQuotes(const Vector_<CalibrationQuote_>& quotes, bool allowEmpty = false) {
            REQUIRE(allowEmpty || !quotes.empty(), "InvalidGSRSLVCalibration: quotes must be nonempty");
            std::set<String_> names;
            for (const auto& quote : quotes) {
                REQUIRE(!quote.name_.empty() && names.insert(quote.name_).second,
                        "InvalidGSRSLVCalibration: quote names must be nonempty and unique");
                REQUIRE(std::isfinite(quote.price_) && quote.price_ >= 0.0 && std::isfinite(quote.priceScale_) && quote.priceScale_ > 0.0,
                        "InvalidGSRSLVCalibration: price must be finite and nonnegative; price scale finite and positive");
            }
        }

        void ValidateSolverSettings(const GSRCalibrationSettings_& settings) {
            REQUIRE(settings.maxIterations_ > 0, "InvalidGSRSLVCalibration: max iterations must be positive");
            for (double value : {settings.gradientTolerance_, settings.stepTolerance_, settings.finiteDifferenceStep_})
                REQUIRE(std::isfinite(value) && value > 0.0, "InvalidGSRSLVCalibration: tolerances must be finite and positive");
            for (double value : {settings.priorWeight_, settings.smoothingWeight_, settings.numericalErrorFraction_})
                REQUIRE(std::isfinite(value) && value >= 0.0,
                        "InvalidGSRSLVCalibration: penalty weights and numerical error fraction must be finite and nonnegative");
        }

        void ValidateMonteCarloSettings(const GSRMonteCarloSettings_& settings) {
            REQUIRE(settings.paths_ >= 4 && settings.paths_ % 2 == 0,
                    "InvalidGSRSLVCalibration: fit and validation paths must be even and at least four");
            REQUIRE(settings.seed_ >= 0, "InvalidGSRSLVCalibration: seeds must be nonnegative");
            REQUIRE(settings.conditionalPaths_ >= 4 && settings.conditionalPaths_ % 4 == 0,
                    "InvalidGSRSLVCalibration: conditional paths must be divisible by four and at least four");
        }

        void ValidateSettings(const GSRSLVCalibrationSettings_& settings) {
            ValidateSolverSettings(settings.solver_);
            ValidateMonteCarloSettings(settings.pricing_);
            ValidateMonteCarloSettings(settings.validation_);
            REQUIRE(std::isfinite(settings.validationSigma_) && settings.validationSigma_ > 0.0,
                    "InvalidGSRSLVCalibration: validation sigma must be finite and positive");
            REQUIRE(settings.pricing_.seed_ != settings.validation_.seed_,
                    "InvalidGSRSLVCalibration: fit and validation seeds must be distinct and nonnegative");
        }

        void ValidateParameterBounds(const GSRSLVCalibrationParameter_& parameter) {
            REQUIRE(std::isfinite(parameter.lower_) && std::isfinite(parameter.upper_) && parameter.lower_ >= 0.0 &&
                        parameter.upper_ > parameter.lower_ && std::isfinite(parameter.scale_) && parameter.scale_ > 0.0 &&
                        std::isfinite(parameter.upper_ / parameter.scale_),
                    "InvalidGSRSLVCalibration: bounds must satisfy 0 <= lower < upper, with a positive finite scale");
        }

        double NormalizeParameter(const GSRSLVCalibrationParameter_& parameter, double value, bool leverage) {
            if (leverage)
                REQUIRE(parameter.lower_ / parameter.scale_ > 0.0,
                        "InvalidGSRSLVCalibration: normalized leverage lower bound must be strictly positive");
            REQUIRE(value >= parameter.lower_ && value <= parameter.upper_, "InvalidGSRSLVCalibration: initial parameter outside bounds");
            REQUIRE(parameter.upper_ / parameter.scale_ > parameter.lower_ / parameter.scale_ && std::isfinite(value / parameter.scale_) &&
                        (value == 0.0 || value / parameter.scale_ > 0.0),
                    "InvalidGSRSLVCalibration: parameter normalization is unresolved or nonfinite");
            return value / parameter.scale_;
        }

        struct ParameterValue_ {
            std::pair<int, int> coordinate_;
            double value_;
        };

        class Problem_ {
            const GSRSLVModelData_& initial_;
            const Vector_<CalibrationQuote_>& quotes_;
            const Vector_<GSRSLVCalibrationParameter_>& parameters_;
            const GSRSLVCalibrationSettings_& settings_;
            GSRSLVPricingInternal::PreparedPricer_ pricer_;
            Vector_<std::pair<int, int>> coordinates_;
            Vector_<> guess_;
            Matrix_<> nodeScales_;
            struct Edge_ {
                std::pair<int, int> first_, second_;
                double weight_;
            };
            Vector_<Edge_> smoothing_;

            double Change(const Matrix_<>& values, const std::pair<int, int>& coordinate) const {
                const auto [row, col] = coordinate;
                return (values(row, col) - initial_.leverage_->values_(row, col)) / nodeScales_(row, col);
            }

            bool Selected(int row, int col) const {
                return std::find(coordinates_.begin(), coordinates_.end(), std::make_pair(row, col)) != coordinates_.end();
            }

            ParameterValue_ Locate(const String_& label) const {
                if (label == "kappa")
                    return {{-1, 0}, initial_.kappa_};
                if (label == "volOfVol")
                    return {{-1, 1}, initial_.volOfVol_};
                for (int row = 0; row < nodeScales_.Rows(); ++row)
                    for (int col = 0; col < nodeScales_.Cols(); ++col)
                        if (label == "leverage:" + String::FromInt(row) + ":" + String::FromInt(col))
                            return {{row, col}, initial_.leverage_->values_(row, col)};
                THROW("InvalidGSRSLVCalibration: unknown parameter " + label);
            }

            void SmoothingEdge(int row, int col, int otherRow, int otherCol, double gap) {
                if (Selected(row, col) || Selected(otherRow, otherCol))
                    smoothing_.push_back({{row, col}, {otherRow, otherCol}, std::sqrt(settings_.solver_.smoothingWeight_ / gap)});
            }

            void PrepareSmoothing() {
                if (settings_.solver_.smoothingWeight_ == 0.0)
                    return;
                for (int row = 0; row < nodeScales_.Rows(); ++row)
                    for (int col = 0; col < nodeScales_.Cols(); ++col) {
                        if (row > 0)
                            SmoothingEdge(row, col, row - 1, col, initial_.leverage_->rateShifts_[row] - initial_.leverage_->rateShifts_[row - 1]);
                        if (col > 0)
                            SmoothingEdge(row, col, row, col - 1, initial_.leverage_->times_[col] - initial_.leverage_->times_[col - 1]);
                    }
            }

            void AddSmoothing(const Matrix_<>& values, Vector_<>* residuals) const {
                for (const auto& edge : smoothing_)
                    residuals->push_back(edge.weight_ * (Change(values, edge.first_) - Change(values, edge.second_)));
            }

            size_t AddPriorJacobian(Matrix_<>* result) const {
                size_t row = quotes_.size();
                if (settings_.solver_.priorWeight_ > 0.0)
                    for (size_t col = 0; col < parameters_.size(); ++col, ++row)
                        if (row < static_cast<size_t>(result->Rows()))
                            (*result)(row, col) = std::sqrt(settings_.solver_.priorWeight_);
                return row;
            }

            void AddPenaltyJacobian(Matrix_<>* result) const {
                size_t row = AddPriorJacobian(result);
                for (const auto& edge : smoothing_) {
                    if (row >= static_cast<size_t>(result->Rows()))
                        break;
                    for (size_t col = 0; col < coordinates_.size(); ++col)
                        (*result)(row, col) =
                            edge.weight_ * ((coordinates_[col] == edge.first_ ? 1.0 : 0.0) - (coordinates_[col] == edge.second_ ? 1.0 : 0.0));
                    ++row;
                }
            }

        public:
            Problem_(const GSRSLVModelData_& initial,
                     const Vector_<CalibrationQuote_>& quotes,
                     const Vector_<GSRSLVCalibrationParameter_>& parameters,
                     const GSRSLVCalibrationSettings_& settings)
                : initial_(initial), quotes_(quotes), parameters_(parameters), settings_(settings),
                  pricer_(initial, Options(quotes), settings.pricing_),
                  nodeScales_(initial.leverage_->values_.Rows(), initial.leverage_->values_.Cols(), 1.0) {
                REQUIRE(!parameters.empty(), "InvalidGSRSLVCalibration: selected parameters must be nonempty");
                std::set<String_> labels;
                for (const auto& parameter : parameters) {
                    REQUIRE(labels.insert(parameter.label_).second, "InvalidGSRSLVCalibration: duplicate parameter label");
                    ValidateParameterBounds(parameter);
                    const auto location = Locate(parameter.label_);
                    const auto& coordinate = location.coordinate_;
                    guess_.push_back(NormalizeParameter(parameter, location.value_, coordinate.first >= 0));
                    if (coordinate.first >= 0)
                        nodeScales_(coordinate.first, coordinate.second) = parameter.scale_;
                    coordinates_.push_back(coordinate);
                }
                PrepareSmoothing();
            }

            const Vector_<>& Guess() const { return guess_; }
            double Bound(size_t i, bool upper) const { return (upper ? parameters_[i].upper_ : parameters_[i].lower_) / parameters_[i].scale_; }
            bool Stochastic(size_t i) const { return coordinates_[i].first < 0; }
            bool UseAAD() const { return settings_.useAADJacobian_; }
            int JacobianEvaluations(size_t columns) const { return UseAAD() ? 1 : 2 * static_cast<int>(columns); }

            Handle_<GSRSLVModelData_> Model(const Vector_<>& x) const {
                auto values = initial_.leverage_->values_;
                GSRSLVSettings_ settings{initial_.kappa_, initial_.volOfVol_, initial_.varianceCorrelations_, initial_.maxStep_};
                for (size_t i = 0; i < x.size(); ++i) {
                    const auto [row, col] = coordinates_[i];
                    if (row >= 0)
                        values(row, col) = x[i] * parameters_[i].scale_;
                    else if (col == 0)
                        settings.kappa_ = x[i] * parameters_[i].scale_;
                    else
                        settings.volOfVol_ = x[i] * parameters_[i].scale_;
                }
                const Handle_<GSRLeverageData_> leverage(
                    new GSRLeverageData_(initial_.leverage_->name_, initial_.leverage_->rateShifts_, initial_.leverage_->times_, values));
                return Handle_<GSRSLVModelData_>(new GSRSLVModelData_(initial_.name_, initial_.gaussian_, leverage, settings));
            }

            Vector_<> Residuals(const Vector_<>& x) const {
                const auto model = Model(x);
                const auto prices = pricer_.Price(*model);
                Vector_<> result;
                for (size_t i = 0; i < prices.size(); ++i)
                    result.push_back((prices[i].price_ - quotes_[i].price_) / quotes_[i].priceScale_);
                if (settings_.solver_.priorWeight_ > 0.0)
                    for (size_t i = 0; i < x.size(); ++i)
                        result.push_back(std::sqrt(settings_.solver_.priorWeight_) * (x[i] - guess_[i]));
                if (settings_.solver_.smoothingWeight_ > 0.0)
                    AddSmoothing(model->leverage_->values_, &result);
                return result;
            }

            Matrix_<> Jacobian(const Vector_<>& x, size_t rows) const {
                if (!UseAAD())
                    return DifferenceJacobian(*this, x, rows, settings_.solver_.finiteDifferenceStep_);
                Vector_<String_> labels;
                for (const auto& parameter : parameters_)
                    labels.push_back(parameter.label_);
                const auto prices = pricer_.Jacobian(*Model(x), labels);
                Matrix_<> result(rows, x.size(), 0.0);
                for (size_t row = 0; row < std::min(rows, quotes_.size()); ++row)
                    for (size_t col = 0; col < x.size(); ++col)
                        result(row, col) = prices(row, col) * parameters_[col].scale_ / quotes_[row].priceScale_;
                AddPenaltyJacobian(&result);
                return result;
            }
        };

        class SubsetProblem_ {
            const Problem_& problem_;
            Vector_<> fixed_;
            Vector_<size_t> selected_;
            double differenceStep_;

        public:
            SubsetProblem_(const Problem_& problem, const Vector_<>& fixed, const Vector_<size_t>& selected, double differenceStep)
                : problem_(problem), fixed_(fixed), selected_(selected), differenceStep_(differenceStep) {}
            Vector_<> Expand(const Vector_<>& x) const {
                auto result = fixed_;
                for (size_t i = 0; i < selected_.size(); ++i)
                    result[selected_[i]] = x[i];
                return result;
            }
            double Bound(size_t i, bool upper) const { return problem_.Bound(selected_[i], upper); }
            Vector_<> Residuals(const Vector_<>& x) const { return problem_.Residuals(Expand(x)); }
            int JacobianEvaluations(size_t columns) const { return problem_.JacobianEvaluations(columns); }
            Matrix_<> Jacobian(const Vector_<>& x, size_t rows) const {
                if (!problem_.UseAAD())
                    return DifferenceJacobian(*this, x, rows, differenceStep_);
                const auto full = problem_.Jacobian(Expand(x), rows);
                Matrix_<> result(rows, selected_.size());
                for (size_t row = 0; row < rows; ++row)
                    for (size_t col = 0; col < selected_.size(); ++col)
                        result(row, col) = full(row, selected_[col]);
                return result;
            }
        };

        int WarmStart(const Problem_& problem, const GSRSLVCalibrationSettings_& settings, Vector_<>* x, GSRSLVCalibrationResult_* result) {
            int warmIterations = 0;
            if (settings.staged_) {
                Vector_<size_t> stochastic, leverage;
                for (size_t i = 0; i < x->size(); ++i)
                    (problem.Stochastic(i) ? stochastic : leverage).push_back(i);
                if (!stochastic.empty() && !leverage.empty())
                    for (const auto& selected : {stochastic, leverage}) {
                        const SubsetProblem_ subset(problem, *x, selected, settings.solver_.finiteDifferenceStep_);
                        Vector_<> values;
                        for (size_t i : selected)
                            values.push_back((*x)[i]);
                        GSRSLVCalibrationResult_ warm;
                        Fit(subset, {settings.solver_.maxIterations_, settings.solver_.gradientTolerance_, settings.solver_.stepTolerance_}, &values,
                            &warm);
                        *x = subset.Expand(values);
                        warmIterations += warm.iterations_;
                        result->evaluations_ += warm.evaluations_;
                    }
            }
            return warmIterations;
        }

        void FitDiagnostics(const Problem_& problem,
                            const Vector_<>& x,
                            const Vector_<CalibrationQuote_>& quotes,
                            const Vector_<GSRSLVCalibrationParameter_>& parameters,
                            const GSRSLVCalibrationSettings_& settings,
                            GSRSLVCalibrationResult_* result) {
            result->fitWithinTolerance_ = true;
            const auto prices = PriceGSRSLVEuropeanOptions(*result->model_, Options(quotes), settings.pricing_);
            for (size_t i = 0; i < prices.size(); ++i) {
                result->modelPrices_.push_back(prices[i].price_);
                result->standardErrors_.push_back(prices[i].standardError_);
                result->conditionalErrors_.push_back(prices[i].conditionalError_);
                result->residuals_.push_back(prices[i].price_ - quotes[i].price_);
                result->fitWithinTolerance_ = result->fitWithinTolerance_ && std::abs(result->residuals_.back()) <= quotes[i].priceScale_;
            }
            for (size_t i = 0; i < x.size(); ++i) {
                result->parameters_.push_back(x[i] * parameters[i].scale_);
                result->activeBounds_.push_back(x[i] == problem.Bound(i, false) || x[i] == problem.Bound(i, true));
            }
            result->quoteJacobian_ = problem.Jacobian(x, quotes.size());
            result->evaluations_ += problem.JacobianEvaluations(x.size());
            RankDiagnostics(result->quoteJacobian_, result);
            for (int row = 0; row < result->quoteJacobian_.Rows(); ++row)
                for (int col = 0; col < result->quoteJacobian_.Cols(); ++col)
                    result->quoteJacobian_(row, col) *= quotes[row].priceScale_ / parameters[col].scale_;
        }

        GSRSLVCalibrationResult_ FitOnly(const GSRSLVModelData_& initial,
                                         const Vector_<CalibrationQuote_>& quotes,
                                         const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                         const GSRSLVCalibrationSettings_& settings) {
            ValidateSettings(settings);
            ValidateQuotes(quotes);
            const Problem_ problem(initial, quotes, parameters, settings);
            auto x = problem.Guess();
            GSRSLVCalibrationResult_ result;
            const int warmIterations = WarmStart(problem, settings, &x, &result);
            Fit(problem, {settings.solver_.maxIterations_, settings.solver_.gradientTolerance_, settings.solver_.stepTolerance_}, &x, &result);
            result.iterations_ += warmIterations;
            result.model_ = problem.Model(x);
            FitDiagnostics(problem, x, quotes, parameters, settings, &result);
            return result;
        }

        void ValidateHeldOutQuotes(const Vector_<CalibrationQuote_>& heldOut, const Vector_<CalibrationQuote_>& quotes) {
            ValidateQuotes(heldOut, true);
            for (const auto& quote : heldOut)
                for (const auto& fitted : quotes)
                    REQUIRE(quote.name_ != fitted.name_, "InvalidGSRSLVCalibration: held-out quote overlaps fit names");
        }

        void NumericalValidation(const GSRSLVModelData_& validation,
                                 const Vector_<CalibrationQuote_>& quotes,
                                 const GSRSLVCalibrationSettings_& settings,
                                 GSRSLVCalibrationResult_* result) {
            const auto refined = PriceGSRSLVEuropeanOptions(validation, Options(quotes), settings.validation_);
            result->numericalValidationPassed_ = true;
            for (size_t i = 0; i < quotes.size(); ++i) {
                result->validationPrices_.push_back(refined[i].price_);
                result->validationStandardErrors_.push_back(refined[i].standardError_);
                result->validationConditionalErrors_.push_back(refined[i].conditionalError_);
                const double error = std::abs(refined[i].price_ - result->modelPrices_[i]) +
                                     settings.validationSigma_ * std::hypot(refined[i].standardError_, result->standardErrors_[i]) +
                                     refined[i].conditionalError_ + result->conditionalErrors_[i];
                result->numericalErrors_.push_back(error);
                result->numericalValidationPassed_ =
                    result->numericalValidationPassed_ && error <= settings.solver_.numericalErrorFraction_ * quotes[i].priceScale_;
            }
        }

        void HeldOutValidation(const GSRSLVModelData_& validation,
                               const Vector_<CalibrationQuote_>& heldOut,
                               const GSRSLVCalibrationSettings_& settings,
                               GSRSLVCalibrationResult_* result) {
            result->heldOutWithinTolerance_ = true;
            if (heldOut.empty())
                return;
            const auto prices = PriceGSRSLVEuropeanOptions(validation, Options(heldOut), settings.validation_);
            for (size_t i = 0; i < prices.size(); ++i) {
                result->heldOutPrices_.push_back(prices[i].price_);
                result->heldOutResiduals_.push_back(prices[i].price_ - heldOut[i].price_);
                result->heldOutStandardErrors_.push_back(prices[i].standardError_);
                result->heldOutConditionalErrors_.push_back(prices[i].conditionalError_);
                result->heldOutWithinTolerance_ =
                    result->heldOutWithinTolerance_ &&
                    std::abs(result->heldOutResiduals_.back()) + settings.validationSigma_ * prices[i].standardError_ + prices[i].conditionalError_ <=
                        heldOut[i].priceScale_;
            }
        }
    } // namespace

    namespace GSRSLVCalibrationInternal {
        GSRSLVCalibrationResult_ FitModel(const GSRSLVModelData_& initial,
                                          const Vector_<CalibrationQuote_>& quotes,
                                          const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                          const GSRSLVCalibrationSettings_& settings) {
            return FitOnly(initial, quotes, parameters, settings);
        }
    } // namespace GSRSLVCalibrationInternal

    GSRSLVCalibrationResult_ CalibrateGSRSLV(const GSRSLVModelData_& initial,
                                             const Vector_<CalibrationQuote_>& quotes,
                                             const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                             const GSRSLVCalibrationSettings_& settings,
                                             const Vector_<CalibrationQuote_>& heldOut) {
        ValidateHeldOutQuotes(heldOut, quotes);
        auto result = FitOnly(initial, quotes, parameters, settings);
        GSRSLVSettings_ fine{result.model_->kappa_, result.model_->volOfVol_, result.model_->varianceCorrelations_, result.model_->maxStep_ / 2.0};
        const GSRSLVModelData_ validation("validation", result.model_->gaussian_, result.model_->leverage_, fine);
        auto refinedSettings = settings;
        REQUIRE(settings.pricing_.conditionalPaths_ <= std::numeric_limits<int>::max() / 2,
                "InvalidGSRSLVCalibration: validation conditional budget overflows");
        refinedSettings.validation_.conditionalPaths_ = std::max(settings.validation_.conditionalPaths_, 2 * settings.pricing_.conditionalPaths_);
        NumericalValidation(validation, quotes, refinedSettings, &result);
        HeldOutValidation(validation, heldOut, refinedSettings, &result);
        return result;
    }
} // namespace Dal
