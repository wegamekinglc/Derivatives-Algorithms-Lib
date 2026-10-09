//
// Created by dal-implementer on 2026-6-28.
//
// AAD tape micro-benchmarks: Clear vs Rewind vs ZeroAdjoints vs PropagateToStart.
// Baselines the per-MC-batch tape-management cost so future optimizations
// (e.g. tape reuse, lazy clear) can be measured against the current native backend.

#include <array>
#include <utility>

#include <dal/benchmarks/aad.hpp>
#include <dal/benchmarks/bench.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/platform/platform.hpp>

#include "blackscholessegmentedperf.hpp"
#include "bumpoveraadperf.hpp"
#include "segmentedmontecarloperf.hpp"
#include "segmentedpathperf.hpp"

using namespace Dal;
using namespace Dal::AAD;

namespace {
    using BenchmarkRunner_ = int (*)(int, char**);
    BenchmarkRunner_ SelectedBenchmark(int argc, char** argv) {
        if (argc < 2)
            return nullptr;
        constexpr std::array<std::pair<const char*, BenchmarkRunner_>, 4> COMMANDS{{
            {"--bump-over-aad", RunBumpOverAADBenchmarks},
            {"--financial-segmented-mc", RunSegmentedMonteCarloBenchmarks},
            {"--financial-segmented-path", RunBlackScholesSegmentedBenchmarks},
            {"--segmented-path", RunSegmentedPathBenchmarks},
        }};
        for (const auto& command : COMMANDS)
            if (std::string(argv[1]) == command.first)
                return command.second;
        return nullptr;
    }

    // Historical case labels are retained; actual counts include active constants and expression fusion.
    constexpr int kChainSteps = 50000;

    template <bool A_ = true> Number_ BuildChain(const Number_& seed) {
        Number_ x = seed;
        for (int i = 0; i < kChainSteps; ++i) {
            if constexpr (A_)
                x = x * Number_(1.0001) + Number_(0.0001);
            else
                x = x * 1.0001 + 0.0001;
        }
        return x;
    }

    double ExpectedDerivative() { return std::pow(1.0001, kChainSteps); }

    void RunPassiveCases(int repeats, bool diagnostics) {
        double sink = 0.0;
        Bench::Print(Bench::Run(
            "Rewind + passive-constant recording (50K steps)",
            [&]() {
                Rewind(*Tape());
                Number_ seed(1.0);
                const Number_ top = BuildChain<false>(seed);
                sink += Value(top);
            },
            3, repeats));
        Rewind(*Tape());
        Number_ seed(1.0);
        const Number_ top = BuildChain<false>(seed);
        Bench::VerifyAadResult(Value(top), 2.0 * ExpectedDerivative() - 1.0);
        if (diagnostics)
            Bench::PrintTapeStatistics("passive constants", *Tape(), 1, 1);
        Bench::Print(Bench::Run(
            "PropagateToStart passive constants (50K steps)",
            [&]() {
                ZeroAdjoints(*Tape());
                Adjoint(top) = 1.0;
                PropagateToStart(*Tape());
                sink += Adjoint(seed);
            },
            3, repeats));
        Bench::VerifyAadResult(Adjoint(seed), ExpectedDerivative());
        Bench::DoNotOptimize(&sink);
    }

    void RunVectorCase(size_t width, const char* name, int repeats, bool diagnostics) {
        Clear(*Tape());
        auto resetter = SetNumResultsForAAD(true, width);
        Number_ seed(1.0);
        const Number_ top = BuildChain(seed);
        Bench::VerifyAadResult(Value(top), 2.0 * ExpectedDerivative() - 1.0);
        Bench::DoNotOptimize(&top);
        if (diagnostics)
            Bench::PrintTapeStatistics(name, *Tape(), 1, 1);
        const auto topNode = std::prev(Tape()->nodes_.End());
        const auto seedNode = Tape()->nodes_.Begin();
        double sink = 0.0;
        Bench::Print(Bench::Run(
            name,
            [&]() {
                for (size_t j = 0; j < width; ++j)
                    topNode->Adjoint(j) = 1.0;
                PropagateToStart(*Tape());
                sink += seedNode->Adjoint(0);
            },
            3, repeats));
        for (size_t j = 0; j < width; ++j)
            Bench::VerifyAadResult(seedNode->Adjoint(j), ExpectedDerivative() * (repeats + 3));
        Bench::DoNotOptimize(&sink);
        Clear(*Tape());
    }
} // namespace

int main(int argc, char** argv) {
    if (const auto run = SelectedBenchmark(argc, argv))
        return run(argc - 1, argv + 1);
    const bool diagnostics = argc == 2 && std::string(argv[1]) == "--diagnostics";
    if (argc > 1 && !diagnostics)
        return 2;
    constexpr int kRepeats = 100;
    constexpr int kZeroRepeats = 1000;

    Bench::PrintHeader();

    // Clear + re-record: full teardown + rebuild (the current per-MC-batch pattern).
    {
        double sink = 0.0;
        auto r = Bench::Run(
            "Clear + re-record (100K nodes)",
            [&]() {
                Clear(*Tape());
                Number_ s(1.0);
                PutOnTape(s);
                Number_ v = BuildChain(s);
                sink += Value(v);
            },
            3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    // Rewind + re-record: reuses the blocklists, re-registers inputs, recomputes the chain.
    {
        double sink = 0.0;
        auto r = Bench::Run(
            "Rewind + re-record (100K nodes)",
            [&]() {
                Rewind(*Tape());
                Number_ s(1.0);
                PutOnTape(s);
                Number_ v = BuildChain(s);
                sink += Value(v);
            },
            3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    // Retain fresh handles after the timed Clear/Rewind cases; their discarded graphs cannot own these values.
    Rewind(*Tape());
    Number_ seed(1.0);
    PutOnTape(seed);
    const Number_ top = BuildChain(seed);
    Bench::VerifyAadResult(Value(top), 2.0 * ExpectedDerivative() - 1.0);
    Bench::DoNotOptimize(&top);
    if (diagnostics)
        Bench::PrintTapeStatistics("active constants", *Tape(), 1, 1);

    // ZeroAdjoints sweep: tape already recorded, just zero all adjoints in place.
    {
        auto r = Bench::Run("ZeroAdjoints sweep (100K nodes)", [&]() { ZeroAdjoints(*Tape()); }, 3, kZeroRepeats);
        Bench::Print(r);
    }

    // PropagateToStart: seed the top, sweep adjoints back to the seed.
    {
        double sink = 0.0;
        auto r = Bench::Run(
            "PropagateToStart (100K nodes)",
            [&]() {
                ZeroAdjoints(*Tape());
                Adjoint(top) = 1.0;
                PropagateToStart(*Tape());
                sink += Adjoint(seed);
            },
            3, kRepeats);
        Bench::Print(r);
        Bench::VerifyAadResult(Adjoint(seed), ExpectedDerivative());
        Bench::DoNotOptimize(&sink);
    }

    RunVectorCase(10, "PropagateToStart multi-mode (100K nodes, 10 results)", kRepeats, diagnostics);
    RunPassiveCases(kRepeats, diagnostics);
    RunVectorCase(1, "PropagateToStart multi-mode (50K steps, 1 result)", kRepeats, diagnostics);
    RunVectorCase(4, "PropagateToStart multi-mode (50K steps, 4 results)", kRepeats, diagnostics);
    RunVectorCase(16, "PropagateToStart multi-mode (50K steps, 16 results)", kRepeats, diagnostics);
    RunVectorCase(64, "PropagateToStart multi-mode (50K steps, 64 results)", kRepeats, diagnostics);
    Clear(*Tape());

    return RunSegmentedPathBenchmarks(1, argv);
}
