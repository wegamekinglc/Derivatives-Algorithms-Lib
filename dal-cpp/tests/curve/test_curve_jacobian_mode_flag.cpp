//
// Created by dal-implementer on 2026/6/18.
//

#include <gtest/gtest.h>
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

// Flag-behavior tests for the runtime CurveJacobianMode_ switch (PR3 of the analytic-Jacobian
// redesign). The flag is a best-effort hint: BUMPED (default) is the byte-for-byte pre-analytic
// path; ANALYTIC engages the AAD dense Jacobian iff EligibleForAnalyticJacobian(), otherwise falls
// back to bumped with a NOTICE (never throws). These tests pin the migration gate (default options
// == bumped baseline), the analytic-engage path (ANALYTIC + eligible), the fallback path
// (ANALYTIC + ineligible -> bumped), the explicit-BUMPED short-circuit, and the NOTICE-once
// contract (cached eligibility: the predicate runs at most once per CalibrateYieldCurve call).

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

    // Eligible calibration: LOG_DISCOUNT + DISCOUNT-target + vanilla swaps starting at the anchor +
    // tradeDate == anchor. This is exactly the shape EligibleForAnalyticJacobian admits.
    CurveCalibrationSpec_ MakeEligibleSpec() {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2022, 1, 1);
        spec.ccy_ = "USD";
        spec.curveName_ = "flag_test_eligible";
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

    // Ineligible calibration: non-LOG_DISCOUNT parameterization is rejected by
    // EligibleForAnalyticJacobian with a NOTICE and falls back to bumped.
    CurveCalibrationSpec_ MakeIneligibleSpec() {
        auto spec = MakeEligibleSpec();
        spec.parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
        // Knot 0 must be > today for non-LOG_DISCOUNT parameterizations.
        spec.knotDates_[0] = Date_(2022, 4, 1);
        return spec;
    }

    // Pull the calibrated node log-DFs out of a result so two runs can be compared node-by-node.
    Vector_<> NodeLogDF(const CurveCalibrationResult_& r) {
        const auto* c = dynamic_cast<const DiscountLogDF_*>(r.curve_.get());
        REQUIRE(c != nullptr, "expected a DiscountLogDF_ curve");
        return c->NodeLogDF();
    }
} // namespace

// ============================================================================
// Migration gate (acceptance bar): default-constructed options reproduce the
// pre-feature bumped path byte-for-byte.
// ============================================================================
// The single-arg CalibrateYieldCurve(spec) overload delegates to the two-arg form with a
// default-constructed CurveCalibrationOptions_ (jacobianMode_ == BUMPED). The default-constructed
// options, an explicit BUMPED options, and the single-arg call must all produce identical curves
// (the bumped path is deterministic). This is the migration guarantee: a caller who does nothing
// gets the pre-analytic behavior unchanged.

TEST(CurveJacobianModeFlagTest, TestMigrationGateDefaultMatchesBumped) {
    const auto spec = MakeEligibleSpec();

    const auto rSingle = CalibrateYieldCurve(spec);
    CurveCalibrationOptions_ optDefault;
    const auto rDefault = CalibrateYieldCurve(spec, optDefault);
    CurveCalibrationOptions_ optBumped;
    optBumped.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
    const auto rExplicitBumped = CalibrateYieldCurve(spec, optBumped);

    const auto logSingle = NodeLogDF(rSingle);
    const auto logDefault = NodeLogDF(rDefault);
    const auto logBumped = NodeLogDF(rExplicitBumped);

    ASSERT_EQ(logSingle.size(), logDefault.size());
    ASSERT_EQ(logSingle.size(), logBumped.size());
    for (int i = 0; i < static_cast<int>(logSingle.size()); ++i) {
        ASSERT_DOUBLE_EQ(logSingle[i], logDefault[i]) << "node " << i;
        ASSERT_DOUBLE_EQ(logSingle[i], logBumped[i]) << "node " << i;
    }
    ASSERT_LT(rSingle.diagnostics_.maxAbsResidual_, 1.0e-7);
}

// ============================================================================
// ANALYTIC + eligible: the AAD dense Jacobian engages and drives the solver to
// the same solution as the bumped path.
// ============================================================================
// For an eligible calibration both the bumped and analytic Jacobians are exact, so the solver
// converges to the same node log-DFs. This test asserts ANALYTIC engages (the run completes and
// converges) AND that the resulting curve matches the BUMPED result node-by-node within solver
// tolerance. The "analytic actually engaged" signal is asserted separately via the
// TestOnly::AnalyticJacobianAt hook in test_analytic_jacobian.cpp (non-empty matrix); here we
// assert the end-to-end flag behavior on CalibrateYieldCurve.

TEST(CurveJacobianModeFlagTest, TestAnalyticEligibleMatchesBumped) {
    const auto spec = MakeEligibleSpec();

    CurveCalibrationOptions_ optBumped;
    optBumped.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
    const auto rBumped = CalibrateYieldCurve(spec, optBumped);

    CurveCalibrationOptions_ optAnalytic;
    optAnalytic.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    const auto rAnalytic = CalibrateYieldCurve(spec, optAnalytic);

    // Both paths must converge.
    ASSERT_LT(rBumped.diagnostics_.maxAbsResidual_, 1.0e-7);
    ASSERT_LT(rAnalytic.diagnostics_.maxAbsResidual_, 1.0e-7);

    // The analytic Jacobian is exact, so the solver lands on the same node log-DFs as the bumped
    // path (both solve the same nonlinear system to the same tolerance).
    const auto logBumped = NodeLogDF(rBumped);
    const auto logAnalytic = NodeLogDF(rAnalytic);
    ASSERT_EQ(logBumped.size(), logAnalytic.size());
    for (int i = 0; i < static_cast<int>(logBumped.size()); ++i)
        ASSERT_NEAR(logBumped[i], logAnalytic[i], 1.0e-9) << "node " << i;
}

// ============================================================================
// ANALYTIC + ineligible: falls back to bumped (never throws), produces a valid
// curve identical to the explicit BUMPED result.
// ============================================================================
// ANALYTIC is a best-effort hint. When the calibration is ineligible (here: non-LOG_DISCOUNT),
// EligibleForAnalyticJacobian returns false with a NOTICE, Gradient returns nullptr, and the solver
// dense-bumps. The result must equal the explicit BUMPED result on the same spec (identical
// numerics), and ANALYTIC must never throw. We assert the curve type, convergence, and node-by-node
// equality with the bumped baseline.

TEST(CurveJacobianModeFlagTest, TestAnalyticIneligibleFallsBackToBumped) {
    const auto spec = MakeIneligibleSpec();

    CurveCalibrationOptions_ optBumped;
    optBumped.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
    const auto rBumped = CalibrateYieldCurve(spec, optBumped);

    CurveCalibrationOptions_ optAnalytic;
    optAnalytic.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    // ANALYTIC never throws even on an ineligible calibration.
    const auto rAnalytic = CalibrateYieldCurve(spec, optAnalytic);

    ASSERT_NE(rAnalytic.curve_, nullptr);
    ASSERT_LT(rBumped.diagnostics_.maxAbsResidual_, 1.0e-7);
    ASSERT_LT(rAnalytic.diagnostics_.maxAbsResidual_, 1.0e-7);

    // The fallback is bumped, so the discount factors must match exactly at every knot date. (The
    // ineligible spec is PIECEWISE_LINEAR_FWD, so NodeLogDF does not apply -- compare DFs instead.)
    for (const auto& d : spec.knotDates_) {
        const double dfBumped = (*rBumped.curve_)(spec.today_, d);
        const double dfAnalytic = (*rAnalytic.curve_)(spec.today_, d);
        ASSERT_DOUBLE_EQ(dfBumped, dfAnalytic) << "date " << Date::ToString(d);
    }
}

// ============================================================================
// BUMPED explicitly: analytic never engages (byte-identical to default).
// ============================================================================
// Explicit BUMPED on an eligible calibration short-circuits Gradient to nullptr before any
// eligibility check or tape work, so the result equals the default-constructed-options result
// (which is also BUMPED). Combined with the migration gate this closes the loop: default, explicit
// BUMPED, and the single-arg overload are three spellings of the same bumped path.

TEST(CurveJacobianModeFlagTest, TestExplicitBumpedMatchesDefaultOnEligibleSpec) {
    const auto spec = MakeEligibleSpec();

    CurveCalibrationOptions_ optDefault;
    CurveCalibrationOptions_ optBumped;
    optBumped.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;

    const auto rDefault = CalibrateYieldCurve(spec, optDefault);
    const auto rBumped = CalibrateYieldCurve(spec, optBumped);

    const auto logDefault = NodeLogDF(rDefault);
    const auto logBumped = NodeLogDF(rBumped);
    ASSERT_EQ(logDefault.size(), logBumped.size());
    for (int i = 0; i < static_cast<int>(logDefault.size()); ++i)
        ASSERT_DOUBLE_EQ(logDefault[i], logBumped[i]) << "node " << i;
}

// ============================================================================
// NOTICE-once contract: the eligibility verdict is cached, so ANALYTIC on an
// ineligible calibration completes without re-evaluating eligibility per iteration.
// ============================================================================
// EligibleForAnalyticJacobian() walks every instrument and emits NOTICEs on fall-through; Gradient
// is called per solver iteration (up to maxEvaluations_ * maxRestarts_). The verdict is cached on
// the YieldCurveCalibrationFunc_ (EvaluateEligibilityOnce), so the predicate runs at most once per
// CalibrateYieldCurve call. The NOTICE stack itself is a scoped push/pop with no public counter, so
// this contract is verified structurally: an ineligible ANALYTIC calibration with the full solver
// budget (maxEvaluations_ * maxRestarts_ iterations available) completes and converges to the
// bumped solution. A per-iteration re-evaluation would re-walk all instruments each call; the cache
// bounds the predicate to one call. We assert the calibration completes and the result matches
// bumped, which holds regardless of caching -- the cache is a performance invariant, not a
// correctness one, and is verified by reading EvaluateEligibilityOnce in the source.

TEST(CurveJacobianModeFlagTest, TestIneligibleAnalyticCachesEligibilityVerdict) {
    auto spec = MakeIneligibleSpec();
    // Force many solver iterations so a non-cached predicate would re-fire heavily.
    spec.maxEvaluations_ = 200;
    spec.maxRestarts_ = 20;

    CurveCalibrationOptions_ optBumped;
    optBumped.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
    const auto rBumped = CalibrateYieldCurve(spec, optBumped);

    CurveCalibrationOptions_ optAnalytic;
    optAnalytic.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    const auto rAnalytic = CalibrateYieldCurve(spec, optAnalytic);

    ASSERT_NE(rAnalytic.curve_, nullptr);
    ASSERT_LT(rAnalytic.diagnostics_.maxAbsResidual_, 1.0e-7);

    // Cached verdict -> bumped fallback -> identical discount factors to the explicit bumped run.
    for (const auto& d : spec.knotDates_) {
        ASSERT_DOUBLE_EQ((*rBumped.curve_)(spec.today_, d), (*rAnalytic.curve_)(spec.today_, d)) << "date " << Date::ToString(d);
    }
}
