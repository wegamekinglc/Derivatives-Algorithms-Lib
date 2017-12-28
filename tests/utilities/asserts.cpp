//
// Created by Cheng Li on 2017/12/28.
//

#include <dal/utilities/asserts.hpp>
#include <gtest/gtest.h>

TEST(AssertsTest, DAL_ASSERT_Test) { ASSERT_THROW(DAL_ASSERT(1 == 2, "Error"), dal::Error); }