//
// Created by Cheng Li on 2018/2/4.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal/platform/platform.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/exceptions.hpp>

using Dal::DateTime_;
using namespace Dal;
using namespace Dal::DateTime;

TEST(DateTimeTest, TestRejectsInvalidFractionsAndFields) {
    const Date_ date(2017, 1, 1);
    for (double fraction :
         {-0.5, 1.0, std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        ASSERT_THROW(DateTime_(date, fraction), Exception_);
    ASSERT_THROW(DateTime_(date, -1, 60, 0), Exception_);
    ASSERT_THROW(DateTime_(date, 0, 60, 0), Exception_);
    ASSERT_THROW(DateTime_(date, 0, 0, 60), Exception_);
    ASSERT_THROW(DateTime_(date, std::numeric_limits<int>::max(), 0, 0), Exception_);
    ASSERT_THROW(DateTime_(-1LL), Exception_);
    ASSERT_THROW((DateTime_(std::numeric_limits<long long>::max())), Exception_);
}

TEST(DateTimeTest, TestAdditionRejectsNonfiniteAndOutOfRangeWithoutMutation) {
    DateTime_ date(Date_(2017, 1, 1), 0.5);
    const auto original = date;
    for (double fraction : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), 1e20}) {
        ASSERT_THROW(date += fraction, Exception_);
        ASSERT_EQ(date, original);
    }
}

TEST(DateTimeTest, TestNullDateTime) {
    DateTime_ dt;
    ASSERT_FALSE(dt.IsValid());
}

TEST(DateTimeTest, TestDateTimeWithDateAndFrac) {
    DateTime_ src(Date_(2017, 1, 1), 0.5);
    DateTime_ exp(Date_(2017, 1, 1), 12, 0, 0);
    ASSERT_EQ(src, exp);
}

TEST(DateTimeTest, TestDateTimeWithMaximumFrac) {
    DateTime_ src(Date_(2017, 1, 1), 23, 59, 59);
    DateTime_ exp(Date_(2017, 1, 1), 0.999988425925926);
    ASSERT_EQ(src, exp);
}

TEST(DateTimeTest, TestDateTimeWithUnityFrac) {
    ASSERT_THROW(DateTime_(Date_(2017, 1, 1), 24, 0, 0), Dal::Exception_);
}

TEST(DateTimeTest, TestDateTimeWithHMS) {
    int yyyy = 2017;
    int mm = 1;
    int dd = 1;
    int h = 12;
    int m = 20;
    int s = 25;

    DateTime_ src(Date_(yyyy, mm, dd), h, m, s);
    DateTime_ exp(Date_(yyyy, mm, dd), ((h * 60 + m) * 60 + s) / 86400.);
    ASSERT_EQ(src, exp);
}

TEST(DateTimeTest, TestDateTestWithMSEC) {
    long long msec = 1514808915000;
    DateTime_ src(msec);
    DateTime_ exp(Date_(2018, 1, 1), 12, 15, 15);
    ASSERT_EQ(MSec(src), MSec(exp));
}

TEST(DateTimeTest, TestDateTimeSub) {
    DateTime_ lhs(Date_(2018, 1, 1), 12, 15, 15);
    DateTime_ rhs(Date_(2010, 1, 13), 15, 0, 0);

    double interval = lhs - rhs;
    ASSERT_NEAR(interval, 2909.88559, 1e-4);
}

TEST(DateTimeTest, TestDateTimeLess) {
    DateTime_ lhs(Date_(2010, 1, 13), 15, 0, 0);
    DateTime_ rhs(Date_(2018, 1, 1), 12, 15, 15);
    ASSERT_TRUE(lhs < rhs);
}

TEST(DateTimeTest, TestDateTimeGreater) {
    DateTime_ lhs(Date_(2010, 1, 13), 15, 0, 0);
    DateTime_ rhs(Date_(2018, 1, 1), 12, 15, 15);
    ASSERT_TRUE(rhs > lhs);
}

TEST(DateTimeTest, TestDateTimeLessEqual) {
    DateTime_ lhs(Date_(2010, 1, 13), 15, 0, 0);
    DateTime_ rhs(Date_(2018, 1, 1), 12, 15, 15);
    ASSERT_TRUE(lhs <= rhs);
    ASSERT_TRUE(rhs <= rhs);
}

TEST(DateTimeTest, TestDateTimeGreaterEqual) {
    DateTime_ lhs(Date_(2010, 1, 13), 15, 0, 0);
    DateTime_ rhs(Date_(2018, 1, 1), 12, 15, 15);
    ASSERT_TRUE(rhs >= lhs);
    ASSERT_TRUE(rhs >= rhs);
}

TEST(DateTimeTest, TestDateTimeNotEqual) {
    DateTime_ lhs(Date_(2010, 1, 13), 15, 0, 0);
    DateTime_ rhs(Date_(2018, 1, 1), 12, 15, 15);
    ASSERT_TRUE(lhs != rhs);
}

TEST(DateTimeTest, TestDateTimeHour) {
    DateTime_ src(Date_(2017, 1, 1), 0.5);
    ASSERT_EQ(Hour(src), 12);
}

TEST(DateTimeTest, TestDateTimeMinute) {
    DateTime_ src(Date_(2017, 1, 1), 0.5);
    ASSERT_EQ(Minute(src), 0);
}

TEST(DateTimeTest, TestDateTimeToString) {
    DateTime_ src(Date_(2017, 1, 1), 0.5);
    ASSERT_EQ(String_("2017-01-01 12:00:00"), ToString(src));
}

TEST(DateTimeTest, TestDateTimeMinimum) {
    DateTime_ src = Minimum();
    DateTime_ exp(Date::Minimum());
    ASSERT_EQ(src, exp);
}

TEST(DateTimeTest, TestDateTimeNumericValueOf) {
    DateTime_ src(Date_(2018, 1, 1), 12, 15, 15);
    ASSERT_NEAR(NumericValueOf(src), 43101.51059, 1e-4);
}

TEST(DateTimeTest, TestDateTimeOperatorPlus) {
    DateTime_ src(Date_(2018, 1, 1), 12, 15, 15);
    auto new_dt = src + 1.0;
    ASSERT_NEAR(NumericValueOf(new_dt), 43102.51059, 1e-4);
    ASSERT_EQ(new_dt.Date(), Date_(2018, 1, 2));
}

TEST(DateTimeTest, TestDateTimeInplaceOperatorPlus) {
    DateTime_ src(Date_(2018, 1, 1), 12, 15, 15);
    src += 1.0;
    ASSERT_NEAR(NumericValueOf(src), 43102.51059, 1e-4);
    ASSERT_EQ(src.Date(), Date_(2018, 1, 2));
}
