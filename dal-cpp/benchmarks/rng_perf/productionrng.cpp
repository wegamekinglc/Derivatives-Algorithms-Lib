//
// Created by Codex on 2026/10/9.
//

#include <dal/platform/platform.hpp>

#include <dal/benchmarks/bench.hpp>
#include <dal/script/simulation.hpp>

#include "productionrng.hpp"

using namespace Dal;

void RunProductionRngCases(int repeats) {
    constexpr int kDim = 10;
    constexpr int kNumPaths = 100000;
    for (const auto* method : {"mrg32", "irn"}) {
        for (const auto* precision : {"Default", "Precise"}) {
            for (const int dimension : {10, 52}) {
                const int paths = dimension == 52 ? 8192 : kNumPaths;
                double sink = 0.0;
                const auto result = Bench::Run(
                    std::string("CreateRNG ") + method + " " + precision + (dimension == 52 ? " (8192 x 52D)" : " (100K x 10D)"),
                    [&]() {
                        auto generator = Script::CreateRNG(method, dimension, false, std::nullopt, precision);
                        Vector_<> dst(dimension);
                        for (int path = 0; path < paths; ++path) {
                            generator->FillNormal(&dst);
                            sink += dst[0];
                        }
                    },
                    2, repeats);
                Bench::Print(result);
                Bench::DoNotOptimize(&sink);
            }
        }
    }

    {
        double sink = 0.0;
        const auto result = Bench::Run(
            "MRG32 FillUniform (100K x 10D)",
            [&]() {
                auto generator = New(RNGType_("MRG32"), 1024, kDim, false);
                Vector_<> dst(kDim);
                for (int path = 0; path < kNumPaths; ++path) {
                    generator->FillUniform(&dst);
                    sink += dst[0];
                }
            },
            2, repeats);
        Bench::Print(result);
        Bench::DoNotOptimize(&sink);
    }
}
