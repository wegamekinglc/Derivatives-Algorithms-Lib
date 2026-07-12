//
// Created by wegam on 2023/3/26.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>

#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/cell.hpp>
#include <dal/storage/splat.hpp>

#include <memory>

using namespace Dal;

TEST(PiecewiseConstantTest, TestPiecewiseConstant) {
    Vector_<Date_> knots = {Date_(2021, 3, 26), Date_(2022, 3, 26), Date_(2023, 3, 26)};
    Vector_<> right = {1.0, 2.0, 3.0};

    PiecewiseConstant_ pwc(knots, right);

    Date_ d(2022, 6, 26);
    ASSERT_NEAR(pwc.IntegralTo(d), 549.0, 1e-8);
}

TEST(PiecewiseConstantTest, TestIntegral) {
    Vector_<Date_> knots = {Date_(2021, 3, 26), Date_(2022, 3, 26), Date_(2023, 3, 26)};
    Vector_<> right = {1.0, 2.0, 3.0};

    PiecewiseConstant_ pwc(knots, right);

    Date_ from(2022, 6, 26);
    Date_ to(2023, 6, 26);
    ASSERT_NEAR(PWC::Integral(pwc, from, to), 822, 1e-8);
}

TEST(PiecewiseConstantTest, TestFlatExtrapolationBeforeFirstKnot) {
    Vector_<Date_> knots = {Date_(2021, 3, 26), Date_(2022, 3, 26)};
    Vector_<> right = {2.0, 3.0};

    PiecewiseConstant_ pwc(knots, right);

    Date_ before(2021, 3, 16);
    bool isKnot = true;
    ASSERT_NEAR(pwc.IntegralTo(before), -20.0, 1e-8);
    ASSERT_NEAR(PWC::F(pwc, before, &isKnot), 2.0, 1e-8);
    ASSERT_FALSE(isKnot);
}

TEST(PiecewiseConstantTest, TestDiscountPwcPersistenceIsExplicitlyUnsupported) {
    const Vector_<Date_> knots = {Date_(2021, 3, 26), Date_(2022, 3, 26)};
    const Vector_<> right = {0.02, 0.03};
    const PiecewiseConstant_ forwards(knots, right);
    const std::unique_ptr<DiscountCurve_> curve(NewDiscountPWC("pwc", "USD", forwards));

    ASSERT_THROW(Splat(*curve), Exception_);
}

TEST(PiecewiseConstantTest, TestDiscountPwcFactoryMatchesTypedCurve) {
    const Vector_<Date_> knots{Date_(2021, 3, 26), Date_(2022, 3, 26), Date_(2023, 3, 26)};
    const Vector_<> right{0.01, 0.02, 0.03};
    const PiecewiseConstant_ forwards(knots, right);
    const std::unique_ptr<DiscountCurve_> factory(NewDiscountPWC("factory", "USD", forwards));
    const Tape::DiscountPWC_<double> typed("typed", "USD", knots, right);
    const Vector_<Date_> queries{Date_(2021, 3, 1), Date_(2021, 3, 26), Date_(2021, 9, 26), Date_(2022, 3, 26), Date_(2024, 3, 26)};

    ASSERT_EQ(typed.NX(), 3);
    for (const auto& from : queries)
        for (const auto& to : queries)
            ASSERT_DOUBLE_EQ((*factory)(from, to), typed(from, to));
}

TEST(PiecewiseConstantTest, TestDiscountPwcAadGradientMatchesCentralDifference) {
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);

    const Vector_<Date_> knots{Date_(2021, 3, 26), Date_(2022, 3, 26), Date_(2023, 3, 26)};
    const Vector_<> right{0.01, 0.02, 0.03};
    Vector_<AAD::Number_> activeRight(right.size());
    for (int i = 0; i < static_cast<int>(right.size()); ++i)
        AAD::RegisterIndependent(activeRight[i], right[i]);
    AAD::NewRecording(*tape);

    const Tape::DiscountPWC_<AAD::Number_> active("active", "USD", knots, activeRight);
    const Date_ from(2021, 6, 26);
    const Date_ to(2024, 3, 26);
    AAD::Number_ result = active(from, to);
    AAD::Adjoint(result) = 1.0;
    AAD::PropagateToStart(*tape);

    constexpr double bump = 1.0e-6;
    for (int column = 0; column < static_cast<int>(right.size()); ++column) {
        auto up = right;
        auto down = right;
        up[column] += bump;
        down[column] -= bump;
        const Tape::DiscountPWC_<double> curveUp("up", "USD", knots, up);
        const Tape::DiscountPWC_<double> curveDown("down", "USD", knots, down);
        const double centralDifference = (curveUp(from, to) - curveDown(from, to)) / (2.0 * bump);
        ASSERT_NEAR(AAD::AdjointValue(activeRight[column]), centralDifference, 1.0e-9) << "column=" << column;
    }

    AAD::Clear(*tape);
}

TEST(PiecewiseConstantTest, TestDiscountPwcApplyDxMatchesReconstruction) {
    const Vector_<Date_> knots{Date_(2021, 3, 26), Date_(2022, 3, 26), Date_(2023, 3, 26)};
    const Vector_<> right{0.01, 0.02, 0.03};
    const Vector_<> dx{0.001, -0.002, 0.003};
    Tape::DiscountPWC_<double> bumped("bumped", "USD", knots, right);
    bumped.ApplyDX(dx.begin(), 1.0);

    auto expectedRight = right;
    for (int i = 0; i < static_cast<int>(expectedRight.size()); ++i)
        expectedRight[i] += dx[i];
    const Tape::DiscountPWC_<double> expected("expected", "USD", knots, expectedRight);
    const Vector_<Date_> queries{Date_(2021, 3, 1), Date_(2021, 9, 26), Date_(2022, 9, 26), Date_(2024, 3, 26)};
    for (const auto& from : queries)
        for (const auto& to : queries)
            ASSERT_DOUBLE_EQ(bumped(from, to), expected(from, to));
}
