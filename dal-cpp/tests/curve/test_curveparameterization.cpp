//
// Created by dal-implementer on 2026/7/12.
//

#include <gtest/gtest.h>

#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/yclogdf.hpp>

using namespace Dal;

TEST(CurveParameterizationTest, TestLayoutsMatchStableSolverColumnOrder) {
    const Date_ anchor(2024, 1, 15);
    const Vector_<Date_> maturities{Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2025, 1, 15)};
    const DayBasis_ dayCount("ACT_365F");

    const auto logDfDefinition = MakeCurveDefinition("logdf", "USD", CurveParameterization_::Value_::LOG_DISCOUNT, LogDfScheme_::Value_::LOG_LINEAR,
                                                     maturities, anchor, dayCount);
    const auto logDfLayout = BuildCurveParameterLayout(logDfDefinition);
    ASSERT_EQ(logDfDefinition.nodeDates_.size(), 4);
    ASSERT_EQ(logDfDefinition.nodeDates_.front(), anchor);
    ASSERT_EQ(logDfLayout.storageNodeCount_, 4);
    ASSERT_EQ(logDfLayout.parameterCount_, 3);
    ASSERT_EQ(logDfLayout.paramsPerDeclaredKnot_, 1);
    ASSERT_TRUE(logDfLayout.pinnedAnchor_);

    const auto pwcDefinition = MakeCurveDefinition("pwc", "USD", CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD,
                                                   LogDfScheme_::Value_::LOG_LINEAR, maturities, anchor, dayCount);
    const auto pwcLayout = BuildCurveParameterLayout(pwcDefinition);
    ASSERT_EQ(pwcDefinition.nodeDates_, maturities);
    ASSERT_EQ(pwcLayout.storageNodeCount_, 3);
    ASSERT_EQ(pwcLayout.parameterCount_, 3);
    ASSERT_EQ(pwcLayout.paramsPerDeclaredKnot_, 1);
    ASSERT_FALSE(pwcLayout.pinnedAnchor_);

    const auto pwlDefinition = MakeCurveDefinition("pwl", "USD", CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD,
                                                   LogDfScheme_::Value_::LOG_LINEAR, maturities, anchor, dayCount);
    const auto pwlLayout = BuildCurveParameterLayout(pwlDefinition);
    ASSERT_EQ(pwlDefinition.nodeDates_, maturities);
    ASSERT_EQ(pwlLayout.storageNodeCount_, 3);
    ASSERT_EQ(pwlLayout.parameterCount_, 6);
    ASSERT_EQ(pwlLayout.paramsPerDeclaredKnot_, 2);
    ASSERT_FALSE(pwlLayout.pinnedAnchor_);
}

TEST(CurveParameterizationTest, TestTypedFactoryMatchesPublicFactories) {
    const Date_ anchor(2024, 1, 15);
    const Vector_<Date_> maturities{Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2025, 1, 15)};
    const Vector_<Date_> queries{anchor, Date_(2024, 3, 15), maturities[0], Date_(2024, 6, 1), maturities[1], Date_(2026, 1, 15)};
    const DayBasis_ dayCount("ACT_365F");

    const auto assertParity = [&queries](const DiscountCurve_& expected, const DiscountCurve_& actual) {
        for (const auto& from : queries)
            for (const auto& to : queries)
                ASSERT_DOUBLE_EQ(expected(from, to), actual(from, to));
    };

    const Vector_<> pwcParameters{0.01, 0.02, 0.03};
    const auto pwcDefinition = MakeCurveDefinition("pwc", "USD", CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD,
                                                   LogDfScheme_::Value_::LOG_LINEAR, maturities, anchor, dayCount);
    const std::unique_ptr<DiscountCurve_> publicPwc(NewDiscountPWC("pwc", "USD", PiecewiseConstant_(maturities, pwcParameters)));
    const auto typedPwc = BuildDiscountCurveT<double>(pwcDefinition, pwcParameters);
    assertParity(*publicPwc, *typedPwc);

    const Vector_<> pwlParameters{0.011, 0.012, 0.021, 0.022, 0.031, 0.032};
    Vector_<> left(maturities.size());
    Vector_<> right(maturities.size());
    for (int i = 0; i < static_cast<int>(maturities.size()); ++i) {
        left[i] = pwlParameters[2 * i];
        right[i] = pwlParameters[2 * i + 1];
    }
    const auto pwlDefinition = MakeCurveDefinition("pwl", "USD", CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD,
                                                   LogDfScheme_::Value_::LOG_LINEAR, maturities, anchor, dayCount);
    const std::unique_ptr<DiscountCurve_> publicPwl(NewDiscountPWLF("pwl", "USD", PiecewiseLinear_(maturities, left, right)));
    const auto typedPwl = BuildDiscountCurveT<double>(pwlDefinition, pwlParameters);
    assertParity(*publicPwl, *typedPwl);

    const Vector_<> logDfParameters{-0.004, -0.012, -0.03};
    const Vector_<> fullLogDf{0.0, -0.004, -0.012, -0.03};
    const Vector_<Date_> fullNodeDates{anchor, maturities[0], maturities[1], maturities[2]};
    const Vector_<LogDfScheme_> schemes{LogDfScheme_::Value_::LOG_LINEAR, LogDfScheme_::Value_::LOG_CUBIC_NATURAL, LogDfScheme_::Value_::MIXED};
    for (const auto& scheme : schemes) {
        const auto definition =
            MakeCurveDefinition("logdf", "USD", CurveParameterization_::Value_::LOG_DISCOUNT, scheme, maturities, anchor, dayCount);
        const std::unique_ptr<DiscountCurve_> publicLogDf(NewDiscountLogDF("logdf", "USD", fullNodeDates, fullLogDf, dayCount, scheme));
        const auto typedLogDf = BuildDiscountCurveT<double>(definition, logDfParameters);
        assertParity(*publicLogDf, *typedLogDf);
    }
}

TEST(CurveParameterizationTest, TestLogDfAnchorIsPrependedExactlyOnce) {
    const Date_ anchor(2024, 1, 15);
    const Vector_<Date_> withAnchor{anchor, Date_(2024, 4, 15), Date_(2024, 7, 15)};
    const auto definition = MakeCurveDefinition("logdf", "USD", CurveParameterization_::Value_::LOG_DISCOUNT, LogDfScheme_::Value_::LOG_LINEAR,
                                                withAnchor, anchor, DayBasis_("ACT_365F"));
    ASSERT_EQ(definition.nodeDates_, withAnchor);
    ASSERT_THROW(BuildDiscountCurveT<double>(definition, Vector_<>{-0.01}), Exception_);
}

TEST(CurveParameterizationTest, TestTypedFactoryPropagatesThroughActiveBase) {
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);

    const Date_ anchor(2024, 1, 15);
    const Vector_<Date_> maturities{Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2025, 1, 15)};
    const DayBasis_ dayCount("ACT_365F");
    const auto baseDefinition = MakeCurveDefinition("base", "USD", CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD,
                                                    LogDfScheme_::Value_::LOG_LINEAR, maturities, anchor, dayCount);
    const auto spreadDefinition = MakeCurveDefinition("spread", "USD", CurveParameterization_::Value_::LOG_DISCOUNT,
                                                      LogDfScheme_::Value_::LOG_CUBIC_NATURAL, maturities, anchor, dayCount);
    auto activeBaseParameters = RegisterCurveParameters(Vector_<>{0.01, 0.015, 0.02});
    auto activeSpreadParameters = RegisterCurveParameters(Vector_<>{-0.001, -0.003, -0.008});
    AAD::NewRecording(*tape);

    const auto activeBase = BuildDiscountCurveT<AAD::Number_>(baseDefinition, activeBaseParameters);
    const Handle_<Tape::DiscountCurve_<AAD::Number_>> activeBaseHandle(activeBase);
    const auto activeSpread =
        BuildDiscountCurveT<AAD::Number_, Tape::DiscountCurve_<AAD::Number_>>(spreadDefinition, activeSpreadParameters, activeBaseHandle);
    AAD::Number_ result = (*activeSpread)(anchor, Date_(2024, 10, 15));
    AAD::Adjoint(result) = 1.0;
    AAD::PropagateToStart(*tape);

    ASSERT_NE(AAD::AdjointValue(activeBaseParameters[0]), 0.0);
    ASSERT_NE(AAD::AdjointValue(activeBaseParameters[1]), 0.0);
    ASSERT_NE(AAD::AdjointValue(activeSpreadParameters[0]), 0.0);
    ASSERT_NE(AAD::AdjointValue(activeSpreadParameters[1]), 0.0);
    AAD::Clear(*tape);
}
