//
// Created by dal-implementer on 2026-6-28.
//
// Random-number-generator micro-benchmark; see docs/methodology/random.md
// (Benchmark Coverage) for the fast/precise Sobol case split and the
// precise=false pinning of the BrownianBridge / pseudo-random cases.

#include <dal/platform/platform.hpp>
#include <dal/math/random/brownianbridge.hpp>
#include <dal/math/random/pseudorandom.hpp>
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
        auto r = Bench::Run("Sobol FillNormal fast (100K x 10D)", [&]() {
            std::unique_ptr<SequenceSet_> rsg(NewSobol(kDim, 0, /*precise=*/false));
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
        auto r = Bench::Run("Sobol FillNormal precise opt-in (100K x 10D)", [&]() {
            std::unique_ptr<SequenceSet_> rsg(NewSobol(kDim, 0, /*precise=*/true, /*polish=*/true));
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
            std::unique_ptr<SequenceSet_> rsg(NewSobol(kDim, 0, /*precise=*/false));
            Vector_<> dst(kDim);
            for (int i = 0; i < kNumPaths; ++i) {
                rsg->FillUniform(&dst);
                sink += dst[0];
            }
        }, 2, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    {
        double sink = 0.0;
        auto r = Bench::Run("BrownianBridge FillNormal (100K x 10D)", [&]() {
            std::unique_ptr<Random_> inner(NewSobol(kDim, 0, /*precise=*/false));
            BrownianBridge_ bb(std::move(inner));
            Vector_<> dst(kDim);
            for (int i = 0; i < kNumPaths; ++i) {
                bb.FillNormal(&dst);
                sink += dst[0];
            }
        }, 2, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    {
        double sink = 0.0;
        auto r = Bench::Run("MRG32k3a FillNormal (100K x 10D)", [&]() {
            std::unique_ptr<Random_> rsg(New(RNGType_("MRG32"), 1024, kDim, /*precise=*/false));
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
        auto r = Bench::Run("ShuffledIRN FillNormal (100K x 10D)", [&]() {
            std::unique_ptr<Random_> rsg(New(RNGType_("IRN"), 1024, kDim, /*precise=*/false));
            Vector_<> dst(kDim);
            for (int i = 0; i < kNumPaths; ++i) {
                rsg->FillNormal(&dst);
                sink += dst[0];
            }
        }, 2, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}
