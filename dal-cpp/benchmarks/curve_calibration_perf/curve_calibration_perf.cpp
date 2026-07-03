//
// Created by dal-implementer on 2026-7-4.
//
// End-to-end curve-calibration micro-benchmark.
// Drives CalibrateYieldCurve -> YieldCurveCalibrationFunc_::F -> Underdetermined::Find
// -> AnalyticJacobian + yclogdf RebuildInterp / BuildNaturalCubicFppCoef on a real
// 24-instrument swap curve. jacobian_perf is synthetic (no curve, no instruments, no
// solver iteration); this is the genuine calibration hot path.
//
// Four cases cross {LOG_LINEAR, LOG_CUBIC_NATURAL} x {ANALYTIC, BUMPED} Jacobian mode
// so candidate #6a (curve rebuild on every F) and #5 (duplicate at-solution Gradient)
// are both measurable. A fifth APPROXIMATE-mode case covers Underdetermined::Approximate
// + the implicit CG penalty solve (G9). Mirrors the instrument set from
// examples/yield_curve_jacobian/yield_curve_jacobian.cpp.

#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/platform/initall.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/math/vectors.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/benchmarks/bench.hpp>

using namespace Dal;

namespace {
    // 24-instrument vanilla-swap Phase A calibration: 24 instruments on 25 annual knots
    // (24 free params + the today_ anchor), square so EXACT converges and
    // CurveJacobianMode_::ANALYTIC engages the AAD tape.
    CurveCalibrationSpec_ BuildCalibrationSpec(LogDfScheme_::Value_ scheme) {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2022, 1, 1);
        spec.ccy_ = "USD";
        spec.curveName_ = "curve_calibration_perf";
        spec.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        spec.calibrateDiscountCurve_ = true;
        spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.liborBasis_ = DayBasis_("ACT_365F");
        spec.tolerance_ = 1.0e-10;
        spec.fitTolerance_ = 1.0e-8;
        spec.smoothingWeight_ = 1.0;
        spec.logDfScheme_ = LogDfScheme_(scheme);

        RateLegConvention_ leg;
        leg.paymentLag_ = 0;
        leg.paymentFrequency_ = PeriodLength_("12M");
        leg.dayBasis_ = DayBasis_("ACT_365F");
        leg.accrualHolidays_ = Holidays::None();
        leg.paymentHolidays_ = Holidays::None();
        leg.businessDayConvention_ = BizDayConvention_("Unadjusted");
        leg.paymentConvention_ = BizDayConvention_("Unadjusted");

        RateIndexConvention_ idx;
        idx.forecastTenor_ = PeriodLength_("12M");
        idx.dayBasis_ = DayBasis_("ACT_365F");
        idx.fixingLag_ = 0;
        idx.spotLag_ = 0;
        idx.fixingHolidays_ = Holidays::None();
        idx.accrualHolidays_ = Holidays::None();
        idx.businessDayConvention_ = BizDayConvention_("Unadjusted");
        idx.useProjectionCurve_ = false;

        constexpr int nInstruments = 24;
        spec.knotDates_.reserve(nInstruments + 1);
        spec.knotDates_.push_back(spec.today_);
        for (int y = 1; y <= nInstruments; ++y)
            spec.knotDates_.push_back(Date_(2022 + y, 1, 1));

        spec.instruments_.reserve(nInstruments);
        for (int y = 1; y <= nInstruments; ++y) {
            const double frac = static_cast<double>(y - 1) / static_cast<double>(nInstruments - 1);
            const double parPct = 1.00 + 2.50 * frac;
            spec.instruments_.emplace_back(new Swap_(spec.today_, spec.today_, Date_(2022 + y, 1, 1), parPct / 100.0, leg, idx, leg));
        }
        return spec;
    }

    CurveCalibrationOptions_ OptionsFor(CurveJacobianMode_::Value_ mode) {
        CurveCalibrationOptions_ opts;
        opts.jacobianMode_ = CurveJacobianMode_(mode);
        return opts;
    }

    double SinkFromResult(const CurveCalibrationResult_& res) {
        double sink = res.diagnostics_.maxAbsResidual_;
        if (!res.diagnostics_.jacobian_.Empty())
            sink += res.diagnostics_.jacobian_(0, 0);
        return sink;
    }
} // namespace

int main() {
    RegisterAll_::Init();
    constexpr int kRepeats = 5;
    Bench::PrintHeader();

    const auto specLin = BuildCalibrationSpec(LogDfScheme_::Value_::LOG_LINEAR);
    const auto specCub = BuildCalibrationSpec(LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    const auto optsAnalytic = OptionsFor(CurveJacobianMode_::Value_::ANALYTIC);
    const auto optsBumped = OptionsFor(CurveJacobianMode_::Value_::BUMPED);

    auto runCase = [&](const char* name, const CurveCalibrationSpec_& spec, const CurveCalibrationOptions_& opts) {
        double sink = 0.0;
        auto r = Bench::Run(name, [&]() {
            const auto res = CalibrateYieldCurve(spec, opts);
            sink += SinkFromResult(res);
        }, 1, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    };

    runCase("CalibrateYieldCurve LOG_LINEAR  ANALYTIC  (24 swaps)", specLin, optsAnalytic);
    runCase("CalibrateYieldCurve LOG_LINEAR  BUMPED    (24 swaps)", specLin, optsBumped);
    runCase("CalibrateYieldCurve LOG_CUBIC_NATURAL ANALYTIC (24 swaps)", specCub, optsAnalytic);
    runCase("CalibrateYieldCurve LOG_CUBIC_NATURAL BUMPED   (24 swaps)", specCub, optsBumped);

    // APPROXIMATE-mode case (G9): drives Underdetermined::Approximate + XDecompByCG_
    // implicit penalty solve. APPROXIMATE ignores the per-row AAD Jacobian and uses CG
    // on the implicit (W + J^T J) matrix, so it is a distinct hot path from the four
    // EXACT cases above.
    {
        CurveCalibrationSpec_ specApprox = BuildCalibrationSpec(LogDfScheme_::Value_::LOG_LINEAR);
        specApprox.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
        double sink = 0.0;
        auto r = Bench::Run("CalibrateYieldCurve LOG_LINEAR APPROXIMATE (24 swaps)", [&]() {
            const auto res = CalibrateYieldCurve(specApprox, optsAnalytic);
            sink += SinkFromResult(res);
        }, 1, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}
