//
// Created by dal-implementer on 2026-7-4.
//
// End-to-end curve-calibration micro-benchmark.
// Drives CalibrateYieldCurve -> YieldCurveCalibrationFunc_::F -> Underdetermined::Find
// -> AnalyticJacobian and the scalar-templated curve factory on a real
// 23-instrument swap curve. jacobian_perf is synthetic (no curve, no instruments, no
// solver iteration); this is the genuine calibration hot path.
//
// Cases cross {PWC, PWL, LOG_LINEAR, LOG_CUBIC_NATURAL, MIXED} x
// {ANALYTIC, BUMPED} Jacobian mode
// and split default diagnostics from solve-only timing so candidate #6a (curve rebuild
// on every F) and #5 (duplicate at-solution Gradient) are separately measurable. An
// APPROXIMATE-mode case covers Underdetermined::Approximate + the implicit CG penalty solve (G9).
// Mirrors the instrument set from
// examples/yield_curve_jacobian/yield_curve_jacobian.cpp.

#include <memory>
#include <string>
#include <utility>
#include <vector>
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
    // Every representation uses the same 23 annual swaps and 24 future-node count.
    // PWC/PWL knots are half-year shifted so the PWC quote ladder has independent
    // residual rows; LOG_DISCOUNT retains the stable annual grid and prepends today.
    CurveCalibrationSpec_ BuildCalibrationSpec(CurveParameterization_::Value_ parameterization,
                                               LogDfScheme_::Value_ scheme = LogDfScheme_::Value_::LOG_LINEAR) {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2022, 1, 1);
        spec.ccy_ = "USD";
        spec.curveName_ = "curve_calibration_perf";
        spec.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        spec.calibrateDiscountCurve_ = true;
        spec.parameterization_ = CurveParameterization_(parameterization);
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.liborBasis_ = DayBasis_("ACT_365F");
        spec.tolerance_ = 1.0e-10;
        spec.fitTolerance_ = 1.0e-8;
        spec.smoothingWeight_ = 1.0;
        spec.logDfScheme_ = LogDfScheme_(scheme);
        spec.initialGuess_ = 0.03;

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

        constexpr int nInstruments = 23;
        constexpr int nFutureKnots = 24;
        const bool logDiscount = parameterization == CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotDates_.reserve(nFutureKnots + (logDiscount ? 1 : 0));
        if (logDiscount) {
            spec.knotDates_.push_back(spec.today_);
            for (int y = 1; y <= nFutureKnots; ++y)
                spec.knotDates_.push_back(Date_(2022 + y, 1, 1));
        } else {
            for (int y = 0; y < nFutureKnots - 1; ++y)
                spec.knotDates_.push_back(Date_(2022 + y, 7, 1));
            spec.knotDates_.push_back(Date_(2045, 1, 1));
        }

        spec.instruments_.reserve(nInstruments);
        for (int y = 1; y <= nInstruments; ++y) {
            const double frac = static_cast<double>(y - 1) / static_cast<double>(nInstruments - 1);
            const double parPct = 1.00 + 2.50 * frac;
            spec.instruments_.emplace_back(new Swap_(spec.today_, spec.today_, Date_(2022 + y, 1, 1), parPct / 100.0, leg, idx, leg));
        }
        return spec;
    }

    CurveCalibrationOptions_ OptionsFor(CurveJacobianMode_::Value_ mode, bool computeEffJacobianInverse = true, bool computeForwardJacobian = true) {
        CurveCalibrationOptions_ opts;
        opts.jacobianMode_ = CurveJacobianMode_(mode);
        opts.computeEffJacobianInverse_ = computeEffJacobianInverse;
        opts.computeForwardJacobian_ = computeForwardJacobian;
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

    const auto optsAnalytic = OptionsFor(CurveJacobianMode_::Value_::ANALYTIC);
    const auto optsAnalyticSolveOnly = OptionsFor(CurveJacobianMode_::Value_::ANALYTIC, false, false);
    const auto optsBumped = OptionsFor(CurveJacobianMode_::Value_::BUMPED);
    const auto optsBumpedSolveOnly = OptionsFor(CurveJacobianMode_::Value_::BUMPED, false, false);

    auto runCase = [&](const char* name, const CurveCalibrationSpec_& spec, const CurveCalibrationOptions_& opts) {
        double sink = 0.0;
        auto r = Bench::Run(
            name,
            [&]() {
                const auto res = CalibrateYieldCurve(spec, opts);
                sink += SinkFromResult(res);
            },
            1, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    };

    struct CurveCase_ {
        std::string name;
        CurveCalibrationSpec_ spec;
    };
    const std::vector<CurveCase_> cases = {
        {"PWC", BuildCalibrationSpec(CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD)},
        {"PWL", BuildCalibrationSpec(CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD)},
        {"LOG_LINEAR", BuildCalibrationSpec(CurveParameterization_::Value_::LOG_DISCOUNT, LogDfScheme_::Value_::LOG_LINEAR)},
        {"LOG_CUBIC_NATURAL", BuildCalibrationSpec(CurveParameterization_::Value_::LOG_DISCOUNT, LogDfScheme_::Value_::LOG_CUBIC_NATURAL)},
        {"MIXED", BuildCalibrationSpec(CurveParameterization_::Value_::LOG_DISCOUNT, LogDfScheme_::Value_::MIXED)},
    };
    for (const auto& curveCase : cases) {
        const std::string prefix = "CalibrateYieldCurve " + curveCase.name;
        runCase((prefix + " ANALYTIC +DIAG (23 swaps)").c_str(), curveCase.spec, optsAnalytic);
        runCase((prefix + " ANALYTIC SOLVE (23 swaps)").c_str(), curveCase.spec, optsAnalyticSolveOnly);
        runCase((prefix + " BUMPED +DIAG (23 swaps)").c_str(), curveCase.spec, optsBumped);
        runCase((prefix + " BUMPED SOLVE (23 swaps)").c_str(), curveCase.spec, optsBumpedSolveOnly);
    }

    // APPROXIMATE-mode case (G9): drives Underdetermined::Approximate + XDecompByCG_
    // with the selected analytic Jacobian in the implicit (W + J^T J) penalty solve,
    // so it remains a distinct hot path from the EXACT cases above.
    {
        CurveCalibrationSpec_ specApprox = BuildCalibrationSpec(CurveParameterization_::Value_::LOG_DISCOUNT, LogDfScheme_::Value_::LOG_LINEAR);
        specApprox.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
        double sink = 0.0;
        auto r = Bench::Run(
            "CalibrateYieldCurve LOG_LINEAR APPROXIMATE (23 swaps)",
            [&]() {
                const auto res = CalibrateYieldCurve(specApprox, optsAnalytic);
                sink += SinkFromResult(res);
            },
            1, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}
