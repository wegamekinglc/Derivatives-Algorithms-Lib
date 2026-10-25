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
// examples/curves/yield_curve_jacobian/yield_curve_jacobian.cpp.

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <dal/platform/platform.hpp>
#include <dal/platform/initall.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycpwlf.hpp>
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

    void RunPwlQueries(int nodeCount) {
        const Date_ anchor(2022, 1, 1);
        Vector_<Date_> knots;
        Vector_<> left, right;
        for (int i = 0; i < nodeCount; ++i) {
            knots.push_back(anchor.AddDays(31 * i));
            left.push_back(0.02 + 0.0001 * (i % 7));
            right.push_back(0.021 + 0.0002 * (i % 5));
        }
        const Tape::DiscountPWLF_<double> curve("queries", "USD", knots, left, right);
        Vector_<Date_> queries;
        for (int i = 0; i < 4096; ++i)
            queries.push_back(anchor.AddDays((17 * i) % (31 * nodeCount + 60) - 30));
        double sink = 0.0;
        const std::string name = "PWL DF queries (4096 x " + std::to_string(nodeCount) + " nodes)";
        const auto result = Bench::Run(name.c_str(), [&]() {
            for (const auto& date : queries)
                sink += curve(anchor, date);
        }, 1, 10);
        Bench::Print(result);
        Bench::DoNotOptimize(&sink);
    }

    struct LogDfFixture_ {
        Date_ anchor_{2025, 1, 15};
        Date_ nonAnchor_{2026, 1, 15};
        DayBasis_ dayCount_{"ACT_365F"};
        Vector_<Date_> nodes_;
        Vector_<> logDf_;
        Vector_<> baseLogDf_;
        Vector_<> times_;
        Vector_<Date_> queries_;

        LogDfFixture_() {
            for (int i = 0; i <= 21; ++i) {
                nodes_.push_back(Date_(2025 + i, 1, 15));
                const double t = dayCount_(anchor_, nodes_.back(), nullptr);
                times_.push_back(t);
                logDf_.push_back(-(0.02 + 0.0005 * i) * t);
                baseLogDf_.push_back(-(0.005 + 0.0002 * i) * t);
            }
            for (int i = 0; i < 4096; ++i)
                queries_.push_back(anchor_.AddDays(1 + (i * 1777) % 7665));
        }
    };

    double ExpectedLogDf(double query, const Vector_<>& times, const Vector_<>& values) {
        const auto knot = std::lower_bound(times.begin(), times.end(), query);
        if (knot != times.end() && *knot == query)
            return values[knot - times.begin()];
        if (query > times.back()) {
            const int n = static_cast<int>(times.size());
            const double excess = (query - times[n - 1]) / (times[n - 1] - times[n - 2]);
            return -excess * values[n - 2] + (1.0 + excess) * values[n - 1];
        }
        if (knot == times.begin())
            return values[0];
        const int upper = static_cast<int>(knot - times.begin());
        const int lower = upper - 1;
        const double weight = (query - times[lower]) / (times[upper] - times[lower]);
        return (1.0 - weight) * values[lower] + weight * values[upper];
    }

    double ExpectedDf(const LogDfFixture_& fixture, const Date_& from, const Date_& to, bool withBase) {
        const double fromTime = fixture.dayCount_(fixture.anchor_, from, nullptr);
        const double toTime = fixture.dayCount_(fixture.anchor_, to, nullptr);
        double logDf = ExpectedLogDf(toTime, fixture.times_, fixture.logDf_) - ExpectedLogDf(fromTime, fixture.times_, fixture.logDf_);
        if (withBase)
            logDf += ExpectedLogDf(toTime, fixture.times_, fixture.baseLogDf_) - ExpectedLogDf(fromTime, fixture.times_, fixture.baseLogDf_);
        return std::exp(logDf);
    }

    void RunLogDfCase(const std::string& name,
                      const LogDfFixture_& fixture,
                      const Tape::DiscountLogDF_<double>& curve,
                      const Vector_<Date_>& from,
                      const Vector_<Date_>& to,
                      bool withBase) {
        double expectedSum = 0.0;
        for (int i = 0; i < static_cast<int>(from.size()); ++i) {
            const double expected = ExpectedDf(fixture, from[i], to[i], withBase);
            REQUIRE(std::abs(curve(from[i], to[i]) - expected) <= 2.0e-12, "Log-DF query correctness check failed");
            expectedSum += expected;
        }
        double sink = 0.0;
        const auto result = Bench::Run(
            name,
            [&]() {
                for (int i = 0; i < static_cast<int>(from.size()); ++i)
                    sink += curve(from[i], to[i]);
            },
            2,
            10,
            20);
        REQUIRE(std::abs(sink - 202.0 * expectedSum) <= 1.0e-6, "Log-DF timed query sum mismatch");
        Bench::Print(result);
        Bench::DoNotOptimize(&sink);
    }

    Vector_<Date_> Repeat(const Date_& date, size_t count) { return Vector_<Date_>(count, date); }

    void RunLogDfQueries() {
        const LogDfFixture_ fixture;
        const Tape::DiscountLogDF_<double> curve(
            "queries", "USD", fixture.nodes_, fixture.logDf_, fixture.dayCount_, LogDfScheme_::Value_::LOG_LINEAR);
        const Handle_<DiscountCurve_> base(
            new Tape::DiscountLogDF_<double>("base", "USD", fixture.nodes_, fixture.baseLogDf_, fixture.dayCount_, LogDfScheme_::Value_::LOG_LINEAR));
        const Tape::DiscountLogDF_<double> withBase(
            "with_base", "USD", fixture.nodes_, fixture.logDf_, fixture.dayCount_, LogDfScheme_::Value_::LOG_LINEAR, base);
        const auto anchors = Repeat(fixture.anchor_, fixture.queries_.size());
        const auto nonAnchors = Repeat(fixture.nonAnchor_, fixture.queries_.size());

        RunLogDfCase("LogDF anchor -> 4096 queries", fixture, curve, anchors, fixture.queries_, false);
        RunLogDfCase("LogDF 4096 queries -> anchor", fixture, curve, fixture.queries_, anchors, false);
        RunLogDfCase("LogDF non-anchor -> 4096 queries", fixture, curve, nonAnchors, fixture.queries_, false);
        RunLogDfCase("LogDF non-anchor -> same non-anchor", fixture, curve, nonAnchors, nonAnchors, false);
        RunLogDfCase("LogDF anchor -> anchor", fixture, curve, anchors, anchors, false);
        RunLogDfCase("LogDF anchor -> 4096 queries with base", fixture, withBase, anchors, fixture.queries_, true);
        RunLogDfCase("LogDF non-anchor -> 4096 queries with base", fixture, withBase, nonAnchors, fixture.queries_, true);
    }
} // namespace

int main() {
    RegisterAll_::Init();
    constexpr int kRepeats = 5;
    Bench::PrintHeader();

    for (const int nodeCount : {8, 24, 64, 256})
        RunPwlQueries(nodeCount);
    RunLogDfQueries();

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
