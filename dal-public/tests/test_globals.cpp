//
// Created by dal-tester on 2026/8/15.
//

#include <gtest/gtest.h>

#include <dal-public/src/global.hpp>

using Dal::Date_;

TEST(GlobalsTest, TestEvaluationDateRoundTrip) {
    const Date_ previous = Dal::GetEvaluationDate();
    const Date_ target(2030, 1, 15);

    Dal::SetEvaluationDate(target);
    const Date_ observed = Dal::GetEvaluationDate();
    Dal::SetEvaluationDate(previous);

    ASSERT_EQ(Dal::Date::Year(observed), 2030);
    ASSERT_EQ(Dal::Date::Month(observed), 1);
    ASSERT_EQ(Dal::Date::Day(observed), 15);
    ASSERT_EQ(Dal::GetEvaluationDate(), previous);
}

TEST(GlobalsTest, TestEvaluationDateStableAcrossReads) {
    const Date_ previous = Dal::GetEvaluationDate();
    const Date_ target(2028, 6, 30);

    Dal::SetEvaluationDate(target);
    const Date_ first = Dal::GetEvaluationDate();
    const Date_ second = Dal::GetEvaluationDate();
    Dal::SetEvaluationDate(previous);

    ASSERT_EQ(first, target);
    ASSERT_EQ(second, target);
}
