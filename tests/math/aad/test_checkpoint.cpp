//
// Created by wegam on 2023/2/18.
//

#include <dal/math/aad/aad.hpp>
#include <gtest/gtest.h>

#ifndef USE_ADEPT

using namespace Dal::AAD;

TEST(AADCheckpointTest, TestWithCheckpoint) {
    auto& tape = Number_::getTape();
    tape.setActive();

    Number_ s1(1.0);
    Number_ s2(2.0);

    tape.registerInput(s1);
    tape.registerInput(s2);

    Number_ s3 = s1 + s2;
    Position_ begin = tape.getPosition();
    Number_ value = s3 * 2.0;
    value.setGradient(1.0);
    tape.evaluate(tape.getPosition(), begin);

    ASSERT_NEAR(value.value(), 6.0, 1e-10);
    ASSERT_NEAR(s3.getGradient(), 2.0, 1e-10);

    tape.evaluate(begin, tape.getZeroPosition());
    ASSERT_NEAR(s1.getGradient(), 2.0, 1e-10);
}

TEST(AADCheckpointTest, TestWithCheckpointWithForLoop) {
    auto& tape = Number_::getTape();
    for (int m = 0; m < 3; ++m) {
        tape.reset();
        tape.setActive();

        int n = 10000;
        Number_ s1(1.0);
        Number_ s2(2.0);

        tape.registerInput(s1);
        tape.registerInput(s2);

        Number_ s3 = s1 + s2;
        Position_ begin = tape.getPosition();
        for (int i = 0; i < n; ++i) {
            Number_ value;
            if (i % 2 == 0)
                value = s3 * 1.01;
            else
                value = s3 * 0.99;
            value.setGradient(1.0);
            tape.evaluate(tape.getPosition(), begin);
            if (i % 2 == 0) {
                ASSERT_NEAR(value.value(), 3 * 1.01, 1e-10);
                ASSERT_NEAR(s3.getGradient(), (i + 1) / 2 * 2 + (i + 1) % 2 * 1.01, 1e-10);
            } else {
                ASSERT_NEAR(value.value(), 3 * 0.99, 1e-10);
                ASSERT_NEAR(s3.getGradient(), (i + 1) / 2 * 2 + (i + 1) % 2 * 0.99, 1e-10);
            }
            tape.resetTo(begin);
        }
        tape.evaluate(tape.getPosition(), tape.getZeroPosition());
        ASSERT_NEAR(s1.getGradient(), n, 1e-10);
        ASSERT_NEAR(s2.getGradient(), n, 1e-10);
    }
}

#endif