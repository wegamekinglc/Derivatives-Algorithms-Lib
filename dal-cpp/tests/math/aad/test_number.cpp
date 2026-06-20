//
// Created by wegam on 2021/1/18.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/aad/aad.hpp>

using namespace Dal::AAD;

TEST(AADTest, TestNumberAdd) {

    {
        Clear(*Tape());

        Number_ s1(1.0);
        Number_ s2(2.0);
        PutOnTape(s1);
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = s1 + s2;
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());

        ASSERT_NEAR(Value(value), 3.0, 1e-10);
        ASSERT_NEAR(Adjoint(s1), 1.0, 1e-10);
        ASSERT_NEAR(Adjoint(s2), 1.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s1 = 1.0;
        PutOnTape(s1);
        NewRecording(*Tape());

        Number_ value = s1 + 2.0;
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());

        ASSERT_NEAR(Value(value), 3.0, 1e-10);
        ASSERT_NEAR(Adjoint(s1), 1.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s2 = 2.0;
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = 1.0 + s2;
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());

        ASSERT_NEAR(Value(value), 3.0, 1e-10);
        ASSERT_NEAR(Adjoint(s2), 1.0, 1e-10);
    }
}

TEST(AADTest, TestNumberSub) {

    {
        Clear(*Tape());

        Number_ s1(1.0);
        Number_ s2(2.0);
        PutOnTape(s1);
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = s1 - s2;
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());

        ASSERT_NEAR(Value(value), -1.0, 1e-10);
        ASSERT_NEAR(Adjoint(s1), 1.0, 1e-10);
        ASSERT_NEAR(Adjoint(s2), -1.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s1(1.0);
        PutOnTape(s1);
        NewRecording(*Tape());

        Number_ value = s1 - 2.0;
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 1.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s2(2.0);
        PutOnTape(s2);
        NewRecording(*Tape());
        
        Number_ value = 1.0 - s2;
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s2), -1.0, 1e-10);
    }
}

TEST(AADTest, TestNumberMultiply) {

    {
        Clear(*Tape());

        Number_ s1(3.0);
        Number_ s2(2.0);
        PutOnTape(s1);
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = s1 * s2;
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Value(value), 6.0, 1e-10);
        ASSERT_NEAR(Adjoint(s1), 2.0, 1e-10);
        ASSERT_NEAR(Adjoint(s2), 3.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s1 = 3.0;
        PutOnTape(s1);
        NewRecording(*Tape());

        Number_ value = s1 * 2.0;
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 2.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s2 = 2.0;
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = 3.0 * s2;
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s2), 3.0, 1e-10);
    }
}

TEST(AADTest, TestNumberDivide) {
    {
        Clear(*Tape());

        Number_ s1(3.0);
        Number_ s2(2.0);
        PutOnTape(s1);
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = s1 / s2;
        ASSERT_NEAR(Value(value), 1.5, 1e-10);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 1. / Value(s2), 1e-10);
        ASSERT_NEAR(Adjoint(s2), -Value(s1) / Value(s2) / Value(s2), 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s1 = 3.0;
        PutOnTape(s1);
        NewRecording(*Tape());

        Number_ value = s1 / 2.0;
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 1. / 2.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s2 = 2.0;
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = 3.0 / s2;
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s2), -3.0 / Value(s2) / Value(s2), 1e-10);
    }
}

TEST(AADTest, TestNumberPow) {

    {
        Clear(*Tape());

        Number_ s1(3.0);
        Number_ s2(2.0);
        PutOnTape(s1);
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = pow(s1, s2);
        ASSERT_NEAR(Value(value), std::pow(Value(s1), Value(s2)), 1e-10);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());

        ASSERT_NEAR(Adjoint(s1), Value(s2) * std::pow(Value(s1), Value(s2) - 1), 1e-10);
        ASSERT_NEAR(Adjoint(s2), Value(value) * std::log(Value(s1)), 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s1 = 3.0;
        Number_ s2(2.0);
        PutOnTape(s1);
        NewRecording(*Tape());

        Number_ value = pow(s1, 2.0);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), Value(s2) * std::pow(Value(s1), 1.0), 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s2 = 2.0;
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = pow(3.0, s2);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s2), Value(value) * std::log(3.0), 1e-10);
    }
}

TEST(AADTest, TestNumberMax) {

    {
        Clear(*Tape());

        Number_ s1(3.0);
        Number_ s2(2.0);
        PutOnTape(s1);
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = max(s1, s2);
        ASSERT_NEAR(Value(value), 3.0, 1e-10);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 1.0, 1e-10);
        ASSERT_NEAR(Adjoint(s2), 0.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s1 = 3.0;
        PutOnTape(s1);
        NewRecording(*Tape());

        Number_ value = max(s1, 2.0);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 1.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s2 = 2.0;
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = max(3.0, s2);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s2), 0.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s1 = 2.0;
        Number_ s2 = 3.0;
        PutOnTape(s1);
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = max(s1, s2);
        ASSERT_NEAR(Value(value), 3.0, 1e-10);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 0.0, 1e-10);
        ASSERT_NEAR(Adjoint(s2), 1.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s1 = 2.0;
        PutOnTape(s1);
        NewRecording(*Tape());

        Number_ value = max(s1, 3.0);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 0.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s2 = 3.0;
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = max(2.0, s2);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s2), 1.0, 1e-10);
    }
}

TEST(AADTest, TestNumberMin) {
    {
        Clear(*Tape());

        Number_ s1(3.0);
        Number_ s2(2.0);
        PutOnTape(s1);
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = min(s1, s2);
        ASSERT_NEAR(Value(value), 2.0, 1e-10);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 0.0, 1e-10);
        ASSERT_NEAR(Adjoint(s2), 1.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s1 = 3.0;
        PutOnTape(s1);
        NewRecording(*Tape());

        Number_ value = min(s1, 2.0);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 0.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s2 = 2.0;
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = min(3.0, s2);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s2), 1.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s1 = 2.0;
        Number_ s2 = 3.0;
        PutOnTape(s1);
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = min(s1, s2);
        ASSERT_NEAR(Value(value), 2.0, 1e-10);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 1.0, 1e-10);
        ASSERT_NEAR(Adjoint(s2), 0.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s1 = 2.0;
        PutOnTape(s1);
        NewRecording(*Tape());

        Number_ value = min(s1, 3.0);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 1.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s2 = 3.0;
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ value = min(2.0, s2);
        Adjoint(value) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s2), 0.0, 1e-10);
    }
}


TEST(AADTest, TestNumberEqualAdd) {

    {
        Clear(*Tape());

        Number_ s1(1.0);
        Number_ s2(2.0);
        PutOnTape(s1);
        PutOnTape(s2);
        NewRecording(*Tape());
        
        Number_ s3 = s1;
        s3 += s2;
        ASSERT_NEAR(Value(s3), 3.0, 1e-10);
        Adjoint( s3) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 1.0, 1e-10);
        ASSERT_NEAR(Adjoint(s2), 1.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s1 = Number_(1.0);
        PutOnTape(s1);
        NewRecording(*Tape());
        
        Number_ s3 = s1;
        s3 += 2.0;
        ASSERT_NEAR(Value(s3), 3.0, 1e-10);
        Adjoint( s3) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 1.0, 1e-10);
    }
}


TEST(AADTest, TestNumberEqualSub) {
    {
        Clear(*Tape());

        Number_ s1(1.0);
        Number_ s2(2.0);
        PutOnTape(s1);
        PutOnTape(s2);
        NewRecording(*Tape());
        
        Number_ s3 = s1;
        s3 -= s2;
        ASSERT_NEAR(Value(s3), -1.0, 1e-10);
        Adjoint( s3) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 1.0, 1e-10);
        ASSERT_NEAR(Adjoint(s2), -1.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s1 = Number_(1.0);
        PutOnTape(s1);
        NewRecording(*Tape());
        
        Number_ s3 = s1;
        s3 -= 2.0;
        ASSERT_NEAR(Value(s3), -1.0, 1e-10);
        Adjoint( s3) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 1.0, 1e-10);
    }
}


TEST(AADTest, TestNumberEqualMultiply) {

    {
        Clear(*Tape());

        Number_ s1(2.0);
        Number_ s2(3.0);
        PutOnTape(s1);
        PutOnTape(s2);
        NewRecording(*Tape());
        
        Number_ s3 = s1;
        s3 *= s2;
        ASSERT_NEAR(Value(s3), 6.0, 1e-10);
        Adjoint( s3) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 3.0, 1e-10);
        ASSERT_NEAR(Adjoint(s2), 2.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s1 = 2.0;
        PutOnTape(s1);
        NewRecording(*Tape());

        Number_ s3 = s1;
        s3 *= 3.0;
        ASSERT_NEAR(Value(s3), 6.0, 1e-10);
        Adjoint( s3) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 3.0, 1e-10);
    }
}


TEST(AADTest, TestNumberEqualDivide) {

    {
        Clear(*Tape());

        Number_ s1(2.0);
        Number_ s2(3.0);
        PutOnTape(s1);
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ s3 = s1;
        s3 /= s2;
        ASSERT_NEAR(Value(s3), 0.66666666666666, 1e-10);
        Adjoint( s3) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 1 / 3.0, 1e-10);
        ASSERT_NEAR(Adjoint(s2), -2.0 / 9.0, 1e-10);
    }

    {
        Clear(*Tape());

        Number_ s1 = 2.0;
        PutOnTape(s1);
        NewRecording(*Tape());
        
        Number_ s3 = s1;
        s3 /= 3.0;
        ASSERT_NEAR(Value(s3), 0.66666666666666, 1e-10);
        Adjoint( s3) = 1.0;
        PropagateToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), 1 / 3.0, 1e-10);
    }
}


TEST(AADTest, TestNumberNegative) {
    Clear(*Tape());

    Number_ s1(2.0);
    PutOnTape(s1);
    NewRecording(*Tape());
    
    Number_ value = -s1;
    ASSERT_NEAR(Value(value), -2.0, 1e-10);
    Adjoint( value) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_NEAR(Adjoint(s1), -1.0, 1e-10);
}


TEST(AADTest, TestNumberPositive) {
    Clear(*Tape());

    Number_ s1(2.0);
    PutOnTape(s1);
    NewRecording(*Tape());

    Number_ value = +s1;
    ASSERT_NEAR(Value(value), 2.0, 1e-10);
    Adjoint( value) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_NEAR(Adjoint(s1), 1.0, 1e-10);
}


TEST(AADTest, TestNumberExp) {
    Clear(*Tape());

    Number_ s1(2.0);
    PutOnTape(s1);
    NewRecording(*Tape());

    Number_ value = exp(s1);
    ASSERT_NEAR(Value(value), std::exp(2.0), 1e-10);
    Adjoint( value) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_NEAR(Adjoint(s1), std::exp(2.0), 1e-10);
}


TEST(AADTest, TestNumberLog) {
    Clear(*Tape());

    Number_ s1(2.0);
    PutOnTape(s1);
    NewRecording(*Tape());

    Number_ value = log(s1);
    ASSERT_NEAR(Value(value), std::log(2.0), 1e-10);
    Adjoint( value) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_NEAR(Adjoint(s1), 0.5, 1e-10);
}


TEST(AADTest, TestNumberSqrt) {
    Clear(*Tape());

    Number_ s1(2.0);
    PutOnTape(s1);
    NewRecording(*Tape());

    Number_ value = sqrt(s1);
    ASSERT_NEAR(Value(value), std::sqrt(2.0), 1e-10);
    Adjoint( value) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_NEAR(Adjoint(s1), 0.5 / std::sqrt(2.0), 1e-10);
}


TEST(AADTest, TestNumberAbs) {
    Clear(*Tape());

    Number_ s1(-2.0);
    PutOnTape(s1);
    NewRecording(*Tape());

    Number_ value = abs(s1);
    ASSERT_NEAR(Value(value), 2.0, 1e-10);
    Adjoint( value) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_NEAR(Adjoint(s1), -1.0, 1e-10);
}
