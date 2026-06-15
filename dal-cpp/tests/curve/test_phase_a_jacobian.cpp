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
    CurveCalibrationSpec_ MakePhaseASpec(LogDfScheme_ scheme = LogDfScheme_::Value_::LOG_LINEAR) {
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

    // Column-by-column central-difference agreement check. Asserts that every entry of the
    // analytic Jacobian J (from AnalyticJacobianAt) matches a two-sided central difference of
    // F(x) at step h, with a tolerance that scales with the magnitude of the FD value. Used by
    // the per-scheme and per-instrument-type sweep tests so the comparison logic is identical.
    void AssertMatchesCentralDifference(const CurveCalibrationSpec_& spec, const Vector_<>& x, double h, double relTol) {
        const Matrix_<> J = TestOnly::AnalyticJacobianAt(spec, x);
        const int nRows = static_cast<int>(spec.instruments_.size());
        const int nCols = static_cast<int>(x.size());
        ASSERT_EQ(J.Rows(), nRows);
        ASSERT_EQ(J.Cols(), nCols);
        for (int c = 0; c < nCols; ++c) {
            Vector_<> xUp = x;
            Vector_<> xDn = x;
            xUp[c] += h;
            xDn[c] -= h;
            const Vector_<> fUp = EvalResiduals(spec, xUp);
            const Vector_<> fDn = EvalResiduals(spec, xDn);
            for (int r = 0; r < nRows; ++r) {
                const double fd = (fUp[r] - fDn[r]) / (2.0 * h);
                const double an = J(r, c);
                if (std::abs(fd) < relTol) {
                    ASSERT_NEAR(an, 0.0, relTol) << "row=" << r << " col=" << c << " FD=" << fd;
                } else {
                    ASSERT_NEAR(an, fd, relTol * std::max(1.0, std::abs(fd))) << "row=" << r << " col=" << c;
                }
            }
        }
    }
} // namespace

// ============================================================================
// Category 1: AAD-tape Jacobian matches two-sided central differences
// ============================================================================
// The Phase A override returns dModelRate_i/dx_j via one reverse sweep per row.
// Each non-zero entry must match a two-sided central difference of F(x) at step
// 1e-6 within 1e-9.

TEST(PhaseAAADJacobianTest, TestMatchesCentralDifferenceLogLinear) {
    auto spec = MakePhaseASpec(LogDfScheme_::Value_::LOG_LINEAR);
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
    auto spec = MakePhaseASpec();
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
// Category 3: Solve convergence -- the AAD-tape Jacobian drives the solver to a fit
// ============================================================================
// We do NOT assert iteration count (linesearch varies), only that the residual converges.

TEST(PhaseAAADJacobianTest, TestSolveConvergesLogLinear) {
    auto specAAD = MakePhaseASpec();

    const auto rAAD = CalibrateYieldCurve(specAAD);

    ASSERT_LT(rAAD.diagnostics_.maxAbsResidual_, 1.0e-7);

    const auto* cAAD = dynamic_cast<const DiscountLogDF_*>(rAAD.curve_.get());
    ASSERT_NE(cAAD, nullptr);
    ASSERT_EQ(static_cast<int>(cAAD->NodeLogDF().size()), 6);
}

// ============================================================================
// Category 4: Ineligibility -- the AAD Jacobian falls back to bumping with a NOTICE
// ============================================================================
// Phase A is ineligible for non-LOG_DISCOUNT parameterizations. EligibleForPhaseA
// returns false, Gradient returns nullptr (the solver dense-bumps), and a NOTICE
// fires. We cannot easily assert the NOTICE text from a unit test, but we CAN assert
// the fallback behavior: TestOnly::AnalyticJacobianAt returns an EMPTY matrix on a
// non-LOG_DISCOUNT spec (Gradient returned nullptr).

TEST(PhaseAAADJacobianTest, TestIneligibleParameterizationFallsBack) {
    auto spec = MakePhaseASpec();
    spec.parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
    // Knot 0 must be > today for non-LOG_DISCOUNT.
    spec.knotDates_[0] = Date_(2022, 4, 1);
    // PLF needs x of size 2 * nKnots = 12
    const Vector_<> x(12, -0.005);
    const Matrix_<> J = TestOnly::AnalyticJacobianAt(spec, x);
    ASSERT_EQ(J.Rows(), 0); // empty -> ineligible, solver dense-bumps
    ASSERT_EQ(J.Cols(), 0);
}

// A FORECAST-target calibration (calibrateDiscountCurve_ == false) is ineligible for the AAD
// Jacobian. EligibleForPhaseA returns false, Gradient returns nullptr, and
// TestOnly::AnalyticJacobianAt returns an EMPTY matrix (the solver dense-bumps instead).
TEST(PhaseAAADJacobianTest, TestIneligibleForecastTargetFallsBack) {
    auto spec = MakePhaseASpec();
    spec.calibrateDiscountCurve_ = false;
    spec.targetTenor_ = PeriodLength_("3M");
    Vector_<> baseLogDF(spec.knotDates_.size(), 0.0);
    for (int i = 1; i < static_cast<int>(spec.knotDates_.size()); ++i)
        baseLogDF[i] = -0.02 * (i * 0.25);
    spec.discountCurves_[spec.targetCollateral_] = Handle_<DiscountCurve_>(
        NewDiscountLogDF("base", spec.ccy_, spec.knotDates_, baseLogDF, spec.liborBasis_, spec.logDfScheme_));
    const Vector_<> x = {-0.005, -0.012, -0.025, -0.04, -0.06};
    const Matrix_<> J = TestOnly::AnalyticJacobianAt(spec, x);
    ASSERT_EQ(J.Rows(), 0); // empty -> ineligible, solver dense-bumps
    ASSERT_EQ(J.Cols(), 0);
}

// ============================================================================
// Category 5: Tape isolation -- two consecutive Gradient calls do not leak state
// ============================================================================
// If the TapeGuard_ leaks adjoints, the second call's Jacobian would inherit
// the first call's residuals and produce wrong numbers. We assert the second
// call reproduces the first exactly.

TEST(PhaseAAADJacobianTest, TestTapeIsolationAcrossCalls) {
    auto spec = MakePhaseASpec();
    const Vector_<> x = {-0.005, -0.012, -0.025, -0.04, -0.06};
    const Matrix_<> J1 = TestOnly::AnalyticJacobianAt(spec, x);
    const Matrix_<> J2 = TestOnly::AnalyticJacobianAt(spec, x);
    ASSERT_EQ(J1.Rows(), J2.Rows());
    ASSERT_EQ(J1.Cols(), J2.Cols());
    for (int r = 0; r < J1.Rows(); ++r)
        for (int c = 0; c < J1.Cols(); ++c)
            ASSERT_NEAR(J1(r, c), J2(r, c), 1e-12) << "row=" << r << " col=" << c;
}

// ============================================================================
// Category 6: All three LogDfScheme_ values must match central differences
// ============================================================================
// Phase A eligibility is scheme-agnostic -- the templated DiscountLogDF_<T_> dispatches on
// scheme inside the tape. The existing TestMatchesCentralDifferenceLogLinear covers LOG_LINEAR;
// these two tests cover LOG_CUBIC_NATURAL and MIXED, exercising the natural-cubic and
// mixed-cutoff spline branches of the AAD path.

TEST(PhaseAAADJacobianTest, TestMatchesCentralDifferenceLogCubicNatural) {
    auto spec = MakePhaseASpec(LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    const Vector_<> x = {-0.005, -0.012, -0.025, -0.04, -0.06};
    AssertMatchesCentralDifference(spec, x, 1.0e-6, 1.0e-9);
}

TEST(PhaseAAADJacobianTest, TestMatchesCentralDifferenceMixed) {
    auto spec = MakePhaseASpec(LogDfScheme_::Value_::MIXED);
    const Vector_<> x = {-0.005, -0.012, -0.025, -0.04, -0.06};
    AssertMatchesCentralDifference(spec, x, 1.0e-6, 1.0e-9);
}

// ============================================================================
// Category 7: Single-instrument tape canary (Deposit)
// ============================================================================
// A single Deposit isolates the tape from multi-row sparsity. The Jacobian is 1 x N: one reverse
// sweep over one residual. If the tape mis-computes dResidual/d(logDF_node), this test catches
// it directly -- there is no confounding with structural-zero assembly across rows. We assert
// the single row matches a central difference for every node column, and that the columns the
// deposit does NOT touch (beyond its maturity) are EXACTLY zero (AAD structural zero, not noise).

TEST(PhaseAAADJacobianTest, TestSingleDepositTapeMatchesCentralDifference) {
    CurveCalibrationSpec_ spec;
    spec.today_ = Date_(2022, 1, 1);
    spec.ccy_ = "USD";
    spec.curveName_ = "phase_a_deposit_canary";
    spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
    spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
    spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
    spec.liborBasis_ = DayBasis_("ACT_365F");
    spec.tolerance_ = 1.0e-10;
    spec.fitTolerance_ = 1.0e-8;
    spec.smoothingWeight_ = 1.0;
    spec.logDfScheme_ = LogDfScheme_::Value_::LOG_LINEAR;

    spec.knotDates_ = {
        Date_(2022, 1, 1), Date_(2022, 4, 1), Date_(2022, 7, 1), Date_(2023, 1, 1),
        Date_(2024, 1, 1), Date_(2025, 1, 1),
    };

    // One 3M deposit starting at the anchor. Its cashflow lands at 2022-04-01 (knot column 0).
    RateIndexConvention_ idx = AnnualIndexPA();
    spec.instruments_ = {Handle_<YCInstrument_>(
        new Deposit_(spec.today_, spec.today_, Date_(2022, 4, 1), 0.011, idx))};

    const Vector_<> x = {-0.005, -0.012, -0.025, -0.04, -0.06};
    AssertMatchesCentralDifference(spec, x, 1.0e-6, 1.0e-9);

    // The deposit's only cashflow is at 2022-04-01 -- solver column 0 under LOG_LINEAR. Columns
    // 1..4 must be EXACTLY zero (AAD structural zero, no FD noise).
    const Matrix_<> J = TestOnly::AnalyticJacobianAt(spec, x);
    ASSERT_EQ(J.Rows(), 1);
    ASSERT_EQ(J.Cols(), 5);
    ASSERT_NE(J(0, 0), 0.0) << "deposit sensitivity at its own maturity column must be nonzero";
    for (int c = 1; c < 5; ++c)
        ASSERT_EQ(J(0, c), 0.0) << "deposit row col " << c << " = " << J(0, c) << " (expected exactly zero)";
}

// ============================================================================
// Category 8: Mixed instrument types in one calibration (Deposit + FRA + Swap)
// ============================================================================
// Phase A is eligible for vanilla Swap, Deposit, FRA, and Future. A calibration mixing all
// three primary cash instrument types exercises the per-instrument dispatch in PhaseAJacobian_
// (DepositRateT_ + ForwardRateT_ + SwapRateT_) in a single recording. The AAD Jacobian must
// still match central differences row by row, and structural zeros must appear for instruments
// whose cashflows end before later nodes.

TEST(PhaseAAADJacobianTest, TestMixedInstrumentCalibrationMatchesCentralDifference) {
    CurveCalibrationSpec_ spec;
    spec.today_ = Date_(2022, 1, 1);
    spec.ccy_ = "USD";
    spec.curveName_ = "phase_a_mixed";
    spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
    spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
    spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
    spec.liborBasis_ = DayBasis_("ACT_365F");
    spec.tolerance_ = 1.0e-10;
    spec.fitTolerance_ = 1.0e-8;
    spec.smoothingWeight_ = 1.0;
    spec.logDfScheme_ = LogDfScheme_::Value_::LOG_LINEAR;

    spec.knotDates_ = {
        Date_(2022, 1, 1), Date_(2022, 4, 1), Date_(2022, 7, 1), Date_(2023, 1, 1),
        Date_(2024, 1, 1), Date_(2025, 1, 1),
    };

    // All three instruments start at the anchor (eligibility requires span.first == anchor).
    const RateIndexConvention_ idx = AnnualIndexPA();
    const auto fixedLeg = AnnualLegPA();
    const auto floatLeg = AnnualLegPA();
    spec.instruments_ = {
        // Deposit 3M -> cashflow at 2022-04-01 (column 0).
        Handle_<YCInstrument_>(new Deposit_(spec.today_, spec.today_, Date_(2022, 4, 1), 0.011, idx)),
        // FRA at anchor -> 6M fixing, cashflow at 2022-07-01 (column 1).
        Handle_<YCInstrument_>(new FRA_(spec.today_, spec.today_, Date_(2022, 7, 1), 0.012, idx)),
        // Swap 3Y -> cashflows through 2025-01-01 (columns 0..4).
        Handle_<YCInstrument_>(new Swap_(spec.today_, spec.today_, Date_(2025, 1, 1), 0.018, fixedLeg, idx, floatLeg)),
    };

    const Vector_<> x = {-0.005, -0.012, -0.025, -0.04, -0.06};
    AssertMatchesCentralDifference(spec, x, 1.0e-6, 1.0e-9);

    // Structural zeros: the deposit (row 0) ends at 2022-04-01, so columns 1..4 must be exactly
    // zero. The FRA (row 1) ends at 2022-07-01, so columns 2..4 must be exactly zero.
    const Matrix_<> J = TestOnly::AnalyticJacobianAt(spec, x);
    ASSERT_EQ(J.Rows(), 3);
    ASSERT_EQ(J.Cols(), 5);
    for (int c = 1; c < 5; ++c)
        ASSERT_EQ(J(0, c), 0.0) << "deposit row col " << c << " = " << J(0, c);
    for (int c = 2; c < 5; ++c)
        ASSERT_EQ(J(1, c), 0.0) << "fra row col " << c << " = " << J(1, c);
}
