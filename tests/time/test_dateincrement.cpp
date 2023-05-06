//
// Created by wegam on 2020/12/13.
//

#include <gtest/gtest.h>
#include <dal/time/dateincrement.hpp>
#include <dal/time/holidays.hpp>

using namespace Dal;

TEST(DateIncrementTest, TestIsLiborTenor) {
    String_ tenor("3m");
    ASSERT_TRUE(IsLiborTenor(tenor));

    tenor = String_("3Y");
    ASSERT_FALSE(IsLiborTenor(tenor));
}

TEST(DateIncrementTest, TestIsSwapTenor) {
    String_ tenor("3m");
    ASSERT_FALSE(IsSwapTenor(tenor));

    tenor = String_("3Y");
    ASSERT_TRUE(IsSwapTenor(tenor));
}

TEST(DateIncrementTest, TestIncrementNextSpecial) {
    Date_ ref_date(2019, 12, 13);

    // IMM
    auto s1 = Date::ParseIncrement("IMM");
    auto s2 = ref_date + *s1;
    ASSERT_EQ(Date_(2019, 12, 18), s2);
    s2 = ref_date - *s1;
    ASSERT_EQ(Date_(2019, 9, 18), s2);

    // IMM1
    s1 = Date::ParseIncrement("IMM1");
    s2 = ref_date + *s1;
    ASSERT_EQ(Date_(2019, 12, 18), s2);
    s2 = ref_date - *s1;
    ASSERT_EQ(Date_(2019, 11, 20), s2);

    // CDS
    s1 = Date::ParseIncrement("CDS");
    s2 = ref_date + *s1;
    ASSERT_EQ(Date_(2019, 12, 20), s2);
    s2 = ref_date - *s1;
    ASSERT_EQ(Date_(2019, 9, 20), s2);

    // EOM
    s1 = Date::ParseIncrement("EOM");
    s2 = ref_date + *s1;
    ASSERT_EQ(Date_(2019, 12, 31), s2);
    s2 = ref_date - *s1;
    ASSERT_EQ(Date_(2019, 11, 30), s2);
}

TEST(DateIncrementTest, TestIncrementMultistep) {
    Date_ ref_date(2020, 10, 9);

    // Year
    auto s1 = Date::ParseIncrement("1Y");
    auto s2 = ref_date + *s1;
    ASSERT_EQ(Date_(2021, 10, 9), s2);
    s2 = ref_date - *s1;
    ASSERT_EQ(Date_(2019, 10, 9), s2);

    // Month
    s1 = Date::ParseIncrement("1M");
    s2 = ref_date + *s1;
    ASSERT_EQ(Date_(2020, 11, 9), s2);
    s2 = ref_date - *s1;
    ASSERT_EQ(Date_(2020, 9, 9), s2);

    // CD
    s1 = Date::ParseIncrement("1CD");
    s2 = ref_date + *s1;
    ASSERT_EQ(Date_(2020, 10, 10), s2);
    s2 = ref_date - *s1;
    ASSERT_EQ(Date_(2020, 10, 8), s2);

    // BD
    s1 = Date::ParseIncrement("1BD");
    s2 = ref_date + *s1;
    ASSERT_EQ(Date_(2020, 10, 12), s2);
    s2 = ref_date - *s1;
    ASSERT_EQ(Date_(2020, 10, 8), s2);

    // BD with holiday center
    s1 = Date::ParseIncrement("1BD;CN.SSE");
    s2 = ref_date + *s1;
    ASSERT_EQ(Date_(2020, 10, 12), s2);
    s2 = ref_date - *s1;
    ASSERT_EQ(Date_(2020, 9, 30), s2);
}

TEST(DateIncrementTest, TestNBusDays) {
    Date_ ref_date(2020, 10, 9);
    auto s1 = Date::NBusDays(1, Holidays_(String_("CN.SSE")));

    auto s2 = ref_date + *s1;
    ASSERT_EQ(Date_(2020, 10, 12), s2);
    s2 = ref_date - *s1;
    ASSERT_EQ(Date_(2020, 9, 30), s2);
}