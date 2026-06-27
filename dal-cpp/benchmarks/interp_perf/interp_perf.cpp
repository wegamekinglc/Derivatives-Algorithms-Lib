//
// Created by dal-implementer on 2026-6-28.
//
// Interpolation micro-benchmark.
// Builds a cubic interpolator on 50 knots and measures the full sweep over
// 10K monotonically-increasing query points, the dominant inner loop in
// PDE/spline evaluation paths.

#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/math/vectors.hpp>
#include <dal/benchmarks/bench.hpp>

using namespace Dal;

namespace {
    constexpr int kNumKnots = 50;
    constexpr int kNumQueries = 10000;
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

    return 0;
}
