//
// Created by Cheng Li on 2017/12/28.
//

#include <gtest/gtest.h>
#include <dal/utilities/asserts.hpp>


TEST(AssertsTest, DAL_ASSERT_Test) {
    ASSERT_THROW(DAL_ASSERT(1 == 2, "Error"), dal::Error);
}