//
// Created by dal-tester on 2026/6/14.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/storage/json.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

namespace {
    RateLegConvention_ AnnualLeg() {
        RateLegConvention_ leg;
        leg.paymentLag_ = 0;
        leg.paymentFrequency_ = PeriodLength_("12M");
        leg.dayBasis_ = DayBasis_("ACT_365F");
        leg.accrualHolidays_ = Holidays::None();
        leg.paymentHolidays_ = Holidays::None();
        // Calendar="all" means every day is a business day. DAL's Holidays::None() still
        // treats weekends as non-business, so use Unadjusted to avoid rolling IMM/stub dates.
        leg.businessDayConvention_ = BizDayConvention_("Unadjusted");
        leg.paymentConvention_ = BizDayConvention_("Unadjusted");
        return leg;
    }

    RateIndexConvention_ AnnualIndex() {
        RateIndexConvention_ idx;
        idx.forecastTenor_ = PeriodLength_("12M");
        idx.dayBasis_ = DayBasis_("ACT_365F");
        idx.fixingLag_ = 0;
        idx.spotLag_ = 0;
        idx.fixingHolidays_ = Holidays::None();
        idx.accrualHolidays_ = Holidays::None();
        idx.businessDayConvention_ = BizDayConvention_("Unadjusted");
        idx.useProjectionCurve_ = false;
        return idx;
    }

    Vector_<Date_> PtirdsKnotDates() {
        return {
            Date_(2022, 1, 1),
            Date_(2022, 3, 15), Date_(2022, 6, 15), Date_(2022, 9, 21), Date_(2022, 12, 21),
            Date_(2023, 3, 15), Date_(2023, 6, 21), Date_(2023, 9, 20), Date_(2023, 12, 20),
            Date_(2024, 3, 15),
            Date_(2025, 1, 1), Date_(2027, 1, 1), Date_(2029, 1, 1), Date_(2032, 1, 1),
        };
    }

    // 13 instruments from rateslib Table 6.2: 9 single-period FRAs (the short end) plus
    // 4 annual-roll vanilla swaps (3y, 5y, 7y, 10y).
    Vector_<Handle_<YCInstrument_>> PtirdsInstruments(const Date_& today) {
        const auto fixedLeg = AnnualLeg();
        const auto floatIdx = AnnualIndex();
        const auto floatLeg = AnnualLeg();
        const auto mk = [&](const Date_& start, const Date_& end, double parPct) {
            return Handle_<YCInstrument_>(
                new Swap_(today, start, end, parPct / 100.0, fixedLeg, floatIdx, floatLeg));
        };
        Vector_<Handle_<YCInstrument_>> v;
        v.reserve(13);
        v.push_back(mk(Date_(2022, 1, 1), Date_(2022, 1, 3), 1.00));   // 1-business-day
        v.push_back(mk(Date_(2022, 3, 15), Date_(2022, 6, 15), 1.05));
        v.push_back(mk(Date_(2022, 6, 15), Date_(2022, 9, 21), 1.12));
        v.push_back(mk(Date_(2022, 9, 21), Date_(2022, 12, 21), 1.16));
        v.push_back(mk(Date_(2022, 12, 21), Date_(2023, 3, 15), 1.21));
        v.push_back(mk(Date_(2023, 3, 15), Date_(2023, 6, 21), 1.27));
        v.push_back(mk(Date_(2023, 6, 21), Date_(2023, 9, 20), 1.45));
        v.push_back(mk(Date_(2023, 9, 20), Date_(2023, 12, 20), 1.68));
        v.push_back(mk(Date_(2023, 12, 20), Date_(2024, 3, 15), 1.92));
        v.push_back(mk(Date_(2022, 1, 1), Date_(2025, 1, 1), 1.68));   // 3y
        v.push_back(mk(Date_(2022, 1, 1), Date_(2027, 1, 1), 2.10));   // 5y
        v.push_back(mk(Date_(2022, 1, 1), Date_(2029, 1, 1), 2.20));   // 7y
        v.push_back(mk(Date_(2022, 1, 1), Date_(2032, 1, 1), 2.07));   // 10y
        return v;
    }

    CurveCalibrationSpec_ BuildBaseSpec(const Date_& today, LogDfScheme_ scheme) {
        CurveCalibrationSpec_ spec;
        spec.today_ = today;
        spec.ccy_ = "USD";
        spec.curveName_ = "ptirds";
        spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.liborBasis_ = DayBasis_("ACT_365F");
        spec.tolerance_ = 1.0e-10;
        spec.fitTolerance_ = 1.0e-8;
        spec.smoothingWeight_ = 1.0;
        spec.knotDates_ = PtirdsKnotDates();
        spec.instruments_ = PtirdsInstruments(today);
        spec.logDfScheme_ = scheme;
        return spec;
    }

    // Authoritative reference: rateslib v2.7.x docs, Table 6.2 at
    // https://rateslib.com/py/en/2.7.x/z_ptirds_curve.html -- solved node discount factors
    // for the three interpolation schemes. The three columns are distinct by design; each
    // scheme must be checked against its OWN column. The mixed column reproduces log-linear
    // exactly for nodes 0..10 (through 2025-01-01) and diverges only at 2027/2029/2032.
    const Vector_<> EXPECTED_LOG_LINEAR = {
        1.000000, 0.998002, 0.995368, 0.992383, 0.989522,
        0.986774, 0.983421, 0.979878, 0.975791, 0.971397,
        0.950979, 0.900384, 0.857395, 0.814369,
    };
    const Vector_<> EXPECTED_LOG_CUBIC = {
        1.000000, 0.997990, 0.995355, 0.992371, 0.989509,
        0.986762, 0.983408, 0.979866, 0.975779, 0.971385,
        0.950979, 0.900395, 0.857430, 0.814470,
    };
    const Vector_<> EXPECTED_MIXED = {
        1.000000, 0.998002, 0.995368, 0.992383, 0.989522,
        0.986774, 0.983421, 0.979878, 0.975791, 0.971397,
        0.950979, 0.900384, 0.857422, 0.814460,
    };

    // Cross-check solved node DFs against the rateslib reference column, then verify
    // repricing residuals are below the fit-tolerance ceiling.
    void AssertNodesAndResiduals(const CurveCalibrationResult_& r, const Vector_<>& expectedColumn) {
        ASSERT_LT(r.diagnostics_.maxAbsResidual_, 1.0e-8)
            << "repricing residual exceeded 1e-8; max=" << r.diagnostics_.maxAbsResidual_;
        const auto* c = dynamic_cast<const DiscountLogDF_*>(r.curve_.get());
        ASSERT_NE(c, nullptr) << "calibrated curve is not a DiscountLogDF_";
        const auto dfs = c->NodeDF();
        const auto dates = c->NodeDates();
        ASSERT_EQ(static_cast<int>(dfs.size()), 14);
        ASSERT_EQ(static_cast<int>(expectedColumn.size()), 14);
        for (int i = 0; i < 14; ++i) {
            ASSERT_NEAR(dfs[i], expectedColumn[i], 1.0e-6)
                << "node " << i << " (" << Date::ToString(dates[i]) << ")";
        }
    }
} // namespace

TEST(PTIRDSCurveTest, TestLogLinearMatchesRateslib) {
    const Date_ today(2022, 1, 1);
    auto spec = BuildBaseSpec(today, LogDfScheme_::Value_::LOG_LINEAR);
    const auto r = CalibrateYieldCurve(spec);
    AssertNodesAndResiduals(r, EXPECTED_LOG_LINEAR);
}

TEST(PTIRDSCurveTest, TestLogCubicMatchesRateslib) {
    const Date_ today(2022, 1, 1);
    auto spec = BuildBaseSpec(today, LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    const auto r = CalibrateYieldCurve(spec);
    AssertNodesAndResiduals(r, EXPECTED_LOG_CUBIC);
}

TEST(PTIRDSCurveTest, TestMixedMatchesRateslib) {
    const Date_ today(2022, 1, 1);
    auto spec = BuildBaseSpec(today, LogDfScheme_::Value_::MIXED);
    const auto r = CalibrateYieldCurve(spec);
    AssertNodesAndResiduals(r, EXPECTED_MIXED);
}

TEST(PTIRDSCurveTest, TestApplyDXPinsAnchorAndRebuildsInterp) {
    // Direct construction with a known log-linear curve: 5 nodes, anchor pinned at logDF=0.
    const Date_ anchor(2022, 1, 1);
    const Vector_<Date_> dates = {anchor, Date_(2023, 1, 1), Date_(2024, 1, 1), Date_(2025, 1, 1), Date_(2026, 1, 1)};
    const DayBasis_ basis("ACT_365F");
    const Vector_<> logDF = {0.0, -0.02, -0.04, -0.06, -0.08};
    std::unique_ptr<DiscountCurve_> dc(NewDiscountLogDF("applydx", "USD", dates, logDF, basis, LogDfScheme_::Value_::LOG_LINEAR));
    auto* c = dynamic_cast<DiscountLogDF_*>(dc.get());
    ASSERT_NE(c, nullptr);

    // NX == nNodes - 1 (anchor excluded from the free-node vector).
    ASSERT_EQ(c->NX(), 4);

    // Reference DFs at nodes and an interior date.
    const double dfNode3Before = (*c)(anchor, dates[3]);
    const double dfMidBefore = (*c)(anchor, Date_(2024, 7, 1));
    ASSERT_NEAR(dfNode3Before, std::exp(-0.06), 1e-12);

    // Bump the free nodes by a known delta; anchor must stay pinned at DF=1.
    const Vector_<> dx = {0.001, 0.002, 0.003, 0.004};
    c->ApplyDX(dx.begin(), 1.0);

    // anchor logDF untouched -> DF(from=anchor,to=anchor) == 1.0 trivially; check node 0 logDF.
    const auto bumpedLogDF = c->NodeLogDF();
    ASSERT_NEAR(bumpedLogDF[0], 0.0, 1e-15);
    for (int i = 1; i < 5; ++i)
        ASSERT_NEAR(bumpedLogDF[i], logDF[i] + dx[i - 1], 1e-15);

    // operator() must reflect the rebuilt interp: DF at node 3 shifts by exp(dx[2]).
    const double dfNode3After = (*c)(anchor, dates[3]);
    ASSERT_NEAR(dfNode3After, std::exp(-0.06 + 0.003), 1e-12);

    // Interior date also shifts (log-linear: shift equals the linear interpolation of dx).
    const double dfMidAfter = (*c)(anchor, Date_(2024, 7, 1));
    const double ratio = dfMidAfter / dfMidBefore;
    ASSERT_GT(ratio, 1.0);   // bump is positive -> DF rises

    // Inverse delta restores the original DFs (proves the rebuild is consistent).
    c->ApplyDX(dx.begin(), -1.0);
    const double dfNode3Restored = (*c)(anchor, dates[3]);
    ASSERT_NEAR(dfNode3Restored, dfNode3Before, 1e-12);
    const double dfMidRestored = (*c)(anchor, Date_(2024, 7, 1));
    ASSERT_NEAR(dfMidRestored, dfMidBefore, 1e-12);
}

namespace {
    // Small 5-node log-linear curve reused by serialization, extrapolation, and validation tests.
    // Anchor pinned at logDF=0; subsequent nodes descend at -0.02 per year.
    constexpr double PTOL = 1e-10;

    std::unique_ptr<DiscountLogDF_> MakeFiveNodeCurve(LogDfScheme_ scheme) {
        const Vector_<Date_> dates = {Date_(2022, 1, 1), Date_(2023, 1, 1), Date_(2024, 1, 1),
                                      Date_(2025, 1, 1), Date_(2026, 1, 1)};
        const Vector_<> logDF = {0.0, -0.02, -0.04, -0.06, -0.08};
        // Hold the factory result in an owning pointer first so a failed dynamic_cast cannot leak it.
        std::unique_ptr<DiscountCurve_> base(NewDiscountLogDF("rt", "USD", dates, logDF, DayBasis_("ACT_365F"), scheme));
        DiscountLogDF_* cast = dynamic_cast<DiscountLogDF_*>(base.get());
        REQUIRE(cast != nullptr, "NewDiscountLogDF did not return a DiscountLogDF_");
        (void)base.release();
        return std::unique_ptr<DiscountLogDF_>(cast);
    }
} // namespace

TEST(PTIRDSCurveTest, TestSerializationRoundTripLogLinear) {
    auto original = MakeFiveNodeCurve(LogDfScheme_::Value_::LOG_LINEAR);
    ASSERT_NE(original, nullptr);
    const String_ blob = JSON::WriteString(*original);
    const auto restored = handle_cast<DiscountLogDF_>(JSON::ReadString(blob, false));
    ASSERT_NE(restored, nullptr);
    ASSERT_EQ(restored->Scheme(), LogDfScheme_::Value_::LOG_LINEAR);
    const auto oLog = original->NodeLogDF();
    const auto rLog = restored->NodeLogDF();
    ASSERT_EQ(oLog.size(), rLog.size());
    for (int i = 0; i < static_cast<int>(oLog.size()); ++i)
        ASSERT_NEAR(rLog[i], oLog[i], PTOL);
    // off-node DF must match too
    const Date_ anchor(2022, 1, 1);
    ASSERT_NEAR((*restored)(anchor, Date_(2024, 7, 1)), (*original)(anchor, Date_(2024, 7, 1)), PTOL);
}

TEST(PTIRDSCurveTest, TestSerializationRoundTripLogCubic) {
    // Cubic needs >= 3 knots; the 5-node set suffices.
    auto original = MakeFiveNodeCurve(LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    ASSERT_NE(original, nullptr);
    const String_ blob = JSON::WriteString(*original);
    const auto restored = handle_cast<DiscountLogDF_>(JSON::ReadString(blob, false));
    ASSERT_NE(restored, nullptr);
    ASSERT_EQ(restored->Scheme(), LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    const auto oLog = original->NodeLogDF();
    const auto rLog = restored->NodeLogDF();
    for (int i = 0; i < static_cast<int>(oLog.size()); ++i)
        ASSERT_NEAR(rLog[i], oLog[i], PTOL);
    const Date_ anchor(2022, 1, 1);
    ASSERT_NEAR((*restored)(anchor, Date_(2024, 7, 1)), (*original)(anchor, Date_(2024, 7, 1)), PTOL);
}

TEST(PTIRDSCurveTest, TestSerializationRoundTripMixed) {
    // MIXED previously threw a TODO on Write; now the parent curve writes scheme + nodes and the
    // interpolator is rebuilt on Read, so the round-trip must succeed for the mixed scheme too.
    auto original = MakeFiveNodeCurve(LogDfScheme_::Value_::MIXED);
    ASSERT_NE(original, nullptr);
    const String_ blob = JSON::WriteString(*original);
    ASSERT_FALSE(blob.empty());
    const auto restored = handle_cast<DiscountLogDF_>(JSON::ReadString(blob, false));
    ASSERT_NE(restored, nullptr);
    ASSERT_EQ(restored->Scheme(), LogDfScheme_::Value_::MIXED);
    const auto oLog = original->NodeLogDF();
    const auto rLog = restored->NodeLogDF();
    for (int i = 0; i < static_cast<int>(oLog.size()); ++i)
        ASSERT_NEAR(rLog[i], oLog[i], PTOL);
    const Date_ anchor(2022, 1, 1);
    ASSERT_NEAR((*restored)(anchor, Date_(2024, 7, 1)), (*original)(anchor, Date_(2024, 7, 1)), PTOL);
}

TEST(PTIRDSCurveTest, TestExtrapolationFlatForwardPastLastNode) {
    // Flat-forward extrapolation: past the last node the final segment's log-DF slope is extended,
    // so the instantaneous forward rate equals the last segment's forward.
    auto c = MakeFiveNodeCurve(LogDfScheme_::Value_::LOG_LINEAR);
    ASSERT_NE(c, nullptr);
    const DayBasis_ basis("ACT_365F");
    const Date_ anchor(2022, 1, 1);
    const auto& dates = c->NodeDates();
    const Date_ penulNode = dates[dates.size() - 2];
    const Date_ lastNode = dates[dates.size() - 1];
    const Date_ beyond1(2027, 1, 1);   // one year past the last node
    const Date_ beyond2(2031, 1, 1);   // five years past the last node

    // The "expected" flat forward is the last segment's own forward rate, derived from the curve's
    // node DFs and the last segment's year-fraction (the segment crosses 2024-02-29 so it is not
    // exactly 1.0 under ACT/365F).
    const double dfPenul = (*c)(anchor, penulNode);
    const double dfLast = (*c)(anchor, lastNode);
    const double yfLastSeg = basis(penulNode, lastNode, nullptr);
    const double expectedForward = -std::log(dfLast / dfPenul) / yfLastSeg;
    ASSERT_GT(expectedForward, 0.0);

    // Forward over [lastNode, beyond1] equals the last segment's forward.
    const double dfBeyond1 = (*c)(anchor, beyond1);
    const double fwdLastToBeyond = -std::log(dfBeyond1 / dfLast) / basis(lastNode, beyond1, nullptr);
    ASSERT_NEAR(fwdLastToBeyond, expectedForward, PTOL);

    // The same forward holds further out -- flat forward by construction.
    const double dfBeyond2 = (*c)(anchor, beyond2);
    const double fwdBeyond1ToBeyond2 = -std::log(dfBeyond2 / dfBeyond1) / basis(beyond1, beyond2, nullptr);
    ASSERT_NEAR(fwdBeyond1ToBeyond2, expectedForward, PTOL);

    // Forward between two dates that both lie past the last node is also flat at expectedForward.
    const double dfB1 = (*c)(anchor, beyond1);
    const double dfB2 = (*c)(anchor, beyond2);
    const double fwdB1B2 = -std::log(dfB2 / dfB1) / basis(beyond1, beyond2, nullptr);
    ASSERT_NEAR(fwdB1B2, expectedForward, PTOL);
    ASSERT_GT(dfB1, 0.0);
    ASSERT_GT(dfB2, 0.0);
}

TEST(PTIRDSCurveTest, TestInputValidationThrows) {
    const DayBasis_ basis("ACT_365F");
    const Vector_<Date_> goodDates = {Date_(2022, 1, 1), Date_(2023, 1, 1), Date_(2024, 1, 1)};
    const Vector_<> goodLog = {0.0, -0.02, -0.04};

    {   // mismatched lengths: 3 dates, 2 logDFs
        const Vector_<> shortLog = {0.0, -0.02};
        ASSERT_THROW(NewDiscountLogDF("x", "USD", goodDates, shortLog, basis, LogDfScheme_::Value_::LOG_LINEAR),
                     Dal::Exception_);
    }
    {   // unsorted dates
        const Vector_<Date_> unsorted = {Date_(2022, 1, 1), Date_(2024, 1, 1), Date_(2023, 1, 1)};
        ASSERT_THROW(NewDiscountLogDF("x", "USD", unsorted, goodLog, basis, LogDfScheme_::Value_::LOG_LINEAR),
                     Dal::Exception_);
    }
    {   // non-unique dates
        const Vector_<Date_> dup = {Date_(2022, 1, 1), Date_(2022, 1, 1), Date_(2023, 1, 1)};
        ASSERT_THROW(NewDiscountLogDF("x", "USD", dup, goodLog, basis, LogDfScheme_::Value_::LOG_LINEAR),
                     Dal::Exception_);
    }
    {   // empty inputs
        const Vector_<Date_> emptyDates;
        const Vector_<> emptyLog;
        ASSERT_THROW(NewDiscountLogDF("x", "USD", emptyDates, emptyLog, basis, LogDfScheme_::Value_::LOG_LINEAR),
                     Dal::Exception_);
    }
    {   // single-node curve (need at least 2)
        const Vector_<Date_> oneDate = {Date_(2022, 1, 1)};
        const Vector_<> oneLog = {0.0};
        ASSERT_THROW(NewDiscountLogDF("x", "USD", oneDate, oneLog, basis, LogDfScheme_::Value_::LOG_LINEAR),
                     Dal::Exception_);
    }
    {   // anchor not pinned at logDF=0
        const Vector_<> badAnchor = {0.01, -0.02, -0.04};
        ASSERT_THROW(NewDiscountLogDF("x", "USD", goodDates, badAnchor, basis, LogDfScheme_::Value_::LOG_LINEAR),
                     Dal::Exception_);
    }
}
