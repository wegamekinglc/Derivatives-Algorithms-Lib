#include <dal/platform/platform.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

#include <dal/benchmarks/bench.hpp>

#include "reference.hpp"

using namespace Dal;
using namespace Dal::PdeRiskBench;

int RunPdeRiskBenchmark(int argc, char** argv) {
    try {
        REQUIRE(argc == 4, "usage: pde_perf --risk CASE verify|risk|reference|copy REPEATS");
        RegisterAll_::Init();
        const auto request = Request(argv[1]);
        const std::string mode(argv[2]), count(argv[3]);
        REQUIRE(!count.empty() && count.find_first_not_of("0123456789") == std::string::npos, "invalid repeat count");
        const auto repeats = std::stoul(count);
        REQUIRE(repeats > 0 && repeats <= 10000, "repeat count outside [1,10000]");
        REQUIRE(mode == "verify" || mode == "risk" || mode == "reference" || mode == "copy", "unknown benchmark mode");
        double sink = 0;
        if (mode == "verify") {
            const auto reference = Reference(request);
            const auto risk = EvaluateEarlyExercisePdeRisk(request);
            CheckIdentity(reference, risk);
            REQUIRE(risk.Status() == EarlyExercisePdeRiskStatus_(std::string(argv[1]) == "Boundary17" ? "PARTIAL" : "COMPLETE"),
                    "benchmark aggregate status mismatch");
            const auto& e = risk.Execution();
            std::cout << std::setprecision(17) << "{\"case\":\"" << argv[1] << "\",\"status\":\"" << risk.Status().String() << "\",\"plan_id\":\""
                      << risk.Plan().planId_ << "\",\"price\":" << risk.Price() << ",\"steps\":" << risk.Plan().plannedSteps_
                      << ",\"scenarios\":" << risk.Scenarios().size() << ",\"solves\":" << e.totalSolves_ << ",\"work\":" << e.workUnits_
                      << ",\"payload\":" << e.numericPayloadBytes_ << ",\"workspace\":" << e.peakWorkspaceBytes_
                      << ",\"peak_native\":" << e.peakNativeBytes_ << ",\"planned_payload\":" << e.plannedNumericPayloadBytes_
                      << ",\"planned_workspace\":" << e.plannedWorkspaceBytes_ << ",\"planned_peak\":" << e.plannedPeakNativeBytes_ << "}\n";
            return 0;
        }
        const auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < repeats; ++i) {
            if (mode == "reference") {
                const auto result = Reference(request);
                sink += result.base_->Price();
                Bench::DoNotOptimize(&result);
            } else {
                const auto result = EvaluateEarlyExercisePdeRisk(request);
                sink += result.Price();
                if (mode == "copy") {
                    const auto risks = result.Risks();
                    const auto scenarios = result.Scenarios();
                    Bench::DoNotOptimize(&risks);
                    Bench::DoNotOptimize(&scenarios);
                }
                Bench::DoNotOptimize(&result);
            }
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        Bench::DoNotOptimize(&sink);
        std::cout << "{\"case\":\"" << argv[1] << "\",\"mode\":\"" << mode << "\",\"repeats\":" << repeats << ",\"elapsed_ns\":" << elapsed
                  << ",\"ns_per_call\":" << elapsed / repeats << "}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
