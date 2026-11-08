#include <dal/platform/platform.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

#include <dal/benchmarks/bench.hpp>

#include "reference.hpp"

using namespace Dal;
using namespace Dal::PdeRiskBench;

namespace {
    size_t RepeatCount(const std::string& count) {
        REQUIRE(!count.empty() && count.find_first_not_of("0123456789") == std::string::npos, "invalid repeat count");
        const auto repeats = std::stoul(count);
        REQUIRE(repeats > 0 && repeats <= 10000, "repeat count outside [1,10000]");
        return repeats;
    }

    void PrintIdentity(const char* name, const EarlyExercisePdeRiskRequest_& request) {
        const auto reference = Reference(request);
        const auto risk = EvaluateEarlyExercisePdeRisk(request);
        CheckIdentity(reference, risk);
        REQUIRE(risk.Status() == EarlyExercisePdeRiskStatus_(std::string(name) == "Boundary17" ? "PARTIAL" : "COMPLETE"),
                "benchmark aggregate status mismatch");
        const auto& e = risk.Execution();
        std::cout << std::setprecision(17) << "{\"case\":\"" << name << "\",\"status\":\"" << risk.Status().String() << "\",\"plan_id\":\""
                  << risk.Plan().planId_ << "\",\"price\":" << risk.Price() << ",\"steps\":" << risk.Plan().plannedSteps_
                  << ",\"scenarios\":" << risk.Scenarios().size() << ",\"solves\":" << e.totalSolves_ << ",\"work\":" << e.workUnits_
                  << ",\"payload\":" << e.numericPayloadBytes_ << ",\"workspace\":" << e.peakWorkspaceBytes_
                  << ",\"peak_native\":" << e.peakNativeBytes_ << ",\"planned_payload\":" << e.plannedNumericPayloadBytes_
                  << ",\"planned_workspace\":" << e.plannedWorkspaceBytes_ << ",\"planned_peak\":" << e.plannedPeakNativeBytes_ << "}\n";
    }

    int64_t Measure(const EarlyExercisePdeRiskRequest_& request, const std::string& mode, size_t repeats) {
        double sink = 0;
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

        return elapsed;
    }
} // namespace

int RunPdeRiskBenchmark(int argc, char** argv) {
    try {
        REQUIRE(argc == 4, "usage: pde_perf --risk CASE verify|risk|reference|copy REPEATS");
        RegisterAll_::Init();
        const auto request = Request(argv[1]);
        const std::string mode(argv[2]);
        const auto repeats = RepeatCount(argv[3]);
        REQUIRE(mode == "verify" || mode == "risk" || mode == "reference" || mode == "copy", "unknown benchmark mode");
        if (mode == "verify") {
            PrintIdentity(argv[1], request);
            return 0;
        }
        const auto elapsed = Measure(request, mode, repeats);
        std::cout << "{\"case\":\"" << argv[1] << "\",\"mode\":\"" << mode << "\",\"repeats\":" << repeats << ",\"elapsed_ns\":" << elapsed
                  << ",\"ns_per_call\":" << elapsed / repeats << "}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
