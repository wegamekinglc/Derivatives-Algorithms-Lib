//
// Created by Codex on 2026/10/06.
//

#pragma once

#include <dal/script/blockedreplay.hpp>

namespace Dal::Script::Detail {
    class PassiveOutputCollector_ {
        const Vector_<RiskOutputCoordinate_>& outputs_;
        Vector_<>* sums_;

    public:
        PassiveOutputCollector_(const Vector_<RiskOutputCoordinate_>& outputs, Vector_<>* sums) : outputs_(outputs), sums_(sums) {}

        double operator()(const Vector_<>& values, size_t, double) const {
            for (size_t row = 0; row < outputs_.size(); ++row) {
                const auto& output = outputs_[row];
                REQUIRE2(output.slot_ < values.size(), "InvalidJacobianReplay: scalar slot is out of range; output=" + output.id_, ScriptError_);
                AddReplayGradient(values[output.slot_], &(*sums_)[row], output.id_, "value");
            }
            return 0.0;
        }

        [[nodiscard]] AAD::ProfilingArrayStatistics_ WorkspaceArrays() const { return ProfileArrays(*sums_); }
    };

    inline size_t PassiveWorkerFixedPayloadBytes(bool checkedPaths) {
        return sizeof(DoubleSimulationState_<PreparedScript_>) + sizeof(PassiveOutputCollector_) + (checkedPaths ? sizeof(LocalCheckedPaths_) : 0);
    }

    inline void
    PreflightPassiveReplay(size_t outputs, const BatchPlan_& batches, size_t threads, bool checkedPaths, const AADBlockReplaySettings_& limits) {
        REQUIRE2(limits.maxWidth_ > 0 && limits.maxWidth_ <= AAD::ADJ_SIZE, "InvalidJacobianReplay: block width is out of range", ScriptError_);
        const auto values = ReplayExtentProduct(outputs, sizeof(double));
        RequireReplayCapacity(limits.numericResultBudgetBytes_, values, "Numeric result");
        auto fixed = ReplayExtentSum(values, sizeof(AADBlockReplayResult_));
        fixed = ReplayExtentSum(fixed, ReplayExtentProduct(std::min(threads, batches.BatchCount()), PassiveWorkerFixedPayloadBytes(checkedPaths)));
        fixed = ReplayExtentSum(fixed, ReplayExtentProduct(batches.BatchCount(), ReplayExtentSum(values, sizeof(Vector_<>) + sizeof(TaskHandle_))));
        RequireReplayCapacity(limits.scratchBudgetBytes_, fixed, "Scratch buffer");
    }

    inline void EvaluatePassiveOutputBatch(const PreparedScript_& product,
                                           const Handle_<ModelData_>& modelData,
                                           const MonteCarloSettings_& simulation,
                                           const std::optional<ScriptCompiled_>& program,
                                           const PathBatch_& batch,
                                           const Vector_<RiskOutputCoordinate_>& outputs,
                                           Vector_<>* sums,
                                           BufferCapacityBudget_* scratch,
                                           bool checkedPaths) {
        auto capacity = BufferCapacityScope_::ForWorker(scratch, PassiveWorkerFixedPayloadBytes(checkedPaths));
        auto model = CreateModel<double>(modelData);
        model->Allocate(product.TimeLine(), product.DefLine());
        model->Init(product.TimeLine(), product.DefLine());
        DoubleSimulationState_<PreparedScript_> state(product, *model, simulation.rsg_, simulation.useBb_, simulation.normalPrecision_);
        PassiveOutputCollector_ collector(outputs, sums);
        auto run = [&](auto* evaluator, const auto& evaluate) {
            static_cast<void>(EvaluateDoubleBatch(*model, &state, evaluator, batch, product.PayOffIdx(), evaluate, collector));
        };
        if (program)
            run(&state.compiledState_, [&](const auto& path, auto& evaluator) { program->Evaluate(path, evaluator); });
        else
            run(&state.evaluator_, [&](const auto& path, auto& evaluator) { product.Evaluate(path, evaluator); });
    }

    inline void RunPassiveOutputTasks(const PreparedScript_& product,
                                      const Handle_<ModelData_>& model,
                                      const MonteCarloSettings_& simulation,
                                      const std::optional<ScriptCompiled_>& program,
                                      const BatchPlan_& batches,
                                      const Vector_<RiskOutputCoordinate_>& outputs,
                                      Vector_<Vector_<>>* slots,
                                      BufferCapacityBudget_* scratch,
                                      bool checkedPaths) {
        auto futureCapacity = BufferCapacityScope_::ForWorker(scratch, ReplayExtentProduct(batches.BatchCount(), sizeof(TaskHandle_)));
        Dal::Detail::BufferCapacitySuspension_ suspension;
        SimulationTaskGroup_ tasks(ThreadPool_::GetInstance(), batches.BatchCount());
        for (size_t batch = 0; batch < batches.BatchCount(); ++batch) {
            const auto range = batches.BatchAt(batch);
            tasks.Spawn([&, batch, range] {
                EvaluatePassiveOutputBatch(product, model, simulation, program, range, outputs, &(*slots)[batch], scratch, checkedPaths);
                return true;
            });
        }
        tasks.Complete();
    }

    inline AADBlockReplayResult_ EvaluatePassiveOutputReplay(const PreparedScript_& product,
                                                             Handle_<ModelData_> model,
                                                             size_t paths,
                                                             const Vector_<RiskOutputCoordinate_>& requestedOutputs,
                                                             AADBlockReplaySettings_ limits = {}) {
        product.RequireExecutable();
        REQUIRE2(!product.Simulation().enableAad_ && !product.AllExpired() && !product.Product().ContainsExercise(),
                 "UnsupportedJacobianReplay: requires prepared passive non-exercise live product", ScriptError_);
        REQUIRE2(paths > 0, "InvalidJacobianReplay: positive path count is required", ScriptError_);
        const auto outputs = requestedOutputs;
        REQUIRE2(!outputs.empty(), "InvalidJacobianReplay: outputs must not be empty", ScriptError_);
        ValidateReplayOutputs(product, outputs);
        const auto simulation = product.Simulation();
        ValidateRNG(simulation.rsg_);
        auto* pool = ThreadPool_::GetInstance();
        const BatchPlan_ batches(paths, pool->NumThreads());
        const bool checkedPaths = typeid(*model) == typeid(BSModelData_);
        PreflightPassiveReplay(outputs.size(), batches, pool->NumThreads(), checkedPaths, limits);
        const auto program = simulation.compiled_.value_or(false) ? std::optional<ScriptCompiled_>(product.Compile()) : std::nullopt;
        BufferCapacityBudget_ scratch(limits.scratchBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
        BufferCapacityScope_ coordinator(&scratch, sizeof(AADBlockReplayResult_));
        AADBlockReplayResult_ result(outputs.size(), 0, {});
        result.replayAttempts_ = 1;
        Vector_<Vector_<>> slots;
        slots.reserve(batches.BatchCount());
        for (size_t batch = 0; batch < batches.BatchCount(); ++batch)
            slots.emplace_back(outputs.size(), 0.0);
        try {
            RunPassiveOutputTasks(product, model, simulation, program, batches, outputs, &slots, &scratch, checkedPaths);
        } catch (const Exception_& error) {
            THROW2("JacobianReplayFailed: passive; attempt=1; output=" + outputs.front().id_ + "; cause=" + String_(error.what()), ScriptError_);
        }
        for (size_t row = 0; row < outputs.size(); ++row) {
            for (const auto& batch : slots)
                AddReplayGradient(batch[row], &result.values_[row], outputs[row].id_, "value");
            result.values_[row] /= static_cast<double>(paths);
        }
        result.executedPaths_ = paths;
        result.peakScratchBytes_ = scratch.PeakCapacityBytes();
        return result;
    }
} // namespace Dal::Script::Detail
