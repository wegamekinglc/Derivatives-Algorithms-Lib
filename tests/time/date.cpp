//
// Created by Cheng Li on 2018/2/2.
//

#include <gtest/gtest.h>
#include <dal/platform/strict.hpp>
#include <dal/time/date.hpp>

using dal::Date_;
using namespace dal;
using namespace dal::date;


TEST(DateTest, TestNullDate) {
    Date_ dt;
    ASSERT_FALSE(dt.IsValid());
}

TEST(DateTest, TestWithYMDDate) {
    short yyyy = 2017;
    short mm = 12;
    short dd = 31;

    Date_ dt(yyyy, mm, dd);
    ASSERT_EQ(Year(dt), yyyy);
    ASSERT_EQ(Month(dt), mm);
    ASSERT_EQ(Day(dt), dd);
}

TEST(DateTest, TestDateCopy) {
    Date_ src(2017, 1, 1);
    Date_ dst(src);

    ASSERT_EQ(src, dst);
}

TEST(DateTest, TestDateCopyOperator) {
    Date_ src(2017, 1, 1);
    Date_ dst;
    dst = src;

    ASSERT_EQ(src, dst);
}

TEST(DateTest, TestDateAddDays) {
    Date_ src(2017, 1, 1);
    Date_ dst1 = src.AddDays(1);
    ASSERT_EQ(Date_(2017, 1, 2), dst1);

    Date_ dst2 = src.AddDays(-1);
    ASSERT_EQ(Date_(2016, 12, 31), dst2);
}

TEST(DateTest, TestDateSelfAdd) {
    Date_ src(2017, 1, 1);
    ++src;
    ASSERT_EQ(Date_(2017, 1, 2), src);
}

TEST(DateTest, TestDateSelfSub) {
    Date_ src(2017, 1, 1);
    --src;
    ASSERT_EQ(Date_(2016, 12, 31), src);
}

TEST(DateTest, TestDateUnEqual) {
    Date_ lhs(2018, 1, 1);
    Date_ rhs(2017, 1, 1);
    ASSERT_TRUE(lhs != rhs);
}

TEST(DateTest, TestDateLess) {
    Date_ lhs(2017, 12, 31);
    Date_ rhs(2018, 1, 1);
    ASSERT_TRUE(lhs < rhs);
}

TEST(DateTest, TestDateGreater) {
    Date_ lhs(2018, 1, 1);
    Date_ rhs(2017, 12, 31);
    ASSERT_TRUE(lhs > rhs);
}

TEST(DateTest, TestDateLessEqual) {
    Date_ lhs(2017, 12, 31);
    Date_ rhs(2018, 1, 1);
    ASSERT_TRUE(lhs <= rhs);
    ASSERT_TRUE(rhs <= rhs);
}

TEST(DateTest, TestDateGreaterEqual) {
    Date_ lhs(2018, 1, 1);
    Date_ rhs(2017, 12, 31);
    ASSERT_TRUE(lhs >= rhs);
    ASSERT_TRUE(rhs >= rhs);
}

TEST(DateTest, TestDateSub) {
    Date_ lhs(2016, 2, 28);
    Date_ rhs(2016, 3, 1);
    ASSERT_EQ(rhs - lhs, 2);

    lhs = Date_(2018, 2, 28);
    rhs = Date_(2018, 3, 1);
    ASSERT_EQ(rhs - lhs, 1);
}

TEST(DateTest, TestDateNumericValue) {
    Date_  src(2017, 1, 1);
    ASSERT_DOUBLE_EQ(NumericValueOf(src), 42736.);
}




