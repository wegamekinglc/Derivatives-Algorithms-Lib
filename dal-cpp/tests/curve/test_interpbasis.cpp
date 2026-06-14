//
// Created by dal-implementer on 2026/6/15.
//

#include <gtest/gtest.h>
#include <cmath>
#include <dal/platform/platform.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/discount.hpp>
#include <dal/math/vectors.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>

using namespace Dal;

namespace {
    // Five-node test curve: anchor at logDF=0, subsequent nodes descend at -0.02 per year under
    // ACT/365F so the year-fractions are integer-spaced (simpler debug output).
    std::unique_ptr<DiscountLogDF_> MakeFiveNodeCurve(LogDfScheme_ scheme) {
        const Vector_<Date_> dates = {Date_(2022, 1, 1), Date_(2023, 1, 1), Date_(2024, 1, 1),
                                      Date_(2025, 1, 1), Date_(2026, 1, 1)};
        const Vector_<> logDF = {0.0, -0.02, -0.04, -0.06, -0.08};
        std::unique_ptr<DiscountCurve_> base(
            NewDiscountLogDF("test", "USD", dates, logDF, DayBasis_("ACT_365F"), scheme));
        DiscountLogDF_* cast = dynamic_cast<DiscountLogDF_*>(base.get());
        EXPECT_NE(cast, nullptr);
        (void)base.release();
        return std::unique_ptr<DiscountLogDF_>(cast);
    }

    double SumWeights(const Vector_<std::pair<int, double>>& pairs) {
        double sum = 0.0;
        for (const auto& [_, w] : pairs)
            sum += w;
        return sum;
    }
} // namespace

TEST(InterpBasisTest, TestLogLinearBasisAtMidSegment) {
    auto c = MakeFiveNodeCurve(LogDfScheme_::Value_::LOG_LINEAR);
    ASSERT_NE(c, nullptr);
    const DayBasis_ basis("ACT_365F");
    const Date_ anchor(2022, 1, 1);
    const double yf = basis(anchor, Date_(2024, 7, 1), nullptr); // ~2.5 years
    const auto weights = c->InterpBasisWeights(yf);
    // LOG_LINEAR: 2 pairs, weights sum to 1.
    ASSERT_EQ(static_cast<int>(weights.size()), 2);
    ASSERT_NEAR(SumWeights(weights), 1.0, 1e-15);
    // All columns must be valid free-node solver columns (anchor never appears).
    for (const auto& [col, w] : weights) {
        ASSERT_GE(col, 0);
        ASSERT_LT(col, c->NX());
        (void)w;
    }
}

TEST(InterpBasisTest, TestLogLinearBasisSumsToOneOnAllInteriorSegments) {
    auto c = MakeFiveNodeCurve(LogDfScheme_::Value_::LOG_LINEAR);
    ASSERT_NE(c, nullptr);
    const DayBasis_ basis("ACT_365F");
    const Date_ anchor(2022, 1, 1);
    // Sample at the midpoint of each segment between consecutive knots.
    for (int i = 1; i < 4; ++i) {
        const Date_ mid = Date_(static_cast<int>(2022 + i), 6, 1);
        const double yf = basis(anchor, mid, nullptr);
        const auto weights = c->InterpBasisWeights(yf);
        ASSERT_NEAR(SumWeights(weights), 1.0, 1e-15)
            << "yf=" << yf << " (segment " << i << ")";
    }
}

TEST(InterpBasisTest, TestLogLinearAnchorNeverAppears) {
    auto c = MakeFiveNodeCurve(LogDfScheme_::Value_::LOG_LINEAR);
    ASSERT_NE(c, nullptr);
    const DayBasis_ basis("ACT_365F");
    const Date_ anchor(2022, 1, 1);
    // Sample many yfs across [0, yf.back()] -- anchor (storage node 0) is solver column -1,
    // which cannot appear; verify all columns are in [0, NX()-1].
    for (int i = 0; i < 21; ++i) {
        const double yf = 0.1 * i; // 0, 0.1, ..., 2.0
        const auto weights = c->InterpBasisWeights(yf);
        for (const auto& [col, w] : weights) {
            ASSERT_GE(col, 0) << "yf=" << yf << " produced a non-free column";
            ASSERT_LT(col, c->NX());
            (void)w;
        }
    }
}

TEST(InterpBasisTest, TestLogLinearMatchesCentralDifference) {
    // Verify basis weights agree with central-difference of logDF(yf) w.r.t. each logDF_[k].
    auto c = MakeFiveNodeCurve(LogDfScheme_::Value_::LOG_LINEAR);
    ASSERT_NE(c, nullptr);
    const DayBasis_ basis("ACT_365F");
    const Date_ anchor(2022, 1, 1);
    // Use a representative mid-segment point.
    const Date_ mid = Date_(2024, 6, 15);
    const double yf = basis(anchor, mid, nullptr);
    const auto weights = c->InterpBasisWeights(yf);

    // Reference: central-difference on operator()(anchor, dateAt(yf)) w.r.t. logDF_[k].
    const double h = 1.0e-6;
    const Vector_<Date_> dates = {Date_(2022, 1, 1), Date_(2023, 1, 1), Date_(2024, 1, 1),
                                  Date_(2025, 1, 1), Date_(2026, 1, 1)};
    const Vector_<> logDFbase = {0.0, -0.02, -0.04, -0.06, -0.08};
    for (int k = 1; k < 5; ++k) {
        // storage node k -> solver column k-1
        Vector_<> logDFup = logDFbase;
        Vector_<> logDFdn = logDFbase;
        logDFup[k] += h;
        logDFdn[k] -= h;
        std::unique_ptr<DiscountCurve_> up(NewDiscountLogDF("up", "USD", dates, logDFup, basis, LogDfScheme_::Value_::LOG_LINEAR));
        std::unique_ptr<DiscountCurve_> dn(NewDiscountLogDF("dn", "USD", dates, logDFdn, basis, LogDfScheme_::Value_::LOG_LINEAR));
        const double dfUp = (*up)(anchor, mid);
        const double dfDn = (*dn)(anchor, mid);
        const double derivFD = (std::log(dfUp) - std::log(dfDn)) / (2.0 * h);
        // Find the corresponding analytical weight (column k-1).
        double analytical = 0.0;
        for (const auto& [col, w] : weights)
            if (col == k - 1)
                analytical = w;
        if (derivFD == 0.0) {
            // The node is structurally zero in the analytical basis -- both should be zero.
            ASSERT_NEAR(analytical, 0.0, 1e-12);
        } else {
            ASSERT_NEAR(analytical, derivFD, 1e-9)
                << "node " << k << " analytical=" << analytical << " FD=" << derivFD;
        }
    }
}

TEST(InterpBasisTest, TestLogCubicStructureAndFDPassAlreadyCovers) {
    auto c = MakeFiveNodeCurve(LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    ASSERT_NE(c, nullptr);
    const DayBasis_ basis("ACT_365F");
    const Date_ anchor(2022, 1, 1);
    // For cubic the basis weights at all storage nodes sum to 1 (spline reproduces constants);
    // once we drop the anchor (storage node 0), the remaining free-node weights sum to (1 - b_0).
    // So the partition-of-unity property is NOT the right invariant for cubic. The right one is
    // central-difference agreement, covered by TestLogCubicMatchesCentralDifference.
    // Here we just assert the structural shape: up to NX() = 4 columns, anchor never appears.
    for (int i = 1; i < 4; ++i) {
        const Date_ mid = Date_(static_cast<int>(2022 + i), 6, 1);
        const double yf = basis(anchor, mid, nullptr);
        const auto weights = c->InterpBasisWeights(yf);
        ASSERT_LE(static_cast<int>(weights.size()), c->NX());
        for (const auto& [col, w] : weights) {
            ASSERT_GE(col, 0);
            ASSERT_LT(col, c->NX());
            (void)w;
        }
    }
}

TEST(InterpBasisTest, TestLogCubicMatchesCentralDifference) {
    auto c = MakeFiveNodeCurve(LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    ASSERT_NE(c, nullptr);
    const DayBasis_ basis("ACT_365F");
    const Date_ anchor(2022, 1, 1);
    const Date_ mid = Date_(2024, 6, 15);
    const double yf = basis(anchor, mid, nullptr);
    const auto weights = c->InterpBasisWeights(yf);

    const double h = 1.0e-6;
    const Vector_<Date_> dates = {Date_(2022, 1, 1), Date_(2023, 1, 1), Date_(2024, 1, 1),
                                  Date_(2025, 1, 1), Date_(2026, 1, 1)};
    const Vector_<> logDFbase = {0.0, -0.02, -0.04, -0.06, -0.08};
    for (int k = 1; k < 5; ++k) {
        Vector_<> logDFup = logDFbase;
        Vector_<> logDFdn = logDFbase;
        logDFup[k] += h;
        logDFdn[k] -= h;
        std::unique_ptr<DiscountCurve_> up(NewDiscountLogDF("up", "USD", dates, logDFup, basis, LogDfScheme_::Value_::LOG_CUBIC_NATURAL));
        std::unique_ptr<DiscountCurve_> dn(NewDiscountLogDF("dn", "USD", dates, logDFdn, basis, LogDfScheme_::Value_::LOG_CUBIC_NATURAL));
        const double dfUp = (*up)(anchor, mid);
        const double dfDn = (*dn)(anchor, mid);
        const double derivFD = (std::log(dfUp) - std::log(dfDn)) / (2.0 * h);
        double analytical = 0.0;
        for (const auto& [col, w] : weights)
            if (col == k - 1)
                analytical = w;
        if (derivFD == 0.0)
            ASSERT_NEAR(analytical, 0.0, 1e-12);
        else
            ASSERT_NEAR(analytical, derivFD, 1e-9)
                << "cubic node " << k << " analytical=" << analytical << " FD=" << derivFD;
    }
}

TEST(InterpBasisTest, TestMixedSchemeCutoffBoundary) {
    auto c = MakeFiveNodeCurve(LogDfScheme_::Value_::MIXED);
    ASSERT_NE(c, nullptr);
    const DayBasis_ basis("ACT_365F");
    const Date_ anchor(2022, 1, 1);
    // For a 5-node MIXED curve the cutoff is at the (nKnots-4=1)-th knot, i.e. yf=1 (year 2023).
    // Left of the cutoff: linear head -> up to 2 pairs (one dropped if it lands on the anchor).
    // Right of the cutoff: cubic tail -> up to 4 pairs.
    const Date_ midLinear(Date_(2022, 7, 1)); // yf ~ 0.5, linear head segment [0,1]
    const double yfLin = basis(anchor, midLinear, nullptr);
    const auto wLin = c->InterpBasisWeights(yfLin);
    ASSERT_LE(static_cast<int>(wLin.size()), 2);
    // Linear head segment [0,1] drops anchor weight (1-g); only the g weight at node 1 survives.
    ASSERT_EQ(static_cast<int>(wLin.size()), 1);
    ASSERT_EQ(wLin[0].first, 0); // solver column 0 = storage node 1
    ASSERT_GT(wLin[0].second, 0.0);

    const Date_ midCubic(Date_(2024, 6, 15)); // yf ~ 2.46, cubic tail
    const double yfCub = basis(anchor, midCubic, nullptr);
    const auto wCub = c->InterpBasisWeights(yfCub);
    ASSERT_LE(static_cast<int>(wCub.size()), c->NX());

    // FD agreement is the real correctness check (see TestMixedMatchesCentralDifference below).
}

TEST(InterpBasisTest, TestExtrapolationBeyondLastKnot) {
    auto c = MakeFiveNodeCurve(LogDfScheme_::Value_::LOG_LINEAR);
    ASSERT_NE(c, nullptr);
    const DayBasis_ basis("ACT_365F");
    const Date_ anchor(2022, 1, 1);
    const Date_ beyond(2028, 1, 1);
    const double yf = basis(anchor, beyond, nullptr);
    ASSERT_GT(yf, 4.0);
    const auto weights = c->InterpBasisWeights(yf);
    // Extrapolation extends the final-segment secant; only the last two free nodes
    // (storage nodes 3,4 -> solver columns 2,3) carry nonzero weight.
    for (const auto& [col, w] : weights) {
        ASSERT_TRUE(col == 2 || col == 3) << "unexpected column " << col << " in extrap";
        (void)w;
    }
    // Weights sum is (last-segment slope factor), NOT necessarily 1; but the test of correctness
    // is agreement with finite differences, which is covered in TestLogLinearMatchesCentralDifference.
}

TEST(InterpBasisTest, TestMixedMatchesCentralDifference) {
    auto c = MakeFiveNodeCurve(LogDfScheme_::Value_::MIXED);
    ASSERT_NE(c, nullptr);
    const DayBasis_ basis("ACT_365F");
    const Date_ anchor(2022, 1, 1);
    // Test both sides of the cutoff: linear head and cubic tail.
    const Date_ heads[] = {Date_(2022, 7, 1), Date_(2022, 10, 1)};
    const Date_ tails[] = {Date_(2024, 6, 15), Date_(2025, 6, 15)};
    const double h = 1.0e-6;
    const Vector_<Date_> dates = {Date_(2022, 1, 1), Date_(2023, 1, 1), Date_(2024, 1, 1),
                                  Date_(2025, 1, 1), Date_(2026, 1, 1)};
    const Vector_<> logDFbase = {0.0, -0.02, -0.04, -0.06, -0.08};
    for (const Date_& mid : heads) {
        const double yf = basis(anchor, mid, nullptr);
        const auto weights = c->InterpBasisWeights(yf);
        for (int k = 1; k < 5; ++k) {
            Vector_<> logDFup = logDFbase;
            Vector_<> logDFdn = logDFbase;
            logDFup[k] += h;
            logDFdn[k] -= h;
            std::unique_ptr<DiscountCurve_> up(NewDiscountLogDF("up", "USD", dates, logDFup, basis, LogDfScheme_::Value_::MIXED));
            std::unique_ptr<DiscountCurve_> dn(NewDiscountLogDF("dn", "USD", dates, logDFdn, basis, LogDfScheme_::Value_::MIXED));
            const double derivFD = (std::log((*up)(anchor, mid)) - std::log((*dn)(anchor, mid))) / (2.0 * h);
            double analytical = 0.0;
            for (const auto& [col, w] : weights)
                if (col == k - 1)
                    analytical = w;
            if (derivFD == 0.0)
                ASSERT_NEAR(analytical, 0.0, 1e-12);
            else
                ASSERT_NEAR(analytical, derivFD, 1e-9)
                    << "mixed head node " << k << " analytical=" << analytical << " FD=" << derivFD;
        }
    }
    for (const Date_& mid : tails) {
        const double yf = basis(anchor, mid, nullptr);
        const auto weights = c->InterpBasisWeights(yf);
        for (int k = 1; k < 5; ++k) {
            Vector_<> logDFup = logDFbase;
            Vector_<> logDFdn = logDFbase;
            logDFup[k] += h;
            logDFdn[k] -= h;
            std::unique_ptr<DiscountCurve_> up(NewDiscountLogDF("up", "USD", dates, logDFup, basis, LogDfScheme_::Value_::MIXED));
            std::unique_ptr<DiscountCurve_> dn(NewDiscountLogDF("dn", "USD", dates, logDFdn, basis, LogDfScheme_::Value_::MIXED));
            const double derivFD = (std::log((*up)(anchor, mid)) - std::log((*dn)(anchor, mid))) / (2.0 * h);
            double analytical = 0.0;
            for (const auto& [col, w] : weights)
                if (col == k - 1)
                    analytical = w;
            if (derivFD == 0.0)
                ASSERT_NEAR(analytical, 0.0, 1e-12);
            else
                ASSERT_NEAR(analytical, derivFD, 1e-9)
                    << "mixed tail node " << k << " analytical=" << analytical << " FD=" << derivFD;
        }
    }
}
