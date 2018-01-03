//
// Created by Cheng Li on 2017/12/20.
//

#include <dal/platform/strict.hpp>
#include <gtest/gtest.h>

TEST(StrictTest, AsIntTest) {
    auto float_v = 2.4;
    ASSERT_EQ(2, dal::AsInt(float_v));
}
