//
// Created by dal-implementer on 2026/7/12.
//

#include <gtest/gtest.h>

#include <algorithm>

#include <dal/curve/logdfinterp.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/math/aad/aad.hpp>
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
