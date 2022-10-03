//
// Created by wegam on 2022/10/2.
//

#include <gtest/gtest.h>
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

