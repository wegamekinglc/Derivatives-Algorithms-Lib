//
// Created by wegam on 2021/1/18.
//

#ifndef AADET_ENABLED

#include <dal/math/aad/aad.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace Dal::AAD;

TEST(AADNumberTest, TestNumberAdd) {

    Number_::tape_->Clear();

    Number_ s1(1.0); 
    Number_ s2(2.0); 
    s1.PutOnTape();
    s2.PutOnTape();
    Number_ value = s1 + s2;
    ASSERT_NEAR(value.Value(), 3.0, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    ASSERT_NEAR(s2.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Rewind();

    s1 = 1.0;
    s1.PutOnTape();
    value = s1 + 2.0;
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Rewind();

    s2 = 2.0;
    s2.PutOnTape();
    value = 1.0 + s2;
    value.PropagateToStart();
    ASSERT_NEAR(s2.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Clear();
}

TEST(AADNumberTest, TestNumberSub) {

    Number_::tape_->Clear();

    Number_ s1(1.0);
    Number_ s2(2.0);
    s1.PutOnTape();
    s2.PutOnTape();
    Number_ value = s1 - s2;
    ASSERT_NEAR(value.Value(), -1.0, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    ASSERT_NEAR(s2.Adjoint(), -1.0, 1e-10);
    Number_::tape_->Rewind();

    s1 = 1.0;
    s1.PutOnTape();
    value = s1 - 2.0;
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Rewind();

    s2 = 2.0;
    s2.PutOnTape();
    value = 1.0 - s2;
    value.PropagateToStart();
    ASSERT_NEAR(s2.Adjoint(), -1.0, 1e-10);
    Number_::tape_->Clear();
}

TEST(AADNumberTest, TestNumberMultiply) {

    Number_::tape_->Clear();

    Number_ s1(3.0);
    Number_ s2(2.0);
    s1.PutOnTape();
    s2.PutOnTape();
    Number_ value = s1 * s2;
    ASSERT_NEAR(value.Value(), 6.0, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 2.0, 1e-10);
    ASSERT_NEAR(s2.Adjoint(), 3.0, 1e-10);
    Number_::tape_->Rewind();

    s1 = 3.0;
    s1.PutOnTape();
    value = s1 * 2.0;
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 2.0, 1e-10);
    Number_::tape_->Rewind();

    s2 = 2.0;
    s2.PutOnTape();
    value = 3.0 * s2;
    value.PropagateToStart();
    ASSERT_NEAR(s2.Adjoint(), 3.0, 1e-10);
    Number_::tape_->Clear();
}

TEST(AADNumberTest, TestNumberDivide) {

    Number_::tape_->Clear();

    Number_ s1(3.0);
    Number_ s2(2.0);
    s1.PutOnTape();
    s2.PutOnTape();
    Number_ value = s1 / s2;
    ASSERT_NEAR(value.Value(), 1.5, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1./ s2.Value(), 1e-10);
    ASSERT_NEAR(s2.Adjoint(), -s1.Value() / s2.Value() / s2.Value(), 1e-10);
    Number_::tape_->Rewind();

    s1 = 3.0;
    s1.PutOnTape();
    value = s1 / 2.0;
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1. / 2.0, 1e-10);
    Number_::tape_->Rewind();

    s2 = 2.0;
    s2.PutOnTape();
    value = 3.0 / s2;
    value.PropagateToStart();
    ASSERT_NEAR(s2.Adjoint(), -3.0 / s2.Value() / s2.Value(), 1e-10);
    Number_::tape_->Clear();
}

TEST(AADNumberTest, TestNumberPow) {
    Number_::tape_->Clear();

    Number_ s1(3.0);
    Number_ s2(2.0);
    s1.PutOnTape();
    s2.PutOnTape();
    Number_ value = Pow(s1, s2);
    ASSERT_NEAR(value.Value(), std::pow(s1.Value(), s2.Value()), 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), s2.Value() * std::pow(s1.Value(), s2.Value() - 1), 1e-10);
    ASSERT_NEAR(s2.Adjoint(), value.Value() * std::log(s1.Value()), 1e-10);
    Number_::tape_->Rewind();

    s1 = 3.0;
    s1.PutOnTape();
    value = Pow(s1, 2.0);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), s2.Value() * std::pow(s1.Value(), 1.0), 1e-10);
    Number_::tape_->Rewind();

    s2 = 2.0;
    s2.PutOnTape();
    value = Pow(3.0, s2);
    value.PropagateToStart();
    ASSERT_NEAR(s2.Adjoint(), value.Value() * std::log(3.0), 1e-10);
    Number_::tape_->Clear();
}

TEST(AADNumberTest, TestNumberMax) {
    Number_::tape_->Clear();

    Number_ s1(3.0);
    Number_ s2(2.0);
    s1.PutOnTape();
    s2.PutOnTape();
    Number_ value = Max(s1, s2);
    ASSERT_NEAR(value.Value(), 3.0, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    ASSERT_NEAR(s2.Adjoint(), 0.0, 1e-10);
    Number_::tape_->Rewind();

    s1 = 3.0;
    s1.PutOnTape();
    value = Max(s1, 2.0);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Rewind();

    s2 = 2.0;
    s2.PutOnTape();
    value = Max(3.0, s2);
    value.PropagateToStart();
    ASSERT_NEAR(s2.Adjoint(), 0.0, 1e-10);
    Number_::tape_->Rewind();

    s1 = 2.0;
    s2 = 3.0;
    s1.PutOnTape();
    s2.PutOnTape();
    value = Max(s1, s2);
    ASSERT_NEAR(value.Value(), 3.0, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 0.0, 1e-10);
    ASSERT_NEAR(s2.Adjoint(), 1.0, 1e-10);

    s1 = 2.0;
    s1.PutOnTape();
    value = Max(s1, 3.0);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 0.0, 1e-10);
    Number_::tape_->Rewind();

    s2 = 3.0;
    s2.PutOnTape();
    value = Max(2.0, s2);
    value.PropagateToStart();
    ASSERT_NEAR(s2.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Clear();
}

TEST(AADNumberTest, TestNumberMin) {
    Number_::tape_->Clear();

    Number_ s1(3.0);
    Number_ s2(2.0);
    s1.PutOnTape();
    s2.PutOnTape();
    Number_ value = Min(s1, s2);
    ASSERT_NEAR(value.Value(), 2.0, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 0.0, 1e-10);
    ASSERT_NEAR(s2.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Rewind();

    s1 = 3.0;
    s1.PutOnTape();
    value = Min(s1, 2.0);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 0.0, 1e-10);
    Number_::tape_->Rewind();

    s2 = 2.0;
    s2.PutOnTape();
    value = Min(3.0, s2);
    value.PropagateToStart();
    ASSERT_NEAR(s2.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Rewind();

    s1 = 2.0;
    s2 = 3.0;
    s1.PutOnTape();
    s2.PutOnTape();
    value = Min(s1, s2);
    ASSERT_NEAR(value.Value(), 2.0, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    ASSERT_NEAR(s2.Adjoint(), 0.0, 1e-10);
    Number_::tape_->Rewind();

    s1 = 2.0;
    s1.PutOnTape();
    value = Min(s1, 3.0);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Rewind();

    s2 = 3.0;
    s2.PutOnTape();
    value = Min(2.0, s2);
    value.PropagateToStart();
    ASSERT_NEAR(s2.Adjoint(), 0.0, 1e-10);
    Number_::tape_->Clear();
}


TEST(AADNumberTest, TestNumberEqualAdd) {

    Number_::tape_->Clear();

    Number_ s1(1.0);
    Number_ s2(2.0);
    s1.PutOnTape();
    s2.PutOnTape();
    s1 += s2;
    ASSERT_NEAR(s1.Value(), 3.0, 1e-10);
    s1.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    ASSERT_NEAR(s2.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Rewind();

    s1 = 1.0;
    s1.PutOnTape();
    s1 += 2.0;
    ASSERT_NEAR(s1.Value(), 3.0, 1e-10);
    s1.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Clear();
}


TEST(AADNumberTest, TestNumberEqualSub) {

    Number_::tape_->Clear();

    Number_ s1(1.0);
    Number_ s2(2.0);
    s1.PutOnTape();
    s2.PutOnTape();
    s1 -= s2;
    ASSERT_NEAR(s1.Value(), -1.0, 1e-10);
    s1.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    ASSERT_NEAR(s2.Adjoint(), -1.0, 1e-10);
    Number_::tape_->Rewind();

    s1 = 1.0;
    s1.PutOnTape();
    s1 -= 2.0;
    ASSERT_NEAR(s1.Value(), -1.0, 1e-10);
    s1.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Clear();
}


TEST(AADNumberTest, TestNumberEqualMultiply) {

    Number_::tape_->Clear();

    Number_ s1(2.0);
    Number_ s2(3.0);
    s1.PutOnTape();
    s2.PutOnTape();
    s1 *= s2;
    ASSERT_NEAR(s1.Value(), 6.0, 1e-10);
    s1.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    ASSERT_NEAR(s2.Adjoint(), 2.0, 1e-10);
    Number_::tape_->Rewind();

    s1 = 2.0;
    s1.PutOnTape();
    s1 *= 3.0;
    ASSERT_NEAR(s1.Value(), 6.0, 1e-10);
    s1.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Clear();
}


TEST(AADNumberTest, TestNumberEqualDivide) {

    Number_::tape_->Clear();

    Number_ s1(2.0);
    Number_ s2(3.0);
    s1.PutOnTape();
    s2.PutOnTape();
    s1 /= s2;
    ASSERT_NEAR(s1.Value(), 0.66666666666666, 1e-10);
    s1.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    ASSERT_NEAR(s2.Adjoint(), -2.0 / 9.0, 1e-10);
    Number_::tape_->Rewind();

    s1 = 2.0;
    s1.PutOnTape();
    s1 /= 3.0;
    ASSERT_NEAR(s1.Value(), 0.66666666666666, 1e-10);
    s1.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Rewind();
}


TEST(AADNumberTest, TestNumberNegative) {
    Number_::tape_->Clear();
    Number_ s1(2.0);
    s1.PutOnTape();
    Number_ value = -s1;
    ASSERT_NEAR(value.Value(), -2.0, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), -1.0, 1e-10);
    Number_::tape_->Clear();
}


TEST(AADNumberTest, TestNumberPositive) {
    Number_::tape_->Clear();
    Number_ s1(2.0);
    s1.PutOnTape();
    Number_ value = +s1;
    ASSERT_NEAR(value.Value(), 2.0, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 1.0, 1e-10);
    Number_::tape_->Clear();
}


TEST(AADNumberTest, TestNumberExp) {
    Number_::tape_->Clear();
    Number_ s1(2.0);
    s1.PutOnTape();
    Number_ value = Exp(s1);
    ASSERT_NEAR(value.Value(), std::exp(2.0), 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), std::exp(2.0), 1e-10);
    Number_::tape_->Clear();
}


TEST(AADNumberTest, TestNumberLog) {
    Number_::tape_->Clear();
    Number_ s1(2.0);
    s1.PutOnTape();
    Number_ value = Log(s1);
    ASSERT_NEAR(value.Value(), std::log(2.0), 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 0.5, 1e-10);
    Number_::tape_->Clear();
}


TEST(AADNumberTest, TestNumberSqrt) {
    Number_::tape_->Clear();
    Number_ s1(2.0);
    s1.PutOnTape();
    Number_ value = Sqrt(s1);
    ASSERT_NEAR(value.Value(), std::sqrt(2.0), 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), 0.5 / std::sqrt(2.0), 1e-10);
    Number_::tape_->Clear();
}


TEST(AADNumberTest, TestNumberFabs) {
    Number_::tape_->Clear();
    Number_ s1(-2.0);
    s1.PutOnTape();
    Number_ value = Fabs(s1);
    ASSERT_NEAR(value.Value(), 2.0, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(s1.Adjoint(), -1.0, 1e-10);
    Number_::tape_->Clear();
}

#endif