//
// Created by wegam on 2026/5/30.
//

#include <gtest/gtest.h>

#include <public/src/global.hpp>

using Dal::Date_;

TEST(PublicApiTest, TestEvaluationDateRoundTrip) {
    // Set a known date and read it back
    Date_ d(2026, 5, 30);
    Dal::SetEvaluationDate(d);
    Date_ result = Dal::GetEvaluationDate();

    // Use free functions Year(), Month(), Day() for Date_ access
    ASSERT_EQ(Dal::Date::Year(d), Dal::Date::Year(result));
    ASSERT_EQ(Dal::Date::Month(d), Dal::Date::Month(result));
    ASSERT_EQ(Dal::Date::Day(d), Dal::Date::Day(result));
}

TEST(PublicApiTest, TestPublicHeaderIncludeLinks) {
    // This test simply verifies the public types compile and link.
    // If we get here, the test binary linked against dal_public successfully.
    ASSERT_TRUE(true);
}
