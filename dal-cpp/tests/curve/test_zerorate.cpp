//
// Created by dal-implementer on 2026/7/12.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>

#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/yczerorate.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/platform/platform.hpp>
#include <dal/time/daybasis.hpp>

using namespace Dal;

namespace {
    const Date_ ANCHOR(2024, 1, 15);
    const DayBasis_ ACT_365F("ACT_365F");

    Vector_<Date_> FutureDates() { return {Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2025, 1, 15), Date_(2026, 1, 15), Date_(2028, 1, 15)}; }

    Vector_<> ZeroRates() { return {0.012, 0.015, 0.019, 0.023, 0.027}; }

    Vector_<Date_> WithAnchor(const Vector_<Date_>& futureDates) {
        Vector_<Date_> result{ANCHOR};
        for (const auto& date : futureDates)
            result.push_back(date);
        return result;
    }

    Vector_<> MappedLogDf(const Vector_<Date_>& futureDates, const Vector_<>& zeroRates, const DayBasis_& dayCount) {
        Vector_<> result{0.0};
        for (int i = 0; i < static_cast<int>(futureDates.size()); ++i)
            result.push_back(-zeroRates[i] * dayCount(ANCHOR, futureDates[i], nullptr));
        return result;
    }

    std::unique_ptr<DiscountLogDF_> LogDfOracle(const Vector_<Date_>& futureDates,
                                                const Vector_<>& zeroRates,
                                                const DayBasis_& dayCount,
                                                LogDfScheme_ scheme,
                                                const Handle_<DiscountCurve_>& base = {}) {
        return std::make_unique<DiscountLogDF_>("oracle", "USD", WithAnchor(futureDates), MappedLogDf(futureDates, zeroRates, dayCount), dayCount,
                                                scheme, base);
    }

    double CentralDifference(const Vector_<Date_>& futureDates,
                             const Vector_<>& zeroRates,
                             LogDfScheme_ scheme,
                             const Date_& query,
                             int parameterIndex,
                             const Handle_<DiscountCurve_>& base = {}) {
        constexpr double bump = 1.0e-6;
        Vector_<> up = zeroRates;
        Vector_<> down = zeroRates;
        up[parameterIndex] += bump;
        down[parameterIndex] -= bump;
        const Tape::DiscountZeroRate_<double> curveUp("up", "USD", ANCHOR, futureDates, up, ACT_365F, scheme, base);
        const Tape::DiscountZeroRate_<double> curveDown("down", "USD", ANCHOR, futureDates, down, ACT_365F, scheme, base);
        return (curveUp(ANCHOR, query) - curveDown(ANCHOR, query)) / (2.0 * bump);
    }
} // namespace

TEST(ZeroRateCurveTest, TestOneNodeMapsContinuousRateAndPinsAnchor) {
    const Vector_<Date_> dates{Date_(2025, 1, 15)};
    const Vector_<> rates{0.025};
    const Tape::DiscountZeroRate_<double> curve("zero", "USD", ANCHOR, dates, rates, ACT_365F, LogDfScheme_::Value_::LOG_LINEAR);
    const double yf = ACT_365F(ANCHOR, dates.front(), nullptr);

    ASSERT_DOUBLE_EQ(curve(ANCHOR, ANCHOR), 1.0);
    ASSERT_NEAR(curve(ANCHOR, dates.front()), std::exp(-rates.front() * yf), 1.0e-15);
    ASSERT_EQ(curve.AnchorDate(), ANCHOR);
    ASSERT_EQ(curve.NodeDates(), dates);
    ASSERT_EQ(curve.NodeZeroRates(), rates);
    ASSERT_EQ(curve.DayCount().String(), String_("ACT_365F"));
    ASSERT_EQ(curve.Scheme(), LogDfScheme_(LogDfScheme_::Value_::LOG_LINEAR));
    ASSERT_EQ(curve.NX(), 1);
}

TEST(ZeroRateCurveTest, TestDayCountControlsMappedLogDiscountFactor) {
    const Vector_<Date_> dates{Date_(2025, 1, 15)};
    const Vector_<> rates{0.025};
    const Tape::DiscountZeroRate_<double> act365("act365", "USD", ANCHOR, dates, rates, DayBasis_("ACT_365F"), LogDfScheme_::Value_::LOG_LINEAR);
    const Tape::DiscountZeroRate_<double> act360("act360", "USD", ANCHOR, dates, rates, DayBasis_("ACT_360"), LogDfScheme_::Value_::LOG_LINEAR);

    ASSERT_NEAR(act365(ANCHOR, dates.front()), std::exp(-rates.front() * DayBasis_("ACT_365F")(ANCHOR, dates.front(), nullptr)), 1.0e-15);
    ASSERT_NEAR(act360(ANCHOR, dates.front()), std::exp(-rates.front() * DayBasis_("ACT_360")(ANCHOR, dates.front(), nullptr)), 1.0e-15);
    ASSERT_NE(act365(ANCHOR, dates.front()), act360(ANCHOR, dates.front()));
}

TEST(ZeroRateCurveTest, TestZeroNegativeAndArbitraryIntervals) {
    const Vector_<Date_> dates{Date_(2024, 7, 15), Date_(2025, 1, 15), Date_(2026, 1, 15)};
    const Vector_<> rates{0.0, -0.004, 0.01};
    const Tape::DiscountZeroRate_<double> curve("zero", "USD", ANCHOR, dates, rates, ACT_365F, LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    const Date_ from(2024, 5, 15);
    const Date_ to(2025, 5, 15);

    ASSERT_DOUBLE_EQ(curve(from, from), 1.0);
    ASSERT_NEAR(curve(from, to) * curve(to, from), 1.0, 1.0e-14);
    ASSERT_GT(curve(ANCHOR, dates[1]), 1.0);
    ASSERT_GT(curve(from, to), 0.0);
}

TEST(ZeroRateCurveTest, TestAllSchemesMatchMappedLogDfOracleIncludingExtrapolation) {
    const auto dates = FutureDates();
    const auto rates = ZeroRates();
    const Vector_<Date_> queries{Date_(2023, 10, 15), ANCHOR,       Date_(2024, 3, 1), dates[0], Date_(2024, 10, 15), dates[2],
                                 Date_(2027, 1, 15),  dates.back(), Date_(2030, 1, 15)};
    const Vector_<LogDfScheme_> schemes{LogDfScheme_::Value_::LOG_LINEAR, LogDfScheme_::Value_::LOG_CUBIC_NATURAL, LogDfScheme_::Value_::MIXED};

    for (const auto& scheme : schemes) {
        const Tape::DiscountZeroRate_<double> curve("zero", "USD", ANCHOR, dates, rates, ACT_365F, scheme);
        const auto oracle = LogDfOracle(dates, rates, ACT_365F, scheme);
        for (const auto& from : queries) {
            for (const auto& to : queries)
                ASSERT_NEAR(curve(from, to), (*oracle)(from, to), 1.0e-14) << "scheme=" << scheme.String();
        }
    }
}

TEST(ZeroRateCurveTest, TestSchemeSpecificMinimumFutureNodes) {
    const Vector_<Date_> one{Date_(2025, 1, 15)};
    const Vector_<Date_> two{Date_(2025, 1, 15), Date_(2026, 1, 15)};
    const Vector_<Date_> three{Date_(2025, 1, 15), Date_(2026, 1, 15), Date_(2027, 1, 15)};

    ASSERT_NO_THROW(Tape::DiscountZeroRate_<double>("linear", "USD", ANCHOR, one, Vector_<>{0.02}, ACT_365F, LogDfScheme_::Value_::LOG_LINEAR));
    ASSERT_THROW(Tape::DiscountZeroRate_<double>("cubic", "USD", ANCHOR, one, Vector_<>{0.02}, ACT_365F, LogDfScheme_::Value_::LOG_CUBIC_NATURAL),
                 Exception_);
    ASSERT_NO_THROW(
        Tape::DiscountZeroRate_<double>("cubic", "USD", ANCHOR, two, Vector_<>{0.02, 0.021}, ACT_365F, LogDfScheme_::Value_::LOG_CUBIC_NATURAL));
    ASSERT_THROW(Tape::DiscountZeroRate_<double>("mixed", "USD", ANCHOR, two, Vector_<>{0.02, 0.021}, ACT_365F, LogDfScheme_::Value_::MIXED),
                 Exception_);
    ASSERT_NO_THROW(
        Tape::DiscountZeroRate_<double>("mixed", "USD", ANCHOR, three, Vector_<>{0.02, 0.021, 0.022}, ACT_365F, LogDfScheme_::Value_::MIXED));
}

TEST(ZeroRateCurveTest, TestContextFreeDayCountAndGeometryValidation) {
    const Vector_<Date_> dates{Date_(2025, 1, 15), Date_(2026, 1, 15)};
    const Vector_<> rates{0.02, 0.021};

    ASSERT_THROW(Tape::DiscountZeroRate_<double>("act365l", "USD", ANCHOR, dates, rates, DayBasis_("ACT_365L"), LogDfScheme_::Value_::LOG_LINEAR),
                 Exception_);
    ASSERT_NO_THROW(Tape::DiscountZeroRate_<double>("actact", "USD", ANCHOR, dates, rates, DayBasis_("ACT_ACT"), LogDfScheme_::Value_::LOG_LINEAR));
    ASSERT_NO_THROW(Tape::DiscountZeroRate_<double>("bond", "USD", ANCHOR, dates, rates, DayBasis_("BOND"), LogDfScheme_::Value_::LOG_LINEAR));
    ASSERT_THROW(Tape::DiscountZeroRate_<double>("bad_bond", "USD", Date_(2024, 1, 31), Vector_<Date_>{Date_(2024, 2, 1)}, Vector_<>{0.02},
                                                 DayBasis_("BOND"), LogDfScheme_::Value_::LOG_LINEAR),
                 Exception_);
}

TEST(ZeroRateCurveTest, TestInvalidInputsAreRejected) {
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto inf = std::numeric_limits<double>::infinity();

    ASSERT_THROW(Tape::DiscountZeroRate_<double>("empty", "USD", ANCHOR, {}, {}, ACT_365F, LogDfScheme_::Value_::LOG_LINEAR), Exception_);
    ASSERT_THROW(
        Tape::DiscountZeroRate_<double>("mismatch", "USD", ANCHOR, FutureDates(), Vector_<>{0.01}, ACT_365F, LogDfScheme_::Value_::LOG_LINEAR),
        Exception_);
    ASSERT_THROW(Tape::DiscountZeroRate_<double>("unordered", "USD", ANCHOR, Vector_<Date_>{Date_(2025, 1, 15), Date_(2024, 7, 15)},
                                                 Vector_<>{0.01, 0.02}, ACT_365F, LogDfScheme_::Value_::LOG_LINEAR),
                 Exception_);
    ASSERT_THROW(Tape::DiscountZeroRate_<double>("duplicate", "USD", ANCHOR, Vector_<Date_>{Date_(2025, 1, 15), Date_(2025, 1, 15)},
                                                 Vector_<>{0.01, 0.02}, ACT_365F, LogDfScheme_::Value_::LOG_LINEAR),
                 Exception_);
    ASSERT_THROW(
        Tape::DiscountZeroRate_<double>("anchor", "USD", ANCHOR, Vector_<Date_>{ANCHOR}, Vector_<>{0.01}, ACT_365F, LogDfScheme_::Value_::LOG_LINEAR),
        Exception_);
    ASSERT_THROW(Tape::DiscountZeroRate_<double>("before", "USD", ANCHOR, Vector_<Date_>{Date_(2024, 1, 14)}, Vector_<>{0.01}, ACT_365F,
                                                 LogDfScheme_::Value_::LOG_LINEAR),
                 Exception_);
    ASSERT_THROW(Tape::DiscountZeroRate_<double>("nan", "USD", ANCHOR, Vector_<Date_>{Date_(2025, 1, 15)}, Vector_<>{nan}, ACT_365F,
                                                 LogDfScheme_::Value_::LOG_LINEAR),
                 Exception_);
    ASSERT_THROW(Tape::DiscountZeroRate_<double>("inf", "USD", ANCHOR, Vector_<Date_>{Date_(2025, 1, 15)}, Vector_<>{inf}, ACT_365F,
                                                 LogDfScheme_::Value_::LOG_LINEAR),
                 Exception_);
}

TEST(ZeroRateCurveTest, TestApplyDxUsesZeroRateCoordinatesAndIsReversible) {
    const auto dates = FutureDates();
    const auto rates = ZeroRates();
    const Vector_<> bump{0.001, -0.002, 0.003, -0.004, 0.005};
    Tape::DiscountZeroRate_<double> curve("zero", "USD", ANCHOR, dates, rates, ACT_365F, LogDfScheme_::Value_::MIXED);
    const Vector_<Date_> queries{ANCHOR, Date_(2024, 10, 15), Date_(2027, 1, 15), Date_(2030, 1, 15)};
    Vector_<> originalValues;
    for (const auto& query : queries)
        originalValues.push_back(curve(ANCHOR, query));

    curve.ApplyDX(bump.begin(), 1.0);
    Vector_<> expectedRates = rates;
    for (int i = 0; i < static_cast<int>(rates.size()); ++i)
        expectedRates[i] += bump[i];
    ASSERT_EQ(curve.NodeZeroRates(), expectedRates);
    const Tape::DiscountZeroRate_<double> expected("expected", "USD", ANCHOR, dates, expectedRates, ACT_365F, LogDfScheme_::Value_::MIXED);
    for (const auto& query : queries)
        ASSERT_DOUBLE_EQ(curve(ANCHOR, query), expected(ANCHOR, query));

    curve.ApplyDX(bump.begin(), -1.0);
    ASSERT_EQ(curve.NodeZeroRates(), rates);
    for (int i = 0; i < static_cast<int>(queries.size()); ++i)
        ASSERT_NEAR(curve(ANCHOR, queries[i]), originalValues[i], 1.0e-15);
}

TEST(ZeroRateCurveTest, TestFactoryPassiveBaseAndClonePreserveCurveBehavior) {
    const auto dates = FutureDates();
    const auto rates = ZeroRates();
    const Handle_<DiscountCurve_> base(NewDiscountPWC("base", "USD", PiecewiseConstant_(Vector_<Date_>{ANCHOR}, Vector_<>{0.01})));
    std::unique_ptr<DiscountCurve_> publicCurve(
        NewDiscountZeroRate("zero", "USD", ANCHOR, dates, rates, ACT_365F, LogDfScheme_::Value_::MIXED, base));
    const auto* typed = dynamic_cast<const DiscountZeroRate_*>(publicCurve.get());
    ASSERT_NE(typed, nullptr);
    const auto zeroOnly = std::make_unique<DiscountZeroRate_>("zero_only", "USD", ANCHOR, dates, rates, ACT_365F, LogDfScheme_::Value_::MIXED);
    const Date_ query(2027, 1, 15);
    ASSERT_NEAR((*publicCurve)(ANCHOR, query), (*zeroOnly)(ANCHOR, query) * (*base)(ANCHOR, query), 1.0e-14);

    std::unique_ptr<YCComponent_> cloned(typed->Clone("clone", {}));
    const auto* clonedTyped = dynamic_cast<const DiscountZeroRate_*>(cloned.get());
    ASSERT_NE(clonedTyped, nullptr);
    ASSERT_EQ(clonedTyped->NodeZeroRates(), rates);
    ASSERT_NEAR((*clonedTyped)(ANCHOR, query), (*typed)(ANCHOR, query), 1.0e-15);

    const Handle_<DiscountCurve_> replacementBase(
        NewDiscountPWC("replacement_base", "USD", PiecewiseConstant_(Vector_<Date_>{ANCHOR}, Vector_<>{0.015})));
    YCComponent_::substitutions_t substitutions;
    substitutions[base.get()] = handle_cast<YCComponent_>(replacementBase);
    std::unique_ptr<YCComponent_> substituted(typed->Clone("substituted", substitutions));
    const auto* substitutedTyped = dynamic_cast<const DiscountZeroRate_*>(substituted.get());
    ASSERT_NE(substitutedTyped, nullptr);
    ASSERT_NEAR((*substitutedTyped)(ANCHOR, query), (*zeroOnly)(ANCHOR, query) * (*replacementBase)(ANCHOR, query), 1.0e-14);
}

TEST(ZeroRateCurveTest, TestActiveNodeAndOffNodeAdjointsMatchMappedChainRule) {
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);

    const Vector_<Date_> dates{Date_(2024, 7, 15), Date_(2025, 1, 15), Date_(2026, 1, 15)};
    const Vector_<> rates{0.012, 0.018, 0.024};
    Vector_<AAD::Number_> activeRates(rates.size());
    for (int i = 0; i < static_cast<int>(rates.size()); ++i) {
        activeRates[i] = rates[i];
        AAD::PutOnTape(activeRates[i]);
    }
    AAD::NewRecording(*tape);

    const Tape::DiscountZeroRate_<AAD::Number_> curve("active", "USD", ANCHOR, dates, activeRates, ACT_365F, LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    const AAD::Number_ nodeDf = curve(ANCHOR, dates[1]);
    AAD::Adjoint(nodeDf) = 1.0;
    AAD::PropagateToStart(*tape);
    const double nodeTime = ACT_365F(ANCHOR, dates[1], nullptr);
    ASSERT_NEAR(AAD::AdjointValue(activeRates[0]), 0.0, 1.0e-14);
    ASSERT_NEAR(AAD::AdjointValue(activeRates[1]), -nodeTime * AAD::Value(nodeDf), 1.0e-13);
    ASSERT_NEAR(AAD::AdjointValue(activeRates[2]), 0.0, 1.0e-14);
    AAD::Clear(*tape);

    for (int parameterIndex = 0; parameterIndex < static_cast<int>(rates.size()); ++parameterIndex) {
        AAD::Clear(*tape);
        Vector_<AAD::Number_> offNodeRates(rates.size());
        for (int i = 0; i < static_cast<int>(rates.size()); ++i) {
            offNodeRates[i] = rates[i];
            AAD::PutOnTape(offNodeRates[i]);
        }
        AAD::NewRecording(*tape);
        const Tape::DiscountZeroRate_<AAD::Number_> offNodeCurve("active", "USD", ANCHOR, dates, offNodeRates, ACT_365F,
                                                                 LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
        const Date_ query(2025, 7, 15);
        AAD::Number_ value = offNodeCurve(ANCHOR, query);
        AAD::Adjoint(value) = 1.0;
        AAD::PropagateToStart(*tape);
        const double expected = CentralDifference(dates, rates, LogDfScheme_::Value_::LOG_CUBIC_NATURAL, query, parameterIndex);
        ASSERT_NEAR(AAD::AdjointValue(offNodeRates[parameterIndex]), expected, 1.0e-8);
    }
    AAD::Clear(*tape);
}

TEST(ZeroRateCurveTest, TestActiveCurveWithPassiveAndActiveBasesPropagatesSensitivities) {
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);

    const Vector_<Date_> dates{Date_(2024, 7, 15), Date_(2025, 1, 15), Date_(2026, 1, 15)};
    const Vector_<> rates{0.012, 0.018, 0.024};
    const Handle_<DiscountCurve_> passiveBase(NewDiscountPWC("passive_base", "USD", PiecewiseConstant_(Vector_<Date_>{ANCHOR}, Vector_<>{0.01})));
    Vector_<AAD::Number_> activeRates(rates.size());
    for (int i = 0; i < static_cast<int>(rates.size()); ++i) {
        activeRates[i] = rates[i];
        AAD::PutOnTape(activeRates[i]);
    }
    AAD::NewRecording(*tape);
    const Tape::DiscountZeroRate_<AAD::Number_> passiveBaseCurve("spread", "USD", ANCHOR, dates, activeRates, ACT_365F,
                                                                 LogDfScheme_::Value_::LOG_CUBIC_NATURAL, passiveBase);
    AAD::Number_ passiveBaseValue = passiveBaseCurve(ANCHOR, Date_(2025, 7, 15));
    AAD::Adjoint(passiveBaseValue) = 1.0;
    AAD::PropagateToStart(*tape);
    ASSERT_NE(AAD::AdjointValue(activeRates[1]), 0.0);
    AAD::Clear(*tape);

    Vector_<AAD::Number_> activeBaseRates{AAD::Number_(0.01)};
    Vector_<AAD::Number_> activeSpreadRates(rates.size());
    AAD::PutOnTape(activeBaseRates[0]);
    for (int i = 0; i < static_cast<int>(rates.size()); ++i) {
        activeSpreadRates[i] = rates[i];
        AAD::PutOnTape(activeSpreadRates[i]);
    }
    AAD::NewRecording(*tape);
    const auto activeBase = std::make_shared<Tape::DiscountPWC_<AAD::Number_>>("active_base", "USD", Vector_<Date_>{ANCHOR}, activeBaseRates);
    const Handle_<Tape::DiscountCurve_<AAD::Number_>> activeBaseHandle(activeBase);
    const Tape::DiscountZeroRate_<AAD::Number_, Tape::DiscountCurve_<AAD::Number_>> activeBaseCurve(
        "spread", "USD", ANCHOR, dates, activeSpreadRates, ACT_365F, LogDfScheme_::Value_::LOG_CUBIC_NATURAL, activeBaseHandle);
    AAD::Number_ activeBaseValue = activeBaseCurve(ANCHOR, Date_(2025, 7, 15));
    AAD::Adjoint(activeBaseValue) = 1.0;
    AAD::PropagateToStart(*tape);

    ASSERT_NE(AAD::AdjointValue(activeBaseRates[0]), 0.0);
    ASSERT_NE(AAD::AdjointValue(activeSpreadRates[0]), 0.0);
    ASSERT_NE(AAD::AdjointValue(activeSpreadRates[1]), 0.0);
    ASSERT_NE(AAD::AdjointValue(activeSpreadRates[2]), 0.0);
    AAD::Clear(*tape);
}
