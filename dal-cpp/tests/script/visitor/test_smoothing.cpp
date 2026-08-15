//
// Created by wegamekinglc on 2026/8/15.
//
// Coverage for dal/script/visitor/smoothing.hpp: the smoothed call-spread
// (CSpr) and butterfly (BFly) condition kernels used by fuzzy evaluators.

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>
#include <dal/script/visitor/smoothing.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(SmoothingTest, TestCSprEpsOutsideBand) {
    ASSERT_DOUBLE_EQ(CSpr(-0.2, 0.2), 0.0);
    ASSERT_DOUBLE_EQ(CSpr(0.2, 0.2), 1.0);
}

TEST(SmoothingTest, TestCSprEpsBandEdgesAreInclusive) {
    // x == -eps/2 is not < -eps/2: the ramp starts exactly at 0.
    ASSERT_DOUBLE_EQ(CSpr(-0.1, 0.2), 0.0);
    // x == +eps/2 is not > +eps/2: the ramp ends exactly at 1.
    ASSERT_DOUBLE_EQ(CSpr(0.1, 0.2), 1.0);
}

TEST(SmoothingTest, TestCSprEpsIsLinearInsideBand) {
    ASSERT_DOUBLE_EQ(CSpr(0.0, 0.2), 0.5);
    ASSERT_DOUBLE_EQ(CSpr(0.05, 0.2), 0.75);
    ASSERT_DOUBLE_EQ(CSpr(-0.05, 0.2), 0.25);
}

TEST(SmoothingTest, TestCSprDiscreteBounds) {
    ASSERT_DOUBLE_EQ(CSpr(-2.0, -1.0, 2.0), 0.0);
    ASSERT_DOUBLE_EQ(CSpr(3.0, -1.0, 2.0), 1.0);
    ASSERT_DOUBLE_EQ(CSpr(0.5, -1.0, 2.0), 0.5);
    ASSERT_DOUBLE_EQ(CSpr(-1.0, -1.0, 2.0), 0.0);
    ASSERT_DOUBLE_EQ(CSpr(2.0, -1.0, 2.0), 1.0);
}

TEST(SmoothingTest, TestBFlyEpsPeaksAtZero) {
    ASSERT_DOUBLE_EQ(BFly(0.0, 0.2), 1.0);
    ASSERT_DOUBLE_EQ(BFly(0.05, 0.2), 0.5);
    ASSERT_DOUBLE_EQ(BFly(-0.05, 0.2), 0.5);
}

TEST(SmoothingTest, TestBFlyEpsVanishesOutsideBand) {
    ASSERT_DOUBLE_EQ(BFly(0.2, 0.2), 0.0);
    ASSERT_DOUBLE_EQ(BFly(-0.2, 0.2), 0.0);
    // The band edges themselves: x == +/-eps/2 is inside the ramp, value 0.
    ASSERT_DOUBLE_EQ(BFly(0.1, 0.2), 0.0);
    ASSERT_DOUBLE_EQ(BFly(-0.1, 0.2), 0.0);
}

TEST(SmoothingTest, TestBFlyDiscreteAsymmetricBounds) {
    ASSERT_DOUBLE_EQ(BFly(0.0, -2.0, 1.0), 1.0);
    ASSERT_DOUBLE_EQ(BFly(-1.0, -2.0, 1.0), 0.5);
    ASSERT_DOUBLE_EQ(BFly(0.5, -2.0, 1.0), 0.5);
    ASSERT_DOUBLE_EQ(BFly(-3.0, -2.0, 1.0), 0.0);
    ASSERT_DOUBLE_EQ(BFly(2.0, -2.0, 1.0), 0.0);
    // Bounds themselves are reached with value 0.
    ASSERT_DOUBLE_EQ(BFly(-2.0, -2.0, 1.0), 0.0);
    ASSERT_DOUBLE_EQ(BFly(1.0, -2.0, 1.0), 0.0);
}
