//
// Created by dal-implementer on 2026-7-4.
//
// Implied-vol Brent root-find micro-benchmark.
// BlackIV / BachelierIV drive BracketedBrent_ / Brent_ (the only root finder in the
// codebase) -- up to 30 Brent iterations x 1 BlackOpt per iter, called from
// MertonIVS_::ImpliedVol and distribution utilities. Zero benchmark coverage before.
// 1e4 solves across a moneyness x TTE grid; each solve prices once, inverts via Brent.

#include <dal/platform/platform.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/math/vectors.hpp>
#include <dal/protocol/optiontype.hpp>
#include <dal/benchmarks/bench.hpp>

using namespace Dal;

namespace {
    constexpr int kNumMoneyness = 100;
    constexpr int kNumTte = 100;
} // namespace

int main() {
    constexpr int kRepeats = 10;
    Bench::PrintHeader();

    Vector_<> moneyness(kNumMoneyness);
    for (int i = 0; i < kNumMoneyness; ++i)
        moneyness[i] = 0.6 + 0.8 * (static_cast<double>(i) + 0.5) / static_cast<double>(kNumMoneyness);
    Vector_<> tte(kNumTte);
    for (int i = 0; i < kNumTte; ++i)
        tte[i] = 0.1 + 5.0 * (static_cast<double>(i) + 0.5) / static_cast<double>(kNumTte);

    constexpr double fwd = 100.0;
    constexpr double baseVol = 0.20;
    const OptionType_ call(OptionType_::Value_::CALL);
    const OptionType_ put(OptionType_::Value_::PUT);

    // Black: lognormal vol; Bachelier: absolute normal vol (~ baseVol * fwd keeps the
    // two scales comparable across the moneyness grid, and ensures non-trivial premiums
    // on both sides so the intrinsic-value precondition in BachelierIV holds).
    constexpr double bachVol = baseVol * fwd;

    // Moneyness windows narrow enough that Brent converges from the default guess for
    // most (moneyness, TTE) cells. A few extreme cells exhaust Brent's 30 iterations
    // and throw; the benchmark measures the solver loop cost (including failed
    // iterations), so the throw is swallowed and the loop moves on.
    Vector_<> moneynessBlack = moneyness; // [0.6, 1.4]
    Vector_<> moneynessBach(kNumMoneyness);
    for (int i = 0; i < kNumMoneyness; ++i)
        moneynessBach[i] = 0.85 + 0.30 * (static_cast<double>(i) + 0.5) / static_cast<double>(kNumMoneyness);

    auto runCase = [&](const char* name, const auto& pricer, const auto& ivSolver, const OptionType_& type, double pricerVol,
                       const Vector_<>& moneynessGrid) {
        double sink = 0.0;
        auto r = Bench::Run(name, [&]() {
            for (int j = 0; j < kNumTte; ++j) {
                const double vol = pricerVol * std::sqrt(tte[j]);
                for (int i = 0; i < kNumMoneyness; ++i) {
                    const double strike = fwd / moneynessGrid[i];
                    const double price = pricer(fwd, vol, strike, type);
                    try {
                        sink += ivSolver(fwd, strike, type, price);
                    } catch (const Dal::Exception_&) {
                        sink += price;
                    }
                }
            }
        }, 1, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    };

    runCase("BlackIV call (1e4 solves, moneyness x TTE)",
            [](double f, double v, double k, const OptionType_& t) { return Distribution::BlackOpt(f, v, k, t); },
            [](double f, double k, const OptionType_& t, double p) { return Distribution::BlackIV(f, k, t, p); },
            call, baseVol, moneynessBlack);
    runCase("BlackIV put (1e4 solves, moneyness x TTE)",
            [](double f, double v, double k, const OptionType_& t) { return Distribution::BlackOpt(f, v, k, t); },
            [](double f, double k, const OptionType_& t, double p) { return Distribution::BlackIV(f, k, t, p); },
            put, baseVol, moneynessBlack);
    runCase("BachelierIV call (1e4 solves, moneyness x TTE)",
            [](double f, double v, double k, const OptionType_& t) { return Distribution::BachelierOpt(f, v, k, t); },
            [](double f, double k, const OptionType_& t, double p) { return Distribution::BachelierIV(f, k, t, p); },
            call, bachVol, moneynessBach);
    runCase("BachelierIV put (1e4 solves, moneyness x TTE)",
            [](double f, double v, double k, const OptionType_& t) { return Distribution::BachelierOpt(f, v, k, t); },
            [](double f, double k, const OptionType_& t, double p) { return Distribution::BachelierIV(f, k, t, p); },
            put, bachVol, moneynessBach);

    return 0;
}
