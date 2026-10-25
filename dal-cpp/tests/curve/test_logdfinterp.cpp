#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include <dal/curve/logdfinterp.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/math/interp/interpmixed.hpp>

using namespace Dal;

namespace {
    Handle_<Interp1_> LegacyInterpolator(const Vector_<>& yf, const Vector_<>& logDf, LogDfScheme_ scheme) {
        switch (scheme.Switch()) {
        case LogDfScheme_::Value_::LOG_LINEAR:
            return Handle_<Interp1_>(Interp::NewLinear("linear", yf, logDf));
        case LogDfScheme_::Value_::LOG_CUBIC_NATURAL: {
            const Interp::Boundary_ natural(2, 0.0);
            return Handle_<Interp1_>(Interp::NewCubic("cubic", yf, logDf, natural, natural));
        }
        case LogDfScheme_::Value_::MIXED: {
            MixedSchemeSpec_ spec;
            spec.cutoffYf_ = yf[std::max(1, static_cast<int>(yf.size()) - 5)];
            return Handle_<Interp1_>(NewMixedLogDF("mixed", yf, logDf, spec));
        }
        default:
            THROW("Unsupported test log-DF scheme");
        }
    }

    double LegacyCurveValue(const Vector_<>& yf, const Vector_<>& logDf, LogDfScheme_ scheme, double query) {
        if (query <= yf.back())
            return (*LegacyInterpolator(yf, logDf, scheme))(query);
        const int n = static_cast<int>(yf.size());
        const double excess = (query - yf[n - 1]) / (yf[n - 1] - yf[n - 2]);
        return -excess * logDf[n - 2] + (1.0 + excess) * logDf[n - 1];
    }
} // namespace

TEST(LogDfInterpolationTest, TestAllSchemesReproduceLegacyCurvePolicy) {
    const Vector_<> yf{0.0, 0.25, 0.75, 1.5, 3.0, 5.0};
    const Vector_<> logDf{0.0, -0.004, -0.013, -0.03, -0.075, -0.14};
    const Vector_<> queries{-0.2, 0.0, 0.1, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 5.4};
    const Vector_<LogDfScheme_> schemes{LogDfScheme_::Value_::LOG_LINEAR, LogDfScheme_::Value_::LOG_CUBIC_NATURAL, LogDfScheme_::Value_::MIXED};

    for (const auto& scheme : schemes) {
        const LogDfInterpolation_ interpolation(yf, scheme);
        for (const double query : queries) {
            const double expected = LegacyCurveValue(yf, logDf, scheme, query);
            ASSERT_DOUBLE_EQ(interpolation.Evaluate(logDf, query), expected) << "scheme=" << scheme.String() << ", query=" << query;
        }
    }
}

TEST(LogDfInterpolationTest, TestDoubleKnotQueriesNeedNoScratchCapacity) {
    const Vector_<> yf{0.0, 0.25, 0.75, 1.5, 3.0, 5.0};
    const Vector_<> values{-0.0, -0.004, -0.013, -0.03, -0.075, -0.14};
    for (const LogDfScheme_ scheme : {LogDfScheme_::Value_::LOG_LINEAR, LogDfScheme_::Value_::LOG_CUBIC_NATURAL,
                                    LogDfScheme_::Value_::MIXED}) {
        const LogDfInterpolation_ interpolation(yf, scheme);
        auto nonfinite = values;
        nonfinite[2] = std::numeric_limits<double>::quiet_NaN();
        BufferCapacityBudget_ budget(0);
        const BufferCapacityScope_ scope(&budget);
        for (size_t i = 0; i < yf.size(); ++i)
            ASSERT_EQ(interpolation.Evaluate(values, yf[i]), 0.0 + values[i]);
        ASSERT_FALSE(std::signbit(interpolation.Evaluate(values, yf.front())));
        ASSERT_EQ(interpolation.Evaluate(nonfinite, yf[1]), values[1]);
        ASSERT_TRUE(std::isnan(interpolation.Evaluate(nonfinite, yf[2])));
        ASSERT_EQ(budget.PeakCapacityBytes(), 0);
    }
}

TEST(LogDfInterpolationTest, TestNaturalCubicAadCurveMatchesPrimalBeforeAnchor) {
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);

    const Date_ anchor(2022, 1, 1);
    const Date_ beforeAnchor(2021, 10, 1);
    const Vector_<Date_> nodeDates{anchor, Date_(2022, 7, 1), Date_(2023, 1, 1), Date_(2024, 1, 1)};
    const Vector_<> logDf{0.0, -0.01, -0.025, -0.06};
    Vector_<AAD::Number_> activeLogDf(logDf.size());
    for (int i = 0; i < static_cast<int>(logDf.size()); ++i)
        activeLogDf[i] = logDf[i];

    const DayBasis_ dayCount("ACT_365F");
    const LogDfScheme_ scheme(LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    const Tape::DiscountLogDF_<double> primal("primal", "USD", nodeDates, logDf, dayCount, scheme);
    const Tape::DiscountLogDF_<AAD::Number_> active("active", "USD", nodeDates, activeLogDf, dayCount, scheme);

    ASSERT_NEAR(AAD::Value(active(anchor, beforeAnchor)), primal(anchor, beforeAnchor), 1.0e-14);
    AAD::Clear(*tape);
}

TEST(LogDfInterpolationTest, TestSchemeSpecificMinimumSizesAreValidated) {
    ASSERT_THROW(LogDfInterpolation_(Vector_<>{0.0}, LogDfScheme_::Value_::LOG_LINEAR), Exception_);
    ASSERT_THROW(LogDfInterpolation_(Vector_<>{0.0, 1.0}, LogDfScheme_::Value_::LOG_CUBIC_NATURAL), Exception_);
    ASSERT_THROW(LogDfInterpolation_(Vector_<>{0.0, 1.0, 2.0}, LogDfScheme_::Value_::MIXED), Exception_);

    ASSERT_NO_THROW(LogDfInterpolation_(Vector_<>{0.0, 1.0}, LogDfScheme_::Value_::LOG_LINEAR));
    ASSERT_NO_THROW(LogDfInterpolation_(Vector_<>{0.0, 1.0, 2.0}, LogDfScheme_::Value_::LOG_CUBIC_NATURAL));
    ASSERT_NO_THROW(LogDfInterpolation_(Vector_<>{0.0, 1.0, 2.0, 3.0}, LogDfScheme_::Value_::MIXED));
}

TEST(LogDfInterpolationTest, TestApplyDxUsesUpdatedNodesWithoutRebuildingGeometry) {
    const Date_ anchor(2022, 1, 1);
    const Vector_<Date_> nodeDates{anchor, Date_(2022, 7, 1), Date_(2023, 1, 1), Date_(2024, 1, 1)};
    const Vector_<> logDf{0.0, -0.01, -0.025, -0.06};
    const Vector_<> bump{0.001, -0.002, 0.003};
    const DayBasis_ dayCount("ACT_365F");
    const LogDfScheme_ scheme(LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    Tape::DiscountLogDF_<double> bumped("bumped", "USD", nodeDates, logDf, dayCount, scheme);
    bumped.ApplyDX(bump.begin(), 1.0);

    Vector_<> expectedNodes = logDf;
    for (int i = 1; i < static_cast<int>(expectedNodes.size()); ++i)
        expectedNodes[i] += bump[i - 1];
    const Tape::DiscountLogDF_<double> expected("expected", "USD", nodeDates, expectedNodes, dayCount, scheme);
    const Vector_<Date_> queries{anchor, Date_(2022, 4, 1), Date_(2022, 10, 1), Date_(2023, 6, 1), Date_(2025, 1, 1)};
    for (const auto& query : queries)
        ASSERT_DOUBLE_EQ(bumped(anchor, query), expected(anchor, query));
}

TEST(LogDfInterpolationTest, TestCurveAnchorIntervalsPreserveLegacyValuesAndBase) {
    const Date_ anchor(2022, 1, 1);
    const Vector_<Date_> nodeDates{anchor, Date_(2022, 7, 1), Date_(2023, 1, 1), Date_(2024, 1, 1)};
    const Vector_<> logDf{0.0, -0.01, -0.025, -0.06};
    const Vector_<> baseLogDf{0.0, -0.002, -0.004, -0.007};
    const DayBasis_ dayCount("ACT_365F");
    const Handle_<DiscountCurve_> base(
        new Tape::DiscountLogDF_<double>("base", "USD", nodeDates, baseLogDf, dayCount, LogDfScheme_::Value_::LOG_LINEAR));
    const Vector_<> yf{0.0, 181.0 / 365.0, 1.0, 2.0};
    const Vector_<Date_> queries{Date_(2021, 10, 1), anchor, Date_(2022, 4, 1), nodeDates[1], Date_(2023, 7, 1), Date_(2025, 1, 1)};

    for (const LogDfScheme_ scheme : {LogDfScheme_::Value_::LOG_LINEAR, LogDfScheme_::Value_::LOG_CUBIC_NATURAL,
                                     LogDfScheme_::Value_::MIXED}) {
        const Tape::DiscountLogDF_<double> curve("curve", "USD", nodeDates, logDf, dayCount, scheme, base);
        for (const auto& from : queries) {
            for (const auto& to : queries) {
                const double t0 = dayCount(anchor, from, nullptr);
                const double t1 = dayCount(anchor, to, nullptr);
                const double own = std::exp(LegacyCurveValue(yf, logDf, scheme, t1) - LegacyCurveValue(yf, logDf, scheme, t0));
                const double baseValue = std::exp(LegacyCurveValue(yf, baseLogDf, LogDfScheme_::Value_::LOG_LINEAR, t1) -
                                                  LegacyCurveValue(yf, baseLogDf, LogDfScheme_::Value_::LOG_LINEAR, t0));
                ASSERT_NEAR(curve(from, to), own * baseValue, 1.0e-13) << "scheme=" << scheme.String();
            }
        }
    }
}

TEST(LogDfInterpolationTest, TestActiveAnchorIntervalMatchesCentralDifference) {
    const Date_ anchor(2022, 1, 1);
    const Date_ query(2022, 10, 1);
    const Vector_<Date_> nodeDates{anchor, Date_(2022, 7, 1), Date_(2023, 1, 1), Date_(2024, 1, 1)};
    const Vector_<> logDf{0.0, -0.01, -0.025, -0.06};
    const DayBasis_ dayCount("ACT_365F");
    const LogDfScheme_ scheme(LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);
    Vector_<AAD::Number_> activeLogDf(logDf.size());
    for (int i = 0; i < static_cast<int>(logDf.size()); ++i) {
        activeLogDf[i] = logDf[i];
        if (i > 0)
            AAD::PutOnTape(activeLogDf[i]);
    }
    AAD::NewRecording(*tape);
    const Tape::DiscountLogDF_<AAD::Number_> active("active", "USD", nodeDates, activeLogDf, dayCount, scheme);
    AAD::Number_ value = active(anchor, query);
    AAD::Adjoint(value) = 1.0;
    AAD::PropagateToStart(*tape);

    for (int parameter = 1; parameter < static_cast<int>(logDf.size()); ++parameter) {
        Vector_<> up = logDf, down = logDf;
        up[parameter] += 1.0e-6;
        down[parameter] -= 1.0e-6;
        const Tape::DiscountLogDF_<double> curveUp("up", "USD", nodeDates, up, dayCount, scheme);
        const Tape::DiscountLogDF_<double> curveDown("down", "USD", nodeDates, down, dayCount, scheme);
        const double expected = (curveUp(anchor, query) - curveDown(anchor, query)) / 2.0e-6;
        ASSERT_NEAR(AAD::AdjointValue(activeLogDf[parameter]), expected, 1.0e-8);
    }
    AAD::Clear(*tape);
}

TEST(LogDfInterpolationTest, TestActiveAnchorQueryNeedsNoInterpolationScratch) {
    const Date_ anchor(2022, 1, 1);
    const Vector_<Date_> nodeDates{anchor, Date_(2022, 7, 1), Date_(2023, 1, 1), Date_(2024, 1, 1)};
    const Vector_<> logDf{0.0, -0.01, -0.025, -0.06};
    const DayBasis_ dayCount("ACT_365F");
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);
    Vector_<AAD::Number_> activeLogDf(logDf.size());
    for (int i = 0; i < static_cast<int>(logDf.size()); ++i)
        activeLogDf[i] = logDf[i];
    const Tape::DiscountLogDF_<AAD::Number_> active(
        "active", "USD", nodeDates, activeLogDf, dayCount, LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    AAD::Number_ value;
    {
        BufferCapacityBudget_ budget(0);
        const BufferCapacityScope_ scope(&budget);
        value = active(anchor, anchor);
        ASSERT_EQ(budget.PeakCapacityBytes(), 0);
    }
    ASSERT_DOUBLE_EQ(AAD::Value(value), 1.0);
    AAD::Clear(*tape);
}

TEST(LogDfInterpolationTest, TestActiveNonAnchorSameDateQueryNeedsNoInterpolationScratch) {
    const Date_ anchor(2022, 1, 1);
    const Date_ query(2022, 10, 1);
    const Vector_<Date_> nodeDates{anchor, Date_(2022, 7, 1), Date_(2023, 1, 1), Date_(2024, 1, 1)};
    const Vector_<> logDf{0.0, -0.01, -0.025, -0.06};
    const DayBasis_ dayCount("ACT_365F");
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);
    Vector_<AAD::Number_> activeLogDf(logDf.size());
    for (int i = 0; i < static_cast<int>(logDf.size()); ++i)
        activeLogDf[i] = logDf[i];
    const Tape::DiscountLogDF_<AAD::Number_> active(
        "active", "USD", nodeDates, activeLogDf, dayCount, LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    AAD::Number_ value;
    {
        BufferCapacityBudget_ budget(0);
        const BufferCapacityScope_ scope(&budget);
        value = active(query, query);
        ASSERT_EQ(budget.PeakCapacityBytes(), 0);
    }
    ASSERT_DOUBLE_EQ(AAD::Value(value), 1.0);
    AAD::Clear(*tape);
}
