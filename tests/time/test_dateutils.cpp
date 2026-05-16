//
// Created by Copilot on 2026/5/14.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/time/date.hpp>
#include <dal/time/dateutils.hpp>
#include <dal/utilities/exceptions.hpp>

using namespace Dal;

TEST(DateUtilsTest, TestIsDateStringRecognizesSupportedFormats) {
    ASSERT_TRUE(Date::IsDateString("2024-05-14"));
    ASSERT_TRUE(Date::IsDateString("5/14/2024"));
    ASSERT_TRUE(Date::IsDateString("5/14/24"));
    ASSERT_TRUE(Date::IsDateString("2024/05/14"));
    ASSERT_FALSE(Date::IsDateString("20240514"));
    ASSERT_FALSE(Date::IsDateString("2024-05-14 12:00:00"));
}

TEST(DateUtilsTest, TestFromStringParsesIsoAndUsFormats) {
    ASSERT_EQ(Date::FromString("2024-05-14"), Date_(2024, 5, 14));
    ASSERT_EQ(Date::FromString("5/14/2024"), Date_(2024, 5, 14));
    ASSERT_EQ(Date::FromString("5/14/24"), Date_(2024, 5, 14));
}

TEST(DateUtilsTest, TestFromStringThrowsOnUnsupportedFormat) {
    ASSERT_THROW(Date::FromString("abc"), Dal::Exception_);
    ASSERT_THROW(Date::FromString("2024-05"), Dal::Exception_);
}

TEST(DateUtilsTest, TestMonthFromFutureCode) {
    ASSERT_EQ(Date::MonthFromFutureCode('F'), 1);
    ASSERT_EQ(Date::MonthFromFutureCode('M'), 6);
    ASSERT_EQ(Date::MonthFromFutureCode('Z'), 12);
}

TEST(DateUtilsTest, TestMonthFromFutureCodeRejectsInvalidCodes) {
    ASSERT_THROW(Date::MonthFromFutureCode('a'), Dal::Exception_);
    ASSERT_THROW(Date::MonthFromFutureCode('B'), Dal::Exception_);
}
