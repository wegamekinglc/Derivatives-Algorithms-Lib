//
// Created by wegamekinglc on 2020/11/23.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/time/datetimeutils.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/datetime.hpp>

using namespace Dal;

TEST(DateTimeUtilsTest, TestIsDateTimeString) {
    // standard datetime string
    String_ s("2020-11-12 12:30:24");
    ASSERT_TRUE(DateTime::IsDateTimeString(s));

    // standard date string is not a datetime string
    s = "2020-11-12";
    ASSERT_FALSE(DateTime::IsDateTimeString(s));

    // other good format
    s = "2020-11-13 12:30";
    ASSERT_TRUE(DateTime::IsDateTimeString(s));
}

TEST(DateTimeUtilsTest, TestDateTimeFromString) {
    String_ s("2020-11-12 12:30:24");
    auto val = DateTime::FromString(s);
    ASSERT_EQ(DateTime_(Date_(2020, 11, 12), 12, 30, 24), val);

    // only date string
    s = "2020-11-12";
    val = DateTime::FromString(s);
    ASSERT_EQ(DateTime_(Date_(2020, 11, 12), 0, 0, 0), val);
}

