//
// Created by Codex on 2026/7/13.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <algorithm>
#include <cmath>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/time/dateincrement.hpp>

namespace Dal {
    namespace {
        bool ValidFixingIdentity(const FixingIdentity_& identity) {
            return !identity.indexName_.empty() && identity.fixingHour_ >= 0 && identity.fixingHour_ < 24 && identity.fixingMinute_ >= 0 &&
                   identity.fixingMinute_ < 60;
        }

        void ValidateResetConfig(const CrossCurrencySwapConfig_& config) {
            if (config.notionalMode_ == XccyNotionalMode_::Value_::FIXED)
                return;
            REQUIRE(config.fxReset_.fixingLag_ >= 0, "Resettable XCCY plan requires a non-negative FX fixing lag");
            REQUIRE(config.fxReset_.fixingHour_ >= 0 && config.fxReset_.fixingHour_ < 24 && config.fxReset_.fixingMinute_ >= 0 &&
                        config.fxReset_.fixingMinute_ < 60,
                    "Resettable XCCY plan requires a valid FX fixing time");
            REQUIRE(ValidFixingIdentity(config.domesticRateFixing_) && ValidFixingIdentity(config.foreignRateFixing_),
                    "Resettable XCCY plan requires explicit domestic and foreign rate fixing identities");
        }

        DateTime_ RateFixingTime(const Date_& date, const FixingIdentity_& identity) {
            if (identity.fixingHour_ < 0 || identity.fixingMinute_ < 0)
                return DateTime_(date);
            return DateTime_(date, identity.fixingHour_, identity.fixingMinute_);
        }

        Date_ FxFixingDate(const Date_& effectiveDate, const FxResetConvention_& convention) {
            const Date_ lagged = convention.fixingLag_ == 0
                                     ? effectiveDate
                                     : Date::NBusDays(convention.fixingLag_, convention.fixingHolidays_)->BackFrom(effectiveDate);
            return Holidays::Adjust(convention.fixingHolidays_, lagged, convention.fixingConvention_);
        }

        void SetRateFixings(Vector_<XccyCouponPeriod_>* periods, const FixingIdentity_& identity) {
            if (identity.indexName_.empty())
                return;
            for (auto& period : *periods) {
                period.rateIndexName_ = identity.indexName_;
                period.rateFixingTime_ = RateFixingTime(period.schedule_.fixingDate_, identity);
            }
        }

        bool RequestLess(const FixingRequest_& lhs, const FixingRequest_& rhs) {
            return lhs.indexName_ < rhs.indexName_ || (lhs.indexName_ == rhs.indexName_ && lhs.fixingTime_ < rhs.fixingTime_);
        }

        bool SameRequest(const FixingRequest_& lhs, const FixingRequest_& rhs) {
            return lhs.indexName_ == rhs.indexName_ && lhs.fixingTime_ == rhs.fixingTime_;
        }

        void AddHistoricalRateRequests(const Vector_<XccyCouponPeriod_>& periods,
                                       const DateTime_& valuationTime,
                                       const String_& context,
                                       Vector_<FixingRequest_>* requests) {
            for (const auto& period : periods) {
                if (period.schedule_.paymentDate_ < valuationTime.Date())
                    continue;
                if (period.rateIndexName_.empty()) {
                    REQUIRE(!(period.schedule_.fixingDate_ < valuationTime.Date()),
                            context + " has no canonical fixing identity for an unpaid historical fixing");
                    continue;
                }
                if (!(period.rateFixingTime_ < valuationTime))
                    continue;
                requests->push_back({period.rateIndexName_, period.rateFixingTime_});
            }
        }

        void MarkRemainingCouponNotionals(const XccyCashflowPlan_& plan, const DateTime_& valuationTime, std::vector<bool>* required) {
            for (int i = 0; i < static_cast<int>(plan.domesticPeriods_.size()); ++i)
                (*required)[i] = plan.domesticPeriods_[i].schedule_.paymentDate_ >= valuationTime.Date();
        }

        void MarkFinalExchangeNotional(const XccyCashflowPlan_& plan, const DateTime_& valuationTime, std::vector<bool>* required) {
            if (plan.config_.convention_.finalNotionalExchange_ && plan.maturity_ >= valuationTime.Date() && !required->empty())
                required->back() = true;
        }

        void MarkUnsettledMtmNotionals(const XccyCashflowPlan_& plan, const DateTime_& valuationTime, std::vector<bool>* required) {
            for (const auto& reset : plan.resets_) {
                REQUIRE(reset.domesticPeriodIndex_ > 0 && reset.domesticPeriodIndex_ < static_cast<int>(required->size()),
                        "XCCY reset event references an invalid domestic period");
                if (reset.effectiveDate_ >= valuationTime.Date()) {
                    (*required)[reset.domesticPeriodIndex_] = true;
                    (*required)[reset.domesticPeriodIndex_ - 1] = true;
                }
            }
        }

        std::vector<bool> RequiredDomesticNotionalPeriods(const XccyCashflowPlan_& plan, const DateTime_& valuationTime) {
            std::vector<bool> result(plan.domesticPeriods_.size(), false);
            if (plan.config_.notionalMode_ == XccyNotionalMode_::Value_::FIXED)
                return result;
            MarkRemainingCouponNotionals(plan, valuationTime, &result);
            MarkFinalExchangeNotional(plan, valuationTime, &result);
            if (plan.config_.notionalMode_ == XccyNotionalMode_::Value_::MARK_TO_MARKET)
                MarkUnsettledMtmNotionals(plan, valuationTime, &result);
            return result;
        }

        template <class T_> T_ DiscountFromValuation(const Tape::DiscountCurve_<T_>& curve, const DateTime_& valuationTime, const Date_& date) {
            const T_ result = date == valuationTime.Date() ? T_(1.0) : curve(valuationTime.Date(), date);
            const double value = Dal::AAD::Value(result);
            REQUIRE(std::isfinite(value) && value > 0.0, "XCCY pricing requires positive finite discount factors");
            return result;
        }

        template <class T_, class F_>
        T_ ResolveObservedValue(const String_& indexName,
                                const DateTime_& fixingTime,
                                const DateTime_& valuationTime,
                                const MarketFixingSnapshot_& fixings,
                                const String_& context,
                                F_&& activeValue) {
            if (fixingTime < valuationTime)
                return T_(fixings.Require(indexName, fixingTime, context));
            if (fixingTime == valuationTime) {
                const std::optional<double> supplied = fixings.Find(indexName, fixingTime);
                if (supplied)
                    return T_(*supplied);
            }
            return activeValue();
        }

        template <class T_> T_ ActiveFxForward(const XccyCashflowPlan_& plan, const XccyMarketView_<T_>& market, const DateTime_& fixingTime) {
            const auto& domesticDiscount = market.domestic_->Discount(plan.config_.convention_.domesticIndex_.collateral_);
            const auto& foreignDiscount = market.foreign_->Discount(plan.config_.convention_.foreignIndex_.collateral_);
            REQUIRE(domesticDiscount.ccy_ == plan.config_.pair_.domestic_ && foreignDiscount.ccy_ == plan.config_.pair_.foreign_,
                    "XCCY FX forward curves do not match the configured currency pair");
            const T_ domesticDf = DiscountFromValuation(domesticDiscount, market.valuationTime_, fixingTime.Date());
            const T_ foreignDf = DiscountFromValuation(foreignDiscount, market.valuationTime_, fixingTime.Date());
            const T_ basisDf = market.basis_ ? DiscountFromValuation(*market.basis_, market.valuationTime_, fixingTime.Date()) : T_(1.0);
            return market.fxSpot_ * foreignDf / (domesticDf * basisDf);
        }

        template <class T_> void ValidateMarketView(const XccyCashflowPlan_& plan, const XccyMarketView_<T_>& market) {
            REQUIRE(market.valuationTime_.IsValid(), "XCCY pricing requires a valid valuation time");
            REQUIRE(market.pair_ == plan.config_.pair_, "XCCY pricing market pair does not match the cashflow plan");
            REQUIRE(market.collateralCurrency_ == plan.config_.pair_.domestic_, "XCCY pricing supports domestic-currency collateral only");
            REQUIRE(market.domestic_ && market.foreign_, "XCCY pricing requires domestic and foreign curve blocks");
            const double fxSpot = Dal::AAD::Value(market.fxSpot_);
            REQUIRE(std::isfinite(fxSpot) && fxSpot > 0.0, "XCCY pricing requires a positive finite FX spot");
            REQUIRE(!market.basis_ || market.basis_->ccy_ == plan.config_.pair_.domestic_,
                    "XCCY basis curve currency must match the domestic currency");
        }

    } // namespace

    XccyCashflowPlan_ BuildXccyCashflowPlan(const Date_& start, const Date_& maturity, const CrossCurrencySwapConfig_& config) {
        ValidateResetConfig(config);
        XccyCashflowPlan_ result;
        result.config_ = config;
        result.start_ = start;
        result.maturity_ = maturity;
        result.domesticPeriods_ =
            BuildLegPeriods<XccyCouponPeriod_>(start, maturity, config.convention_.domesticLeg_, config.convention_.domesticIndex_.fixingLag_,
                                               config.convention_.domesticIndex_.fixingHolidays_);
        result.foreignPeriods_ =
            BuildLegPeriods<XccyCouponPeriod_>(start, maturity, config.convention_.foreignLeg_, config.convention_.foreignIndex_.fixingLag_,
                                               config.convention_.foreignIndex_.fixingHolidays_);
        SetRateFixings(&result.domesticPeriods_, config.domesticRateFixing_);
        SetRateFixings(&result.foreignPeriods_, config.foreignRateFixing_);

        if (config.notionalMode_ == XccyNotionalMode_::Value_::FIXED)
            return result;

        for (int i = 1; i < static_cast<int>(result.domesticPeriods_.size()); ++i) {
            const Date_ effectiveDate = result.domesticPeriods_[i].schedule_.accrualStart_;
            const Date_ fixingDate = FxFixingDate(effectiveDate, config.fxReset_);
            result.resets_.push_back({effectiveDate, DateTime_(fixingDate, config.fxReset_.fixingHour_, config.fxReset_.fixingMinute_), i});
        }
        return result;
    }

    Vector_<FixingRequest_> RequiredHistoricalFixings(const XccyCashflowPlan_& plan, const DateTime_& valuationTime) {
        Vector_<FixingRequest_> result;
        AddHistoricalRateRequests(plan.domesticPeriods_, valuationTime, "XCCY domestic floating coupon", &result);
        AddHistoricalRateRequests(plan.foreignPeriods_, valuationTime, "XCCY foreign floating coupon", &result);

        const std::vector<bool> requiredNotionals = RequiredDomesticNotionalPeriods(plan, valuationTime);
        for (const auto& reset : plan.resets_) {
            REQUIRE(reset.domesticPeriodIndex_ >= 0 && reset.domesticPeriodIndex_ < static_cast<int>(plan.domesticPeriods_.size()),
                    "XCCY reset event references an invalid domestic period");
            if (!requiredNotionals[reset.domesticPeriodIndex_] || !(reset.fxFixingTime_ < valuationTime))
                continue;
            result.push_back({FxIndexName(plan.config_.pair_), reset.fxFixingTime_});
        }

        std::sort(result.begin(), result.end(), RequestLess);
        result.erase(std::unique(result.begin(), result.end(), SameRequest), result.end());
        return result;
    }
    template <class T_>
    XccyResolvedNotionals_<T_>
    ResolveXccyNotionals(const XccyCashflowPlan_& plan, const XccyMarketView_<T_>& market, const MarketFixingSnapshot_& fixings) {
        ValidateMarketView(plan, market);
        XccyResolvedNotionals_<T_> result;
        result.domesticNotionals_ = Vector_<T_>(plan.domesticPeriods_.size(), T_(plan.config_.domesticNotional_));
        if (plan.config_.notionalMode_ == XccyNotionalMode_::Value_::FIXED)
            return result;

        const std::vector<bool> requiredNotionals = RequiredDomesticNotionalPeriods(plan, market.valuationTime_);
        if (plan.config_.notionalMode_ == XccyNotionalMode_::Value_::MARK_TO_MARKET)
            result.mtmDeltas_ = Vector_<T_>(plan.resets_.size(), T_(0.0));
        const String_ fxIndexName = FxIndexName(plan.config_.pair_);
        for (int i = 0; i < static_cast<int>(plan.resets_.size()); ++i) {
            const auto& reset = plan.resets_[i];
            REQUIRE(reset.domesticPeriodIndex_ == i + 1 && reset.domesticPeriodIndex_ < static_cast<int>(result.domesticNotionals_.size()),
                    "XCCY reset events must map consecutively from the second domestic period");
            if (!requiredNotionals[reset.domesticPeriodIndex_])
                continue;
            const T_ fx = ResolveObservedValue<T_>(fxIndexName, reset.fxFixingTime_, market.valuationTime_, fixings, "XCCY domestic notional reset",
                                                   [&]() { return ActiveFxForward(plan, market, reset.fxFixingTime_); });
            const T_ newNotional = T_(plan.config_.foreignNotional_) * fx;
            const T_ previousNotional = result.domesticNotionals_[reset.domesticPeriodIndex_ - 1];
            result.domesticNotionals_[reset.domesticPeriodIndex_] = newNotional;
            if (plan.config_.notionalMode_ == XccyNotionalMode_::Value_::MARK_TO_MARKET && reset.effectiveDate_ >= market.valuationTime_.Date())
                result.mtmDeltas_[i] = newNotional - previousNotional;
        }
        return result;
    }

    template XccyResolvedNotionals_<double>
    ResolveXccyNotionals(const XccyCashflowPlan_&, const XccyMarketView_<double>&, const MarketFixingSnapshot_&);
    template XccyResolvedNotionals_<Dal::AAD::Number_>
    ResolveXccyNotionals(const XccyCashflowPlan_&, const XccyMarketView_<Dal::AAD::Number_>&, const MarketFixingSnapshot_&);

} // namespace Dal

namespace Dal {
    namespace {
        template <class T_>
        const Tape::DiscountCurve_<T_>& ForecastCurve(const Tape::JointCurveBlock_<T_>& block, const RateIndexConvention_& convention) {
            return convention.useProjectionCurve_ ? block.Forward(convention.forecastTenor_, convention.collateral_)
                                                  : block.Discount(convention.collateral_);
        }

        template <class T_> T_ ActiveForwardRate(const XccyCouponPeriod_& period, const Tape::DiscountCurve_<T_>& forecast) {
            const T_ df = forecast(period.schedule_.accrualStart_, period.schedule_.accrualEnd_);
            REQUIRE(Dal::AAD::Value(df) > 0.0 && period.accrual_.dcf_ > 0.0,
                    "XCCY floating coupon requires positive forecast discount factor and accrual fraction");
            return (T_(1.0) / df - T_(1.0)) / static_cast<double>(period.accrual_.dcf_);
        }

        template <class T_>
        T_ CouponRate(const XccyCouponPeriod_& period,
                      const Tape::DiscountCurve_<T_>& forecast,
                      const XccyMarketView_<T_>& market,
                      const MarketFixingSnapshot_& fixings,
                      const String_& context) {
            if (period.rateIndexName_.empty()) {
                REQUIRE(!(period.schedule_.fixingDate_ < market.valuationTime_.Date()),
                        context + " has no canonical fixing identity for an unpaid historical fixing");
                return ActiveForwardRate(period, forecast);
            }
            return ResolveObservedValue<T_>(period.rateIndexName_, period.rateFixingTime_, market.valuationTime_, fixings, context,
                                            [&]() { return ActiveForwardRate(period, forecast); });
        }

        template <class T_>
        T_ ForeignConversionFactor(const Tape::DiscountCurve_<T_>& foreignDiscount, const XccyMarketView_<T_>& market, const Date_& paymentDate) {
            const T_ foreignDf = DiscountFromValuation(foreignDiscount, market.valuationTime_, paymentDate);
            const T_ basisDf = market.basis_ ? DiscountFromValuation(*market.basis_, market.valuationTime_, paymentDate) : T_(1.0);
            return market.fxSpot_ * foreignDf / basisDf;
        }

        template <class T_> struct XccyCouponValues_ {
            T_ pv_;
            T_ spreadAnnuity_;
        };

        template <class T_>
        T_ DomesticNotional(const XccyCashflowPlan_& plan, const XccyResolvedNotionals_<T_>& resolved, bool fixedNotional, int periodIndex) {
            if (fixedNotional)
                return T_(plan.config_.domesticNotional_);
            return resolved.domesticNotionals_[periodIndex];
        }

        template <class T_>
        XccyCouponValues_<T_> AccumulateDomesticCoupons(const XccyCashflowPlan_& plan,
                                                        const XccyMarketView_<T_>& market,
                                                        const MarketFixingSnapshot_& fixings,
                                                        const Tape::DiscountCurve_<T_>& forecast,
                                                        const Tape::DiscountCurve_<T_>& discount,
                                                        const XccyResolvedNotionals_<T_>& resolved,
                                                        bool fixedNotional) {
            XccyCouponValues_<T_> result{T_(0.0), T_(0.0)};
            for (int i = 0; i < static_cast<int>(plan.domesticPeriods_.size()); ++i) {
                const auto& period = plan.domesticPeriods_[i];
                if (period.schedule_.paymentDate_ < market.valuationTime_.Date())
                    continue;
                const T_ rate = CouponRate(period, forecast, market, fixings, "XCCY domestic floating coupon");
                const T_ df = DiscountFromValuation(discount, market.valuationTime_, period.schedule_.paymentDate_);
                const T_ annuity = DomesticNotional(plan, resolved, fixedNotional, i) * static_cast<double>(period.accrual_.dcf_) * df;
                result.pv_ += rate * annuity;
                result.spreadAnnuity_ += annuity;
            }
            return result;
        }

        template <class T_>
        XccyCouponValues_<T_> AccumulateForeignCoupons(const XccyCashflowPlan_& plan,
                                                       const XccyMarketView_<T_>& market,
                                                       const MarketFixingSnapshot_& fixings,
                                                       const Tape::DiscountCurve_<T_>& forecast,
                                                       const Tape::DiscountCurve_<T_>& discount) {
            XccyCouponValues_<T_> result{T_(0.0), T_(0.0)};
            for (const auto& period : plan.foreignPeriods_) {
                if (period.schedule_.paymentDate_ < market.valuationTime_.Date())
                    continue;
                const T_ rate = CouponRate(period, forecast, market, fixings, "XCCY foreign floating coupon");
                const T_ annuity = T_(plan.config_.foreignNotional_) * static_cast<double>(period.accrual_.dcf_) *
                                   ForeignConversionFactor(discount, market, period.schedule_.paymentDate_);
                result.pv_ += rate * annuity;
                result.spreadAnnuity_ += annuity;
            }
            return result;
        }

        template <class T_> struct XccyNotionalExchangeAdjustments_ {
            T_ domesticPv_;
            T_ foreignPv_;
        };

        template <class T_>
        XccyNotionalExchangeAdjustments_<T_> InitialNotionalExchangeAdjustments(const XccyCashflowPlan_& plan,
                                                                                const XccyMarketView_<T_>& market,
                                                                                const Tape::DiscountCurve_<T_>& domesticDiscount,
                                                                                const Tape::DiscountCurve_<T_>& foreignDiscount,
                                                                                const XccyResolvedNotionals_<T_>& resolved,
                                                                                bool fixedNotional) {
            XccyNotionalExchangeAdjustments_<T_> result{T_(0.0), T_(0.0)};
            if (!plan.config_.convention_.initialNotionalExchange_)
                return result;

            const Date_ domesticStart = plan.start_;
            if (domesticStart >= market.valuationTime_.Date())
                result.domesticPv_ -= DomesticNotional(plan, resolved, fixedNotional, 0) *
                                      DiscountFromValuation(domesticDiscount, market.valuationTime_, domesticStart);
            const Date_ foreignStart = plan.start_;
            if (foreignStart >= market.valuationTime_.Date())
                result.foreignPv_ -= T_(plan.config_.foreignNotional_) * ForeignConversionFactor(foreignDiscount, market, foreignStart);
            return result;
        }

        template <class T_>
        void ApplyMtmNotionalExchangeAdjustments(const XccyCashflowPlan_& plan,
                                                 const XccyMarketView_<T_>& market,
                                                 const Tape::DiscountCurve_<T_>& domesticDiscount,
                                                 const XccyResolvedNotionals_<T_>& resolved,
                                                 T_* domesticPv) {
            if (plan.config_.notionalMode_ != XccyNotionalMode_::Value_::MARK_TO_MARKET)
                return;

            REQUIRE(resolved.mtmDeltas_.size() == plan.resets_.size(), "XCCY MTM delta count does not match reset events");
            for (int i = 0; i < static_cast<int>(plan.resets_.size()); ++i) {
                if (plan.resets_[i].effectiveDate_ < market.valuationTime_.Date())
                    continue;
                *domesticPv +=
                    resolved.mtmDeltas_[i] * DiscountFromValuation(domesticDiscount, market.valuationTime_, plan.resets_[i].effectiveDate_);
            }
        }
    } // namespace

    template <class T_>
    T_ PriceXccyParSpread(const XccyCashflowPlan_& plan, const XccyMarketView_<T_>& market, const MarketFixingSnapshot_& fixings) {
        ValidateMarketView(plan, market);
        REQUIRE(!plan.domesticPeriods_.empty() && !plan.foreignPeriods_.empty(), "XCCY pricing requires coupon periods on both legs");
        const bool fixedNotional = plan.config_.notionalMode_ == XccyNotionalMode_::Value_::FIXED;
        XccyResolvedNotionals_<T_> resolved;
        if (!fixedNotional)
            resolved = ResolveXccyNotionals<T_>(plan, market, fixings);
        const auto& convention = plan.config_.convention_;
        const auto& domesticDiscount = market.domestic_->Discount(convention.domesticIndex_.collateral_);
        const auto& foreignDiscount = market.foreign_->Discount(convention.foreignIndex_.collateral_);
        const auto& domesticForecast = ForecastCurve(*market.domestic_, convention.domesticIndex_);
        const auto& foreignForecast = ForecastCurve(*market.foreign_, convention.foreignIndex_);
        REQUIRE(domesticDiscount.ccy_ == plan.config_.pair_.domestic_ && foreignDiscount.ccy_ == plan.config_.pair_.foreign_,
                "XCCY discount curves do not match the configured currency pair");

        const Date_ valuationDate = market.valuationTime_.Date();
        const auto domesticCoupons = AccumulateDomesticCoupons(plan, market, fixings, domesticForecast, domesticDiscount, resolved, fixedNotional);
        const auto foreignCoupons = AccumulateForeignCoupons(plan, market, fixings, foreignForecast, foreignDiscount);
        T_ domesticPv = domesticCoupons.pv_;
        T_ foreignPv = foreignCoupons.pv_;

        const auto initialAdjustments = InitialNotionalExchangeAdjustments(plan, market, domesticDiscount, foreignDiscount, resolved, fixedNotional);
        domesticPv += initialAdjustments.domesticPv_;
        foreignPv += initialAdjustments.foreignPv_;
        ApplyMtmNotionalExchangeAdjustments(plan, market, domesticDiscount, resolved, &domesticPv);

        if (convention.finalNotionalExchange_ && plan.maturity_ >= valuationDate) {
            domesticPv += DomesticNotional(plan, resolved, fixedNotional, static_cast<int>(plan.domesticPeriods_.size()) - 1) *
                          DiscountFromValuation(domesticDiscount, market.valuationTime_, plan.maturity_);
            foreignPv += T_(plan.config_.foreignNotional_) * ForeignConversionFactor(foreignDiscount, market, plan.maturity_);
        }

        if (convention.spreadOnForeignLeg_) {
            REQUIRE(Dal::AAD::Value(foreignCoupons.spreadAnnuity_) > 0.0, "XCCY pricing requires a positive remaining foreign spread annuity");
            return (domesticPv - foreignPv) / foreignCoupons.spreadAnnuity_;
        }
        REQUIRE(Dal::AAD::Value(domesticCoupons.spreadAnnuity_) > 0.0, "XCCY pricing requires a positive remaining domestic spread annuity");
        return (foreignPv - domesticPv) / domesticCoupons.spreadAnnuity_;
    }

    template double PriceXccyParSpread(const XccyCashflowPlan_&, const XccyMarketView_<double>&, const MarketFixingSnapshot_&);
    template Dal::AAD::Number_ PriceXccyParSpread(const XccyCashflowPlan_&, const XccyMarketView_<Dal::AAD::Number_>&, const MarketFixingSnapshot_&);
} // namespace Dal
