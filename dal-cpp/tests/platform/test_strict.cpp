//
// Created by Cheng Li on 2017/12/20.
//

#include <gtest/gtest.h>
#include <dal/platform/strict.hpp>

TEST(StrictTest, TestAsInt) {
    auto float_v = 2.4;
    ASSERT_EQ(2, Dal::AsInt(float_v));
}
