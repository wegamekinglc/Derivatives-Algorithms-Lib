//
// Created by Codex on 2026/10/06.
//

#pragma once

#include <set>

#include <dal/script/blockedsimulation.hpp>

namespace Dal::Script::Detail {
    struct AADBlockReplaySettings_ {
        size_t maxWidth_ = 1;
        std::optional<size_t> numericResultBudgetBytes_;
        std::optional<size_t> scratchBudgetBytes_;
        std::optional<size_t> tapeBudgetBytes_;
    };

    struct AADBlockReplayResult_ {
        Vector_<double> values_;
        Matrix_<> jacobian_;
        Vector_<size_t> actualWidths_;
        size_t replayAttempts_ = 0;
        size_t executedPaths_ = 0;
        size_t peakTapeBytes_ = 0;
        size_t peakScratchBytes_ = 0;

        AADBlockReplayResult_(size_t outputs, int inputs, Vector_<size_t>&& widths)
            : values_(outputs, 0.0), jacobian_(static_cast<int>(outputs), inputs, 0.0), actualWidths_(std::move(widths)) {}
    };

    inline size_t ReplayExtentSum(size_t lhs, size_t rhs) {
        REQUIRE2(rhs <= std::numeric_limits<size_t>::max() - lhs, "InvalidJacobianReplay: capacity sum overflows", ScriptError_);
        return lhs + rhs;
    }

    inline size_t ReplayExtentProduct(size_t lhs, size_t rhs) {
        REQUIRE2(rhs == 0 || lhs <= std::numeric_limits<size_t>::max() / rhs, "InvalidJacobianReplay: capacity product overflows", ScriptError_);
        return lhs * rhs;
    }

    inline void RequireReplayCapacity(const std::optional<size_t>& limit, size_t required, const char* kind) {
        REQUIRE2(!limit || required <= *limit,
                 String_(kind) + " capacity budget exceeded before replay [required=" + String_(std::to_string(required)) +
                     ", limit=" + String_(std::to_string(limit.value_or(0))) + "]",
                 ScriptError_);
    }

    inline void ValidateReplayOutputs(const PreparedScript_& product, const Vector_<RiskOutputCoordinate_>& outputs) {
        const auto available = ScriptRiskOutputAxis(product.Product());
        std::set<String_> selected;
        for (const auto& output : outputs) {
            REQUIRE2(selected.insert(output.id_).second, "InvalidJacobianReplay: repeated output ID; output=" + output.id_, ScriptError_);
            REQUIRE2(output.slot_ < available.size(), "InvalidJacobianReplay: unknown output slot; output=" + output.id_, ScriptError_);
            const auto& coordinate = available[output.slot_];
            REQUIRE2(coordinate.id_ == output.id_ && coordinate.label_ == output.label_,
                     "InvalidJacobianReplay: prepared output identity changed; output=" + output.id_, ScriptError_);
        }
    }

    inline void ValidateReplayInputs(const AADBatchSettings_& settings, const Vector_<size_t>& inputs) {
        const auto count = ReplayExtentSum(settings.nParams_, settings.nConstVars_);
        REQUIRE2(count <= static_cast<size_t>(std::numeric_limits<int>::max()), "InvalidJacobianReplay: input extent exceeds int range",
                 ScriptError_);
        std::set<size_t> selected;
        for (const auto input : inputs) {
            REQUIRE2(input < count, "InvalidJacobianReplay: input position is out of range", ScriptError_);
            REQUIRE2(selected.insert(input).second, "InvalidJacobianReplay: repeated input position", ScriptError_);
        }
    }

    inline size_t ReplayMinimumTapeBytes(size_t workers) {
        const size_t resident = sizeof(std::array<AAD::TapNode_, AAD::BLOCK_SIZE>) + sizeof(std::array<double, AAD::DATA_SIZE>) +
                                sizeof(std::array<double*, AAD::DATA_SIZE>) + sizeof(std::array<double, AAD::ADJ_SIZE>);
        return ReplayExtentProduct(workers, ReplayExtentSum(resident, AAD::TapeCleanupCapacityBytes()));
    }

    inline AAD::AdjointBlockPlan_ PlanAADBlockReplay(size_t outputs,
                                                     size_t inputs,
                                                     size_t completeInputs,
                                                     const BatchPlan_& batches,
                                                     size_t threads,
                                                     bool compiled,
                                                     const AADBlockReplaySettings_& limits) {
        const auto workers = std::min(threads, batches.BatchCount());
        AAD::AdjointBlockSettings_ settings;
        settings.maxWidth_ = limits.maxWidth_;
        settings.concurrentWorkers_ = workers;
        settings.batchResultSlots_ = batches.BatchCount();
        settings.numericResultBudgetBytes_ = limits.numericResultBudgetBytes_;
        const auto initial = AAD::PlanAdjointBlocks(outputs, inputs, settings);
        const auto tapeMinimum = ReplayMinimumTapeBytes(workers);
        RequireReplayCapacity(limits.tapeBudgetBytes_, tapeMinimum, "Tape");
        const auto fixedWorker = ReplayExtentSum(AADBlockBatchFixedPayloadBytes(compiled), ReplayExtentProduct(completeInputs, sizeof(int)));
        auto fixed = ReplayExtentSum(initial.NumericResultBytes(), sizeof(AADBlockReplayResult_));
        fixed = ReplayExtentSum(fixed, ReplayExtentProduct(workers, fixedWorker));
        fixed = ReplayExtentSum(fixed, ReplayExtentProduct(batches.BatchCount(), sizeof(AADBlockBatchResult_) + sizeof(TaskHandle_)));
        const auto rowBytes = ReplayExtentProduct(ReplayExtentSum(inputs, 1), sizeof(double));
        const auto lane =
            ReplayExtentSum(ReplayExtentProduct(workers, 2 * sizeof(AAD::Number_)), ReplayExtentProduct(batches.BatchCount(), rowBytes));
        RequireReplayCapacity(limits.scratchBudgetBytes_, ReplayExtentSum(fixed, lane), "Scratch buffer");
        if (limits.scratchBudgetBytes_) {
            settings.maxWidth_ = std::min(initial.Width(), (*limits.scratchBudgetBytes_ - fixed) / lane);
        }
        const auto plan = AAD::PlanAdjointBlocks(outputs, inputs, settings);
        static_cast<void>(ReplayExtentProduct(plan.BlockCount(), batches.BatchCount()));
        return plan;
    }

    inline String_ ReplayInputId(const AADBatchSettings_& settings, size_t position) {
        return position < settings.nParams_ ? "model:" + String_(std::to_string(position))
                                            : "constant:" + String_(std::to_string(position - settings.nParams_));
    }

    inline void AddReplayGradient(double value, double* sum, const String_& output, const String_& input) {
        REQUIRE2(std::isfinite(value) && std::isfinite(*sum + value),
                 "InvalidJacobianRisk: non-finite risk or sum; output=" + output + "; input=" + input, ScriptError_);
        *sum += value;
    }

    inline void ReduceReplayBlock(const Vector_<AADBlockBatchResult_>& batches,
                                  const AAD::AdjointBlock_& block,
                                  const AADBatchSettings_& settings,
                                  const Vector_<RiskOutputCoordinate_>& outputs,
                                  const Vector_<size_t>& inputs,
                                  AADBlockReplayResult_* result) {
        for (size_t lane = 0; lane < block.outputs_; ++lane) {
            const auto row = block.firstOutput_ + lane;
            for (const auto& batch : batches) {
                AddReplayGradient(batch.valueSums_[lane], &result->values_[row], outputs[row].id_, "value");
                for (size_t column = 0; column < inputs.size(); ++column)
                    AddReplayGradient(batch.gradientSums_(static_cast<int>(lane), static_cast<int>(column)),
                                      &result->jacobian_(static_cast<int>(row), static_cast<int>(column)), outputs[row].id_,
                                      ReplayInputId(settings, inputs[column]));
            }
            result->values_[row] /= static_cast<double>(settings.nPaths_);
            for (size_t column = 0; column < inputs.size(); ++column)
                result->jacobian_(static_cast<int>(row), static_cast<int>(column)) /= static_cast<double>(settings.nPaths_);
        }
    }

    inline void RunAADReplayBlock(const PreparedScript_& product,
                                  const Handle_<ModelData_>& model,
                                  const AADBatchSettings_& settings,
                                  const std::optional<ScriptCompiled_>& program,
                                  const BatchPlan_& batches,
                                  const AAD::AdjointBlock_& block,
                                  const Vector_<RiskOutputCoordinate_>& outputs,
                                  const Vector_<size_t>& inputs,
                                  BufferCapacityBudget_* scratch,
                                  AAD::TapeCapacityBudget_* tape,
                                  AADBlockReplayResult_* result) {
        ++result->replayAttempts_;
        try {
            Vector_<AADBlockBatchResult_> slots;
            slots.reserve(batches.BatchCount());
            for (size_t batch = 0; batch < batches.BatchCount(); ++batch)
                slots.emplace_back(block.width_, static_cast<int>(inputs.size()));
            {
                auto futureCapacity = BufferCapacityScope_::ForWorker(scratch, ReplayExtentProduct(batches.BatchCount(), sizeof(TaskHandle_)));
                Dal::Detail::BufferCapacitySuspension_ suspension;
                SimulationTaskGroup_ tasks(ThreadPool_::GetInstance(), batches.BatchCount());
                for (size_t batch = 0; batch < batches.BatchCount(); ++batch) {
                    const auto paths = batches.BatchAt(batch);
                    tasks.Spawn([&, batch, paths] {
                        EvaluateAADBlockBatch(product, model, settings, program, paths, block, outputs, inputs, &slots[batch], scratch, tape);
                        return true;
                    });
                }
                tasks.Complete();
            }
            result->executedPaths_ += settings.nPaths_;
            ReduceReplayBlock(slots, block, settings, outputs, inputs, result);
        } catch (const Exception_& error) {
            THROW2("JacobianReplayFailed: blockFirst=" + String_(std::to_string(block.firstOutput_)) +
                       "; width=" + String_(std::to_string(block.width_)) + "; attempt=" + String_(std::to_string(result->replayAttempts_)) +
                       "; output=" + outputs[block.firstOutput_].id_ + "; cause=" + String_(error.what()),
                   ScriptError_);
        }
    }

    inline AADBlockReplayResult_ EvaluateAADBlockReplay(const PreparedScript_& product,
                                                        Handle_<ModelData_> model,
                                                        AADBatchSettings_ requestedSettings,
                                                        const Vector_<RiskOutputCoordinate_>& requestedOutputs,
                                                        const Vector_<size_t>& requestedInputs,
                                                        AADBlockReplaySettings_ limits = {}) {
        const String_ method = requestedSettings.rsg_;
        const AADBatchSettings_ settings{method,
                                         requestedSettings.useBb_,
                                         requestedSettings.maxNestedIfs_,
                                         requestedSettings.eps_,
                                         requestedSettings.nPaths_,
                                         requestedSettings.nParams_,
                                         requestedSettings.nConstVars_,
                                         requestedSettings.payoffIndex_,
                                         requestedSettings.normalPrecision_};
        product.RequireExecutable();
        REQUIRE2(product.Simulation().enableAad_ && !product.AllExpired() && !product.Product().ContainsExercise(),
                 "UnsupportedJacobianReplay: requires prepared native non-exercise live product", ScriptError_);
        REQUIRE2(settings.nPaths_ > 0, "InvalidJacobianReplay: positive path count is required", ScriptError_);
        const auto outputs = requestedOutputs;
        const auto inputs = requestedInputs;
        ValidateReplayOutputs(product, outputs);
        ValidateReplayInputs(settings, inputs);
        ValidateRNG(settings.rsg_);
        ValidateNormalPrecision(settings.normalPrecision_);
        ValidateAADHistory(product);
        const bool compiled = product.Simulation().compiled_.value_or(false);
        ValidateAADExecution(product, compiled, settings.eps_);
        auto* pool = ThreadPool_::GetInstance();
        const auto threads = pool->NumThreads();
        const BatchPlan_ batches(settings.nPaths_, threads);
        const auto plan = PlanAADBlockReplay(outputs.size(), inputs.size(), ReplayExtentSum(settings.nParams_, settings.nConstVars_), batches,
                                             threads, compiled, limits);
        static_cast<void>(ReplayExtentProduct(plan.BlockCount(), settings.nPaths_));
        const auto program = compiled ? std::optional<ScriptCompiled_>(product.Compile(true)) : std::nullopt;
        Vector_<size_t> widths(plan.BlockCount(), plan.Width());
        AAD::TapeCapacityBudget_ tape(limits.tapeBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
        BufferCapacityBudget_ scratch(limits.scratchBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
        BufferCapacityScope_ coordinator(&scratch, sizeof(AADBlockReplayResult_));
        AADBlockReplayResult_ result(outputs.size(), static_cast<int>(inputs.size()), std::move(widths));
        for (size_t index = 0; index < plan.BlockCount(); ++index) {
            const auto block = plan.Block(index);
            RunAADReplayBlock(product, model, settings, program, batches, block, outputs, inputs, &scratch, &tape, &result);
        }
        result.peakTapeBytes_ = tape.PeakCapacityBytes();
        result.peakScratchBytes_ = scratch.PeakCapacityBytes();
        return result;
    }
} // namespace Dal::Script::Detail
