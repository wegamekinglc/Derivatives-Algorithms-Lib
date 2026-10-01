//
// Created by Codex on 2026/10/1.
//

#include <gtest/gtest.h>

#include <dal-public/src/calendar.hpp>
#include <dal-public/src/global.hpp>

TEST(PublicCalendarTest, TestWeekendAdjustmentAndCounting) {
    Dal::InitGlobalData();
    const Dal::Holidays_ holidays("");
    const Dal::Date_ friday(2026, 7, 31);
    const Dal::Date_ saturday(2026, 8, 1);
    const Dal::Date_ monday(2026, 8, 3);
    ASSERT_TRUE(Dal::IsBusinessDay(holidays, friday));
    ASSERT_FALSE(Dal::IsBusinessDay(holidays, saturday));
    ASSERT_EQ(monday, Dal::NextBusinessDay(holidays, saturday));
    ASSERT_EQ(friday, Dal::PreviousBusinessDay(holidays, saturday));
    ASSERT_EQ(monday, Dal::AdjustBusinessDate(holidays, saturday, Dal::BizDayConvention_("Following")));
    ASSERT_EQ(friday, Dal::AdjustBusinessDate(holidays, saturday, Dal::BizDayConvention_("Preceding")));
    ASSERT_EQ(1, Dal::CountBusinessDays(holidays, friday, monday));
}

TEST(PublicCalendarTest, TestModifiedFollowingKeepsOriginalMonth) {
    Dal::InitGlobalData();
    const Dal::Holidays_ holidays("");
    const Dal::Date_ saturday(2026, 1, 31);
    ASSERT_EQ(Dal::Date_(2026, 2, 2), Dal::AdjustBusinessDate(holidays, saturday, Dal::BizDayConvention_("Following")));
    ASSERT_EQ(Dal::Date_(2026, 1, 30), Dal::AdjustBusinessDate(holidays, saturday, Dal::BizDayConvention_("ModifiedFollowing")));
    ASSERT_EQ(saturday, Dal::AdjustBusinessDate(holidays, saturday, Dal::BizDayConvention_("Unadjusted")));
}
