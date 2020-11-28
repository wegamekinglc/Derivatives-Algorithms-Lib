//
// Created by wegam on 2020/11/28.
//

#include <gtest/gtest.h>
#include <dal/time/holidays.hpp>

using namespace Dal;

TEST(ChinaCalendarTest, TestConstructHolidays) {
    Holidays_ hol("china.sse");
    ASSERT_EQ(hol.String(), "china.sse");
}

TEST(ChinaCalendarTest, TestHolidaysClass) {
    Holidays_ hol("china.sse");
    ASSERT_TRUE(hol.IsHoliday(Date_(2020, 10, 7)));
    ASSERT_FALSE(hol.IsWorkWeekends(Date_(2020, 10, 10)));

    hol = Holidays_("china.ib");
    ASSERT_TRUE(hol.IsHoliday(Date_(2020, 10, 7)));
    ASSERT_TRUE(hol.IsWorkWeekends(Date_(2020, 10, 10)));
}

TEST(ChinaCalendarTest, TestConstructFromMultiple) {
    Holidays_ hol("china.sse china.ib");
    ASSERT_EQ(hol.String(), "china.ib china.sse");

    ASSERT_TRUE(hol.IsHoliday(Date_(2020, 10, 7)));
    ASSERT_TRUE(hol.IsWorkWeekends(Date_(2020, 10, 10)));
}

TEST(ChinaCalendarTest, TestNextBus) {
    Holidays_ hol("china.sse");
    ASSERT_EQ(hol.String(), "china.sse");

    Date_ ref_date(2020, 10, 10);

    auto val = Holidays::NextBus(hol, ref_date);
    ASSERT_EQ(val, Date_(2020, 10, 12));

    hol = Holidays_("china.ib");
    val = Holidays::NextBus(hol, ref_date);
    ASSERT_EQ(val, Date_(2020, 10, 10));
}

TEST(ChinaCalendarTest, TestPrevBus) {
    Holidays_ hol("china.sse");
    ASSERT_EQ(hol.String(), "china.sse");

    Date_ ref_date(2020, 10, 10);

    auto val = Holidays::PrevBus(hol, ref_date);
    ASSERT_EQ(val, Date_(2020, 10, 9));

    hol = Holidays_("china.ib");
    val = Holidays::PrevBus(hol, ref_date);
    ASSERT_EQ(val, Date_(2020, 10, 10));
}
