//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <set>

#include <dal/model/gsrcalibrationinternal.hpp>
#include <dal/model/gsrslvcalibrationinternal.hpp>
#include <dal/model/gsrslvpricinginternal.hpp>

namespace Dal {
    namespace {
        using namespace GSRCalibrationInternal;

        Vector_<GSREuropeanOption_> Options(const Vector_<GSRCalibrationQuote_>& quotes) {
            Vector_<GSREuropeanOption_> result;
            for (const auto& quote : quotes)
                result.push_back(quote.option_);
            return result;
        }

        void ValidateQuotes(const Vector_<GSRCalibrationQuote_>& quotes, bool allowEmpty = false) {
            REQUIRE(allowEmpty || !quotes.empty(), "InvalidGSRSLVCalibration: quotes must be nonempty");
            std::set<String_> names;
            for (const auto& quote : quotes) {
                REQUIRE(!quote.name_.empty() && names.insert(quote.name_).second,
                        "InvalidGSRSLVCalibration: quote names must be nonempty and unique");
                REQUIRE(std::isfinite(quote.price_) && quote.price_ >= 0.0 && std::isfinite(quote.priceScale_) && quote.priceScale_ > 0.0,
                        "InvalidGSRSLVCalibration: price must be finite and nonnegative; price scale finite and positive");
            }
        }

        void ValidateSettings(const GSRSLVCalibrationSettings_& settings) {
            REQUIRE(settings.solver_.maxIterations_ > 0, "InvalidGSRSLVCalibration: max iterations must be positive");
            for (double value : {settings.solver_.gradientTolerance_, settings.solver_.stepTolerance_, settings.solver_.finiteDifferenceStep_,
                                 settings.validationSigma_})
                REQUIRE(std::isfinite(value) && value > 0.0, "InvalidGSRSLVCalibration: tolerances and validation sigma must be finite and positive");
            for (double value : {settings.solver_.priorWeight_, settings.solver_.smoothingWeight_, settings.solver_.numericalErrorFraction_})
                REQUIRE(std::isfinite(value) && value >= 0.0,
                        "InvalidGSRSLVCalibration: penalty weights and numerical error fraction must be finite and nonnegative");
            REQUIRE(settings.pricing_.paths_ >= 4 && settings.pricing_.paths_ % 2 == 0 && settings.validation_.paths_ >= 4 &&
                        settings.validation_.paths_ % 2 == 0,
                    "InvalidGSRSLVCalibration: fit and validation paths must be even and at least four");
            REQUIRE(settings.pricing_.seed_ >= 0 && settings.validation_.seed_ >= 0 && settings.pricing_.seed_ != settings.validation_.seed_,
                    "InvalidGSRSLVCalibration: fit and validation seeds must be distinct and nonnegative");
        }

        class Problem_ {
            const GSRSLVModelData_& initial_;
            const Vector_<GSRCalibrationQuote_>& quotes_;
            const Vector_<GSRSLVCalibrationParameter_>& parameters_;
            const GSRSLVCalibrationSettings_& settings_;
            GSRSLVPricingInternal::PreparedPricer_ pricer_;
            Vector_<std::pair<int, int>> coordinates_;
            Vector_<> guess_;
            Matrix_<> nodeScales_;

            double Change(const Matrix_<>& values, int row, int col) const {
                return (values(row, col) - initial_.leverage_->values_(row, col)) / nodeScales_(row, col);
            }

            bool Selected(int row, int col) const {
                return std::find(coordinates_.begin(), coordinates_.end(), std::make_pair(row, col)) != coordinates_.end();
            }

        public:
            Problem_(const GSRSLVModelData_& initial,
                     const Vector_<GSRCalibrationQuote_>& quotes,
                     const Vector_<GSRSLVCalibrationParameter_>& parameters,
                     const GSRSLVCalibrationSettings_& settings)
                : initial_(initial), quotes_(quotes), parameters_(parameters), settings_(settings),
                  pricer_(initial, Options(quotes), settings.pricing_),
                  nodeScales_(initial.leverage_->values_.Rows(), initial.leverage_->values_.Cols(), 1.0) {
                REQUIRE(!parameters.empty(), "InvalidGSRSLVCalibration: selected parameters must be nonempty");
                std::set<String_> labels;
                for (const auto& parameter : parameters) {
                    REQUIRE(labels.insert(parameter.label_).second, "InvalidGSRSLVCalibration: duplicate parameter label");
                    REQUIRE(std::isfinite(parameter.lower_) && std::isfinite(parameter.upper_) && parameter.lower_ >= 0.0 &&
                                parameter.upper_ > parameter.lower_ && std::isfinite(parameter.scale_) && parameter.scale_ > 0.0 &&
                                std::isfinite(parameter.upper_ / parameter.scale_),
                            "InvalidGSRSLVCalibration: bounds must satisfy 0 <= lower < upper, with a positive finite scale");
                    std::pair<int, int> coordinate{-2, -2};
                    double value = 0.0;
                    if (parameter.label_ == "kappa") {
                        coordinate = {-1, 0};
                        value = initial.kappa_;
                    } else if (parameter.label_ == "volOfVol") {
                        coordinate = {-1, 1};
                        value = initial.volOfVol_;
                    } else {
                        for (int row = 0; row < nodeScales_.Rows(); ++row)
                            for (int col = 0; col < nodeScales_.Cols(); ++col)
                                if (parameter.label_ == "leverage:" + String::FromInt(row) + ":" + String::FromInt(col)) {
                                    coordinate = {row, col};
                                    value = initial.leverage_->values_(row, col);
                                    nodeScales_(row, col) = parameter.scale_;
                                }
                    }
                    REQUIRE(coordinate.first != -2, "InvalidGSRSLVCalibration: unknown parameter " + parameter.label_);
                    if (coordinate.first >= 0)
                        REQUIRE(parameter.lower_ / parameter.scale_ > 0.0,
                                "InvalidGSRSLVCalibration: normalized leverage lower bound must be strictly positive");
                    REQUIRE(value >= parameter.lower_ && value <= parameter.upper_, "InvalidGSRSLVCalibration: initial parameter outside bounds");
                    REQUIRE(parameter.upper_ / parameter.scale_ > parameter.lower_ / parameter.scale_ && std::isfinite(value / parameter.scale_) &&
                                (value == 0.0 || value / parameter.scale_ > 0.0),
                            "InvalidGSRSLVCalibration: parameter normalization is unresolved or nonfinite");
                    coordinates_.push_back(coordinate);
                    guess_.push_back(value / parameter.scale_);
                }
            }

            const Vector_<>& Guess() const { return guess_; }
            double Bound(size_t i, bool upper) const { return (upper ? parameters_[i].upper_ : parameters_[i].lower_) / parameters_[i].scale_; }
            bool Stochastic(size_t i) const { return coordinates_[i].first < 0; }

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
                if (settings_.solver_.smoothingWeight_ > 0.0) {
                    const auto& values = model->leverage_->values_;
                    for (int row = 0; row < values.Rows(); ++row)
                        for (int col = 0; col < values.Cols(); ++col) {
                            if (row > 0 && (Selected(row, col) || Selected(row - 1, col)))
                                result.push_back(std::sqrt(settings_.solver_.smoothingWeight_ /
                                                           (initial_.leverage_->rateShifts_[row] - initial_.leverage_->rateShifts_[row - 1])) *
                                                 (Change(values, row, col) - Change(values, row - 1, col)));
                            if (col > 0 && (Selected(row, col) || Selected(row, col - 1)))
                                result.push_back(std::sqrt(settings_.solver_.smoothingWeight_ /
                                                           (initial_.leverage_->times_[col] - initial_.leverage_->times_[col - 1])) *
                                                 (Change(values, row, col) - Change(values, row, col - 1)));
                        }
                }
                return result;
            }

            Matrix_<> Jacobian(const Vector_<>& x, size_t rows) const {
                return DifferenceJacobian(*this, x, rows, settings_.solver_.finiteDifferenceStep_);
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
            Matrix_<> Jacobian(const Vector_<>& x, size_t rows) const { return DifferenceJacobian(*this, x, rows, differenceStep_); }
        };

        GSRSLVCalibrationResult_ FitOnly(const GSRSLVModelData_& initial,
                                         const Vector_<GSRCalibrationQuote_>& quotes,
                                         const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                         const GSRSLVCalibrationSettings_& settings) {
            ValidateSettings(settings);
            ValidateQuotes(quotes);
            const Problem_ problem(initial, quotes, parameters, settings);
            auto x = problem.Guess();
            GSRSLVCalibrationResult_ result;
            int warmIterations = 0;
            if (settings.staged_) {
                Vector_<size_t> stochastic, leverage;
                for (size_t i = 0; i < x.size(); ++i)
                    (problem.Stochastic(i) ? stochastic : leverage).push_back(i);
                if (!stochastic.empty() && !leverage.empty())
                    for (const auto& selected : {stochastic, leverage}) {
                        const SubsetProblem_ subset(problem, x, selected, settings.solver_.finiteDifferenceStep_);
                        Vector_<> values;
                        for (size_t i : selected)
                            values.push_back(x[i]);
                        GSRSLVCalibrationResult_ warm;
                        Fit(subset, settings.solver_, &values, &warm);
                        x = subset.Expand(values);
                        warmIterations += warm.iterations_;
                        result.evaluations_ += warm.evaluations_;
                    }
            }
            Fit(problem, settings.solver_, &x, &result);
            result.iterations_ += warmIterations;
            result.model_ = problem.Model(x);
            result.fitWithinTolerance_ = true;
            Vector_<GSREuropeanOption_> options;
            for (const auto& quote : quotes)
                options.push_back(quote.option_);
            const auto prices = PriceGSRSLVEuropeanOptions(*result.model_, options, settings.pricing_);
            for (size_t i = 0; i < prices.size(); ++i) {
                result.modelPrices_.push_back(prices[i].price_);
                result.standardErrors_.push_back(prices[i].standardError_);
                result.residuals_.push_back(prices[i].price_ - quotes[i].price_);
                result.fitWithinTolerance_ = result.fitWithinTolerance_ && std::abs(result.residuals_.back()) <= quotes[i].priceScale_;
            }
            for (size_t i = 0; i < x.size(); ++i) {
                result.parameters_.push_back(x[i] * parameters[i].scale_);
                result.activeBounds_.push_back(x[i] == problem.Bound(i, false) || x[i] == problem.Bound(i, true));
            }
            result.quoteJacobian_ = problem.Jacobian(x, quotes.size());
            result.evaluations_ += 2 * static_cast<int>(x.size());
            RankDiagnostics(result.quoteJacobian_, &result);
            for (int row = 0; row < result.quoteJacobian_.Rows(); ++row)
                for (int col = 0; col < result.quoteJacobian_.Cols(); ++col)
                    result.quoteJacobian_(row, col) *= quotes[row].priceScale_ / parameters[col].scale_;
            return result;
        }
    } // namespace

    namespace GSRSLVCalibrationInternal {
        GSRSLVCalibrationResult_ FitModel(const GSRSLVModelData_& initial,
                                          const Vector_<GSRCalibrationQuote_>& quotes,
                                          const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                          const GSRSLVCalibrationSettings_& settings) {
            return FitOnly(initial, quotes, parameters, settings);
        }
    } // namespace GSRSLVCalibrationInternal

    GSRSLVCalibrationResult_ CalibrateGSRSLV(const GSRSLVModelData_& initial,
                                             const Vector_<GSRCalibrationQuote_>& quotes,
                                             const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                             const GSRSLVCalibrationSettings_& settings,
                                             const Vector_<GSRCalibrationQuote_>& heldOut) {
        ValidateQuotes(heldOut, true);
        for (const auto& quote : heldOut)
            for (const auto& fitted : quotes)
                REQUIRE(quote.name_ != fitted.name_, "InvalidGSRSLVCalibration: held-out quote overlaps fit names");
        auto result = FitOnly(initial, quotes, parameters, settings);
        GSRSLVSettings_ fine{result.model_->kappa_, result.model_->volOfVol_, result.model_->varianceCorrelations_, result.model_->maxStep_ / 2.0};
        const GSRSLVModelData_ validation("validation", result.model_->gaussian_, result.model_->leverage_, fine);
        Vector_<GSREuropeanOption_> options;
        for (const auto& quote : quotes)
            options.push_back(quote.option_);
        const auto refined = PriceGSRSLVEuropeanOptions(validation, options, settings.validation_);
        result.numericalValidationPassed_ = true;
        for (size_t i = 0; i < quotes.size(); ++i) {
            result.validationPrices_.push_back(refined[i].price_);
            result.validationStandardErrors_.push_back(refined[i].standardError_);
            const double error = std::abs(refined[i].price_ - result.modelPrices_[i]) +
                                 settings.validationSigma_ * std::hypot(refined[i].standardError_, result.standardErrors_[i]);
            result.numericalErrors_.push_back(error);
            result.numericalValidationPassed_ =
                result.numericalValidationPassed_ && error <= settings.solver_.numericalErrorFraction_ * quotes[i].priceScale_;
        }
        result.heldOutWithinTolerance_ = true;
        if (!heldOut.empty()) {
            options.clear();
            for (const auto& quote : heldOut)
                options.push_back(quote.option_);
            const auto prices = PriceGSRSLVEuropeanOptions(validation, options, settings.validation_);
            for (size_t i = 0; i < prices.size(); ++i) {
                result.heldOutPrices_.push_back(prices[i].price_);
                result.heldOutResiduals_.push_back(prices[i].price_ - heldOut[i].price_);
                result.heldOutStandardErrors_.push_back(prices[i].standardError_);
                result.heldOutWithinTolerance_ =
                    result.heldOutWithinTolerance_ &&
                    std::abs(result.heldOutResiduals_.back()) + settings.validationSigma_ * prices[i].standardError_ <= heldOut[i].priceScale_;
            }
        }
        return result;
    }
} // namespace Dal
