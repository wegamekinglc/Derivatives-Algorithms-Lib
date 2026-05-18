//
// Created by wegam on 2022/10/2.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/cell.hpp>
#include <dal/time/schedules.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/dateincrement.hpp>

using namespace Dal;

TEST(SchedulesTest, TestDateGenerate) {
    Date_ start(2021, 10, 1);
    Date_ maturity(2022, 10, 2);
    Handle_<Date::Increment_> tenor = Date::ParseIncrement("3M");

    auto calculated = DateGenerate(start, maturity, tenor, DateGeneration_("Forward"));
    ASSERT_EQ(calculated.size(), 6);
    Vector_<Date_> expected = {Date_(2021, 10, 1),
                               Date_(2022, 1, 1),
                               Date_(2022, 4, 1),
                               Date_(2022, 7, 1),
                               Date_(2022, 10, 1),
                               Date_(2022, 10, 2)};
    ASSERT_EQ(calculated, expected);

    calculated = DateGenerate(start, maturity, tenor, DateGeneration_("Backward"));
    ASSERT_EQ(calculated.size(), 6);
    expected = {Date_(2021, 10, 1),
                Date_(2021, 10, 2),
                Date_(2022, 1, 2),
                Date_(2022, 4, 2),
                Date_(2022, 7, 2),
                Date_(2022, 10, 2)};
    ASSERT_EQ(calculated, expected);
}

TEST(SchedulesTest, TestMakeScheduleWithHolidays) {
    Date_ start(2021, 10, 1);
    Cell_ maturity = Cell_(Date_(2022, 10, 1));
    Holidays_ hols(Holidays::None());
    Handle_<Date::Increment_> tenor = Date::ParseIncrement("3M");

    auto calculated = MakeSchedule(start, maturity, hols, tenor);
    ASSERT_EQ(calculated.size(), 5);
    Vector_<Date_> expected = {Date_(2021, 10, 1),
                               Date_(2022, 1, 3),
                               Date_(2022, 4, 1),
                               Date_(2022, 7, 1),
                               Date_(2022, 10, 3)};
    ASSERT_EQ(calculated, expected);
}

TEST(SchedulesTest, TestMakeScheduleSupportsModifiedFollowing) {
    const Date_ start(2024, 8, 30);
    const Date_ maturity(2025, 2, 28);
    const Holidays_ hols(Holidays::None());

    const auto calculated = MakeSchedule(start,
                                         maturity,
                                         PeriodLength_("1M"),
                                         hols,
                                         DateGeneration_("Forward"),
                                         BizDayConvention_("ModifiedFollowing"));
    Vector_<Date_> expected = {
        Date_(2024, 8, 30),
        Date_(2024, 9, 30),
        Date_(2024, 10, 30),
        Date_(2024, 11, 29),
        Date_(2024, 12, 30),
        Date_(2025, 1, 30),
        Date_(2025, 2, 28),
    };
    ASSERT_EQ(calculated, expected);
}

TEST(SchedulesTest, TestMakeSchedulePeriodsBuildsFixingPaymentAndDayCountContext) {
    const Date_ start(2024, 1, 31);
    const Date_ maturity(2024, 7, 31);
    const Holidays_ hols(Holidays::None());
    const auto periods = MakeSchedulePeriods(start,
                                             maturity,
                                             PeriodLength_("3M"),
                                             hols,
                                             DayBasis_("ACT_365L"),
                                             2,
                                             hols,
                                             2,
                                             hols,
                                             DateGeneration_("Forward"),
                                             BizDayConvention_("ModifiedFollowing"),
                                             BizDayConvention_("ModifiedFollowing"),
                                             true);
    ASSERT_EQ(periods.size(), 2);
    ASSERT_EQ(periods.front().accrualStart_, Date_(2024, 1, 31));
    ASSERT_EQ(periods.front().accrualEnd_, Date_(2024, 4, 30));
    ASSERT_EQ(periods.front().fixingDate_, Date_(2024, 1, 29));
    ASSERT_EQ(periods.front().paymentDate_, Date_(2024, 5, 2));
    ASSERT_FALSE(periods.front().isStub_);
    ASSERT_TRUE(periods.front().dayCountContext_);
    ASSERT_EQ(periods.front().dayCountContext_->nominalStart_, Date_(2024, 1, 31));
    ASSERT_EQ(periods.front().dayCountContext_->nominalEnd_, Date_(2024, 4, 30));
    ASSERT_EQ(periods.front().dayCountContext_->couponMonths_, 3);
}
