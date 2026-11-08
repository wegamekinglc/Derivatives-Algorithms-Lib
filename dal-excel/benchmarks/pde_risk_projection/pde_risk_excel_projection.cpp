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

    struct Case_ {
        EarlyExercisePdeRiskRequest_ request_;
        Handle_<StorableEarlyExercisePdeRiskRequest_> handle_;
        Handle_<StorableEarlyExercisePdeRiskResult_> result_;
        Matrix_<Cell_> riskTemplate_, scenarioTemplate_;
        EarlyExercisePdeRiskExecution_ before_;
        size_t pairCells_;
    };

    Case_ Prepare(const char* name) {
        Case_ c;
        c.request_ = PdeRiskBench::Request(name);
        c.handle_ = Handle_<StorableEarlyExercisePdeRiskRequest_>(new StorableEarlyExercisePdeRiskRequest_("", c.request_));
        EarlyExercisePdeRiskResult_New("", c.handle_, &c.result_);
        EarlyExercisePdeRiskResult_Get_Risks(c.result_, &c.riskTemplate_);
        EarlyExercisePdeRiskResult_Get_Scenarios(c.result_, &c.scenarioTemplate_);
        c.pairCells_ = c.riskTemplate_.Rows() * c.riskTemplate_.Cols() + c.scenarioTemplate_.Rows() * c.scenarioTemplate_.Cols();
        c.before_ = c.result_->val_.Execution();
        return c;
    }

    int64_t CopyTemplates(const Case_& c, size_t reads) {
        const auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < reads; ++i) {
            const auto a = c.riskTemplate_, b = c.scenarioTemplate_;
            Bench::DoNotOptimize(&a);
            Bench::DoNotOptimize(&b);
        }
        return Nanoseconds(start);
    }

    struct Copies_ {
        size_t cells_ = 0, outputBytes_ = 0, peak_ = 0;
        int64_t nanoseconds_ = 0;
    };

    Copies_ ReadTables(const Case_& c, size_t reads) {
        Copies_ copies;
        const auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < reads; ++i) {
            Matrix_<Cell_> a, b;
            EarlyExercisePdeRiskResult_Get_Risks(c.result_, &a);
            EarlyExercisePdeRiskResult_Get_Scenarios(c.result_, &b);
            REQUIRE(a.Rows() == c.riskTemplate_.Rows() && b.Rows() == c.scenarioTemplate_.Rows(), "projection shape drift");
            copies.cells_ += a.Rows() * a.Cols() + b.Rows() * b.Cols();
            const size_t capacity = Capacity(a) + Capacity(b);
            copies.outputBytes_ += capacity;
            copies.peak_ = std::max(copies.peak_, capacity);
            Bench::DoNotOptimize(&a);
            Bench::DoNotOptimize(&b);
        }
        copies.nanoseconds_ = Nanoseconds(start);
        return copies;
    }

    void Measure(const char* name, const Case_& c, size_t reads) {
        auto start = std::chrono::steady_clock::now();
        const auto native = EvaluateEarlyExercisePdeRisk(c.request_);
        const auto nativeComputeNs = Nanoseconds(start);
        const auto explicitCopyNs = CopyTemplates(c, reads);
        start = std::chrono::steady_clock::now();
        Handle_<StorableEarlyExercisePdeRiskResult_> computed;
        EarlyExercisePdeRiskResult_New("", c.handle_, &computed);
        const auto bindingComputeNs = Nanoseconds(start);
        const auto copies = ReadTables(c, reads);
        const auto after = c.result_->val_.Execution();
        REQUIRE(copies.cells_ == reads * c.pairCells_ && c.before_.workUnits_ == after.workUnits_ && c.before_.totalSolves_ == after.totalSolves_ &&
                    c.before_.numericPayloadBytes_ == after.numericPayloadBytes_ && native.Price() == computed->val_.Price(),
                "projection invariant failed");
        std::cout << "{\"case\":\"" << name << "\",\"reads\":" << reads << ",\"cells\":" << copies.cells_
                  << ",\"output_capacity_bytes\":" << copies.outputBytes_ << ",\"peak_detached_capacity\":" << copies.peak_
                  << ",\"native_compute_ns\":" << nativeComputeNs << ",\"explicit_copy_ns\":" << explicitCopyNs
                  << ",\"binding_compute_ns\":" << bindingComputeNs << ",\"binding_copy_ns\":" << copies.nanoseconds_
                  << ",\"work\":" << after.workUnits_ << ",\"solves\":" << after.totalSolves_ << ",\"payload\":" << after.numericPayloadBytes_
                  << "}\n";
    }
} // namespace

int main() {
    RegisterAll_::Init();
    for (const char* name : {"Bermudan17", "TwoCarry25"}) {
        const auto c = Prepare(name);
        for (const size_t reads : {1, 10, 100, 1000})
            Measure(name, c, reads);
    }
}
