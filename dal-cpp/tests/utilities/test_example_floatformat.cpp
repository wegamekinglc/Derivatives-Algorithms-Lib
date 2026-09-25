//
// Created by Codex on 2026/9/25.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal/platform/platform.hpp>

#include "../../examples/floatformat.hpp"

TEST(ExampleFloatFormatTest, TestFiniteValues) {
    ASSERT_EQ(ExampleFloat(0.0), "0.00");
    ASSERT_EQ(ExampleFloat(12.0), "12.00");
    ASSERT_EQ(ExampleFloat(1.2345), "1.234");
    ASSERT_EQ(ExampleFloat(0.00001234), "0.00001234");
    ASSERT_EQ(ExampleFloat(-0.0000000001234), "-0.0000000001234");
    ASSERT_EQ(ExampleFloat(0.01, 6), "0.010000");
}

TEST(ExampleFloatFormatTest, TestNonFiniteValues) {
    ASSERT_EQ(ExampleFloat(std::numeric_limits<double>::infinity()), "inf");
    ASSERT_EQ(ExampleFloat(-std::numeric_limits<double>::infinity()), "-inf");
    ASSERT_EQ(ExampleFloat(std::numeric_limits<double>::quiet_NaN()), "nan");
}
