//
// Created by dal-tester on 2026/8/15.
//

#include <gtest/gtest.h>

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

TEST(InterpTest, TestFactoriesAndBatchedEvaluation) {
    const Dal::Vector_<> x{0.0, 1.0, 2.0};
    const Dal::Vector_<> y{1.0, 3.0, 5.0};
    const Dal::Vector_<> points{0.0, 0.5, 2.0};
    const Dal::Vector_<Dal::Handle_<Dal::Interp1_>> interpolators{
        Dal::Interp1NewLinear("linear", x, y), Dal::Interp1NewLinearSmoothed("smoothed", x, y, 0.0), Dal::Interp1NewCubic("cubic", x, y, {1}, {2.0})};
    for (const auto& interpolator : interpolators) {
        const auto values = Dal::Interp1Get(interpolator, points);
        ASSERT_EQ(3U, values.size());
        for (int index = 0; index < 3; ++index)
            ASSERT_NEAR(1.0 + 2.0 * points[index], values[index], 1e-10);
    }
}

TEST(InterpTest, TestPositiveSmoothingUsesLegacyFirstDifferencePenalty) {
    const auto interpolator = Dal::Interp1NewLinearSmoothed("smoothed", {0.0, 1.0, 2.0}, {1.0, 3.0, 5.0}, 1.0);
    const auto values = Dal::Interp1Get(interpolator, {0.0, 1.0, 2.0});
    ASSERT_DOUBLE_EQ(1.0, values[0]);
    ASSERT_NEAR(11.0 / 3.0, values[1], 1e-10);
    ASSERT_NEAR(13.0 / 3.0, values[2], 1e-10);
}

TEST(InterpTest, TestBilinearCartesianEvaluation) {
    Dal::Matrix_<> values(2, 2);
    values(0, 0) = 1.0;
    values(0, 1) = 4.0;
    values(1, 0) = 3.0;
    values(1, 1) = 6.0;
    const auto interpolator = Dal::Interp2NewLinear("plane", {0.0, 1.0}, {0.0, 1.0}, values);
    const auto result = Dal::Interp2Get(interpolator, {0.25, 0.75}, {0.0, 0.5, 1.0});
    ASSERT_EQ(2, result.Rows());
    ASSERT_EQ(3, result.Cols());
    ASSERT_DOUBLE_EQ(1.5, result(0, 0));
    ASSERT_DOUBLE_EQ(3.0, result(0, 1));
    ASSERT_DOUBLE_EQ(5.5, result(1, 2));
}

TEST(InterpTest, TestRejectsInvalidHandlesAndSplineSettings) {
    ASSERT_THROW(Dal::Interp1Get({}, {0.0}), Dal::Exception_);
    ASSERT_THROW(Dal::Interp2Get({}, {0.0}, {0.0}), Dal::Exception_);
    ASSERT_THROW(Dal::Interp1NewCubic("bad", {0.0, 1.0}, {0.0, 1.0}, {0}, {}), Dal::Exception_);
    ASSERT_THROW(Dal::Interp1NewCubic("bad", {0.0, 1.0}, {0.0, 1.0}, {}, {1.0}), Dal::Exception_);
    ASSERT_THROW(Dal::Interp1NewLinearSmoothed("bad", {0.0, 1.0}, {0.0, 1.0}, -1.0), Dal::Exception_);
}
