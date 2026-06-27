//
// Created by dal-implementer on 2026-6-28.
//
// AAD tape micro-benchmarks: Clear vs Rewind vs ZeroAdjoints vs PropagateToStart.
// Baselines the per-MC-batch tape-management cost so future optimizations
// (e.g. tape reuse, lazy clear) can be measured against the current native backend.

#include <dal/platform/platform.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/benchmarks/bench.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    // Build a 100K-node chain: x_n = x_{n-1} * 1.0001 + 0.0001.
    // Each multiply+add records two nodes, so 50K iterations produce ~100K nodes.
    constexpr int kChainSteps = 50000;

    Number_ BuildChain(const Number_& seed) {
        Number_ x = seed;
        for (int i = 0; i < kChainSteps; ++i) {
            x = x * Number_(1.0001) + Number_(0.0001);
        }
        return x;
    }
} // namespace

int main() {
    constexpr int kRepeats = 100;
    constexpr int kZeroRepeats = 1000;

    Bench::PrintHeader();

    // Setup a fresh tape with the chain recorded once (used by ZeroAdjoints + Propagate).
    Clear(*Tape());
    Number_ seed(1.0);
    PutOnTape(seed);
    Number_ top = BuildChain(seed);
    Bench::DoNotOptimize(&top);

    // Clear + re-record: full teardown + rebuild (the current per-MC-batch pattern).
    {
        double sink = 0.0;
        auto r = Bench::Run("Clear + re-record (100K nodes)", [&]() {
            Clear(*Tape());
            Number_ s(1.0);
            PutOnTape(s);
            Number_ v = BuildChain(s);
            sink += Value(v);
        }, 3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    // Rewind + re-record: reuses the blocklists, re-registers inputs, recomputes the chain.
    {
        double sink = 0.0;
        auto r = Bench::Run("Rewind + re-record (100K nodes)", [&]() {
            Rewind(*Tape());
            Number_ s(1.0);
            PutOnTape(s);
            Number_ v = BuildChain(s);
            sink += Value(v);
        }, 3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    // ZeroAdjoints sweep: tape already recorded, just zero all adjoints in place.
    {
        auto r = Bench::Run("ZeroAdjoints sweep (100K nodes)", [&]() {
            ZeroAdjoints(*Tape());
        }, 3, kZeroRepeats);
        Bench::Print(r);
    }

    // PropagateToStart: seed the top, sweep adjoints back to the seed.
    {
        double sink = 0.0;
        auto r = Bench::Run("PropagateToStart (100K nodes)", [&]() {
            ZeroAdjoints(*Tape());
            Adjoint(top) = 1.0;
            PropagateToStart(*Tape());
            sink += Adjoint(seed);
        }, 3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}
