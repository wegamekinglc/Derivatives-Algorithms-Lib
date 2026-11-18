//
// Created by wegam on 2020/12/13.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/time/dateutils.hpp>
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

TEST(DateIncrementTest, TestStrictQuarterlyRollsAndMonthlyImm) {
    struct Case_ {
        const char* rule_;
        Date_ from_, next_, previous_;
    };
    const Vector_<Case_> cases{
        {"IMM", {2026, 2, 1}, {2026, 3, 18}, {2025, 12, 17}},
        {"IMM", {2026, 4, 25}, {2026, 6, 17}, {2026, 3, 18}},
        {"IMM", {2026, 3, 18}, {2026, 6, 17}, {2025, 12, 17}},
        {"IMM", {2026, 3, 17}, {2026, 3, 18}, {2025, 12, 17}},
        {"IMM", {2026, 3, 19}, {2026, 6, 17}, {2026, 3, 18}},
        {"IMM", {2026, 12, 31}, {2027, 3, 17}, {2026, 12, 16}},
        {"CDS", {2026, 2, 1}, {2026, 3, 20}, {2025, 12, 20}},
        {"CDS", {2026, 4, 25}, {2026, 6, 20}, {2026, 3, 20}},
        {"CDS", {2026, 3, 20}, {2026, 6, 20}, {2025, 12, 20}},
        {"CDS", {2026, 3, 19}, {2026, 3, 20}, {2025, 12, 20}},
        {"CDS", {2026, 3, 21}, {2026, 6, 20}, {2026, 3, 20}},
        {"CDS", {2026, 12, 31}, {2027, 3, 20}, {2026, 12, 20}},
        {"IMM1", {2026, 2, 1}, {2026, 2, 18}, {2026, 1, 21}},
        {"IMM1", {2026, 2, 18}, {2026, 3, 18}, {2026, 1, 21}},
        {"IMM1", {2026, 4, 25}, {2026, 5, 20}, {2026, 4, 15}},
        {"IMM1", {2026, 12, 16}, {2027, 1, 20}, {2026, 11, 18}},
    };
    for (const auto& row : cases) {
        SCOPED_TRACE(row.rule_);
        SCOPED_TRACE(Date::ToString(row.from_));
        const auto increment = Date::ParseIncrement(row.rule_);
        ASSERT_EQ(row.from_ + *increment, row.next_);
        ASSERT_EQ(row.from_ - *increment, row.previous_);
        ASSERT_LT(row.from_, row.next_);
        ASSERT_LT(row.previous_, row.from_);
        if (String_(row.rule_) != "CDS") {
            const auto direct = Date::ToIMM(String_(row.rule_) == "IMM1");
            ASSERT_EQ(row.from_ + *direct, row.next_);
            ASSERT_EQ(row.from_ - *direct, row.previous_);
        }
    }
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
