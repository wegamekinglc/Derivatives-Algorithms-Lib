//
// Created by dal-implementer on 2026-7-4.
//
// InverseNCDF micro-benchmark.
// InverseNCDF is the per-deviate cost inside every FillNormal (Sobol, PseudoRandom,
// BrownianBridge inner RSG) -- ~82% of FillNormal per -fopt-info-vec-missed. rng_perf
// bundles it with Sobol direction XORs, so the no-polish Acklam body cannot be
// isolated. This target times InverseNCDF directly over 1e6 deviates, polish off/on.

#include <dal/platform/platform.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/math/vectors.hpp>
#include <dal/benchmarks/bench.hpp>

using namespace Dal;

namespace {
    constexpr int kNumDev = 1000000;
} // namespace

int main() {
    constexpr int kRepeats = 10;
    Bench::PrintHeader();

    Vector_<> p(kNumDev);
    for (int i = 0; i < kNumDev; ++i)
        p[i] = (static_cast<double>(i) + 0.5) / static_cast<double>(kNumDev);

    {
        double sink = 0.0;
        auto r = Bench::Run("InverseNCDF polish=false (1e6 deviates)", [&]() {
            for (int i = 0; i < kNumDev; ++i)
                sink += InverseNCDF(p[i], true, false);
        }, 2, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    {
        double sink = 0.0;
        auto r = Bench::Run("InverseNCDF polish=true (1e6 deviates)", [&]() {
            for (int i = 0; i < kNumDev; ++i)
                sink += InverseNCDF(p[i], true, true);
        }, 2, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}
