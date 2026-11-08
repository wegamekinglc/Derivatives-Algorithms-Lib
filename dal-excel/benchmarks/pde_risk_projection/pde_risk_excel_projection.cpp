#include <chrono>
#include <iostream>

#include "../../../dal-cpp/benchmarks/pde_risk_perf/copy.hpp"
#include "../../../dal-cpp/benchmarks/pde_risk_perf/reference.hpp"
#include "../../src/__earlyexercisepderisk.hpp"

using namespace Dal;

namespace {
    size_t Capacity(const Matrix_<Cell_>& table) {
        size_t bytes = table.Capacity() * sizeof(Cell_);
        for (const auto& cell : table)
            if (const auto* text = std::get_if<String_>(&cell.val_))
                bytes += PdeRiskBench::StringAllocation(*text);
        return bytes;
    }
    int64_t Nanoseconds(std::chrono::steady_clock::time_point start) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
    }
} // namespace

int main() {
    RegisterAll_::Init();
    for (const char* name : {"Bermudan17", "TwoCarry25"}) {
        const auto request = PdeRiskBench::Request(name);
        const Handle_<StorableEarlyExercisePdeRiskRequest_> handle(new StorableEarlyExercisePdeRiskRequest_("", request));
        Handle_<StorableEarlyExercisePdeRiskResult_> result;
        EarlyExercisePdeRiskResult_New("", handle, &result);
        Matrix_<Cell_> riskTemplate, scenarioTemplate;
        EarlyExercisePdeRiskResult_Get_Risks(result, &riskTemplate);
        EarlyExercisePdeRiskResult_Get_Scenarios(result, &scenarioTemplate);
        const size_t pairCells = riskTemplate.Rows() * riskTemplate.Cols() + scenarioTemplate.Rows() * scenarioTemplate.Cols();
        const auto before = result->val_.Execution();
        for (const size_t reads : {1, 10, 100, 1000}) {
            auto start = std::chrono::steady_clock::now();
            const auto native = EvaluateEarlyExercisePdeRisk(request);
            const auto nativeComputeNs = Nanoseconds(start);
            start = std::chrono::steady_clock::now();
            for (size_t i = 0; i < reads; ++i) {
                const auto a = riskTemplate, b = scenarioTemplate;
                Bench::DoNotOptimize(&a);
                Bench::DoNotOptimize(&b);
            }
            const auto explicitCopyNs = Nanoseconds(start);
            start = std::chrono::steady_clock::now();
            Handle_<StorableEarlyExercisePdeRiskResult_> computed;
            EarlyExercisePdeRiskResult_New("", handle, &computed);
            const auto bindingComputeNs = Nanoseconds(start);
            size_t cells = 0, outputBytes = 0, peak = 0;
            start = std::chrono::steady_clock::now();
            for (size_t i = 0; i < reads; ++i) {
                Matrix_<Cell_> a, b;
                EarlyExercisePdeRiskResult_Get_Risks(result, &a);
                EarlyExercisePdeRiskResult_Get_Scenarios(result, &b);
                REQUIRE(a.Rows() == riskTemplate.Rows() && b.Rows() == scenarioTemplate.Rows(), "projection shape drift");
                cells += a.Rows() * a.Cols() + b.Rows() * b.Cols();
                const size_t capacity = Capacity(a) + Capacity(b);
                outputBytes += capacity;
                peak = std::max(peak, capacity);
                Bench::DoNotOptimize(&a);
                Bench::DoNotOptimize(&b);
            }
            const auto bindingCopyNs = Nanoseconds(start);
            const auto after = result->val_.Execution();
            REQUIRE(cells == reads * pairCells && before.workUnits_ == after.workUnits_ && before.totalSolves_ == after.totalSolves_ &&
                        before.numericPayloadBytes_ == after.numericPayloadBytes_ && native.Price() == computed->val_.Price(),
                    "projection invariant failed");
            std::cout << "{\"case\":\"" << name << "\",\"reads\":" << reads << ",\"cells\":" << cells << ",\"output_capacity_bytes\":" << outputBytes
                      << ",\"peak_detached_capacity\":" << peak << ",\"native_compute_ns\":" << nativeComputeNs
                      << ",\"explicit_copy_ns\":" << explicitCopyNs << ",\"binding_compute_ns\":" << bindingComputeNs
                      << ",\"binding_copy_ns\":" << bindingCopyNs << ",\"work\":" << after.workUnits_ << ",\"solves\":" << after.totalSolves_
                      << ",\"payload\":" << after.numericPayloadBytes_ << "}\n";
        }
    }
}
