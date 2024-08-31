//
// Created by wegam on 2021/1/18.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/aad/aad.hpp>

using namespace Dal::AAD;

TEST(AADTest, TestNumberAdd) {

    {
        Number_::Tape()->Clear();
        Number_ s1(1.0);
        Number_ s2(2.0);
        Number_::Tape()->registerInput(s1);
        Number_::Tape()->registerInput(s2);

        Number_ value = s1 + s2;
        value.PropagateToStart();

        ASSERT_NEAR(value.value(), 3.0, 1e-10);
        ASSERT_NEAR(GetGradient(s1), 1.0, 1e-10);
        ASSERT_NEAR(GetGradient(s2), 1.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s1 = 1.0;
        Number_::Tape()->registerInput(s1);

        Number_ value = s1 + 2.0;
        value.PropagateToStart();

        ASSERT_NEAR(value.value(), 3.0, 1e-10);
        ASSERT_NEAR(GetGradient(s1), 1.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s2 = 2.0;
        Number_::Tape()->registerInput(s2);

        Number_ value = 1.0 + s2;
        value.PropagateToStart();

        ASSERT_NEAR(value.value(), 3.0, 1e-10);
        ASSERT_NEAR(GetGradient(s2), 1.0, 1e-10);
    }
}

TEST(AADTest, TestNumberSub) {

    {
        Number_::Tape()->Clear();
        Number_ s1(1.0);
        Number_ s2(2.0);
        Number_::Tape()->registerInput(s1);
        Number_::Tape()->registerInput(s2);

        Number_ value = s1 - s2;
        value.PropagateToStart();

        ASSERT_NEAR(value.value(), -1.0, 1e-10);
        ASSERT_NEAR(GetGradient(s1), 1.0, 1e-10);
        ASSERT_NEAR(GetGradient(s2), -1.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s1(1.0);
        Number_::Tape()->registerInput(s1);

        Number_ value = s1 - 2.0;
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 1.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s2(2.0);
        Number_::Tape()->registerInput(s2);
        
        Number_ value = 1.0 - s2;
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s2), -1.0, 1e-10);
    }
}

TEST(AADTest, TestNumberMultiply) {

    {
        Number_::Tape()->Clear();
        Number_ s1(3.0);
        Number_ s2(2.0);
        Number_::Tape()->registerInput(s1);
        Number_::Tape()->registerInput(s2);

        Number_ value = s1 * s2;
        value.PropagateToStart();
        ASSERT_NEAR(value.value(), 6.0, 1e-10);
        ASSERT_NEAR(GetGradient(s1), 2.0, 1e-10);
        ASSERT_NEAR(GetGradient(s2), 3.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s1 = 3.0;
        Number_::Tape()->registerInput(s1);

        Number_ value = s1 * 2.0;
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 2.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s2 = 2.0;
        Number_::Tape()->registerInput(s2);

        Number_ value = 3.0 * s2;
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s2), 3.0, 1e-10);
    }
}

TEST(AADTest, TestNumberDivide) {
    {
        Number_::Tape()->Clear();
        Number_ s1(3.0);
        Number_ s2(2.0);
        Number_::Tape()->registerInput(s1);
        Number_::Tape()->registerInput(s2);

        Number_ value = s1 / s2;
        ASSERT_NEAR(value.value(), 1.5, 1e-10);
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 1. / s2.value(), 1e-10);
        ASSERT_NEAR(GetGradient(s2), -s1.value() / s2.value() / s2.value(), 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s1 = 3.0;
        Number_::Tape()->registerInput(s1);

        Number_ value = s1 / 2.0;
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 1. / 2.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s2 = 2.0;
        Number_::Tape()->registerInput(s2);

        Number_ value = 3.0 / s2;
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s2), -3.0 / s2.value() / s2.value(), 1e-10);
    }
}

TEST(AADTest, TestNumberPow) {

    {
        Number_::Tape()->Clear();
        Number_ s1(3.0);
        Number_ s2(2.0);
        Number_::Tape()->registerInput(s1);
        Number_::Tape()->registerInput(s2);

        Number_ value = pow(s1, s2);
        ASSERT_NEAR(value.value(), std::pow(s1.value(), s2.value()), 1e-10);
        value.PropagateToStart();

        ASSERT_NEAR(GetGradient(s1), s2.value() * std::pow(s1.value(), s2.value() - 1), 1e-10);
        ASSERT_NEAR(GetGradient(s2), value.value() * std::log(s1.value()), 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s1 = 3.0;
        Number_ s2(2.0);
        Number_::Tape()->registerInput(s1);

        Number_ value = pow(s1, 2.0);
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), s2.value() * std::pow(s1.value(), 1.0), 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s2 = 2.0;
        Number_::Tape()->registerInput(s2);

        Number_ value = pow(3.0, s2);
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s2), value.value() * std::log(3.0), 1e-10);
    }
}

TEST(AADTest, TestNumberMax) {

    {
        Number_::Tape()->Clear();
        Number_ s1(3.0);
        Number_ s2(2.0);
        Number_::Tape()->registerInput(s1);
        Number_::Tape()->registerInput(s2);

        Number_ value = max(s1, s2);
        ASSERT_NEAR(value.value(), 3.0, 1e-10);
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 1.0, 1e-10);
        ASSERT_NEAR(GetGradient(s2), 0.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s1 = 3.0;
        Number_::Tape()->registerInput(s1);

        Number_ value = max(s1, 2.0);
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 1.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s2 = 2.0;
        Number_::Tape()->registerInput(s2);

        Number_ value = max(3.0, s2);
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s2), 0.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s1 = 2.0;
        Number_ s2 = 3.0;
        Number_::Tape()->registerInput(s1);
        Number_::Tape()->registerInput(s2);

        Number_ value = max(s1, s2);
        ASSERT_NEAR(value.value(), 3.0, 1e-10);
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 0.0, 1e-10);
        ASSERT_NEAR(GetGradient(s2), 1.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s1 = 2.0;
        Number_::Tape()->registerInput(s1);

        Number_ value = max(s1, 3.0);
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 0.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s2 = 3.0;
        Number_::Tape()->registerInput(s2);

        Number_ value = max(2.0, s2);
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s2), 1.0, 1e-10);
    }
}

TEST(AADTest, TestNumberMin) {
    {
        Number_::Tape()->Clear();
        Number_ s1(3.0);
        Number_ s2(2.0);
        Number_::Tape()->registerInput(s1);
        Number_::Tape()->registerInput(s2);

        Number_ value = min(s1, s2);
        ASSERT_NEAR(value.value(), 2.0, 1e-10);
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 0.0, 1e-10);
        ASSERT_NEAR(GetGradient(s2), 1.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s1 = 3.0;
        Number_::Tape()->registerInput(s1);

        Number_ value = min(s1, 2.0);
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 0.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s2 = 2.0;
        Number_::Tape()->registerInput(s2);

        Number_ value = min(3.0, s2);
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s2), 1.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s1 = 2.0;
        Number_ s2 = 3.0;
        Number_::Tape()->registerInput(s1);
        Number_::Tape()->registerInput(s2);

        Number_ value = min(s1, s2);
        ASSERT_NEAR(value.value(), 2.0, 1e-10);
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 1.0, 1e-10);
        ASSERT_NEAR(GetGradient(s2), 0.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s1 = 2.0;
        Number_::Tape()->registerInput(s1);

        Number_ value = min(s1, 3.0);
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 1.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s2 = 3.0;
        Number_::Tape()->registerInput(s2);

        Number_ value = min(2.0, s2);
        value.PropagateToStart();
        ASSERT_NEAR(GetGradient(s2), 0.0, 1e-10);
    }
}


TEST(AADTest, TestNumberEqualAdd) {

    {
        Number_::Tape()->Clear();
        Number_ s1(1.0);
        Number_ s2(2.0);
        Number_::Tape()->registerInput(s1);
        Number_::Tape()->registerInput(s2);
        
        Number_ s3 = s1;
        s3 += s2;
        ASSERT_NEAR(s3.value(), 3.0, 1e-10);
        s3.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 1.0, 1e-10);
        ASSERT_NEAR(GetGradient(s2), 1.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s1 = Number_(1.0);
        Number_::Tape()->registerInput(s1);
        
        Number_ s3 = s1;
        s3 += 2.0;
        ASSERT_NEAR(s3.value(), 3.0, 1e-10);
        s3.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 1.0, 1e-10);
    }
}


TEST(AADTest, TestNumberEqualSub) {
    {
        Number_::Tape()->Clear();
        Number_ s1(1.0);
        Number_ s2(2.0);
        Number_::Tape()->registerInput(s1);
        Number_::Tape()->registerInput(s2);
        
        Number_ s3 = s1;
        s3 -= s2;
        ASSERT_NEAR(s3.value(), -1.0, 1e-10);
        s3.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 1.0, 1e-10);
        ASSERT_NEAR(GetGradient(s2), -1.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s1 = Number_(1.0);
        Number_::Tape()->registerInput(s1);
        
        Number_ s3 = s1;
        s3 -= 2.0;
        ASSERT_NEAR(s3.value(), -1.0, 1e-10);
        s3.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 1.0, 1e-10);
    }
}


TEST(AADTest, TestNumberEqualMultiply) {

    {
        Number_::Tape()->Clear();
        Number_ s1(2.0);
        Number_ s2(3.0);
        Number_::Tape()->registerInput(s1);
        Number_::Tape()->registerInput(s2);
        
        Number_ s3 = s1;
        s3 *= s2;
        ASSERT_NEAR(s3.value(), 6.0, 1e-10);
        s3.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 3.0, 1e-10);
        ASSERT_NEAR(GetGradient(s2), 2.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s1 = 2.0;
        Number_::Tape()->registerInput(s1);

        Number_ s3 = s1;
        s3 *= 3.0;
        ASSERT_NEAR(s3.value(), 6.0, 1e-10);
        s3.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 3.0, 1e-10);
    }
}


TEST(AADTest, TestNumberEqualDivide) {

    {
        Number_::Tape()->Clear();
        Number_ s1(2.0);
        Number_ s2(3.0);
        Number_::Tape()->registerInput(s1);
        Number_::Tape()->registerInput(s2);

        Number_ s3 = s1;
        s3 /= s2;
        ASSERT_NEAR(s3.value(), 0.66666666666666, 1e-10);
        s3.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 1 / 3.0, 1e-10);
        ASSERT_NEAR(GetGradient(s2), -2.0 / 9.0, 1e-10);
    }

    {
        Number_::Tape()->Clear();
        Number_ s1 = 2.0;
        Number_::Tape()->registerInput(s1);
        
        Number_ s3 = s1;
        s3 /= 3.0;
        ASSERT_NEAR(s3.value(), 0.66666666666666, 1e-10);
        s3.PropagateToStart();
        ASSERT_NEAR(GetGradient(s1), 1 / 3.0, 1e-10);
    }
}


TEST(AADTest, TestNumberNegative) {
    Number_::Tape()->Clear();
    Number_ s1(2.0);
    Number_::Tape()->registerInput(s1);
    
    Number_ value = -s1;
    ASSERT_NEAR(value.value(), -2.0, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(GetGradient(s1), -1.0, 1e-10);
}


TEST(AADTest, TestNumberPositive) {
    Number_::Tape()->Clear();
    Number_ s1(2.0);
    Number_::Tape()->registerInput(s1);

    Number_ value = +s1;
    ASSERT_NEAR(value.value(), 2.0, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(GetGradient(s1), 1.0, 1e-10);
}


TEST(AADTest, TestNumberExp) {
    Number_::Tape()->Clear();
    Number_ s1(2.0);
    Number_::Tape()->registerInput(s1);

    Number_ value = exp(s1);
    ASSERT_NEAR(value.value(), std::exp(2.0), 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(GetGradient(s1), std::exp(2.0), 1e-10);
}


TEST(AADTest, TestNumberLog) {
    Number_::Tape()->Clear();
    Number_ s1(2.0);
    Number_::Tape()->registerInput(s1);

    Number_ value = log(s1);
    ASSERT_NEAR(value.value(), std::log(2.0), 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(GetGradient(s1), 0.5, 1e-10);
}


TEST(AADTest, TestNumberSqrt) {
    Number_::Tape()->Clear();
    Number_ s1(2.0);
    Number_::Tape()->registerInput(s1);

    Number_ value = sqrt(s1);
    ASSERT_NEAR(value.value(), std::sqrt(2.0), 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(GetGradient(s1), 0.5 / std::sqrt(2.0), 1e-10);
}


TEST(AADTest, TestNumberFabs) {
    Number_::Tape()->Clear();
    Number_ s1(-2.0);
    Number_::Tape()->registerInput(s1);

    Number_ value = fabs(s1);
    ASSERT_NEAR(value.value(), 2.0, 1e-10);
    value.PropagateToStart();
    ASSERT_NEAR(GetGradient(s1), -1.0, 1e-10);
}