//
// Created by dal-tester on 2026/8/15.
//

#include <gtest/gtest.h>

#include <dal/math/interp/interp.hpp>

#include <dal-public/src/interp.hpp>

using Dal::String_;
using Dal::Vector_;

TEST(InterpTest, TestNewLinearFactoryReturnsUsableInterpolator) {
    const Vector_<> x = {1., 2., 3., 4., 5.};
    const Vector_<> y = {2.5, 3.5, 1.7, 2.8, 3.6};

    const auto f = Dal::Interp1NewLinear(String_("dal_public_interp_linear"), x, y);

    ASSERT_FALSE(f.IsEmpty());
}

TEST(InterpTest, TestNewLinearExactAtKnots) {
    const Vector_<> x = {1., 2., 3., 4., 5.};
    const Vector_<> y = {2.5, 3.5, 1.7, 2.8, 3.6};

    const auto f = Dal::Interp1NewLinear(String_("dal_public_interp_knots"), x, y);

    for (size_t i = 0; i < x.size(); ++i)
        ASSERT_DOUBLE_EQ((*f)(x[i]), y[i]);
}

TEST(InterpTest, TestNewLinearInterpolatesBetweenKnots) {
    const Vector_<> x = {1., 2., 3., 4., 5.};
    const Vector_<> y = {2.5, 3.5, 1.7, 2.8, 3.6};

    const auto f = Dal::Interp1NewLinear(String_("dal_public_interp_mid"), x, y);

    // midpoint between knots is the average of the bracketing values
    for (size_t i = 0; i + 1 < x.size(); ++i)
        ASSERT_NEAR((*f)(0.5 * (x[i] + x[i + 1])), 0.5 * (y[i] + y[i + 1]), 1e-10);
}

TEST(InterpTest, TestNewLinearFlatExtrapolation) {
    const Vector_<> x = {1., 2., 3., 4., 5.};
    const Vector_<> y = {2.5, 3.5, 1.7, 2.8, 3.6};

    const auto f = Dal::Interp1NewLinear(String_("dal_public_interp_flat"), x, y);

    ASSERT_DOUBLE_EQ((*f)(0.0), y.front());
    ASSERT_DOUBLE_EQ((*f)(100.0), y.back());
}
