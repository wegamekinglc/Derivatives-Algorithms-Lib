//
// Created by Codex on 2026/7/13.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <algorithm>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/xccypricing.hpp>
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

        void AddHistoricalRateRequests(const Vector_<XccyCouponPeriod_>& periods, const DateTime_& valuationTime, Vector_<FixingRequest_>* requests) {
            for (const auto& period : periods) {
                if (period.schedule_.paymentDate_ < valuationTime.Date() || period.rateIndexName_.empty() ||
                    !(period.rateFixingTime_ < valuationTime))
                    continue;
                requests->push_back({period.rateIndexName_, period.rateFixingTime_});
            }
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
        AddHistoricalRateRequests(plan.domesticPeriods_, valuationTime, &result);
        AddHistoricalRateRequests(plan.foreignPeriods_, valuationTime, &result);

        for (const auto& reset : plan.resets_) {
            REQUIRE(reset.domesticPeriodIndex_ >= 0 && reset.domesticPeriodIndex_ < static_cast<int>(plan.domesticPeriods_.size()),
                    "XCCY reset event references an invalid domestic period");
            const auto& targetPeriod = plan.domesticPeriods_[reset.domesticPeriodIndex_];
            if (targetPeriod.schedule_.paymentDate_ < valuationTime.Date() || !(reset.fxFixingTime_ < valuationTime))
                continue;
            result.push_back({FxIndexName(plan.config_.pair_), reset.fxFixingTime_});
        }

        std::sort(result.begin(), result.end(), RequestLess);
        result.erase(std::unique(result.begin(), result.end(), SameRequest), result.end());
        return result;
    }
} // namespace Dal
