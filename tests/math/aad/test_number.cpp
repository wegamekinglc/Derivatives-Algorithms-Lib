//
// Created by wegam on 2021/1/18.
//

#include <dal/math/aad/aad.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace Dal::AAD;

TEST(AADTest, TestNumberAdd) {

    auto& tape = Number_::getTape();
    tape.setActive();

    Number_ s1(1.0); 
    Number_ s2(2.0);
    tape.registerInput(s1);
    tape.registerInput(s2);
    Number_ value = s1 + s2;
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(value.value(), 3.0, 1e-10);
    ASSERT_NEAR(s1.getGradient(), 1.0, 1e-10);
    ASSERT_NEAR(s2.getGradient(), 1.0, 1e-10);
    tape.reset();

    s1 = Number_(1.0);
    tape.registerInput(s1);
    value = s1 + 2.0;
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 1.0, 1e-10);
    tape.reset();

    s2 = Number_(2.0);
    tape.registerInput(s2);
    value = 1.0 + s2;
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s2.getGradient(), 1.0, 1e-10);
    tape.reset();
}

TEST(AADTest, TestNumberSub) {

    auto& tape = Number_::getTape();
    tape.setActive();

    Number_ s1(1.0);
    Number_ s2(2.0);
    tape.registerInput(s1);
    tape.registerInput(s2);
    Number_ value = s1 - s2;
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(value.value(), -1.0, 1e-10);
    ASSERT_NEAR(s1.getGradient(), 1.0, 1e-10);
    ASSERT_NEAR(s2.getGradient(), -1.0, 1e-10);
    tape.reset();

    s1 = Number_(1.0);
    tape.registerInput(s1);
    value = s1 - 2.0;
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 1.0, 1e-10);
    tape.reset();

    s2 = Number_(2.0);
    tape.registerInput(s2);
    value = 1.0 - s2;
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s2.getGradient(), -1.0, 1e-10);
    tape.reset();
}

TEST(AADTest, TestNumberMultiply) {

    auto& tape = Number_::getTape();
    tape.setActive();

    Number_ s1(3.0);
    Number_ s2(2.0);
    tape.registerInput(s1);
    tape.registerInput(s2);
    Number_ value = s1 * s2;
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(value.value(), 6.0, 1e-10);
    ASSERT_NEAR(s1.getGradient(), 2.0, 1e-10);
    ASSERT_NEAR(s2.getGradient(), 3.0, 1e-10);
    tape.reset();

    s1 = Number_(3.0);
    tape.registerInput(s1);
    value = s1 * 2.0;
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 2.0, 1e-10);
    tape.reset();

    s2 = Number_(2.0);
    tape.registerInput(s2);
    value = 3.0 * s2;
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s2.getGradient(), 3.0, 1e-10);
    tape.reset();
}

TEST(AADTest, TestNumberDivide) {

    auto& tape = Number_::getTape();
    tape.setActive();

    Number_ s1(3.0);
    Number_ s2(2.0);
    tape.registerInput(s1);
    tape.registerInput(s2);
    Number_ value = s1 / s2;
    ASSERT_NEAR(value.value(), 1.5, 1e-10);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 1./ s2.value(), 1e-10);
    ASSERT_NEAR(s2.getGradient(), -s1.value() / s2.value() / s2.value(), 1e-10);
    tape.reset();

    s1 = Number_(3.0);
    tape.registerInput(s1);
    value = s1 / 2.0;
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 1. / 2.0, 1e-10);
    tape.reset();

    s2 = Number_(2.0);
    tape.registerInput(s2);
    value = 3.0 / s2;
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s2.getGradient(), -3.0 / s2.value() / s2.value(), 1e-10);
    tape.reset();
}

TEST(AADTest, TestNumberPow) {
    auto& tape = Number_::getTape();
    tape.setActive();

    Number_ s1(3.0);
    Number_ s2(2.0);
    tape.registerInput(s1);
    tape.registerInput(s2);
    Number_ value = pow(s1, s2);
    ASSERT_NEAR(value.value(), std::pow(s1.value(), s2.value()), 1e-10);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), s2.value() * std::pow(s1.value(), s2.value() - 1), 1e-10);
    ASSERT_NEAR(s2.getGradient(), value.value() * std::log(s1.value()), 1e-10);
    tape.reset();

    s1 = Number_(3.0);
    tape.registerInput(s1);
    value = pow(s1, 2.0);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), s2.value() * std::pow(s1.value(), 1.0), 1e-10);
    tape.reset();

    s2 = Number_(2.0);
    tape.registerInput(s2);
    value = pow(3.0, s2);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s2.getGradient(), value.value() * std::log(3.0), 1e-10);
    tape.reset();
}

TEST(AADTest, TestNumberMax) {
    auto& tape = Number_::getTape();
    tape.setActive();

    Number_ s1(3.0);
    Number_ s2(2.0);
    tape.registerInput(s1);
    tape.registerInput(s2);
    Number_ value = max(s1, s2);
    ASSERT_NEAR(value.value(), 3.0, 1e-10);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 1.0, 1e-10);
    ASSERT_NEAR(s2.getGradient(), 0.0, 1e-10);
    tape.reset();

    s1 = Number_(3.0);
    tape.registerInput(s1);
    value = max(s1, 2.0);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 1.0, 1e-10);
    tape.reset();

    s2 = Number_(2.0);
    tape.registerInput(s2);
    value = max(3.0, s2);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s2.getGradient(), 0.0, 1e-10);
    tape.reset();

    s1 = Number_(2.0);
    s2 = Number_(3.0);
    tape.registerInput(s1);
    tape.registerInput(s2);
    value = max(s1, s2);
    ASSERT_NEAR(value.value(), 3.0, 1e-10);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 0.0, 1e-10);
    ASSERT_NEAR(s2.getGradient(), 1.0, 1e-10);

    s1 = Number_(2.0);
    tape.registerInput(s1);
    value = max(s1, 3.0);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 0.0, 1e-10);
    tape.reset();

    s2 = Number_(3.0);
    tape.registerInput(s2);
    value = max(2.0, s2);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s2.getGradient(), 1.0, 1e-10);
    tape.reset();
}

TEST(AADTest, TestNumberMin) {
    auto& tape = Number_::getTape();
    tape.setActive();

    Number_ s1(3.0);
    Number_ s2(2.0);
    tape.registerInput(s1);
    tape.registerInput(s2);
    Number_ value = min(s1, s2);
    ASSERT_NEAR(value.value(), 2.0, 1e-10);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 0.0, 1e-10);
    ASSERT_NEAR(s2.getGradient(), 1.0, 1e-10);
    tape.reset();

    s1 = Number_(3.0);
    tape.registerInput(s1);
    value = min(s1, 2.0);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 0.0, 1e-10);
    tape.reset();

    s2 = Number_(2.0);
    tape.registerInput(s2);
    value = min(3.0, s2);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s2.getGradient(), 1.0, 1e-10);
    tape.reset();

    s1 = Number_(2.0);
    s2 = Number_(3.0);
    tape.registerInput(s1);
    tape.registerInput(s2);
    value = min(s1, s2);
    ASSERT_NEAR(value.value(), 2.0, 1e-10);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 1.0, 1e-10);
    ASSERT_NEAR(s2.getGradient(), 0.0, 1e-10);
    tape.reset();

    s1 = Number_(2.0);
    tape.registerInput(s1);
    value = min(s1, 3.0);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 1.0, 1e-10);
    tape.reset();

    s2 = Number_(3.0);
    tape.registerInput(s2);
    value = min(2.0, s2);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s2.getGradient(), 0.0, 1e-10);
    tape.reset();
}


TEST(AADTest, TestNumberEqualAdd) {

    auto& tape = Number_::getTape();
    tape.setActive();

    Number_ s1(1.0);
    Number_ s2(2.0);
    tape.registerInput(s1);
    tape.registerInput(s2);
    Number_ s3 = s1;
    s3 += s2;
    ASSERT_NEAR(s3.value(), 3.0, 1e-10);
    s3.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 1.0, 1e-10);
    ASSERT_NEAR(s2.getGradient(), 1.0, 1e-10);
    tape.reset();

    s1 = Number_(1.0);
    tape.registerInput(s1);
    s3 = s1;
    s3 += 2.0;
    ASSERT_NEAR(s3.value(), 3.0, 1e-10);
    s3.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 1.0, 1e-10);
    tape.reset();
}


TEST(AADTest, TestNumberEqualSub) {

    auto& tape = Number_::getTape();
    tape.setActive();

    Number_ s1(1.0);
    Number_ s2(2.0);
    tape.registerInput(s1);
    tape.registerInput(s2);
    Number_ s3 = s1;
    s3 -= s2;
    ASSERT_NEAR(s3.value(), -1.0, 1e-10);
    s3.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 1.0, 1e-10);
    ASSERT_NEAR(s2.getGradient(), -1.0, 1e-10);
    tape.reset();

    s1 = Number_(1.0);
    tape.registerInput(s1);
    s3 = s1;
    s3 -= 2.0;
    ASSERT_NEAR(s3.value(), -1.0, 1e-10);
    s3.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 1.0, 1e-10);
    tape.reset();
}


TEST(AADTest, TestNumberEqualMultiply) {

    auto& tape = Number_::getTape();
    tape.setActive();

    Number_ s1(2.0);
    Number_ s2(3.0);
    tape.registerInput(s1);
    tape.registerInput(s2);
    Number_ s3 = s1;
    s3 *= s2;
    ASSERT_NEAR(s3.value(), 6.0, 1e-10);
    s3.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 3.0, 1e-10);
    ASSERT_NEAR(s2.getGradient(), 2.0, 1e-10);
    tape.reset();

    s1 = Number_(2.0);
    tape.registerInput(s1);
    s3 = s1;
    s3 *= 3.0;
    ASSERT_NEAR(s3.value(), 6.0, 1e-10);
    s3.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 3.0, 1e-10);
    tape.reset();
}


TEST(AADTest, TestNumberEqualDivide) {

    auto& tape = Number_::getTape();
    tape.setActive();

    Number_ s1(2.0);
    Number_ s2(3.0);
    tape.registerInput(s1);
    tape.registerInput(s2);
    Number_ s3 = s1;
    s3 /= s2;
    ASSERT_NEAR(s3.value(), 0.66666666666666, 1e-10);
    s3.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 1 / 3.0, 1e-10);
    ASSERT_NEAR(s2.getGradient(), -2.0 / 9.0, 1e-10);
    tape.reset();

    s1 = Number_(2.0);
    tape.registerInput(s1);
    s3 = s1;
    s3 /= 3.0;
    ASSERT_NEAR(s3.value(), 0.66666666666666, 1e-10);
    s3.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 1 / 3.0, 1e-10);
    tape.reset();
}


TEST(AADTest, TestNumberNegative) {
    auto& tape = Number_::getTape();
    tape.setActive();
    Number_ s1(2.0);
    tape.registerInput(s1);
    Number_ value = -s1;
    ASSERT_NEAR(value.value(), -2.0, 1e-10);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), -1.0, 1e-10);
    tape.reset();
}


TEST(AADTest, TestNumberPositive) {
    auto& tape = Number_::getTape();
    tape.setActive();
    Number_ s1(2.0);
    tape.registerInput(s1);
    Number_ value = +s1;
    ASSERT_NEAR(value.value(), 2.0, 1e-10);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 1.0, 1e-10);
    tape.reset();
}


TEST(AADTest, TestNumberExp) {
    auto& tape = Number_::getTape();
    tape.setActive();
    Number_ s1(2.0);
    tape.registerInput(s1);
    Number_ value = exp(s1);
    ASSERT_NEAR(value.value(), std::exp(2.0), 1e-10);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), std::exp(2.0), 1e-10);
    tape.reset();
}


TEST(AADTest, TestNumberLog) {
    auto& tape = Number_::getTape();
    tape.setActive();
    Number_ s1(2.0);
    tape.registerInput(s1);
    Number_ value = log(s1);
    ASSERT_NEAR(value.value(), std::log(2.0), 1e-10);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 0.5, 1e-10);
    tape.reset();
}


TEST(AADTest, TestNumberSqrt) {
    auto& tape = Number_::getTape();
    tape.setActive();
    Number_ s1(2.0);
    tape.registerInput(s1);
    Number_ value = sqrt(s1);
    ASSERT_NEAR(value.value(), std::sqrt(2.0), 1e-10);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), 0.5 / std::sqrt(2.0), 1e-10);
    tape.reset();
}


TEST(AADTest, TestNumberFabs) {
    auto& tape = Number_::getTape();
    tape.setActive();
    Number_ s1(-2.0);
    tape.registerInput(s1);
    Number_ value = fabs(s1);
    ASSERT_NEAR(value.value(), 2.0, 1e-10);
    value.setGradient(1.0);
    tape.evaluate();
    ASSERT_NEAR(s1.getGradient(), -1.0, 1e-10);
    tape.reset();
}