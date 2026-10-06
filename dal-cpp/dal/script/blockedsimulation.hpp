//
// Created by Codex on 2026/10/06.
//

#pragma once

#include <limits>

#include <dal/math/aad/adjointblockroot.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/script/simulation.hpp>

namespace Dal::Script::Detail {
    struct AADBlockBatchResult_ {
        Vector_<double> valueSums_;
        Matrix_<> gradientSums_;
        AADBlockBatchResult_(size_t width, int inputs) : valueSums_(width, 0.0), gradientSums_(static_cast<int>(width), inputs, 0.0) {}
    };

    class BlockPayoffCollector_ {
        const Vector_<RiskOutputCoordinate_>& outputAxis_;
        const AAD::AdjointBlock_ block_;
        const size_t modelInputs_;
        AADBlockBatchResult_* result_;
        Vector_<int> selectedColumns_;
        Vector_<AAD::Number_> outputs_;
        Vector_<AAD::Number_> roots_;

        [[nodiscard]] String_ InputId(size_t position) const {
            return position < modelInputs_ ? "model:" + String_(std::to_string(position))
                                           : "constant:" + String_(std::to_string(position - modelInputs_));
        }

    public:
        BlockPayoffCollector_(const Vector_<RiskOutputCoordinate_>& outputs,
                              const AAD::AdjointBlock_& block,
                              const AADBatchSettings_& settings,
                              const Vector_<size_t>& selectedInputs,
                              AADBlockBatchResult_* result)
            : outputAxis_(outputs), block_(block), modelInputs_(settings.nParams_), result_(result),
              selectedColumns_(settings.nParams_ + settings.nConstVars_, -1), outputs_(block.outputs_) {
            roots_.reserve(block.width_);
            for (size_t column = 0; column < selectedInputs.size(); ++column) {
                REQUIRE2(selectedInputs[column] < selectedColumns_.size(), "InvalidJacobianBatch: input position is out of range", ScriptError_);
                auto& selected = selectedColumns_[selectedInputs[column]];
                REQUIRE2(selected == -1, "InvalidJacobianBatch: repeated input position", ScriptError_);
                selected = static_cast<int>(column);
            }
        }

        AAD::Number_ operator()(const Vector_<AAD::Number_>& values, size_t, const AAD::Number_& zero) {
            for (size_t lane = 0; lane < block_.outputs_; ++lane) {
                const auto& output = outputAxis_[block_.firstOutput_ + lane];
                REQUIRE2(output.slot_ < values.size(), "InvalidJacobianBatch: scalar slot is out of range; output=" + output.id_, ScriptError_);
                outputs_[lane] = values[output.slot_];
                const double sum = result_->valueSums_[lane] + Value(outputs_[lane]);
                REQUIRE2(std::isfinite(Value(outputs_[lane])) && std::isfinite(sum),
                         "InvalidJacobianPayoff: non-finite value or sum; output=" + output.id_, ScriptError_);
                result_->valueSums_[lane] = sum;
            }
            AAD::SeedAdjointBlock(outputs_, {0, block_.outputs_, block_.width_}, zero, &roots_);
            return roots_[0];
        }

        void Seed(AAD::Number_*) const {}

        void AccumulateInput(SimResults_*, size_t position, const AAD::Number_& input, size_t) const {
            const int column = selectedColumns_[position];
            if (column < 0)
                return;
            for (size_t lane = 0; lane < block_.outputs_; ++lane) {
                const double gradient = AAD::NativeOperations_::ReadAdjoint(input, lane);
                auto& sum = result_->gradientSums_(static_cast<int>(lane), column);
                REQUIRE2(std::isfinite(gradient) && std::isfinite(sum + gradient),
                         "InvalidJacobianRisk: non-finite risk or sum; output=" + outputAxis_[block_.firstOutput_ + lane].id_ +
                             "; input=" + InputId(position),
                         ScriptError_);
                sum += gradient;
            }
        }

        [[nodiscard]] AAD::ProfilingArrayStatistics_ WorkspaceArrays() const { return ProfileArrays(outputs_, roots_, selectedColumns_); }
    };

    [[nodiscard]] inline size_t AADBlockBatchFixedPayloadBytes(bool compiled) {
        const auto evaluators =
            compiled ? 2 * sizeof(EvalState_<AAD::Number_>) : sizeof(FuzzyEvaluator_<AAD::Number_>) + sizeof(PastEvaluator_<AAD::Number_>);
        return sizeof(BlockPayoffCollector_) + sizeof(SimResults_) + evaluators;
    }

    inline void ValidateAADBlockResult(const AAD::AdjointBlock_& block, size_t inputs, const AADBlockBatchResult_* result) {
        REQUIRE2(result && result->valueSums_.size() == block.width_ && result->gradientSums_.Rows() == static_cast<int>(block.width_) &&
                     result->gradientSums_.Cols() == static_cast<int>(inputs),
                 "InvalidJacobianBatch: result dimensions disagree", ScriptError_);
    }

    inline void ValidateAADBlockBatch(const AADBatchSettings_& settings,
                                      const AAD::AdjointBlock_& block,
                                      const Vector_<RiskOutputCoordinate_>& outputs,
                                      size_t inputs,
                                      const AADBlockBatchResult_* result) {
        REQUIRE2(block.width_ > 0 && block.width_ <= AAD::ADJ_SIZE && block.outputs_ > 0 && block.outputs_ <= block.width_,
                 "InvalidJacobianBatch: live outputs must fit a bounded width", ScriptError_);
        REQUIRE2(block.firstOutput_ <= outputs.size() && block.outputs_ <= outputs.size() - block.firstOutput_,
                 "InvalidJacobianBatch: output block is out of range", ScriptError_);
        REQUIRE2(inputs <= static_cast<size_t>(std::numeric_limits<int>::max()) &&
                     settings.nParams_ <= std::numeric_limits<size_t>::max() - settings.nConstVars_,
                 "InvalidJacobianBatch: input extent overflows", ScriptError_);
        ValidateAADBlockResult(block, inputs, result);
    }

    inline void EvaluateAADBlockBatch(const PreparedScript_& product,
                                      const Handle_<ModelData_>& modelData,
                                      const AADBatchSettings_& settings,
                                      const std::optional<ScriptCompiled_>& compiledProduct,
                                      const PathBatch_& batch,
                                      const AAD::AdjointBlock_& block,
                                      const Vector_<RiskOutputCoordinate_>& outputs,
                                      const Vector_<size_t>& selectedInputs,
                                      AADBlockBatchResult_* result,
                                      BufferCapacityBudget_* scratchBudget = nullptr,
                                      AAD::TapeCapacityBudget_* tapeBudget = nullptr) {
        ValidateAADBlockBatch(settings, block, outputs, selectedInputs.size(), result);
        product.RequireExecutable();
        REQUIRE2(product.Simulation().enableAad_ && !product.AllExpired() && !product.Product().ContainsExercise(),
                 "UnsupportedJacobianBatch: requires prepared native non-exercise live product", ScriptError_);
        const auto run = [&] {
            auto mode = AAD::SetNumResultsForAAD(true, block.width_);
            std::optional<AAD::TapeCapacityScope_> tapeCapacity;
            if (tapeBudget)
                tapeCapacity.emplace(tapeBudget);
            BlockPayoffCollector_ collector(outputs, block, settings, selectedInputs, result);
            SimResults_ scalarBookkeeping({});
            EvaluateAADBatch(product, modelData, settings, compiledProduct, batch, &scalarBookkeeping, std::move(collector));
        };
        if (scratchBudget) {
            auto capacity = BufferCapacityScope_::ForWorker(scratchBudget, AADBlockBatchFixedPayloadBytes(compiledProduct.has_value()));
            run();
        } else
            run();
    }
} // namespace Dal::Script::Detail
