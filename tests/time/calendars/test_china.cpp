//
// Created by wegam on 2020/11/28.
//

#include <gtest/gtest.h>
#include <dal/time/holidays.hpp>

using namespace Dal;

TEST(ChinaCalendarTest, TestConstructHolidays) {
    Holidays_ hol("CN.SH");
    ASSERT_EQ(hol.String(), "CN.SH");
}

TEST(ChinaCalendarTest, TestHolidaysClass) {
    Holidays_ hol("CN.SH");
    ASSERT_TRUE(hol.IsHoliday(Date_(2020, 10, 7)));
    ASSERT_FALSE(hol.IsWorkWeekends(Date_(2020, 10, 10)));

    hol = Holidays_("CN.IB");
    ASSERT_TRUE(hol.IsHoliday(Date_(2020, 10, 7)));
    ASSERT_TRUE(hol.IsWorkWeekends(Date_(2020, 10, 10)));
}

TEST(ChinaCalendarTest, TestConstructFromMultiple) {
    Holidays_ hol("CN.SH CN.IB");
    ASSERT_EQ(hol.String(), "CN.IB CN.SH");

    ASSERT_TRUE(hol.IsHoliday(Date_(2020, 10, 7)));
    ASSERT_TRUE(hol.IsWorkWeekends(Date_(2020, 10, 10)));
}

TEST(ChinaCalendarTest, TestNextBus) {
    Holidays_ hol("CN.SH");
    ASSERT_EQ(hol.String(), "CN.SH");

    Date_ ref_date(2020, 10, 10);

    auto val = Holidays::NextBus(hol, ref_date);
    ASSERT_EQ(val, Date_(2020, 10, 12));

    hol = Holidays_("CN.IB");
    val = Holidays::NextBus(hol, ref_date);
    ASSERT_EQ(val, Date_(2020, 10, 10));
}

TEST(ChinaCalendarTest, TestPrevBus) {
    Holidays_ hol("CN.SH");
    ASSERT_EQ(hol.String(), "CN.SH");

    Date_ ref_date(2020, 10, 10);

    auto val = Holidays::PrevBus(hol, ref_date);
    ASSERT_EQ(val, Date_(2020, 10, 9));

    hol = Holidays_("CN.IB");
    val = Holidays::PrevBus(hol, ref_date);
    ASSERT_EQ(val, Date_(2020, 10, 10));
}

TEST(ChinaCalendarTest, TestCountBusDays) {
    CountBusDays_ counter(Holidays_("CN.SH"));

    Date_ start_date(2020, 10, 9);
    Date_ end_date(2020, 10, 11);
    auto val = counter(start_date, end_date);
    ASSERT_EQ(val, 1);

    counter = CountBusDays_(Holidays_("CN.IB"));
    val = counter(start_date, end_date);
    ASSERT_EQ(val, 2);
}
