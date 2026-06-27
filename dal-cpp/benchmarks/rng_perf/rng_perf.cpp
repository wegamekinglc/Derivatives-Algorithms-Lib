//
// Created by dal-implementer on 2026-6-28.
//
// Sobol random-number-generator micro-benchmark.
// Builds a SobolRSG_ (dimension 10) and measures FillNormal and FillUniform
// over a 100K-path batch, the dominant MC inner loop.

#include <dal/platform/platform.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/math/vectors.hpp>
#include <dal/benchmarks/bench.hpp>

using namespace Dal;

namespace {
    constexpr int kDim = 10;
    constexpr int kNumPaths = 100000;
} // namespace

int main() {
    constexpr int kRepeats = 10;
    Bench::PrintHeader();

    {
        double sink = 0.0;
        auto r = Bench::Run("Sobol FillNormal (100K x 10D)", [&]() {
            std::unique_ptr<SequenceSet_> rsg(NewSobol(kDim, 0));
            Vector_<> dst(kDim);
            for (int i = 0; i < kNumPaths; ++i) {
                rsg->FillNormal(&dst);
                sink += dst[0];
            }
        }, 2, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    {
        double sink = 0.0;
        auto r = Bench::Run("Sobol FillUniform (100K x 10D)", [&]() {
            std::unique_ptr<SequenceSet_> rsg(NewSobol(kDim, 0));
            Vector_<> dst(kDim);
            for (int i = 0; i < kNumPaths; ++i) {
                rsg->FillUniform(&dst);
                sink += dst[0];
            }
        }, 2, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}
