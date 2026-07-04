//
// Created by dal-implementer on 2026-6-28.
//
// Interpolation micro-benchmark.
// Builds a cubic interpolator on 50 knots and measures the full sweep over
// 10K monotonically-increasing query points, the dominant inner loop in
// PDE/spline evaluation paths. Also exercises the inlined InterpLinearImplX
// template the Dupire model calls once per MC step (1e5 paths x 200 steps over
// a 200-knot spot grid) -- the Interp1_ virtual interface the other cases use
// is NOT what the LV path touches.

#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/math/vectors.hpp>
#include <dal/benchmarks/bench.hpp>

using namespace Dal;

namespace {
    constexpr int kNumKnots = 50;
    constexpr int kNumQueries = 10000;

    // LV-style inline-linear case: 200-knot spot grid, 1e5 paths x 200 steps each.
    constexpr int kLvNumKnots = 200;
    constexpr int kLvNumPaths = 100000;
    constexpr int kLvNumSteps = 200;
} // namespace

int main() {
    constexpr int kRepeats = 10;
    Bench::PrintHeader();

    Vector_<> xKnots(kNumKnots);
    Vector_<> fKnots(kNumKnots);
    for (int i = 0; i < kNumKnots; ++i) {
        const double xv = static_cast<double>(i);
        xKnots[i] = xv;
        fKnots[i] = std::sin(xv * 0.3) + 0.01 * xv;
    }

    // Monotonic query points densely covering the knot range.
    Vector_<> queries(kNumQueries);
    const double step = (xKnots.back() - xKnots.front()) / static_cast<double>(kNumQueries - 1);
    for (int i = 0; i < kNumQueries; ++i)
        queries[i] = xKnots.front() + step * static_cast<double>(i);

    {
        double sink = 0.0;
        auto r = Bench::Run("Cubic interp (50 knots, 10K queries)", [&]() {
            Interp::Boundary_ lhs(2, 0.0);
            Interp::Boundary_ rhs(2, 0.0);
            std::unique_ptr<Interp1_> interp(Interp::NewCubic("cubic", xKnots, fKnots, lhs, rhs));
            for (int i = 0; i < kNumQueries; ++i)
                sink += (*interp)(queries[i]);
        }, 3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    {
        double sink = 0.0;
        auto r = Bench::Run("Linear interp (50 knots, 10K queries)", [&]() {
            std::unique_ptr<Interp1_> interp(Interp::NewLinear("linear", xKnots, fKnots));
            for (int i = 0; i < kNumQueries; ++i)
                sink += (*interp)(queries[i]);
        }, 3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    // Inlined linear (LV-style): the Dupire model calls InterpLinearImplX once per MC
    // step to pick the local vol before evolving logSpot -- N_steps x N_paths. This is
    // the FORCE_INLINE template the model actually links against, not the Interp1_
    // virtual interface the cases above exercise.
    {
        Vector_<> lvX(kLvNumKnots);
        Vector_<> lvY(kLvNumKnots);
        for (int i = 0; i < kLvNumKnots; ++i) {
            lvX[i] = 50.0 + static_cast<double>(i);
            lvY[i] = 0.10 + 0.001 * static_cast<double>(i) + 0.05 * std::sin(0.05 * static_cast<double>(i));
        }
        Vector_<> spotQueries(kLvNumPaths);
        for (int i = 0; i < kLvNumPaths; ++i)
            spotQueries[i] = 60.0 + 130.0 * (static_cast<double>(i) + 0.5) / static_cast<double>(kLvNumPaths);

        double sink = 0.0;
        auto r = Bench::Run("Inlined linear LV-style (1e5 paths x 200 steps, 200 knots)", [&]() {
            for (int step = 0; step < kLvNumSteps; ++step)
                for (int i = 0; i < kLvNumPaths; ++i)
                    sink += InterpLinearImplX(lvX, lvY, spotQueries[i]);
        }, 1, 3);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}
