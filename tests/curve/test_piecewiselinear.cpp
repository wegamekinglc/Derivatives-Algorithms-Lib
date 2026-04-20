//
// Created by GitHub Copilot on 2026/4/15.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/curve/piecewiselinear.hpp>

using namespace Dal;

TEST(PiecewiseLinearTest, TestValueAt) {
    const Vector_<Date_> knots = {Date_(2021, 3, 26), Date_(2022, 3, 26), Date_(2023, 3, 26)};
    const Vector_<> f_left = {10.0, 20.0, 40.0};
    const Vector_<> f_right = {15.0, 25.0, 45.0};

    const PiecewiseLinear_ pwl(knots, f_left, f_right);

    ASSERT_NEAR(pwl.ValueAt(Date_(2021, 1, 1)), 10.0, 1e-10);
    ASSERT_NEAR(pwl.ValueAt(knots[0], false), 10.0, 1e-10);
    ASSERT_NEAR(pwl.ValueAt(knots[0], true), 15.0, 1e-10);
    ASSERT_NEAR(pwl.ValueAt(knots[1], false), 20.0, 1e-10);
    ASSERT_NEAR(pwl.ValueAt(knots[1], true), 25.0, 1e-10);
    ASSERT_NEAR(pwl.ValueAt(Date_(2025, 1, 1)), 45.0, 1e-10);

    const Date_ d_mid(2022, 9, 26);
    const double t = (d_mid - knots[1]) / static_cast<double>(knots[2] - knots[1]);
    const double expected_mid = f_right[1] + t * (f_left[2] - f_right[1]);
    ASSERT_NEAR(pwl.ValueAt(d_mid), expected_mid, 1e-10);
}

TEST(PiecewiseLinearTest, TestIntegralTo) {
    const Vector_<Date_> knots = {Date_(2021, 3, 26), Date_(2022, 3, 26), Date_(2023, 3, 26)};
    const Vector_<> f_left = {10.0, 20.0, 40.0};
    const Vector_<> f_right = {15.0, 25.0, 45.0};

    const PiecewiseLinear_ pwl(knots, f_left, f_right);

    ASSERT_NEAR(pwl.IntegralTo(knots[0]), 0.0, 1e-10);

    const Date_ before_first(2021, 2, 24);
    const double expected_before = -f_left.front() * (knots.front() - before_first);
    ASSERT_NEAR(pwl.IntegralTo(before_first), expected_before, 1e-10);

    const double expected_sofar_1 = (knots[1] - knots[0]) * (f_left[1] + f_right[0]) / 2.0;
    ASSERT_NEAR(pwl.IntegralTo(knots[1]), expected_sofar_1, 1e-10);

    const Date_ d_mid(2022, 9, 26);
    const double elapsed = d_mid - knots[1];
    const double frac = elapsed / static_cast<double>(knots[2] - knots[1]);
    const double f_start = f_right[1];
    const double f_stop = f_start + frac * (f_left[2] - f_start);
    const double expected_mid = expected_sofar_1 + elapsed * (f_start + f_stop) / 2.0;
    ASSERT_NEAR(pwl.IntegralTo(d_mid), expected_mid, 1e-10);

    const double expected_sofar_2 = expected_sofar_1 + (knots[2] - knots[1]) * (f_left[2] + f_right[1]) / 2.0;
    const Date_ after_last(2023, 6, 26);
    const double expected_after = expected_sofar_2 + f_right.back() * (after_last - knots.back());
    ASSERT_NEAR(pwl.IntegralTo(after_last), expected_after, 1e-10);
}

TEST(PiecewiseLinearTest, TestUpdateRefreshesCachedIntegrals) {
    const Vector_<Date_> knots = {Date_(2021, 3, 26), Date_(2022, 3, 26), Date_(2023, 3, 26)};
    const Vector_<> f_left = {10.0, 20.0, 40.0};
    const Vector_<> f_right = {15.0, 25.0, 45.0};

    PiecewiseLinear_ pwl(knots, f_left, f_right);

    const double old_knot2 = pwl.IntegralTo(knots[2]);

    pwl.fLeft_[2] = 100.0;

    ASSERT_NEAR(pwl.IntegralTo(knots[2]), old_knot2, 1e-10);

    const double new_knot2_expected = (knots[1] - knots[0]) * (pwl.fLeft_[1] + pwl.fRight_[0]) / 2.0
                                      + (knots[2] - knots[1]) * (pwl.fLeft_[2] + pwl.fRight_[1]) / 2.0;

    pwl.Update();

    ASSERT_NEAR(pwl.sofar_[2], new_knot2_expected, 1e-10);
    ASSERT_NEAR(pwl.IntegralTo(knots[2]), new_knot2_expected, 1e-10);
}
