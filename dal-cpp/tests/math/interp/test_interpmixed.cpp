//
// Created by dal-tester on 2026/6/14.
//

#include <gtest/gtest.h>

#include <cmath>

#include <dal/platform/platform.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/math/interp/interpmixed.hpp>

using namespace Dal;

TEST(InterpMixedTest, TestCutoffOnKnotReproducesKnotValues) {
    // 5-knot log-DF curve with the cutoff snapped to the second knot (yf=1.0).
    // Both sub-interpolators must reproduce the shared knot, giving C0 continuity at the join.
    const Vector_<> yf = {0.0, 1.0, 2.0, 3.0, 4.0};
    const Vector_<> logDF = {0.0, -0.01, -0.02, -0.03, -0.04};
    MixedSchemeSpec_ spec;
    spec.cutoffYf_ = yf[1];

    std::unique_ptr<Interp1_> interp(NewMixedLogDF("mixed", yf, logDF, spec));
    for (int i = 0; i < static_cast<int>(yf.size()); ++i)
        ASSERT_NEAR((*interp)(yf[i]), logDF[i], 1e-10);
}

TEST(InterpMixedTest, TestCutoffBetweenKnotsThrows) {
    // cutoffYf must equal a knot exactly; an in-between value would silently snap down and break
    // the C0 continuity that the comment promises, so the constructor must reject it.
    const Vector_<> yf = {0.0, 1.0, 2.0, 3.0, 4.0};
    const Vector_<> logDF = {0.0, -0.01, -0.02, -0.03, -0.04};
    MixedSchemeSpec_ spec;
    spec.cutoffYf_ = 1.5;   // between yf[1] and yf[2]

    ASSERT_THROW(NewMixedLogDF("mixed", yf, logDF, spec), Dal::Exception_);
}

TEST(InterpMixedTest, TestCutoffBeforeSecondKnotThrows) {
    // cutoffYf at the very first knot would leave a degenerate linear head.
    const Vector_<> yf = {0.0, 1.0, 2.0, 3.0, 4.0};
    const Vector_<> logDF = {0.0, -0.01, -0.02, -0.03, -0.04};
    MixedSchemeSpec_ spec;
    spec.cutoffYf_ = yf[0];

    ASSERT_THROW(NewMixedLogDF("mixed", yf, logDF, spec), Dal::Exception_);
}

TEST(InterpMixedTest, TestCubicTailTooShortThrows) {
    // cutoff at the last knot leaves the cubic tail with only one knot.
    const Vector_<> yf = {0.0, 1.0, 2.0, 3.0, 4.0};
    const Vector_<> logDF = {0.0, -0.01, -0.02, -0.03, -0.04};
    MixedSchemeSpec_ spec;
    spec.cutoffYf_ = yf[4];

    ASSERT_THROW(NewMixedLogDF("mixed", yf, logDF, spec), Dal::Exception_);
}

TEST(InterpMixedTest, TestLinearHeadAndCubicTailOffKnots) {
    const Vector_<> yf = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0};
    const Vector_<> logDF = {0.0, -0.01, -0.05, -0.055, -0.095, -0.10};
    MixedSchemeSpec_ spec;
    spec.cutoffYf_ = yf[2];

    std::unique_ptr<Interp1_> mixed(NewMixedLogDF("mixed", yf, logDF, spec));
    const Vector_<> headYf(yf.begin(), yf.begin() + 3);
    const Vector_<> headLogDF(logDF.begin(), logDF.begin() + 3);
    std::unique_ptr<Interp1_> linearHead(Interp::NewLinear("linear_head", headYf, headLogDF));
    std::unique_ptr<Interp1_> cubicHead(Interp::NewCubic("cubic_head", headYf, headLogDF, spec.cubicLhs_, spec.cubicRhs_));
    const Vector_<> tailYf(yf.begin() + 2, yf.end());
    const Vector_<> tailLogDF(logDF.begin() + 2, logDF.end());
    std::unique_ptr<Interp1_> cubicTail(Interp::NewCubic("cubic_tail", tailYf, tailLogDF, spec.cubicLhs_, spec.cubicRhs_));
    std::unique_ptr<Interp1_> linearTail(Interp::NewLinear("linear_tail", tailYf, tailLogDF));

    const double headPoint = 0.5;
    ASSERT_NEAR((*mixed)(headPoint), (*linearHead)(headPoint), 1e-12);
    ASSERT_GT(std::abs((*mixed)(headPoint) - (*cubicHead)(headPoint)), 1e-6);

    const double tailPoint = 2.5;
    ASSERT_NEAR((*mixed)(tailPoint), (*cubicTail)(tailPoint), 1e-12);
    ASSERT_GT(std::abs((*mixed)(tailPoint) - (*linearTail)(tailPoint)), 1e-6);
}
