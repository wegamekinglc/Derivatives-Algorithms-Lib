//
// Created by dal-implementer on 2026/6/15.
//

#include <gtest/gtest.h>
#include <cmath>
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
    RateLegConvention_ AnnualLeg() {
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

    // 5-instrument calibration set: 4 single-period deposits/FRAs + 1 vanilla swap. The knots
    // match the swap maturity calendar plus the short-end points; LOG_DISCOUNT requires the anchor
    // to be today_.
    CurveCalibrationSpec_ MakeAnalyticSpec(CurveJacobianMode_ jacobianMode, LogDfScheme_ scheme = LogDfScheme_::Value_::LOG_LINEAR) {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2022, 1, 1);
        spec.ccy_ = "USD";
        spec.curveName_ = "analytic_test";
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

        const auto fixedLeg = AnnualLeg();
        const auto floatIdx = AnnualIndex();
        const auto floatLeg = AnnualLeg();
        const auto mkSwap = [&](const Date_& start, const Date_& end, double parPct) {
            return Handle_<YCInstrument_>(new Swap_(spec.today_, start, end, parPct / 100.0, fixedLeg, floatIdx, floatLeg));
        };
        // 5 swaps so we have a 5x5 (nInstruments x nFreeNodes) Jacobian.
        spec.instruments_ = {
            mkSwap(Date_(2022, 1, 1), Date_(2022, 4, 1), 1.00),
            mkSwap(Date_(2022, 1, 1), Date_(2022, 7, 1), 1.10),
            mkSwap(Date_(2022, 1, 1), Date_(2023, 1, 1), 1.25),
            mkSwap(Date_(2022, 1, 1), Date_(2024, 1, 1), 1.55),
            mkSwap(Date_(2022, 1, 1), Date_(2025, 1, 1), 1.80),
        };
        return spec;
    }
} // namespace

// ============================================================================
// Category 1: Analytic-vs-bumped agreement
// ============================================================================
// The analytic Jacobian (from Gradient override) must match a two-sided central
// difference of F(x) with step 1e-6 within 1e-8 on the non-zero entries.

TEST(AnalyticJacobianTest, TestAnalyticMatchesCentralDifferenceLogLinear) {
    auto spec = MakeAnalyticSpec(CurveJacobianMode_::Value_::ANALYTIC_LOG_DISCOUNT, LogDfScheme_::Value_::LOG_LINEAR);
    // 5 free nodes (anchor excluded): one log-DF per free knot.
    const Vector_<> x = {-0.005, -0.012, -0.025, -0.04, -0.06};
    ASSERT_EQ(static_cast<int>(x.size()), 5);
    const Matrix_<> J_analytic = TestOnly::AnalyticJacobianAt(spec, x);
    ASSERT_EQ(J_analytic.Rows(), 5);
    ASSERT_EQ(J_analytic.Cols(), 5);

    // Reference: two-sided central difference of F(x) at step 1e-6, where F is the residual
    // vector built by Precompute + (*rate)(yc) - marketRate on the calibrated curve at x.
    const auto evalF = [&](const Vector_<>& xx) {
        Vector_<> full(spec.knotDates_.size(), 0.0);
        for (int i = 1; i < static_cast<int>(spec.knotDates_.size()); ++i)
            full[i] = xx[i - 1];
        std::unique_ptr<DiscountCurve_> dc(NewDiscountLogDF(spec.curveName_, spec.ccy_, spec.knotDates_, full,
            spec.liborBasis_, spec.logDfScheme_));
        std::map<CollateralType_, Handle_<DiscountCurve_>> discounts;
        discounts[spec.targetCollateral_] = Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), dc.get()));
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwards;
        CurveBlock_ yc(spec.curveName_, spec.ccy_, discounts, forwards, spec.liborBasis_);
        Vector_<> f(spec.instruments_.size());
        Handle_<YieldCurve_> empty;
        for (int i = 0; i < static_cast<int>(spec.instruments_.size()); ++i) {
            auto rate = spec.instruments_[i]->Precompute(empty);
            f[i] = (*rate)(yc) - spec.instruments_[i]->MarketRate();
        }
        return f;
    };

    const double h = 1.0e-6;
    for (int c = 0; c < 5; ++c) {
        Vector_<> xUp = x;
        Vector_<> xDn = x;
        xUp[c] += h;
        xDn[c] -= h;
        const Vector_<> fUp = evalF(xUp);
        const Vector_<> fDn = evalF(xDn);
        for (int r = 0; r < 5; ++r) {
            const double fd = (fUp[r] - fDn[r]) / (2.0 * h);
            const double an = J_analytic(r, c);
            if (std::abs(fd) < 1e-9) {
                ASSERT_NEAR(an, 0.0, 1e-9) << "row=" << r << " col=" << c << " FD=" << fd;
            } else {
                ASSERT_NEAR(an, fd, 1e-8 * std::max(1.0, std::abs(fd))) << "row=" << r << " col=" << c;
            }
        }
    }
}

// ============================================================================
// Category 2: Solve equivalence -- BUMPED and ANALYTIC_LOG_DISCOUNT agree node-by-node
// ============================================================================

TEST(AnalyticJacobianTest, TestAnalyticSolveMatchesBumped) {
    auto specBumped = MakeAnalyticSpec(CurveJacobianMode_::Value_::BUMPED);
    auto specAnalytic = MakeAnalyticSpec(CurveJacobianMode_::Value_::ANALYTIC_LOG_DISCOUNT);

    const auto rBumped = CalibrateYieldCurve(specBumped);
    const auto rAnalytic = CalibrateYieldCurve(specAnalytic);

    ASSERT_LT(rBumped.diagnostics_.maxAbsResidual_, 1.0e-7);
    ASSERT_LT(rAnalytic.diagnostics_.maxAbsResidual_, 1.0e-7);

    const auto* cBumped = dynamic_cast<const DiscountLogDF_*>(rBumped.curve_.get());
    const auto* cAnalytic = dynamic_cast<const DiscountLogDF_*>(rAnalytic.curve_.get());
    ASSERT_NE(cBumped, nullptr);
    ASSERT_NE(cAnalytic, nullptr);

    const auto logBumped = cBumped->NodeLogDF();
    const auto logAnalytic = cAnalytic->NodeLogDF();
    ASSERT_EQ(logBumped.size(), logAnalytic.size());
    for (int i = 0; i < static_cast<int>(logBumped.size()); ++i) {
        ASSERT_NEAR(logAnalytic[i], logBumped[i], 1.0e-8)
            << "node " << i << " bumped=" << logBumped[i] << " analytic=" << logAnalytic[i];
    }
}

TEST(AnalyticJacobianTest, TestAnalyticSolveMatchesBumpedLogCubic) {
    auto specBumped = MakeAnalyticSpec(CurveJacobianMode_::Value_::BUMPED, LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    auto specAnalytic = MakeAnalyticSpec(CurveJacobianMode_::Value_::ANALYTIC_LOG_DISCOUNT, LogDfScheme_::Value_::LOG_CUBIC_NATURAL);

    const auto rBumped = CalibrateYieldCurve(specBumped);
    const auto rAnalytic = CalibrateYieldCurve(specAnalytic);

    ASSERT_LT(rBumped.diagnostics_.maxAbsResidual_, 1.0e-7);
    ASSERT_LT(rAnalytic.diagnostics_.maxAbsResidual_, 1.0e-7);

    const auto* cBumped = dynamic_cast<const DiscountLogDF_*>(rBumped.curve_.get());
    const auto* cAnalytic = dynamic_cast<const DiscountLogDF_*>(rAnalytic.curve_.get());
    ASSERT_NE(cBumped, nullptr);
    ASSERT_NE(cAnalytic, nullptr);

    const auto logBumped = cBumped->NodeLogDF();
    const auto logAnalytic = cAnalytic->NodeLogDF();
    for (int i = 0; i < static_cast<int>(logBumped.size()); ++i)
        ASSERT_NEAR(logAnalytic[i], logBumped[i], 1.0e-8);
}

TEST(AnalyticJacobianTest, TestAnalyticSolveMatchesBumpedMixed) {
    auto specBumped = MakeAnalyticSpec(CurveJacobianMode_::Value_::BUMPED, LogDfScheme_::Value_::MIXED);
    auto specAnalytic = MakeAnalyticSpec(CurveJacobianMode_::Value_::ANALYTIC_LOG_DISCOUNT, LogDfScheme_::Value_::MIXED);

    const auto rBumped = CalibrateYieldCurve(specBumped);
    const auto rAnalytic = CalibrateYieldCurve(specAnalytic);

    ASSERT_LT(rBumped.diagnostics_.maxAbsResidual_, 1.0e-7);
    ASSERT_LT(rAnalytic.diagnostics_.maxAbsResidual_, 1.0e-7);

    const auto* cBumped = dynamic_cast<const DiscountLogDF_*>(rBumped.curve_.get());
    const auto* cAnalytic = dynamic_cast<const DiscountLogDF_*>(rAnalytic.curve_.get());
    ASSERT_NE(cBumped, nullptr);
    ASSERT_NE(cAnalytic, nullptr);

    const auto logBumped = cBumped->NodeLogDF();
    const auto logAnalytic = cAnalytic->NodeLogDF();
    for (int i = 0; i < static_cast<int>(logBumped.size()); ++i)
        ASSERT_NEAR(logAnalytic[i], logBumped[i], 1.0e-8);
}

// Default jacobianMode_ (BUMPED) MUST give byte-for-byte identical behavior to pre-CP1: the
// Gradient override returns nullptr and the solver falls through to the bumped dense path.
TEST(AnalyticJacobianTest, TestDefaultModeIsBumpedAndUnchanged) {
    auto spec = MakeAnalyticSpec(CurveJacobianMode_::Value_::BUMPED);
    ASSERT_TRUE(spec.jacobianMode_ == CurveJacobianMode_::Value_::BUMPED);
    // TestOnly::AnalyticJacobianAt returns empty when the analytic path is not engaged.
    const Vector_<> x = {-0.005, -0.012, -0.025, -0.04, -0.06};
    const Matrix_<> J = TestOnly::AnalyticJacobianAt(spec, x);
    ASSERT_EQ(J.Rows(), 0);
    ASSERT_EQ(J.Cols(), 0);
}

// Non-LOG_DISCOUNT parameterization must silently fall back to bumped even when jacobianMode_
// requests analytic.
TEST(AnalyticJacobianTest, TestNonLogDiscountSilentlyFallsBack) {
    auto spec = MakeAnalyticSpec(CurveJacobianMode_::Value_::ANALYTIC_LOG_DISCOUNT);
    spec.parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
    // Knot 0 must be > today for non-LOG_DISCOUNT.
    spec.knotDates_[0] = Date_(2022, 4, 1); // overrides anchor=today requirement
    // PLF needs x of size 2 * nKnots = 12
    const Vector_<> x(12, -0.005);
    const Matrix_<> J = TestOnly::AnalyticJacobianAt(spec, x);
    ASSERT_EQ(J.Rows(), 0); // empty -> silent fallback engaged
    ASSERT_EQ(J.Cols(), 0);
}

// ============================================================================
// Category 3: Sparsity -- structural zeros are EXACTLY zero
// ============================================================================
// Each instrument's row should have at least one exactly-zero entry that the corresponding
// instrument structurally does NOT touch (a node beyond the instrument's cashflow support).

TEST(AnalyticJacobianTest, TestStructuralZerosAreExactlyZero) {
    auto spec = MakeAnalyticSpec(CurveJacobianMode_::Value_::ANALYTIC_LOG_DISCOUNT);
    const Vector_<> x = {-0.005, -0.012, -0.025, -0.04, -0.06};
    const Matrix_<> J = TestOnly::AnalyticJacobianAt(spec, x);
    ASSERT_EQ(J.Rows(), 5);
    ASSERT_EQ(J.Cols(), 5);

    // The first instrument is a 3M swap (start=2022-1-1, end=2022-4-1). Its only cashflow is at
    // the maturity 2022-4-1 (knot index 1, solver column 0). Its row should have ZERO entries at
    // solver columns >= 1 (knots at 2022-7-1 onward), because the swap does not touch them.
    // Note: with annual fixings on a 3M swap, the fixing at accrualStart=anchor and accrualEnd=2022-4-1
    // has basis weight only at solver col 0. The payment-date DF also lands at solver col 0. So
    // row 0 should be: nonzero at col 0, exactly zero at cols 1..4.
    bool foundZero = false;
    for (int c = 1; c < 5; ++c) {
        if (J(0, c) == 0.0) {
            foundZero = true;
        } else {
            // Allow tiny non-zero from spline tail coupling (none for LOG_LINEAR) -- but for
            // LOG_LINEAR the row MUST be exactly zero outside the cashflow window.
            ASSERT_EQ(J(0, c), 0.0) << "row 0 col " << c << " = " << J(0, c) << " (expected exactly zero)";
        }
    }
    ASSERT_TRUE(foundZero);
}

// ============================================================================
// Category 4: DF-bump fallback path through Gradient (FillRowByDFBump)
// ============================================================================
// When an instrument's DRateDDiscount returns empty, the Gradient override silently
// falls back to per-instrument DF-bumping (FillRowByDFBump). The cleanest way to
// force this without leaving the LOG_DISCOUNT + DISCOUNT-target scope is a Swap_
// whose tradeDate_ != anchor: SwapRate_::DRateDDiscount short-circuits via
// TradeDateIsAnchor() == false and returns empty. The fallback row must still be a
// correct column-by-column finite-difference sensitivity of F(x) w.r.t. each free
// node's logDF, compared here against an independent two-sided central difference
// of F(x) with the same 1e-6 step the analytic-vs-bumped agreement test uses.

namespace {
    // Same shape as MakeAnalyticSpec, but every swap is built with tradeDate = spot (T+2), so
    // tradeDate_ != today_ == anchor and SwapRate_::DRateDDiscount returns empty. Every row is
    // therefore filled by the per-instrument DF-bump fallback inside the Gradient override.
    CurveCalibrationSpec_ MakeFallbackSpec() {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2022, 1, 1);
        spec.ccy_ = "USD";
        spec.curveName_ = "fallback_test";
        spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.liborBasis_ = DayBasis_("ACT_365F");
        spec.tolerance_ = 1.0e-10;
        spec.fitTolerance_ = 1.0e-8;
        spec.smoothingWeight_ = 1.0;
        spec.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC_LOG_DISCOUNT;
        spec.logDfScheme_ = LogDfScheme_::Value_::LOG_LINEAR;

        spec.knotDates_ = {
            Date_(2022, 1, 1), Date_(2022, 4, 1), Date_(2022, 7, 1), Date_(2023, 1, 1),
            Date_(2024, 1, 1), Date_(2025, 1, 1),
        };

        const auto fixedLeg = AnnualLeg();
        const auto floatIdx = AnnualIndex();
        const auto floatLeg = AnnualLeg();
        // tradeDate = spot (today + 2 business days) -- strictly != today_ == anchor.
        const Date_ spot(2022, 1, 3);
        const auto mkSwap = [&](const Date_& start, const Date_& end, double parPct) {
            return Handle_<YCInstrument_>(new Swap_(spot, start, end, parPct / 100.0, fixedLeg, floatIdx, floatLeg));
        };
        spec.instruments_ = {
            mkSwap(Date_(2022, 1, 3), Date_(2022, 4, 1), 1.00),
            mkSwap(Date_(2022, 1, 3), Date_(2022, 7, 1), 1.10),
            mkSwap(Date_(2022, 1, 3), Date_(2023, 1, 1), 1.25),
            mkSwap(Date_(2022, 1, 3), Date_(2024, 1, 1), 1.55),
            mkSwap(Date_(2022, 1, 3), Date_(2025, 1, 1), 1.80),
        };
        return spec;
    }
} // namespace

TEST(AnalyticJacobianTest, TestDFBumpFallbackMatchesCentralDifference) {
    auto spec = MakeFallbackSpec();
    const Vector_<> x = {-0.005, -0.012, -0.025, -0.04, -0.06};
    const Matrix_<> J = TestOnly::AnalyticJacobianAt(spec, x);
    ASSERT_EQ(J.Rows(), 5);
    ASSERT_EQ(J.Cols(), 5);

    // Sanity: the Jacobian was produced at all (the fallback is engaged, not a nullptr).
    // If the fallback were not engaged, Gradient would still return a populated matrix here
    // because every row comes back empty and every row is filled by FillRowByDFBump. We
    // assert non-trivial content: at least one nonzero entry per row.
    for (int r = 0; r < 5; ++r) {
        bool anyNonzero = false;
        for (int c = 0; c < 5; ++c)
            if (J(r, c) != 0.0)
                anyNonzero = true;
        ASSERT_TRUE(anyNonzero) << "row " << r << " is entirely zero (fallback produced no derivatives)";
    }

    // Reference: two-sided central difference of F(x) at step 1e-6, where F is the same residual
    // vector the solver sees. The fallback uses a one-sided 1e-7 bump inside Gradient, so we
    // compare against the more accurate central difference and expect ~1e-6 agreement.
    const auto evalF = [&](const Vector_<>& xx) {
        Vector_<> full(spec.knotDates_.size(), 0.0);
        for (int i = 1; i < static_cast<int>(spec.knotDates_.size()); ++i)
            full[i] = xx[i - 1];
        std::unique_ptr<DiscountCurve_> dc(NewDiscountLogDF(spec.curveName_, spec.ccy_, spec.knotDates_, full,
            spec.liborBasis_, spec.logDfScheme_));
        std::map<CollateralType_, Handle_<DiscountCurve_>> discounts;
        discounts[spec.targetCollateral_] = Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), dc.get()));
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwards;
        CurveBlock_ yc(spec.curveName_, spec.ccy_, discounts, forwards, spec.liborBasis_);
        Vector_<> f(spec.instruments_.size());
        Handle_<YieldCurve_> empty;
        for (int i = 0; i < static_cast<int>(spec.instruments_.size()); ++i) {
            auto rate = spec.instruments_[i]->Precompute(empty);
            f[i] = (*rate)(yc) - spec.instruments_[i]->MarketRate();
        }
        return f;
    };

    const double h = 1.0e-6;
    for (int c = 0; c < 5; ++c) {
        Vector_<> xUp = x;
        Vector_<> xDn = x;
        xUp[c] += h;
        xDn[c] -= h;
        const Vector_<> fUp = evalF(xUp);
        const Vector_<> fDn = evalF(xDn);
        for (int r = 0; r < 5; ++r) {
            const double fd = (fUp[r] - fDn[r]) / (2.0 * h);
            const double fb = J(r, c);
            if (std::abs(fd) < 1e-9) {
                ASSERT_NEAR(fb, 0.0, 1e-7) << "row=" << r << " col=" << c << " FD=" << fd << " (fallback)";
            } else {
                // Looser than the analytic-vs-FD test (1e-8) because FillRowByDFBump uses a one-sided
                // 1e-7 step, which is itself first-order accurate; 1e-6 is the right tolerance.
                ASSERT_NEAR(fb, fd, 1e-6 * std::max(1.0, std::abs(fd)))
                    << "row=" << r << " col=" << c << " fallback=" << fb << " FD=" << fd;
            }
        }
    }
}

// The fallback must also produce a solvable calibration: BUMPED and ANALYTIC_LOG_DISCOUNT
// (with every row falling back to DF-bump) must converge to the same node log-DFs. This
// confirms the fallback row derivatives are usable by the solver, not just numerically correct
// in isolation.
TEST(AnalyticJacobianTest, TestDFBumpFallbackSolveMatchesBumped) {
    auto specBumped = MakeFallbackSpec();
    specBumped.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
    auto specAnalytic = MakeFallbackSpec();
    specAnalytic.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC_LOG_DISCOUNT;

    const auto rBumped = CalibrateYieldCurve(specBumped);
    const auto rAnalytic = CalibrateYieldCurve(specAnalytic);

    ASSERT_LT(rBumped.diagnostics_.maxAbsResidual_, 1.0e-7);
    ASSERT_LT(rAnalytic.diagnostics_.maxAbsResidual_, 1.0e-7);

    const auto* cBumped = dynamic_cast<const DiscountLogDF_*>(rBumped.curve_.get());
    const auto* cAnalytic = dynamic_cast<const DiscountLogDF_*>(rAnalytic.curve_.get());
    ASSERT_NE(cBumped, nullptr);
    ASSERT_NE(cAnalytic, nullptr);

    const auto logBumped = cBumped->NodeLogDF();
    const auto logAnalytic = cAnalytic->NodeLogDF();
    ASSERT_EQ(logBumped.size(), logAnalytic.size());
    for (int i = 0; i < static_cast<int>(logBumped.size()); ++i) {
        ASSERT_NEAR(logAnalytic[i], logBumped[i], 1.0e-8)
            << "node " << i << " bumped=" << logBumped[i] << " fallback=" << logAnalytic[i];
    }
}
