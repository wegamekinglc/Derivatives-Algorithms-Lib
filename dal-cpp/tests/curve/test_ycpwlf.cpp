//
// Created by Codex on 2026/9/12.
//

#include <gtest/gtest.h>
#include <cmath>
#include <dal/platform/platform.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

TEST(DiscountPWLFTest, TestUpdatedClonedAndRestoredCurvesMatchRebuiltIntegral) {
    const Vector_<Date_> knots = {Date_(2025, 1, 1), Date_(2026, 1, 1), Date_(2028, 1, 1)};
    Vector_<> left = {0.031, 0.043, 0.027};
    Vector_<> right = {0.034, 0.039, 0.028};
    const Handle_<DiscountCurve_> base(new Tape::DiscountPWLF_<double>("base", "USD", knots, left, right));
    const Handle_<DiscountCurve_> replacement(new Tape::DiscountPWLF_<double>("replacement", "USD", knots, right, left));
    Tape::DiscountPWLF_<double> curve("spread", "USD", knots, left, right, base);
    const Vector_<Date_> dates = {Date_(2024, 6, 1), knots.front(), Date_(2025, 8, 1), knots[1], Date_(2027, 4, 1), knots.back(), Date_(2029, 1, 1)};
    const Vector_<> dx = {0.0001, -0.0002, 0.0003, -0.0004, 0.0005, -0.0006};
    for (const double leverage : {0.0, 0.75, -0.25}) {
        curve.ApplyDX(dx.begin(), leverage);
        for (int i = 0; i < knots.size(); ++i) {
            left[i] += leverage * dx[2 * i];
            right[i] += leverage * dx[2 * i + 1];
        }
        const PiecewiseLinear_ oracle(knots, left, right);
        const auto clone = curve.Clone("clone", {});
        const auto rebased = curve.Clone("rebased", {{base.get(), handle_cast<YCComponent_>(replacement)}});
        const auto restored = JSON::ReadString(JSON::WriteString(curve), true);
        const auto* clonedCurve = dynamic_cast<const DiscountCurve_*>(clone.get());
        const auto* rebasedCurve = dynamic_cast<const DiscountCurve_*>(rebased.get());
        const auto* restoredCurve = dynamic_cast<const DiscountCurve_*>(restored.get());
        ASSERT_NE(clonedCurve, nullptr);
        ASSERT_NE(rebasedCurve, nullptr);
        ASSERT_NE(restoredCurve, nullptr);
        for (const auto& from : dates) {
            for (const auto& to : dates) {
                const double spreadDf = std::exp(-(oracle.IntegralTo(to) - oracle.IntegralTo(from)) / DAYS_PER_YEAR);
                const double expected = spreadDf * (*base)(from, to);
                ASSERT_EQ(curve(from, to), expected);
                ASSERT_EQ((*clonedCurve)(from, to), expected);
                ASSERT_EQ((*restoredCurve)(from, to), expected);
                ASSERT_EQ((*rebasedCurve)(from, to), spreadDf * (*replacement)(from, to));
            }
        }
    }
}

TEST(DiscountPWLFTest, TestSingleKnotUsesBothExtrapolationRates) {
    const Date_ knot(2025, 1, 1);
    const Tape::DiscountPWLF_<double> curve("single", "USD", {knot}, {0.02}, {0.04});
    ASSERT_EQ(curve(knot.AddDays(-10), knot.AddDays(20)), std::exp(-(10 * 0.02 + 20 * 0.04) / DAYS_PER_YEAR));
    ASSERT_EQ(curve(knot, knot), 1.0);
}
