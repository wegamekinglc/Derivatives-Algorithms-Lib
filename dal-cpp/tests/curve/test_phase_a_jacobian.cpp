//
// Created by dal-implementer on 2026/6/15.
//

#include <gtest/gtest.h>
#include <cmath>
#include <map>
#include <dal/platform/platform.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

namespace {
    RateLegConvention_ AnnualLegPA() {
        RateLegConvention_ leg;
        leg.paymentLag_ = 0;
        leg.paymentFrequency_ = PeriodLength_("12M");
        leg.dayBasis_ = DayBasis_("ACT_365F");
        leg.accrualHolidays_ = Holidays::None();
        leg.paymentHolidays_ = Holidays::None();
        leg.businessDayConvention_ = BizDayConvention_("Unadjusted");
        leg.paymentConvention_ = BizDayConvention_("Unadjusted");
        return leg;
    }

    RateIndexConvention_ AnnualIndexPA() {
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

    // 5-instrument vanilla-swap ladder on LOG_DISCOUNT. Anchor == today_, every swap starts at the
    // anchor, no projection curves, vanilla Swap_ only -- the Phase A eligible shape.
    CurveCalibrationSpec_ MakePhaseASpec(CurveJacobianMode_ jacobianMode, LogDfScheme_ scheme = LogDfScheme_::Value_::LOG_LINEAR) {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2022, 1, 1);
        spec.ccy_ = "USD";
        spec.curveName_ = "phase_a_test";
        spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.liborBasis_ = DayBasis_("ACT_365F");
        spec.tolerance_ = 1.0e-10;
        spec.fitTolerance_ = 1.0e-8;
        spec.smoothingWeight_ = 1.0;
        spec.jacobianMode_ = jacobianMode;
        spec.logDfScheme_ = scheme;

        spec.knotDates_ = {
            Date_(2022, 1, 1), Date_(2022, 4, 1), Date_(2022, 7, 1), Date_(2023, 1, 1),
            Date_(2024, 1, 1), Date_(2025, 1, 1),
        };

        const auto fixedLeg = AnnualLegPA();
        const auto floatIdx = AnnualIndexPA();
        const auto floatLeg = AnnualLegPA();
        const auto mkSwap = [&](const Date_& start, const Date_& end, double parPct) {
            return Handle_<YCInstrument_>(new Swap_(spec.today_, start, end, parPct / 100.0, fixedLeg, floatIdx, floatLeg));
        };
        spec.instruments_ = {
            mkSwap(Date_(2022, 1, 1), Date_(2022, 4, 1), 1.00),
            mkSwap(Date_(2022, 1, 1), Date_(2022, 7, 1), 1.10),
            mkSwap(Date_(2022, 1, 1), Date_(2023, 1, 1), 1.25),
            mkSwap(Date_(2022, 1, 1), Date_(2024, 1, 1), 1.55),
            mkSwap(Date_(2022, 1, 1), Date_(2025, 1, 1), 1.80),
        };
        return spec;
    }

    // Independent residual-vector evaluator for the same calibration set. Used to compute a
    // two-sided central-difference Jacobian that the AAD-tape Jacobian must match within 1e-9.
    // Mirrors the F() body in calibration.cpp for LOG_DISCOUNT + vanilla swap.
    Vector_<> EvalResiduals(const CurveCalibrationSpec_& spec, const Vector_<>& x) {
        Vector_<> full(spec.knotDates_.size(), 0.0);
        for (int i = 1; i < static_cast<int>(spec.knotDates_.size()); ++i)
            full[i] = x[i - 1];
        std::unique_ptr<DiscountCurve_> dc(
            NewDiscountLogDF(spec.curveName_, spec.ccy_, spec.knotDates_, full, spec.liborBasis_, spec.logDfScheme_));
        std::map<CollateralType_, Handle_<DiscountCurve_>> discounts;
        discounts[spec.targetCollateral_] =
            Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), dc.get()));
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwards;
        CurveBlock_ yc(spec.curveName_, spec.ccy_, discounts, forwards, spec.liborBasis_);
        Vector_<> f(spec.instruments_.size());
        Handle_<YieldCurve_> empty;
        for (int i = 0; i < static_cast<int>(spec.instruments_.size()); ++i) {
            auto rate = spec.instruments_[i]->Precompute(empty);
            f[i] = (*rate)(yc) - spec.instruments_[i]->MarketRate();
        }
        return f;
    }
} // namespace

// ============================================================================
// Category 1: AAD-tape Jacobian matches two-sided central differences
// ============================================================================
// The Phase A override returns dModelRate_i/dx_j via one reverse sweep per row.
// Each non-zero entry must match a two-sided central difference of F(x) at step
// 1e-6 within 1e-9.

TEST(PhaseAAADJacobianTest, TestMatchesCentralDifferenceLogLinear) {
    auto spec = MakePhaseASpec(CurveJacobianMode_::Value_::AAD_TAPE, LogDfScheme_::Value_::LOG_LINEAR);
    const Vector_<> x = {-0.005, -0.012, -0.025, -0.04, -0.06};
    ASSERT_EQ(static_cast<int>(x.size()), 5);
    const Matrix_<> J = TestOnly::AnalyticJacobianAt(spec, x);
    ASSERT_EQ(J.Rows(), 5);
    ASSERT_EQ(J.Cols(), 5);

    const double h = 1.0e-6;
    for (int c = 0; c < 5; ++c) {
        Vector_<> xUp = x;
        Vector_<> xDn = x;
        xUp[c] += h;
        xDn[c] -= h;
        const Vector_<> fUp = EvalResiduals(spec, xUp);
        const Vector_<> fDn = EvalResiduals(spec, xDn);
        for (int r = 0; r < 5; ++r) {
            const double fd = (fUp[r] - fDn[r]) / (2.0 * h);
            const double an = J(r, c);
            if (std::abs(fd) < 1e-9) {
                ASSERT_NEAR(an, 0.0, 1e-9) << "row=" << r << " col=" << c << " FD=" << fd;
            } else {
                ASSERT_NEAR(an, fd, 1e-9 * std::max(1.0, std::abs(fd))) << "row=" << r << " col=" << c;
            }
        }
    }
}

// ============================================================================
// Category 2: Structural zeros are EXACTLY zero
// ============================================================================
// AAD produces exact structural zeros (no bump noise). Each row of the Jacobian
// must have at least one exactly-zero entry for a column beyond the instrument's
// cashflow support.

TEST(PhaseAAADJacobianTest, TestStructuralZerosAreExactlyZero) {
    auto spec = MakePhaseASpec(CurveJacobianMode_::Value_::AAD_TAPE);
    const Vector_<> x = {-0.005, -0.012, -0.025, -0.04, -0.06};
    const Matrix_<> J = TestOnly::AnalyticJacobianAt(spec, x);
    ASSERT_EQ(J.Rows(), 5);
    ASSERT_EQ(J.Cols(), 5);

    // The first instrument is a 3M swap (start=2022-1-1, end=2022-4-1). Its only cashflow lands
    // at solver column 0. Row 0 must be EXACTLY zero at columns 1..4 (AAD structural zeros).
    for (int c = 1; c < 5; ++c)
        ASSERT_EQ(J(0, c), 0.0) << "row 0 col " << c << " = " << J(0, c) << " (expected exactly zero)";
}

// ============================================================================
// Category 3: Solve equivalence -- AAD_TAPE and BUMPED agree node-by-node
// ============================================================================
// The calibrated node log(DF) vectors must agree within 1e-8. We do NOT assert
// iteration count (linesearch varies), only that the residual converges.

TEST(PhaseAAADJacobianTest, TestSolveMatchesBumpedLogLinear) {
    auto specBumped = MakePhaseASpec(CurveJacobianMode_::Value_::BUMPED);
    auto specAAD = MakePhaseASpec(CurveJacobianMode_::Value_::AAD_TAPE);

    const auto rBumped = CalibrateYieldCurve(specBumped);
    const auto rAAD = CalibrateYieldCurve(specAAD);

    ASSERT_LT(rBumped.diagnostics_.maxAbsResidual_, 1.0e-7);
    ASSERT_LT(rAAD.diagnostics_.maxAbsResidual_, 1.0e-7);

    const auto* cBumped = dynamic_cast<const DiscountLogDF_*>(rBumped.curve_.get());
    const auto* cAAD = dynamic_cast<const DiscountLogDF_*>(rAAD.curve_.get());
    ASSERT_NE(cBumped, nullptr);
    ASSERT_NE(cAAD, nullptr);

    const auto logBumped = cBumped->NodeLogDF();
    const auto logAAD = cAAD->NodeLogDF();
    ASSERT_EQ(logBumped.size(), logAAD.size());
    for (int i = 0; i < static_cast<int>(logBumped.size()); ++i)
        ASSERT_NEAR(logAAD[i], logBumped[i], 1.0e-8) << "node " << i;
}

// ============================================================================
// Category 4: Ineligibility -- AAD_TAPE silently falls back to CP1 with NOTICE
// ============================================================================
// Phase A is ineligible for non-LOG_DISCOUNT parameterizations. The override
// returns nullptr (the path falls through to bumped), and a NOTICE fires. We
// cannot easily assert the NOTICE text from a unit test, but we CAN assert the
// fallback behavior: TestOnly::AnalyticJacobianAt returns an EMPTY matrix for
// AAD_TAPE on a non-LOG_DISCOUNT spec (the dispatch falls through to bumped).

TEST(PhaseAAADJacobianTest, TestIneligibleParameterizationFallsBack) {
    auto spec = MakePhaseASpec(CurveJacobianMode_::Value_::AAD_TAPE);
    spec.parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
    // Knot 0 must be > today for non-LOG_DISCOUNT.
    spec.knotDates_[0] = Date_(2022, 4, 1);
    // PLF needs x of size 2 * nKnots = 12
    const Vector_<> x(12, -0.005);
    const Matrix_<> J = TestOnly::AnalyticJacobianAt(spec, x);
    ASSERT_EQ(J.Rows(), 0); // empty -> AAD_TAPE fell through to bumped
    ASSERT_EQ(J.Cols(), 0);
}

// A FORECAST-target calibration (calibrateDiscountCurve_ == false) is ineligible for Phase A.
// AAD_TAPE must fall through to the CP1 chain-rule path with a NOTICE; the result must match
// what ANALYTIC_LOG_DISCOUNT would produce on the same spec (both engage CP1). We assert the
// two Jacobians agree entry-by-entry, which is the observable contract of the fall-through.
TEST(PhaseAAADJacobianTest, TestIneligibleForecastTargetFallsBackToCP1) {
    auto makeSpec = [](CurveJacobianMode_ mode) {
        auto spec = MakePhaseASpec(mode);
        spec.calibrateDiscountCurve_ = false;
        spec.targetTenor_ = PeriodLength_("3M");
        Vector_<> baseLogDF(spec.knotDates_.size(), 0.0);
        for (int i = 1; i < static_cast<int>(spec.knotDates_.size()); ++i)
            baseLogDF[i] = -0.02 * (i * 0.25);
        spec.discountCurves_[spec.targetCollateral_] = Handle_<DiscountCurve_>(
            NewDiscountLogDF("base", spec.ccy_, spec.knotDates_, baseLogDF, spec.liborBasis_, spec.logDfScheme_));
        return spec;
    };
    const Vector_<> x = {-0.005, -0.012, -0.025, -0.04, -0.06};
    const Matrix_<> J_aad = TestOnly::AnalyticJacobianAt(makeSpec(CurveJacobianMode_::Value_::AAD_TAPE), x);
    const Matrix_<> J_cp1 = TestOnly::AnalyticJacobianAt(makeSpec(CurveJacobianMode_::Value_::ANALYTIC_LOG_DISCOUNT), x);
    // Both must be non-empty (CP1 engaged) and identical (AAD_TAPE fell through to CP1).
    ASSERT_EQ(J_aad.Rows(), J_cp1.Rows());
    ASSERT_EQ(J_aad.Cols(), J_cp1.Cols());
    ASSERT_GT(J_aad.Rows(), 0);
    for (int r = 0; r < J_aad.Rows(); ++r)
        for (int c = 0; c < J_aad.Cols(); ++c)
            ASSERT_NEAR(J_aad(r, c), J_cp1(r, c), 1e-12);
}

// ============================================================================
// Category 5: Tape isolation -- two consecutive Gradient calls do not leak state
// ============================================================================
// If the TapeGuard_ leaks adjoints, the second call's Jacobian would inherit
// the first call's residuals and produce wrong numbers. We assert the second
// call reproduces the first exactly.

TEST(PhaseAAADJacobianTest, TestTapeIsolationAcrossCalls) {
    auto spec = MakePhaseASpec(CurveJacobianMode_::Value_::AAD_TAPE);
    const Vector_<> x = {-0.005, -0.012, -0.025, -0.04, -0.06};
    const Matrix_<> J1 = TestOnly::AnalyticJacobianAt(spec, x);
    const Matrix_<> J2 = TestOnly::AnalyticJacobianAt(spec, x);
    ASSERT_EQ(J1.Rows(), J2.Rows());
    ASSERT_EQ(J1.Cols(), J2.Cols());
    for (int r = 0; r < J1.Rows(); ++r)
        for (int c = 0; c < J1.Cols(); ++c)
            ASSERT_NEAR(J1(r, c), J2(r, c), 1e-12) << "row=" << r << " col=" << c;
}
