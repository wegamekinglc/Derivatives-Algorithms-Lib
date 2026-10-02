//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>

#include <dal/math/distribution/black.hpp>
#include <dal/math/integral/quadrature.hpp>
#include <dal/math/operators.hpp>
#include <dal/model/gsr.hpp>
#include <dal/model/gsreuropean.hpp>

namespace Dal {
    namespace {
        double NormalCDF(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

        double NormalInterval(double lo, double hi) { return lo > 0.0 ? NormalCDF(-lo) - NormalCDF(-hi) : NormalCDF(hi) - NormalCDF(lo); }

        double Direction(const OptionType_& type) {
            REQUIRE(type == OptionType_::Value_::CALL || type == OptionType_::Value_::PUT, "InvalidGSROption: type must be CALL or PUT");
            return type == OptionType_::Value_::CALL ? 1.0 : -1.0;
        }

        struct Exponential_ {
            double coefficient_;
            double exponent_;
        };

        int SignAt(const Vector_<Exponential_>& terms, double x) {
            double largest = -std::numeric_limits<double>::infinity();
            for (const auto& term : terms)
                largest = std::max(largest, std::log(std::abs(term.coefficient_)) + term.exponent_ * x);
            double value = 0.0;
            for (const auto& term : terms)
                value += std::copysign(std::exp(std::log(std::abs(term.coefficient_)) + term.exponent_ * x - largest), term.coefficient_);
            return (value > 0.0) - (value < 0.0);
        }

        double Root(const Vector_<Exponential_>& terms, double lo, double hi, int leftSign) {
            for (int i = 0; i < 70; ++i) {
                const double mid = 0.5 * (lo + hi);
                if (mid == lo || mid == hi)
                    break;
                const int sign = SignAt(terms, mid);
                if (sign == 0)
                    return mid;
                if (sign == leftSign)
                    lo = mid;
                else
                    hi = mid;
            }
            return 0.5 * (lo + hi);
        }

        Vector_<> ExerciseRoots(const Vector_<Exponential_>& terms, double lo, double hi) {
            Vector_<> roots;
            if (terms.size() < 2)
                return roots;
            const bool sameSign = std::all_of(terms.begin(), terms.end(),
                                              [&](const auto& term) { return (term.coefficient_ > 0.0) == (terms.front().coefficient_ > 0.0); });
            if (sameSign)
                return roots;
            Vector_<Exponential_> derivative;
            for (size_t i = 1; i < terms.size(); ++i)
                derivative.push_back(
                    {terms[i].coefficient_ * (terms[i].exponent_ - terms.front().exponent_), terms[i].exponent_ - terms.front().exponent_});
            Vector_<> stationary{lo};
            for (const auto root : ExerciseRoots(derivative, lo, hi))
                stationary.push_back(root);
            stationary.push_back(hi);
            for (size_t i = 1; i < stationary.size(); ++i) {
                const int left = SignAt(terms, stationary[i - 1]);
                const int right = SignAt(terms, stationary[i]);
                if (left == 0)
                    roots.push_back(stationary[i - 1]);
                if (left * right < 0)
                    roots.push_back(Root(terms, stationary[i - 1], stationary[i], left));
            }
            return roots;
        }

        Vector_<Exponential_> CombineTerms(Vector_<Exponential_> terms) {
            std::sort(terms.begin(), terms.end(), [](const auto& a, const auto& b) { return a.exponent_ < b.exponent_; });
            Vector_<Exponential_> combined;
            for (const auto& term : terms) {
                if (!combined.empty() && term.exponent_ == combined.back().exponent_)
                    combined.back().coefficient_ += term.coefficient_;
                else
                    combined.push_back(term);
            }
            combined.erase(std::remove_if(combined.begin(), combined.end(), [](const auto& term) { return term.coefficient_ == 0.0; }),
                           combined.end());
            return combined;
        }

        Vector_<std::pair<double, double>> PositiveIntervals(const Vector_<Exponential_>& terms, double range) {
            Vector_<> boundaries{-range};
            for (const auto root : ExerciseRoots(terms, -range, range))
                boundaries.push_back(root);
            boundaries.push_back(range);
            Vector_<std::pair<double, double>> intervals;
            for (size_t i = 1; i < boundaries.size(); ++i)
                if (SignAt(terms, 0.5 * (boundaries[i - 1] + boundaries[i])) > 0)
                    intervals.push_back({boundaries[i - 1], boundaries[i]});
            return intervals;
        }

        GSRPriceResult_ ConditionalPositivePart(Vector_<Exponential_> terms) {
            const auto combined = CombineTerms(std::move(terms));
            if (combined.empty())
                return {};
            double range = 10.0;
            for (const auto& term : combined)
                range = std::max(range, 10.0 + std::abs(term.exponent_));
            const auto intervals = PositiveIntervals(combined, range);
            double price = 0.0, tail = 0.0;
            for (const auto& term : combined) {
                const double moment = term.coefficient_ * std::exp(0.5 * term.exponent_ * term.exponent_);
                REQUIRE(std::isfinite(moment), "InvalidGSROption: exponential moment overflow");
                tail += std::abs(moment) * (NormalCDF(-range - term.exponent_) + NormalCDF(term.exponent_ - range));
                for (const auto& interval : intervals)
                    price += moment * NormalInterval(interval.first - term.exponent_, interval.second - term.exponent_);
            }
            return {std::max(price, 0.0), tail};
        }

        struct Bond_ {
            double logValue_;
            Vector_<> loading_;
        };

        struct Cashflow_ {
            double coefficient_;
            Vector_<> loading_;
        };

        void ValidateCoupon(const GSRFloatingCoupon_& coupon) {
            REQUIRE(coupon.fixing_ <= coupon.start_ && coupon.start_ < coupon.end_ && coupon.payment_ >= coupon.end_,
                    "InvalidGSROption: fixing <= start < end <= payment is required");
            REQUIRE(std::isfinite(coupon.indexAccrual_) && coupon.indexAccrual_ > 0.0 && std::isfinite(coupon.couponAccrual_) &&
                        coupon.couponAccrual_ > 0.0,
                    "InvalidGSROption: accrual fractions must be finite and positive");
        }

        class Pricer_ {
            AAD::GSR_<double> model_;
            Date_ today_;
            double expiry_, discount_;
            Matrix_<> variance_, lower_;

            [[nodiscard]] double Time(const Date_& date) const {
                REQUIRE(date.IsValid() && date >= today_, "InvalidGSROption: dates must be valid and on or after the evaluation date");
                return (date - today_) / DAYS_PER_YEAR;
            }

            [[nodiscard]] Bond_ Bond(const Date_& date) const {
                const double time = Time(date);
                REQUIRE(time >= expiry_, "InvalidGSROption: cashflow date precedes expiry");
                const auto loading = model_.GaussianBondLoading(expiry_, time);
                Vector_<> beta(loading.size(), 0.0);
                for (int j = 0; j < lower_.Cols(); ++j)
                    for (int i = 0; i < lower_.Rows(); ++i)
                        beta[j] -= loading[i] * lower_(i, j);
                const double norm = std::inner_product(beta.begin(), beta.end(), beta.begin(), 0.0);
                return {model_.InitialLogDiscount(time) - model_.InitialLogDiscount(expiry_) - 0.5 * norm, std::move(beta)};
            }

            [[nodiscard]] double CouponConvexity(const GSRFloatingCoupon_& coupon) const {
                const double start = Time(coupon.start_), end = Time(coupon.end_);
                const double fixing = Time(coupon.fixing_);
                REQUIRE(fixing >= expiry_, "InvalidGSROption: coupon fixing precedes expiry");
                const auto covariance = model_.GaussianVariance(fixing);
                const auto a = model_.GaussianBondLoading(fixing, start), b = model_.GaussianBondLoading(fixing, end),
                           d = model_.GaussianBondLoading(fixing, Time(coupon.payment_));
                double convexity = 0.0;
                for (int i = 0; i < covariance.Rows(); ++i)
                    for (int j = 0; j < covariance.Cols(); ++j)
                        convexity += (a[i] - b[i]) * (covariance(i, j) - variance_(i, j)) * (d[j] - b[j]);
                return convexity;
            }

            [[nodiscard]] double ProjectionScale(const GSRFloatingCoupon_& coupon) const {
                ValidateCoupon(coupon);
                REQUIRE(!coupon.tenor_.empty() && PeriodLength_(coupon.tenor_).Months() > 0, "InvalidGSROption: floating tenor must be positive");
                const double start = Time(coupon.start_), end = Time(coupon.end_);
                return model_.InitialLogProjection(start, coupon.tenor_) - model_.InitialLogProjection(end, coupon.tenor_) -
                       model_.InitialLogDiscount(start) + model_.InitialLogDiscount(end) + CouponConvexity(coupon);
            }

            [[nodiscard]] Vector_<Cashflow_> Cashflows(const GSRSwaption_& option) const {
                REQUIRE(!option.fixed_.empty() && !option.floating_.empty(), "InvalidGSROption: fixed and floating schedules must be nonempty");
                const double sign = Direction(option.type_);
                Vector_<Cashflow_> result;
                for (const auto& coupon : option.fixed_) {
                    REQUIRE(std::isfinite(coupon.accrual_) && coupon.accrual_ > 0.0, "InvalidGSROption: fixed accrual must be finite and positive");
                    const auto bond = Bond(coupon.payment_);
                    result.push_back({-sign * option.strike_ * coupon.accrual_ * std::exp(bond.logValue_), bond.loading_});
                }
                for (const auto& coupon : option.floating_) {
                    const double scale = ProjectionScale(coupon);
                    const auto start = Bond(coupon.start_), end = Bond(coupon.end_), pay = Bond(coupon.payment_);
                    auto beta = pay.loading_;
                    for (size_t i = 0; i < beta.size(); ++i)
                        beta[i] += start.loading_[i] - end.loading_[i];
                    const double accrual = sign * coupon.couponAccrual_ / coupon.indexAccrual_;
                    result.push_back({accrual * std::exp(scale + start.logValue_ - end.logValue_ + pay.logValue_), beta});
                    result.push_back({-accrual * std::exp(pay.logValue_), pay.loading_});
                }
                return result;
            }

            [[nodiscard]] static GSRPriceResult_ Integrate(const Vector_<Cashflow_>& cashflows,
                                                           const Vector_<size_t>& axes,
                                                           const NormalExpectation_<>& quadrature,
                                                           Vector_<>* normals,
                                                           size_t level) {
                if (level + 1 < axes.size()) {
                    GSRPriceResult_ result;
                    for (size_t i = 0; i < quadrature.Abscissa().size(); ++i) {
                        (*normals)[axes[level]] = quadrature.Abscissa()[i];
                        const auto part = Integrate(cashflows, axes, quadrature, normals, level + 1);
                        result.price_ += quadrature.Weight()[i] * part.price_;
                        result.numericalError_ += quadrature.Weight()[i] * part.numericalError_;
                    }
                    return result;
                }
                Vector_<Exponential_> terms;
                for (const auto& cashflow : cashflows) {
                    double exponent = 0.0;
                    for (size_t i = 0; i + 1 < axes.size(); ++i)
                        exponent += cashflow.loading_[axes[i]] * (*normals)[axes[i]];
                    const double coefficient = cashflow.coefficient_ * std::exp(exponent);
                    REQUIRE(std::isfinite(coefficient), "InvalidGSROption: cashflow overflow");
                    terms.push_back({coefficient, axes.empty() ? 0.0 : cashflow.loading_[axes.back()]});
                }
                return ConditionalPositivePart(std::move(terms));
            }

        public:
            template <class D_>
            Pricer_(const D_& data, const Date_& expiry) : model_(data), today_(data.curve_->evaluationDate_), expiry_(Time(expiry)) {
                variance_ = model_.GaussianVariance(expiry_);
                lower_ = AAD::CovarianceFactor(variance_);
                discount_ = std::exp(model_.InitialLogDiscount(expiry_));
            }

            [[nodiscard]] GSRPriceResult_ Price(const GSRBondOption_& option, const GSRPricingSettings_&) const {
                Direction(option.type_);
                const auto bond = Bond(option.maturity_);
                const double variance = std::inner_product(bond.loading_.begin(), bond.loading_.end(), bond.loading_.begin(), 0.0);
                const double forward = std::exp(bond.logValue_ + 0.5 * variance);
                return {discount_ * Distribution::BlackOpt(forward, std::sqrt(variance), option.strike_, option.type_), 0.0};
            }

            [[nodiscard]] GSRPriceResult_ Price(const GSRCaplet_& option, const GSRPricingSettings_&) const {
                Direction(option.type_);
                const GSRFloatingCoupon_ coupon{option.expiry_,       option.start_,         option.end_,  option.payment_,
                                                option.indexAccrual_, option.couponAccrual_, option.tenor_};
                const double scale = ProjectionScale(coupon);
                const auto start = Bond(coupon.start_), end = Bond(coupon.end_), pay = Bond(coupon.payment_);
                double variance = 0.0, convexity = 0.0;
                for (size_t i = 0; i < start.loading_.size(); ++i) {
                    const double difference = start.loading_[i] - end.loading_[i];
                    variance += difference * difference;
                    convexity += difference * pay.loading_[i];
                }
                const double forward = std::exp(scale + start.logValue_ - end.logValue_ + 0.5 * variance + convexity);
                const double payDF = std::exp(model_.InitialLogDiscount(Time(option.payment_)));
                return {payDF * option.couponAccrual_ / option.indexAccrual_ *
                            Distribution::BlackOpt(forward, std::sqrt(variance), 1.0 + option.indexAccrual_ * option.strike_, option.type_),
                        0.0};
            }

            [[nodiscard]] GSRPriceResult_ Price(const GSRSwaption_& option, const GSRPricingSettings_& settings) const {
                const auto cashflows = Cashflows(option);
                Vector_<size_t> axes;
                Vector_<> importance(model_.NumFactors(), 0.0);
                for (size_t j = 0; j < importance.size(); ++j) {
                    for (const auto& cashflow : cashflows)
                        importance[j] += std::abs(cashflow.coefficient_ * cashflow.loading_[j]);
                    if (importance[j] != 0.0)
                        axes.push_back(j);
                }
                REQUIRE(axes.size() <= 3, "InvalidGSROption: swaption pricing supports at most three effective Gaussian directions");
                std::sort(axes.begin(), axes.end(), [&](size_t a, size_t b) { return importance[a] < importance[b]; });
                Vector_<> normals(model_.NumFactors(), 0.0);
                auto result = Integrate(cashflows, axes, NormalExpectation_<>(settings.quadratureOrder_), &normals, 0);
                if (settings.estimateError_ && axes.size() > 1) {
                    const auto refined = Integrate(cashflows, axes, NormalExpectation_<>(2 * settings.quadratureOrder_), &normals, 0);
                    result.numericalError_ = std::abs(refined.price_ - result.price_) + refined.numericalError_;
                    result.price_ = refined.price_;
                }
                result.price_ *= discount_;
                result.numericalError_ *= discount_;
                return result;
            }
        };

        template <class D_> GSRPriceResult_ Price(const D_& model, const GSREuropeanOption_& option, const GSRPricingSettings_& settings) {
            REQUIRE(settings.quadratureOrder_ >= 2 && settings.quadratureOrder_ <= 64,
                    "InvalidGSRPricing: quadrature order must be between 2 and 64");
            return std::visit(
                [&](const auto& input) {
                    REQUIRE(std::isfinite(input.strike_), "InvalidGSROption: strike must be finite");
                    const auto result = Pricer_(model, input.expiry_).Price(input, settings);
                    REQUIRE(std::isfinite(result.price_) && std::isfinite(result.numericalError_), "InvalidGSROption: nonfinite price");
                    return result;
                },
                option);
        }
    } // namespace

    GSRPriceResult_
    PriceGSREuropeanOption(const MultiFactorGSRModelData_& model, const GSREuropeanOption_& option, const GSRPricingSettings_& settings) {
        return Price(model, option, settings);
    }

    GSRPriceResult_ PriceGSREuropeanOption(const GSRModelData_& model, const GSREuropeanOption_& option, const GSRPricingSettings_& settings) {
        return Price(model, option, settings);
    }
} // namespace Dal
