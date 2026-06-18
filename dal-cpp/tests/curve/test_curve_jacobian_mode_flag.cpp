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

// Flag-behavior tests for the runtime CurveJacobianMode_ switch: BUMPED (default) is the
// byte-for-byte pre-analytic path; ANALYTIC engages the AAD Jacobian iff eligible, else falls
// back to bumped with a NOTICE (never throws).

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

        const auto fixedLeg = AnnualLeg();
        const auto floatIdx = AnnualIndex();
        const auto floatLeg = AnnualLeg();
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

// Migration gate: default-constructed options, explicit BUMPED, and the single-arg overload
// must all produce identical curves -- a caller who does nothing gets the pre-analytic path.

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

// ANALYTIC + eligible: both Jacobians are exact, so the analytic run must converge to the same
// node log-DFs as BUMPED. ("Analytic actually engaged" is asserted via TestOnly::AnalyticJacobianAt
// in test_analytic_jacobian.cpp; here we assert the end-to-end flag behavior.)

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

// ANALYTIC + ineligible: ANALYTIC never throws; on an ineligible spec Gradient returns nullptr
// (solver dense-bumps), so the result equals explicit BUMPED. (PLF spec -> compare DFs, not log-DFs.)

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

// Explicit BUMPED on an eligible spec short-circuits Gradient to nullptr, so it equals default
// options (also BUMPED) -- default, explicit BUMPED, and the single-arg overload are one path.

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

// NOTICE-once: eligibility is cached (EvaluateEligibilityOnce), so the predicate runs at most once
// per CalibrateYieldCurve call even though Gradient fires per iteration. The NOTICE stack has no
// counter, so this is verified structurally -- the run completes and matches bumped; the cache
// itself is a performance invariant, verified by reading the source.

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
