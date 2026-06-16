//
// Created by Claude on 2026/6/16.
//

#include <dal/platform/platform.hpp>
#include <dal/time/date.hpp>
#include <dal/time/holidays.hpp>
#include <gtest/gtest.h>

using namespace Dal;

TEST(TargetCalendarTest, TestConstructHolidays) {
    Holidays_ hol("TARGET");
    ASSERT_EQ(hol.String(), "TARGET");
}

TEST(TargetCalendarTest, TestKnownHolidays) {
    Holidays_ hol("TARGET");
    // New Year's Day 2026 (Thursday)
    ASSERT_TRUE(hol.IsHoliday(Date_(2026, 1, 1)));
    // Good Friday 2026 (April 3 -- Easter is April 5)
    ASSERT_TRUE(hol.IsHoliday(Date_(2026, 4, 3)));
    // Easter Monday 2026 (April 6)
    ASSERT_TRUE(hol.IsHoliday(Date_(2026, 4, 6)));
    // Labour Day 2026 (May 1, Friday)
    ASSERT_TRUE(hol.IsHoliday(Date_(2026, 5, 1)));
    // Christmas Day 2026 (December 25, Friday)
    ASSERT_TRUE(hol.IsHoliday(Date_(2026, 12, 25)));
}

TEST(TargetCalendarTest, TestNonHolidays) {
    Holidays_ hol("TARGET");
    // Regular weekday -- not a TARGET holiday
    ASSERT_FALSE(hol.IsHoliday(Date_(2026, 4, 7)));  // Tuesday after Easter
    ASSERT_FALSE(hol.IsHoliday(Date_(2026, 6, 15))); // ordinary Monday
    // Dec 26 2026 is a Saturday (weekend -- not in holiday list, excluded by calendar engine)
    ASSERT_FALSE(hol.IsHoliday(Date_(2026, 12, 26)));
    ASSERT_TRUE(Date::IsWeekEnd(Date_(2026, 12, 26)));
}

TEST(TargetCalendarTest, TestNextBus) {
    Holidays_ hol("TARGET");
    // Good Friday 2026 should advance past Good Friday, Easter weekend, and Easter Monday
    // Apr 3 (Good Friday) -> Apr 7 (Tuesday)
    const auto val = Holidays::NextBus(hol, Date_(2026, 4, 3));
    ASSERT_EQ(val, Date_(2026, 4, 7));
}
