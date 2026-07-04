//
// Created by dal-implementer on 2026-7-4.
//
// Black / Bachelier vanilla-pricing micro-benchmark.
// BlackOpt and BachelierOpt are the per-eval cost in Dupire LV calibration (5 calls
// per LV node via IVS_::Call central differences), implied-vol inversion, and any
// analytic pricer. Branchy (option-type switch + IsZero early-out); no benchmark
// exercised them. 1e6 calls each across a moneyness sweep.

#include <dal/platform/platform.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/math/vectors.hpp>
#include <dal/protocol/optiontype.hpp>
#include <dal/benchmarks/bench.hpp>

using namespace Dal;

namespace {
    constexpr int kNumCalls = 1000000;

    // Moneyness grid dense around ATM so the type.Payout() early-out at IsZero(vol)
    // never triggers and the NCDF(d) branch is the hot path.
    Vector_<> BuildStrikes() {
        Vector_<> s(kNumCalls);
        for (int i = 0; i < kNumCalls; ++i)
            s[i] = 80.0 + 40.0 * (static_cast<double>(i) + 0.5) / static_cast<double>(kNumCalls);
        return s;
    }
} // namespace

int main() {
    constexpr int kRepeats = 10;
    Bench::PrintHeader();

    const Vector_<> strikes = BuildStrikes();
    constexpr double fwd = 100.0;
    constexpr double vol = 0.20;
    const OptionType_ call(OptionType_::Value_::CALL);
    const OptionType_ put(OptionType_::Value_::PUT);

    {
        double sink = 0.0;
        auto r = Bench::Run("BlackOpt call (1e6, moneyness sweep)", [&]() {
            for (int i = 0; i < kNumCalls; ++i)
                sink += Distribution::BlackOpt(fwd, vol, strikes[i], call);
        }, 2, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    {
        double sink = 0.0;
        auto r = Bench::Run("BlackOpt put (1e6, moneyness sweep)", [&]() {
            for (int i = 0; i < kNumCalls; ++i)
                sink += Distribution::BlackOpt(fwd, vol, strikes[i], put);
        }, 2, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    {
        double sink = 0.0;
        auto r = Bench::Run("BachelierOpt call (1e6, moneyness sweep)", [&]() {
            for (int i = 0; i < kNumCalls; ++i)
                sink += Distribution::BachelierOpt(fwd, vol, strikes[i], call);
        }, 2, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    {
        double sink = 0.0;
        auto r = Bench::Run("BachelierOpt put (1e6, moneyness sweep)", [&]() {
            for (int i = 0; i < kNumCalls; ++i)
                sink += Distribution::BachelierOpt(fwd, vol, strikes[i], put);
        }, 2, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}
