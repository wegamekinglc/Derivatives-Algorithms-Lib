//
// Created by Codex on 2026/7/13.
//

#include <gtest/gtest.h>

#include <dal/curve/xccypricing.hpp>

using namespace Dal;

namespace {
    CrossCurrencySwapConfig_ MakeQuarterlyConfig(XccyNotionalMode_ mode) {
        CrossCurrencySwapConfig_ config;
        config.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        config.domesticNotional_ = 110.0;
        config.foreignNotional_ = 100.0;
        config.notionalMode_ = mode;

        config.convention_.domesticLeg_.paymentFrequency_ = PeriodLength_("3M");
        config.convention_.domesticLeg_.dayBasis_ = DayBasis_("ACT_365F");
        config.convention_.domesticLeg_.accrualHolidays_ = Holidays::None();
        config.convention_.domesticLeg_.paymentHolidays_ = Holidays::None();
        config.convention_.domesticLeg_.businessDayConvention_ = BizDayConvention_("Unadjusted");
        config.convention_.domesticLeg_.paymentConvention_ = BizDayConvention_("Unadjusted");
        config.convention_.foreignLeg_ = config.convention_.domesticLeg_;

        config.convention_.domesticIndex_.fixingLag_ = 2;
        config.convention_.domesticIndex_.fixingHolidays_ = Holidays::None();
        config.convention_.domesticIndex_.forecastTenor_ = PeriodLength_("3M");
        config.convention_.foreignIndex_ = config.convention_.domesticIndex_;

        config.domesticRateFixing_ = {"USD-SOFR-3M", 11, 0};
        config.foreignRateFixing_ = {"EUR-ESTR-3M", 11, 0};
        config.fxReset_.fixingLag_ = 2;
        config.fxReset_.fixingHolidays_ = Holidays::None();
        config.fxReset_.fixingConvention_ = BizDayConvention_("Preceding");
        config.fxReset_.fixingHour_ = 10;
        config.fxReset_.fixingMinute_ = 30;
        return config;
    }

    bool HasRequest(const Vector_<FixingRequest_>& requests, const String_& indexName, const DateTime_& fixingTime) {
        for (const auto& request : requests) {
            if (request.indexName_ == indexName && request.fixingTime_ == fixingTime)
                return true;
        }
        return false;
    }
} // namespace

TEST(XccyPricingTest, TestQuarterlyMtmPlanResetsFromSecondPeriod) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));

    ASSERT_EQ(plan.domesticPeriods_.size(), 4U);
    ASSERT_EQ(plan.foreignPeriods_.size(), 4U);
    ASSERT_EQ(plan.resets_.size(), 3U);
    ASSERT_EQ(plan.resets_[0].effectiveDate_, plan.domesticPeriods_[1].schedule_.accrualStart_);
    ASSERT_EQ(plan.resets_[0].domesticPeriodIndex_, 1);
    ASSERT_EQ(plan.resets_[1].domesticPeriodIndex_, 2);
    ASSERT_EQ(plan.resets_[2].domesticPeriodIndex_, 3);
}

TEST(XccyPricingTest, TestResettableAndMtmPlansUseSameResetDates) {
    const auto resettable = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::RESETTABLE));
    const auto mtm = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));

    ASSERT_EQ(resettable.resets_.size(), mtm.resets_.size());
    for (int i = 0; i < static_cast<int>(resettable.resets_.size()); ++i) {
        ASSERT_EQ(resettable.resets_[i].effectiveDate_, mtm.resets_[i].effectiveDate_);
        ASSERT_EQ(resettable.resets_[i].fxFixingTime_, mtm.resets_[i].fxFixingTime_);
    }
}

TEST(XccyPricingTest, TestFixedPlanHasNoResets) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::FIXED));
    ASSERT_TRUE(plan.resets_.empty());
}

TEST(XccyPricingTest, TestPlanPreservesShortFinalStub) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 2, 15), MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));

    ASSERT_EQ(plan.domesticPeriods_.size(), 5U);
    ASSERT_TRUE(plan.domesticPeriods_.back().schedule_.isStub_);
    ASSERT_EQ(plan.domesticPeriods_.back().schedule_.accrualEnd_, Date_(2025, 2, 15));
    ASSERT_EQ(plan.resets_.size(), 4U);
}

TEST(XccyPricingTest, TestPlanAppliesRateAndFxFixingLagAndTime) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 8), Date_(2024, 7, 8), MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));

    ASSERT_EQ(plan.domesticPeriods_[0].rateFixingTime_, DateTime_(Date_(2024, 1, 4), 11, 0));
    ASSERT_EQ(plan.foreignPeriods_[0].rateFixingTime_, DateTime_(Date_(2024, 1, 4), 11, 0));
    ASSERT_EQ(plan.resets_[0].effectiveDate_, Date_(2024, 4, 8));
    ASSERT_EQ(plan.resets_[0].fxFixingTime_, DateTime_(Date_(2024, 4, 4), 10, 30));
}

TEST(XccyPricingTest, TestRequiredHistoricalFixingsExcludePaidCoupons) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 8), Date_(2025, 1, 8), MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));
    const auto requests = RequiredHistoricalFixings(plan, DateTime_(Date_(2024, 4, 9), 12, 0));

    ASSERT_EQ(requests.size(), 3U);
    ASSERT_TRUE(HasRequest(requests, "USD-SOFR-3M", DateTime_(Date_(2024, 4, 4), 11, 0)));
    ASSERT_TRUE(HasRequest(requests, "EUR-ESTR-3M", DateTime_(Date_(2024, 4, 4), 11, 0)));
    ASSERT_TRUE(HasRequest(requests, "FX[EUR/USD]", DateTime_(Date_(2024, 4, 4), 10, 30)));
}

TEST(XccyPricingTest, TestRequiredHistoricalFixingsIncludeValuationDatePayments) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));
    const auto requests = RequiredHistoricalFixings(plan, DateTime_(Date_(2024, 4, 4), 12, 0));

    ASSERT_EQ(requests.size(), 5U);
    ASSERT_TRUE(HasRequest(requests, "USD-SOFR-3M", DateTime_(Date_(2024, 1, 2), 11, 0)));
    ASSERT_TRUE(HasRequest(requests, "EUR-ESTR-3M", DateTime_(Date_(2024, 1, 2), 11, 0)));
    ASSERT_TRUE(HasRequest(requests, "USD-SOFR-3M", DateTime_(Date_(2024, 4, 2), 11, 0)));
    ASSERT_TRUE(HasRequest(requests, "EUR-ESTR-3M", DateTime_(Date_(2024, 4, 2), 11, 0)));
    ASSERT_TRUE(HasRequest(requests, "FX[EUR/USD]", DateTime_(Date_(2024, 4, 2), 10, 30)));
    for (int i = 1; i < static_cast<int>(requests.size()); ++i) {
        ASSERT_TRUE(requests[i - 1].indexName_ < requests[i].indexName_ ||
                    (requests[i - 1].indexName_ == requests[i].indexName_ && requests[i - 1].fixingTime_ < requests[i].fixingTime_));
    }
}

TEST(XccyPricingTest, TestResetPlanRejectsNegativeFxFixingLag) {
    auto config = MakeQuarterlyConfig(XccyNotionalMode_::Value_::RESETTABLE);
    config.fxReset_.fixingLag_ = -1;

    ASSERT_THROW(static_cast<void>(BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), config)), Dal::Exception_);
}

TEST(XccyPricingTest, TestResetPlanRequiresRateFixingIdentities) {
    auto config = MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    config.domesticRateFixing_ = FixingIdentity_();

    ASSERT_THROW(static_cast<void>(BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), config)), Dal::Exception_);
}
