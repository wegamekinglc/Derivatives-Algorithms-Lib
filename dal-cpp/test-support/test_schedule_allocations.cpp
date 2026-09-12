//
// Created by Codex on 2026/9/12.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/time/schedules.hpp>
#include "bcg_allocation_probe.hpp"

TEST(SchedulesTest, TestContextAllocationCountDoesNotGrowPerPeriod) {
    const Dal::Date_ start(2025, 1, 15);
    const Dal::Date_ end(2045, 1, 15);
    const Dal::PeriodLength_ tenor("1M");
    const Dal::Holidays_ holidays("");
    const Dal::DateGeneration_ generation("Forward");
    const Dal::BizDayConvention_ convention("Unadjusted");
    const auto build = [&]() {
        return Dal::MakeSchedulePeriods(start, end, tenor, holidays, 0, holidays, 0, holidays, generation, convention, convention, false);
    };
    ASSERT_EQ(build().size(), 240);
    Dal::BcgAllocationProbePrivate_::Reset_();
    Dal::BcgAllocationProbePrivate_::Measurement_ measurement;
    const auto periods = build();
    const auto snapshot = measurement.Finish_();
    ASSERT_EQ(periods.size(), 240);
    ASSERT_TRUE(snapshot.balanced_);
    ASSERT_FALSE(snapshot.failedClosed_);
    ASSERT_LT(snapshot.allocationRequests_, 24u);
}
