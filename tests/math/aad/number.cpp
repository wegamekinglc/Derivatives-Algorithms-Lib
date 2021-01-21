//
// Created by wegam on 2021/1/18.
//

#include <dal/math/aad/aad.hpp>
#include <gtest/gtest.h>

using namespace Dal;

TEST(AADNumberTest, TestNumberAdd) {

    Number_::tape_->Clear();

    Number_ s1(1.0);
    Number_ s2(2.0);

    auto value = s1 + s2;
    ASSERT_NEAR(value.Value(), 3.0, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    ASSERT_NEAR(s2.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Rewind();

    s1 = 1.0;
    value = s1 + 2.0;
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Rewind();

    s2 = 2.0;
    value = 1.0 + s2;
    value.PropagateToStart();
    ASSERT_NEAR(s2.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Rewind();
}

