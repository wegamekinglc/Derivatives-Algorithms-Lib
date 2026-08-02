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
#include <dal/curve/yczerorate.hpp>

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

TEST(CurveParameterizationTest, TestPiecewiseConstantDefinitionAllowsAnchorBoundaryOnly) {
    const Date_ anchor(2024, 1, 15);
    const Date_ maturity(2025, 1, 15);
    const DayBasis_ dayCount("ACT_365F");

    const auto definition = MakeCurveDefinition("pwc", "USD", CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD,
                                                LogDfScheme_::Value_::LOG_LINEAR, Vector_<Date_>{anchor, maturity}, anchor, dayCount);

    ASSERT_EQ(definition.nodeDates_, Vector_<Date_>({anchor, maturity}));
    ASSERT_THROW(MakeCurveDefinition("pwc", "USD", CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, LogDfScheme_::Value_::LOG_LINEAR,
                                     Vector_<Date_>{anchor.AddDays(-1), maturity}, anchor, dayCount),
                 Exception_);
}

TEST(CurveParameterizationTest, TestZeroRateDefinitionPrependsAnchorAndPreservesGeometry) {
    const Date_ anchor(2024, 1, 15);
    const Vector_<Date_> maturities{Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2025, 1, 15)};
    const DayBasis_ dayCount("ACT_360");

    const auto definition = MakeCurveDefinition("zero", "USD", CurveParameterization_::Value_::ZERO_RATE, LogDfScheme_::Value_::LOG_CUBIC_NATURAL,
                                                maturities, anchor, dayCount);
    const auto layout = BuildCurveParameterLayout(definition);

    ASSERT_EQ(definition.anchorDate_, anchor);
    ASSERT_EQ(definition.nodeDates_, Vector_<Date_>({anchor, maturities[0], maturities[1], maturities[2]}));
    ASSERT_EQ(definition.dayCount_.String(), String_("ACT_360"));
    ASSERT_EQ(layout.storageNodeCount_, 4);
    ASSERT_EQ(layout.parameterCount_, 3);
    ASSERT_EQ(layout.paramsPerDeclaredKnot_, 1);
    ASSERT_TRUE(layout.pinnedAnchor_);
}

TEST(CurveParameterizationTest, TestCurveDefinitionPreservesLegacyAggregateInitializerOrder) {
    const Vector_<Date_> nodeDates{Date_(2024, 1, 15), Date_(2025, 1, 15)};
    const DayBasis_ dayCount("ACT_365F");
    const CurveDefinition_ definition{"legacy",  "USD",   CurveParameterization_::Value_::LOG_DISCOUNT, LogDfScheme_::Value_::LOG_LINEAR,
                                      nodeDates, dayCount};

    ASSERT_EQ(definition.nodeDates_, nodeDates);
    ASSERT_EQ(definition.dayCount_.String(), String_("ACT_365F"));
    ASSERT_EQ(definition.anchorDate_, Date_());
}

TEST(CurveParameterizationTest, TestZeroRateLayoutRejectsMalformedManualDefinitions) {
    const Date_ anchor(2024, 1, 15);
    const Vector_<Date_> maturities{Date_(2024, 7, 15), Date_(2025, 1, 15), Date_(2026, 1, 15)};
    const DayBasis_ dayCount("ACT_365F");
    const auto valid =
        MakeCurveDefinition("zero", "USD", CurveParameterization_::Value_::ZERO_RATE, LogDfScheme_::Value_::LOG_LINEAR, maturities, anchor, dayCount);

    auto anchorMismatch = valid;
    anchorMismatch.anchorDate_ = Date_(2024, 1, 16);
    ASSERT_THROW(BuildCurveParameterLayout(anchorMismatch), Exception_);

    auto missingFutureNode = valid;
    missingFutureNode.nodeDates_ = Vector_<Date_>{anchor};
    ASSERT_THROW(BuildCurveParameterLayout(missingFutureNode), Exception_);

    auto nonMonotonic = valid;
    nonMonotonic.nodeDates_[2] = nonMonotonic.nodeDates_[1];
    ASSERT_THROW(BuildDiscountCurveT<double>(nonMonotonic, Vector_<>{0.01, 0.02, 0.03}), Exception_);
}

TEST(CurveParameterizationTest, TestZeroRateDefinitionRejectsNonFutureAndDuplicateKnots) {
    const Date_ anchor(2024, 1, 15);
    const DayBasis_ dayCount("ACT_365F");

    ASSERT_THROW(MakeCurveDefinition("zero", "USD", CurveParameterization_::Value_::ZERO_RATE, LogDfScheme_::Value_::LOG_LINEAR,
                                     Vector_<Date_>{anchor, Date_(2024, 7, 15)}, anchor, dayCount),
                 Exception_);
    ASSERT_THROW(MakeCurveDefinition("zero", "USD", CurveParameterization_::Value_::ZERO_RATE, LogDfScheme_::Value_::LOG_LINEAR,
                                     Vector_<Date_>{Date_(2024, 1, 14), Date_(2024, 7, 15)}, anchor, dayCount),
                 Exception_);
    ASSERT_THROW(MakeCurveDefinition("zero", "USD", CurveParameterization_::Value_::ZERO_RATE, LogDfScheme_::Value_::LOG_LINEAR,
                                     Vector_<Date_>{Date_(2024, 7, 15), Date_(2024, 7, 15)}, anchor, dayCount),
                 Exception_);
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

TEST(CurveParameterizationTest, TestZeroRateTypedFactoryMatchesDirectFactoryForEveryScheme) {
    const Date_ anchor(2024, 1, 15);
    const Vector_<Date_> maturities{Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2025, 1, 15), Date_(2026, 1, 15), Date_(2028, 1, 15)};
    const Vector_<> rates{0.012, 0.015, 0.019, 0.023, 0.027};
    const Vector_<Date_> queries{Date_(2023, 10, 15), anchor, Date_(2024, 3, 1), Date_(2024, 10, 15), Date_(2027, 1, 15), Date_(2030, 1, 15)};
    const DayBasis_ dayCount("ACT_365F");
    const Handle_<DiscountCurve_> base(NewDiscountPWC("base", "USD", PiecewiseConstant_(Vector_<Date_>{anchor}, Vector_<>{0.01})));
    const Vector_<LogDfScheme_> schemes{LogDfScheme_::Value_::LOG_LINEAR, LogDfScheme_::Value_::LOG_CUBIC_NATURAL, LogDfScheme_::Value_::MIXED};

    for (const auto& scheme : schemes) {
        const auto definition = MakeCurveDefinition("zero", "USD", CurveParameterization_::Value_::ZERO_RATE, scheme, maturities, anchor, dayCount);
        std::unique_ptr<DiscountCurve_> direct(NewDiscountZeroRate("zero", "USD", anchor, maturities, rates, dayCount, scheme, base));
        const auto typed = BuildDiscountCurveT<double>(definition, rates, base);

        const auto* typedZeroRate = dynamic_cast<const DiscountZeroRate_*>(typed.get());
        ASSERT_NE(typedZeroRate, nullptr);
        ASSERT_EQ(typedZeroRate->NodeDates(), maturities);
        ASSERT_EQ(typedZeroRate->NodeZeroRates(), rates);
        for (const auto& from : queries)
            for (const auto& to : queries)
                ASSERT_NEAR((*typed)(from, to), (*direct)(from, to), 1.0e-14) << "scheme=" << scheme.String();
    }

    const auto definition =
        MakeCurveDefinition("zero", "USD", CurveParameterization_::Value_::ZERO_RATE, LogDfScheme_::Value_::LOG_LINEAR, maturities, anchor, dayCount);
    ASSERT_THROW(BuildDiscountCurveT<double>(definition, Vector_<>{0.01}), Exception_);
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

TEST(CurveParameterizationTest, TestZeroRateTypedFactoryPropagatesMappedAadDerivatives) {
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);

    const Date_ anchor(2024, 1, 15);
    const Vector_<Date_> maturities{Date_(2024, 7, 15), Date_(2025, 1, 15), Date_(2026, 1, 15)};
    const Vector_<> rates{0.012, 0.018, 0.024};
    const DayBasis_ dayCount("ACT_365F");
    const auto definition = MakeCurveDefinition("zero", "USD", CurveParameterization_::Value_::ZERO_RATE, LogDfScheme_::Value_::LOG_CUBIC_NATURAL,
                                                maturities, anchor, dayCount);
    auto activeRates = RegisterCurveParameters(rates);
    AAD::NewRecording(*tape);

    const auto curve = BuildDiscountCurveT<AAD::Number_>(definition, activeRates);
    AAD::Number_ nodeDf = (*curve)(anchor, maturities[1]);
    AAD::Adjoint(nodeDf) = 1.0;
    AAD::PropagateToStart(*tape);

    const double nodeTime = dayCount(anchor, maturities[1], nullptr);
    ASSERT_NEAR(AAD::AdjointValue(activeRates[0]), 0.0, 1.0e-14);
    ASSERT_NEAR(AAD::AdjointValue(activeRates[1]), -nodeTime * AAD::Value(nodeDf), 1.0e-13);
    ASSERT_NEAR(AAD::AdjointValue(activeRates[2]), 0.0, 1.0e-14);
    AAD::Clear(*tape);

    auto offNodeRates = RegisterCurveParameters(rates);
    AAD::NewRecording(*tape);
    const auto offNodeCurve = BuildDiscountCurveT<AAD::Number_>(definition, offNodeRates);
    const Date_ query(2025, 7, 15);
    AAD::Number_ offNodeDf = (*offNodeCurve)(anchor, query);
    AAD::Adjoint(offNodeDf) = 1.0;
    AAD::PropagateToStart(*tape);

    Vector_<> geometry{0.0};
    for (const auto& maturity : maturities)
        geometry.push_back(dayCount(anchor, maturity, nullptr));
    const LogDfInterpolation_ interpolation(geometry, LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    Vector_<> expected(rates.size(), 0.0);
    for (const auto& [index, weight] : interpolation.WeightsAt(dayCount(anchor, query, nullptr))) {
        if (index > 0)
            expected[index - 1] -= AAD::Value(offNodeDf) * weight * geometry[index];
    }
    for (int i = 0; i < static_cast<int>(rates.size()); ++i)
        ASSERT_NEAR(AAD::AdjointValue(offNodeRates[i]), expected[i], 1.0e-13);
    AAD::Clear(*tape);
}

TEST(CurveParameterizationTest, TestZeroRateTypedFactoryPropagatesThroughActiveBase) {
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);

    const Date_ anchor(2024, 1, 15);
    const Vector_<Date_> maturities{Date_(2024, 7, 15), Date_(2025, 1, 15), Date_(2026, 1, 15)};
    const DayBasis_ dayCount("ACT_365F");
    const auto baseDefinition = MakeCurveDefinition("base", "USD", CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD,
                                                    LogDfScheme_::Value_::LOG_LINEAR, maturities, anchor, dayCount);
    const auto spreadDefinition = MakeCurveDefinition("spread", "USD", CurveParameterization_::Value_::ZERO_RATE,
                                                      LogDfScheme_::Value_::LOG_CUBIC_NATURAL, maturities, anchor, dayCount);
    auto activeBaseParameters = RegisterCurveParameters(Vector_<>{0.01, 0.015, 0.02});
    auto activeSpreadParameters = RegisterCurveParameters(Vector_<>{0.012, 0.018, 0.024});
    AAD::NewRecording(*tape);

    const auto activeBase = BuildDiscountCurveT<AAD::Number_>(baseDefinition, activeBaseParameters);
    const Handle_<Tape::DiscountCurve_<AAD::Number_>> activeBaseHandle(activeBase);
    const auto activeSpread =
        BuildDiscountCurveT<AAD::Number_, Tape::DiscountCurve_<AAD::Number_>>(spreadDefinition, activeSpreadParameters, activeBaseHandle);
    AAD::Number_ result = (*activeSpread)(anchor, Date_(2025, 7, 15));
    AAD::Adjoint(result) = 1.0;
    AAD::PropagateToStart(*tape);

    ASSERT_NE(AAD::AdjointValue(activeBaseParameters[0]), 0.0);
    ASSERT_NE(AAD::AdjointValue(activeBaseParameters[1]), 0.0);
    ASSERT_NE(AAD::AdjointValue(activeSpreadParameters[0]), 0.0);
    ASSERT_NE(AAD::AdjointValue(activeSpreadParameters[1]), 0.0);
    ASSERT_NE(AAD::AdjointValue(activeSpreadParameters[2]), 0.0);
    AAD::Clear(*tape);
}
