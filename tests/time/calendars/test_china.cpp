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

TEST(ChinaCalendarTest, TestHolidays) {
    Vector_<Date_> expected_holidays{
// China Shanghai Securities Exchange holiday list in the year 2014
            Date_(2014, 1, 1), Date_(2014, 1, 31),
            Date_(2014, 2, 3), Date_(2014, 2, 4), Date_(2014, 2, 5), Date_(2014, 2, 6),
            Date_(2014, 4, 7),
            Date_(2014, 5, 1), Date_(2014, 5, 2),
            Date_(2014, 6, 2),
            Date_(2014, 9, 8),
            Date_(2014, 10, 1), Date_(2014, 10, 2), Date_(2014, 10, 3), Date_(2014, 10, 6), Date_(2014, 10, 7),
// China Shanghai Securities Exchange holiday list in the year 2015
            Date_(2015, 1, 1), Date_(2015, 1, 2),
            Date_(2015, 2, 18), Date_(2015, 2, 19), Date_(2015, 2, 20), Date_(2015, 2, 23), Date_(2015, 2, 24),
            Date_(2015, 4, 6),
            Date_(2015, 5, 1),
            Date_(2015, 6, 22),
            Date_(2015, 9, 3), Date_(2015, 9, 4),
            Date_(2015, 10, 1), Date_(2015, 10, 2), Date_(2015, 10, 5), Date_(2015, 10, 6), Date_(2015, 10, 7),
// China Shanghai Securities Exchange holiday list in the year 2016
            Date_(2016, 1, 1),
            Date_(2016, 2, 8), Date_(2016, 2, 9), Date_(2016, 2, 10), Date_(2016, 2, 11), Date_(2016, 2, 12),
            Date_(2016, 4, 4),
            Date_(2016, 5, 2),
            Date_(2016, 6, 9), Date_(2016, 6, 10),
            Date_(2016, 9, 15), Date_(2016, 9, 16),
            Date_(2016, 10, 3), Date_(2016, 10, 4), Date_(2016, 10, 5), Date_(2016, 10, 6), Date_(2016, 10, 7),
// China Shanghai Securities Exchange holiday list in the year 2017
            Date_(2017, 1, 1), Date_(2017, 1, 2),
            Date_(2017, 1, 27), Date_(2017, 1, 28), Date_(2017, 1, 29), Date_(2017, 1, 30), Date_(2017, 1, 31),
            Date_(2017, 2, 1), Date_(2017, 2, 2),
            Date_(2017, 4, 2), Date_(2017, 4, 3), Date_(2017, 4, 4),
            Date_(2017, 5, 1),
            Date_(2017, 5, 28), Date_(2017, 5, 29), Date_(2017, 5, 30),
            Date_(2017, 10, 1), Date_(2017, 10, 2), Date_(2017, 10, 3), Date_(2017, 10, 4), Date_(2017, 10, 5),
            Date_(2017, 10, 6), Date_(2017, 10, 7), Date_(2017, 10, 8),
// China Shanghai Securities Exchange holiday list in the year 2018
            Date_(2018, 1, 1),
            Date_(2018, 2, 15), Date_(2018, 2, 16), Date_(2018, 2, 17), Date_(2018, 2, 18), Date_(2018, 2, 19),
            Date_(2018, 2, 20), Date_(2018, 2, 21),
            Date_(2018, 4, 5), Date_(2018, 4, 6), Date_(2018, 4, 7),
            Date_(2018, 4, 29), Date_(2018, 4, 30), Date_(2018, 5, 1),
            Date_(2018, 6, 16), Date_(2018, 6, 17), Date_(2018, 6, 18),
            Date_(2018, 9, 22), Date_(2018, 9, 23), Date_(2018, 9, 24),
            Date_(2018, 10, 1), Date_(2018, 10, 2), Date_(2018, 10, 3), Date_(2018, 10, 4), Date_(2018, 10, 5),
            Date_(2018, 10, 6), Date_(2018, 10, 7),
// China Shanghai Securities Exchange holiday list in the year 2019
            Date_(2019, 1, 1),
            Date_(2019, 2, 4), Date_(2019, 2, 5), Date_(2019, 2, 6), Date_(2019, 2, 7), Date_(2019, 2, 8),
            Date_(2019, 4, 5),
            Date_(2019, 5, 1), Date_(2019, 5, 2), Date_(2019, 5, 3),
            Date_(2019, 6, 7),
            Date_(2019, 9, 13),
            Date_(2019, 10, 1), Date_(2019, 10, 2), Date_(2019, 10, 3), Date_(2019, 10, 4), Date_(2019, 10, 5),
            Date_(2019, 10, 6), Date_(2019, 10, 7),
// China Shanghai Securities Exchange holiday list in the year 2020
            Date_(2020, 1, 1),
            Date_(2020, 1, 24), Date_(2020, 1, 25), Date_(2020, 1, 26), Date_(2020, 1, 27), Date_(2020, 1, 28),
            Date_(2020, 1, 29), Date_(2020, 1, 30), Date_(2020, 1, 31),
            Date_(2020, 4, 4), Date_(2020, 4, 5), Date_(2020, 4, 6),
            Date_(2020, 5, 1), Date_(2020, 5, 2), Date_(2020, 5, 3), Date_(2020, 5, 4), Date_(2020, 5, 5),
            Date_(2020, 6, 25), Date_(2020, 6, 26), Date_(2020, 6, 27),
            Date_(2020, 10, 1), Date_(2020, 10, 2), Date_(2020, 10, 3), Date_(2020, 10, 4), Date_(2020, 10, 5),
            Date_(2020, 10, 6), Date_(2020, 10, 7), Date_(2020, 10, 8),
// China Shanghai Securities Exchange holiday list in the year 2021
            Date_(2021, 1, 1), Date_(2021, 1, 2), Date_(2021, 1, 3),
            Date_(2021, 2, 11), Date_(2021, 2, 12), Date_(2021, 2, 13), Date_(2021, 2, 14), Date_(2021, 2, 15),
            Date_(2021, 2, 16), Date_(2021, 2, 17),
            Date_(2021, 4, 3), Date_(2021, 4, 4), Date_(2021, 4, 5),
            Date_(2021, 5, 1), Date_(2021, 5, 2), Date_(2021, 5, 3), Date_(2021, 5, 4), Date_(2021, 5, 5),
            Date_(2021, 6, 12), Date_(2021, 6, 13), Date_(2021, 6, 14),
            Date_(2021, 9, 19), Date_(2021, 9, 20), Date_(2021, 9, 21),
            Date_(2021, 10, 1), Date_(2021, 10, 2), Date_(2021, 10, 3), Date_(2021, 10, 4), Date_(2021, 10, 5),
            Date_(2021, 10, 6), Date_(2021, 10, 7),
// China Shanghai Securities Exchange holiday list in the year 2022
            Date_(2022, 1, 3), Date_(2022, 1, 31),
            Date_(2022, 2, 1), Date_(2022, 2, 2), Date_(2022, 2, 3), Date_(2022, 2, 4),
            Date_(2022, 4, 4), Date_(2022, 4, 5),
            Date_(2022, 5, 2), Date_(2022, 5, 3), Date_(2022, 5, 4),
            Date_(2022, 6, 3),
            Date_(2022, 9, 12),
            Date_(2022, 10, 3), Date_(2022, 10, 4), Date_(2022, 10, 5), Date_(2022, 10, 6), Date_(2022, 10, 7),
// China Shanghai Securities Exchange holiday list in the year 2023
            Date_(2023, 1, 1), Date_(2023, 1, 2),
            Date_(2023, 1, 21), Date_(2023, 1, 22), Date_(2023, 1, 23), Date_(2023, 1, 24), Date_(2023, 1, 25),
            Date_(2023, 1, 26), Date_(2023, 1, 27),
            Date_(2023, 4, 5), Date_(2023, 4, 29), Date_(2023, 4, 30),
            Date_(2023, 5, 1), Date_(2023, 5, 2), Date_(2023, 5, 3),
            Date_(2023, 6, 22), Date_(2023, 6, 23), Date_(2023, 6, 24),
            Date_(2023, 9, 29), Date_(2023, 9, 30),
            Date_(2023, 10, 1), Date_(2023, 10, 2), Date_(2023, 10, 3), Date_(2023, 10, 4), Date_(2023, 10, 5),
            Date_(2023, 10, 6)
    };

    Holidays_ hol("CN.SH");

    for (auto Date_: expected_holidays)
        ASSERT_TRUE(hol.IsHoliday(Date_) || Date::IsWeekEnd(Date_));
}

TEST(ChinaCalendarTest, TestIBWorkingWeekEnds) {
    Vector_<Date_> expected_ib_working_ends{
            Date_(2014, 1, 26),
            Date_(2014, 2, 8),
            Date_(2014, 5, 4),
            Date_(2014, 9, 28),
            Date_(2014, 10, 11),
// China Inter Bank working weekend list in the year 2015
            Date_(2015, 1, 4),
            Date_(2015, 2, 15),
            Date_(2015, 2, 28),
            Date_(2015, 9, 6),
            Date_(2015, 10, 10),
// China Inter Bank working weekend list in the year 2016
            Date_(2016, 2, 6),
            Date_(2016, 2, 14),
            Date_(2016, 6, 12),
            Date_(2016, 9, 18),
            Date_(2016, 10, 8),
            Date_(2016, 10, 9),
// China Inter Bank working weekend list in the year 2017
            Date_(2017, 1, 22),
            Date_(2017, 2, 4),
            Date_(2017, 4, 1),
            Date_(2017, 5, 27),
            Date_(2017, 9, 30),
// China Inter Bank working weekend list in the year 2018
            Date_(2018, 2, 11),
            Date_(2018, 2, 24),
            Date_(2018, 4, 8),
            Date_(2018, 4, 28),
            Date_(2018, 9, 29),
            Date_(2018, 9, 30),
// China Inter Bank working weekend list in the year 2019
            Date_(2019, 2, 2),
            Date_(2019, 2, 3),
            Date_(2019, 4, 28),
            Date_(2019, 5, 5),
            Date_(2019, 9, 29),
            Date_(2019, 10, 12),
// China Inter Bank working weekend list in the year 2020
            Date_(2020, 1, 19),
            Date_(2020, 4, 26),
            Date_(2020, 5, 9),
            Date_(2020, 6, 28),
            Date_(2020, 9, 27),
            Date_(2020, 10, 10),
// China Inter Bank working weekend list in the year 2021
            Date_(2021, 2, 7),
            Date_(2021, 2, 20),
            Date_(2021, 4, 25),
            Date_(2021, 5, 8),
            Date_(2021, 9, 18),
            Date_(2021, 9, 26),
            Date_(2021, 10, 9),
// China Inter Bank working weekend list in the year 2022
            Date_(2022, 1, 29),
            Date_(2022, 1, 30),
            Date_(2022, 4, 2),
            Date_(2022, 4, 24),
            Date_(2022, 5, 7),
            Date_(2022, 10, 8),
            Date_(2022, 10, 9),
// China Inter Bank working weekend list in the year 2023
            Date_(2023, 1, 28),
            Date_(2023, 1, 29),
            Date_(2023, 4, 23),
            Date_(2023, 5, 6),
            Date_(2023, 6, 25),
            Date_(2023, 10, 7),
            Date_(2023, 10, 8)
    };

    Holidays_ hol("CN.IB");

    for (auto date: expected_ib_working_ends)
        ASSERT_TRUE(hol.IsWorkWeekends(date));
}
