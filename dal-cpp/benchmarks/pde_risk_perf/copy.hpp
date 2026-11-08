#pragma once

#include <chrono>
#include <cstdint>

#include <dal/benchmarks/bench.hpp>
#include <dal/math/pde/earlyexercisepderisk.hpp>

namespace Dal::PdeRiskBench {
    inline size_t StringAllocation(const String_& value) {
        const auto address = reinterpret_cast<uintptr_t>(value.data()), object = reinterpret_cast<uintptr_t>(&value);
        return address >= object && address < object + sizeof(value) ? 0 : value.capacity() + 1;
    }
    inline size_t DetachedCapacity(const Vector_<PDE::EarlyExercisePdeRiskRecord_>& risks,
                                   const Vector_<PDE::EarlyExercisePdeRiskScenario_>& scenarios) {
        size_t bytes = risks.capacity() * sizeof(risks[0]) + scenarios.capacity() * sizeof(scenarios[0]);
        for (const auto& row : risks) {
            for (const auto* text : {&row.riskId_, &row.parameterId_, &row.unit_, &row.reason_, &row.method_, &row.interpretation_})
                bytes += StringAllocation(*text);
            bytes += (row.fineStencil_.capacity() + row.coarseStencil_.capacity()) * sizeof(PDE::EarlyExercisePdeRiskStencilEntry_);
        }
        for (const auto& row : scenarios) {
            bytes += StringAllocation(row.scenarioId_) + StringAllocation(row.planId_);
            if (row.parameterId_)
                bytes += StringAllocation(*row.parameterId_);
            if (row.failure_)
                bytes += StringAllocation(row.failure_->reason_);
        }
        return bytes;
    }
    struct CopyProbe_ {
        int64_t computeNs_, copyNs_;
        size_t records_, outputBytes_, peakDetachedCapacity_, work_, solves_, payload_;
    };
    inline CopyProbe_ CopyProbe(const PDE::EarlyExercisePdeRiskRequest_& request, size_t reads) {
        using Clock_ = std::chrono::steady_clock;
        const auto start = Clock_::now();
        const auto result = PDE::EvaluateEarlyExercisePdeRisk(request);
        const auto computed = Clock_::now();
        size_t records = 0, bytes = 0, peak = 0;
        for (size_t i = 0; i < reads; ++i) {
            const auto risks = result.Risks();
            const auto scenarios = result.Scenarios();
            records += risks.size() + scenarios.size();
            const size_t capacity = DetachedCapacity(risks, scenarios);
            bytes += capacity;
            peak = std::max(peak, capacity);
            Bench::DoNotOptimize(&risks);
            Bench::DoNotOptimize(&scenarios);
        }
        const auto copied = Clock_::now();
        return {std::chrono::duration_cast<std::chrono::nanoseconds>(computed - start).count(),
                std::chrono::duration_cast<std::chrono::nanoseconds>(copied - computed).count(),
                records,
                bytes,
                peak,
                result.Execution().workUnits_,
                result.Execution().totalSolves_,
                result.Execution().numericPayloadBytes_};
    }
} // namespace Dal::PdeRiskBench
