//
// Created by wegam on 2021/1/16.
//

#define AADET_ENABLED 1

#include <dal/math/aad/aad.hpp>
#include <gtest/gtest.h>

using namespace Dal;

TEST(AADExprTest, Test) {
    Number_ s1(1.0);
    Number_ s2(2.0);

    Number_ value = Number_(s1 + s2);
    ASSERT_NEAR(value.Value(), 3.0, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    ASSERT_NEAR(s2.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Rewind();
}
