//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <cmath>
#include <set>

#include <dal/curve/logdfinterp.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/model/gsrmarketcalibrationinternal.hpp>

namespace Dal {
    namespace {
        double Density(double difference, double deviation, double offset = 0.0) {
            return deviation > 0.0 ? NPDF(difference / deviation + offset) : (difference == 0.0 ? NPDF(0.0) : 0.0);
        }
        void ValidateQuote(const GSRMarketQuote_& quote) {
            REQUIRE(std::isfinite(quote.volatility_) && quote.volatility_ >= 0.0, "InvalidGSRMarketQuote: volatility must be finite and nonnegative");
            REQUIRE(std::isfinite(quote.priceScale_) && quote.priceScale_ > 0.0, "InvalidGSRMarketQuote: price scale must be finite and positive");
            REQUIRE(quote.convention_ == "NORMAL" || quote.convention_ == "BLACK" || quote.convention_ == "SHIFTED_BLACK",
                    "InvalidGSRMarketQuote: unknown convention");
            REQUIRE(std::isfinite(quote.shift_), "InvalidGSRMarketQuote: shift must be finite");
            REQUIRE(quote.convention_ == "SHIFTED_BLACK" || quote.shift_ == 0.0, "InvalidGSRMarketQuote: shift requires SHIFTED_BLACK");
        }

        std::pair<double, double> Shifted(double forward, double strike, double shift) {
            const double shiftedForward = forward + shift, shiftedStrike = strike + shift;
            REQUIRE(std::isfinite(shiftedForward) && std::isfinite(shiftedStrike) && shiftedForward > 0.0 && shiftedStrike > 0.0,
                    "InvalidGSRMarketQuote: shifted forward and strike must be finite and positive");
            return {shiftedForward, shiftedStrike};
        }

        GSRMarketQuoteValue_
        QuoteValue(const GSRMarketQuote_& quote, double forward, double annuity, double rootTime, double strike, const OptionType_& type) {
            const double stdDev = quote.volatility_ * rootTime;
            REQUIRE(std::isfinite(forward) && std::isfinite(annuity) && annuity > 0.0 && std::isfinite(stdDev),
                    "InvalidGSRMarketQuote: nonfinite forward, annuity or standard deviation");
            GSRMarketQuoteValue_ result{forward, annuity};
            if (quote.convention_ == "NORMAL") {
                result.price_ = annuity * Distribution::BachelierOpt(forward, stdDev, strike, type);
                result.vega_ = annuity * rootTime * Density(forward - strike, stdDev);
            } else {
                const auto [shiftedForward, shiftedStrike] = Shifted(forward, strike, quote.shift_);
                result.price_ = annuity * Distribution::BlackOpt(shiftedForward, stdDev, shiftedStrike, type);
                result.vega_ = annuity * rootTime * shiftedForward * Density(std::log(shiftedForward / shiftedStrike), stdDev, 0.5 * stdDev);
            }
            REQUIRE(std::isfinite(result.price_) && result.price_ >= 0.0 && std::isfinite(result.vega_),
                    "InvalidGSRMarketQuote: price or vega overflow");
            return result;
        }
        Vector_<> Times(const GSRCurveData_& curve) {
            Vector_<> times;
            for (const auto& date : curve.nodeDates_)
                times.push_back((date - curve.evaluationDate_) / DAYS_PER_YEAR);
            return times;
        }

        class Market_ {
            const GSRCurveData_& curve_;
            LogDfInterpolation_ interpolation_;

            double Projection(const Date_& date, const String_& tenor) const {
                const int months = PeriodLength_(tenor).Months();
                REQUIRE(months > 0, "InvalidGSRMarketQuote: tenor must have positive months");
                for (size_t i = 0; i < curve_.projectionTenors_.size(); ++i)
                    if (PeriodLength_(curve_.projectionTenors_[i]).Months() == months) {
                        Vector_<> values;
                        for (int col = 0; col < curve_.projectionLogDF_.Cols(); ++col)
                            values.push_back(curve_.projectionLogDF_(static_cast<int>(i), col));
                        return interpolation_.Evaluate(values, Time(date));
                    }
                return interpolation_.Evaluate(curve_.discountLogDF_, Time(date));
            }

            double FloatingPV(const GSRFloatingCoupon_& coupon, const Date_& expiry) const {
                REQUIRE(coupon.fixing_ >= expiry && coupon.fixing_ <= coupon.start_ && coupon.start_ < coupon.end_ && coupon.end_ <= coupon.payment_,
                        "InvalidGSRMarketQuote: invalid future coupon schedule");
                static_cast<void>(Time(coupon.fixing_));
                REQUIRE(std::isfinite(coupon.indexAccrual_) && coupon.indexAccrual_ > 0.0 && std::isfinite(coupon.couponAccrual_) &&
                            coupon.couponAccrual_ > 0.0,
                        "InvalidGSRMarketQuote: accruals must be finite and positive");
                return coupon.couponAccrual_ / coupon.indexAccrual_ *
                       std::expm1(Projection(coupon.start_, coupon.tenor_) - Projection(coupon.end_, coupon.tenor_)) * Discount(coupon.payment_);
            }

            std::pair<double, double> ForwardAnnuity(const GSRBondOption_&) const {
                THROW("InvalidGSRMarketQuote: bond options require price quotes");
            }
            std::pair<double, double> ForwardAnnuity(const GSRCaplet_& option) const {
                const GSRFloatingCoupon_ coupon{option.expiry_,       option.start_,         option.end_,  option.payment_,
                                                option.indexAccrual_, option.couponAccrual_, option.tenor_};
                const double pv = FloatingPV(coupon, option.expiry_), annuity = option.couponAccrual_ * Discount(option.payment_);
                return {pv / annuity, annuity};
            }
            std::pair<double, double> ForwardAnnuity(const GSRSwaption_& option) const {
                REQUIRE(!option.fixed_.empty() && !option.floating_.empty(), "InvalidGSRMarketQuote: swap legs must be nonempty");
                double pv = 0.0, annuity = 0.0;
                for (const auto& coupon : option.floating_)
                    pv += FloatingPV(coupon, option.expiry_);
                for (const auto& coupon : option.fixed_) {
                    REQUIRE(coupon.payment_ >= option.expiry_ && std::isfinite(coupon.accrual_) && coupon.accrual_ > 0.0,
                            "InvalidGSRMarketQuote: invalid fixed coupon");
                    annuity += coupon.accrual_ * Discount(coupon.payment_);
                }
                return {pv / annuity, annuity};
            }

        public:
            explicit Market_(const GSRCurveData_& curve) : curve_(curve), interpolation_(Times(curve), LogDfScheme_("LOG_LINEAR")) {}
            double Time(const Date_& date) const {
                REQUIRE(date.IsValid() && date >= curve_.evaluationDate_ && date <= curve_.nodeDates_.back(),
                        "InvalidGSRMarketQuote: date outside curve horizon");
                return (date - curve_.evaluationDate_) / DAYS_PER_YEAR;
            }
            double Discount(const Date_& date) const { return std::exp(interpolation_.Evaluate(curve_.discountLogDF_, Time(date))); }

            GSRMarketQuoteValue_ Convert(const GSRMarketQuote_& quote) const {
                ValidateQuote(quote);
                return std::visit(
                    [&](const auto& option) {
                        REQUIRE(std::isfinite(option.strike_) && (option.type_ == OptionType_("CALL") || option.type_ == OptionType_("PUT")),
                                "InvalidGSRMarketQuote: invalid strike or option type");
                        const double rootTime = std::sqrt(Time(option.expiry_));
                        const auto [forward, annuity] = ForwardAnnuity(option);
                        return QuoteValue(quote, forward, annuity, rootTime, option.strike_, option.type_);
                    },
                    quote.option_);
            }
        };
    } // namespace

    Vector_<GSRMarketQuoteValue_> ConvertGSRMarketQuotes(const GSRCurveData_& curve, const Vector_<GSRMarketQuote_>& quotes) {
        const Market_ market(curve);
        std::set<String_> names;
        Vector_<GSRMarketQuoteValue_> result;
        for (const auto& quote : quotes) {
            REQUIRE(!quote.name_.empty() && names.insert(quote.name_).second, "InvalidGSRMarketQuote: names must be nonempty and unique");
            result.push_back(market.Convert(quote));
        }
        return result;
    }

    namespace GSRSLVCalibrationInternal {
        Vector_<GSRCalibrationQuote_> PriceQuotes(const GSRCurveData_& curve, const Vector_<GSRMarketQuote_>& quotes) {
            const auto values = ConvertGSRMarketQuotes(curve, quotes);
            Vector_<GSRCalibrationQuote_> result;
            for (size_t i = 0; i < quotes.size(); ++i)
                result.push_back({quotes[i].name_, quotes[i].option_, values[i].price_, quotes[i].priceScale_});
            return result;
        }
    } // namespace GSRSLVCalibrationInternal

    GSRSLVCalibrationResult_ CalibrateGSRSLVMarket(const GSRSLVModelData_& initial,
                                                   const Vector_<GSRMarketQuote_>& quotes,
                                                   const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                                   const GSRSLVCalibrationSettings_& settings,
                                                   const Vector_<GSRMarketQuote_>& heldOut) {
        const auto& curve = *initial.gaussian_->curve_;
        return CalibrateGSRSLV(initial, GSRSLVCalibrationInternal::PriceQuotes(curve, quotes), parameters, settings,
                               GSRSLVCalibrationInternal::PriceQuotes(curve, heldOut));
    }
} // namespace Dal
