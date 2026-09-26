//
// Created by Codex on 2026/9/27.
//

#include <dal/benchmarks/bench.hpp>
#include <dal/model/correlatedblackscholes.hpp>
#include <dal/platform/platform.hpp>

#include "correlatedbsperf.hpp"

using namespace Dal;

namespace {
    constexpr size_t kPaths = 100000;

    void RunCase(size_t assets) {
        const Vector_<String_> names{"EQ[AAA]", "EQ[BBB]", "EQ[CCC]"};
        Matrix_<> correlations(static_cast<int>(assets), static_cast<int>(assets), 0.3);
        for (size_t i = 0; i < assets; ++i)
            correlations(static_cast<int>(i), static_cast<int>(i)) = 1.0;
        Vector_<String_> selectedNames;
        Vector_<> spots;
        Vector_<> vols;
        Vector_<> divs;
        for (size_t i = 0; i < assets; ++i) {
            selectedNames.push_back(names[i]);
            spots.push_back(100.0 + 10.0 * i);
            vols.push_back(0.2 + 0.05 * i);
            divs.push_back(0.01);
        }
        AAD::CorrelatedBlackScholes_<> model(selectedNames, spots, vols, divs, 0.05, correlations);
        Vector_<> timeline;
        Vector_<AAD::SampleDef_> definitions(13);
        for (size_t date = 0; date < definitions.size(); ++date) {
            timeline.push_back(static_cast<double>(date) / 12.0);
            definitions[date].indexNames_ = selectedNames;
        }
        model.Allocate(timeline, definitions);
        model.Init(timeline, definitions);
        AAD::Scenario_<> path;
        AAD::AllocatePath(definitions, path);
        AAD::InitializePath(path);
        Vector_<Vector_<>> gaussians(128, Vector_<>(model.SimDim()));
        for (size_t pathId = 0; pathId < gaussians.size(); ++pathId)
            for (size_t factor = 0; factor < model.SimDim(); ++factor)
                gaussians[pathId][factor] = static_cast<double>(static_cast<int>((pathId * 17 + factor * 31) % 101) - 50) * 0.025;
        double sink = 0.0;
        const auto result = Bench::Run(
            "correlated BS path (100K x 12 steps x " + std::to_string(assets) + " assets)",
            [&] {
                for (size_t i = 0; i < kPaths; ++i) {
                    model.GeneratePath(gaussians[i % gaussians.size()], &path);
                    sink += path.back().observations_.back();
                }
            },
            1, 5);
        Bench::Print(result);
        Bench::DoNotOptimize(&sink);
    }
} // namespace

void RunCorrelatedBSPathCases() {
    for (size_t assets : {1u, 2u, 3u})
        RunCase(assets);
}
