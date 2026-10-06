//
// Created by Codex on 2026/10/06.
//

#include <dal/platform/strict.hpp>

#include <algorithm>
#include <limits>

#include <dal/math/aad/adjointblocks.hpp>

namespace Dal::AAD {
    namespace {
        size_t CheckedProduct(size_t lhs, size_t rhs, const char* field) {
            REQUIRE(lhs == 0 || rhs <= std::numeric_limits<size_t>::max() / lhs,
                    String_("NativeAAD.PlanAdjointBlocks: byte extent overflow; field=") + field);
            return lhs * rhs;
        }

        size_t CheckedSum(size_t lhs, size_t rhs, const char* field) {
            REQUIRE(rhs <= std::numeric_limits<size_t>::max() - lhs, String_("NativeAAD.PlanAdjointBlocks: byte extent overflow; field=") + field);
            return lhs + rhs;
        }

        void ValidateBlockSettings(size_t outputs, size_t inputs, const AdjointBlockSettings_& settings) {
            const auto maximum = static_cast<size_t>(std::numeric_limits<int>::max());
            REQUIRE(outputs > 0 && outputs <= maximum, "NativeAAD.PlanAdjointBlocks: output count must fit a positive matrix row extent");
            REQUIRE(inputs <= maximum, "NativeAAD.PlanAdjointBlocks: input count exceeds the matrix column limit");
            REQUIRE(settings.maxWidth_ > 0 && settings.maxWidth_ <= ADJ_SIZE,
                    "NativeAAD.PlanAdjointBlocks: maxWidth must be positive and at most ADJ_SIZE");
            REQUIRE(settings.concurrentWorkers_ > 0, "NativeAAD.PlanAdjointBlocks: concurrentWorkers must be positive");
            REQUIRE(settings.batchResultSlots_ >= settings.concurrentWorkers_,
                    "NativeAAD.PlanAdjointBlocks: batchResultSlots must cover concurrentWorkers");
        }
    } // namespace

    AdjointBlock_ AdjointBlockPlan_::Block(size_t index) const {
        REQUIRE(index < BlockCount(), "NativeAAD.AdjointBlockPlan.Block: block index out of range");
        const size_t first = index * width_;
        return {first, std::min(width_, outputs_ - first), width_};
    }

    AdjointBlockPlan_ PlanAdjointBlocks(size_t outputs, size_t inputs, const AdjointBlockSettings_& settings) {
        ValidateBlockSettings(outputs, inputs, settings);
        const size_t rowValues = CheckedSum(inputs, 1, "numericResult");
        const size_t rowBytes = CheckedProduct(rowValues, sizeof(double), "numericResult");
        const size_t resultBytes = CheckedProduct(outputs, rowBytes, "numericResult");
        REQUIRE(!settings.numericResultBudgetBytes_ || resultBytes <= *settings.numericResultBudgetBytes_,
                "NativeAAD.PlanAdjointBlocks: numeric result payload exceeds numericResultBudgetBytes");
        const size_t rootBytes = CheckedProduct(settings.concurrentWorkers_, sizeof(Number_), "rootScratch");
        const size_t batchBytes = CheckedProduct(settings.batchResultSlots_, rowBytes, "batchScratch");
        const size_t laneBytes = CheckedSum(rootBytes, batchBytes, "minimumScratch");
        size_t width = std::min(settings.maxWidth_, outputs);
        if (settings.minimumScratchBudgetBytes_) {
            REQUIRE(laneBytes <= *settings.minimumScratchBudgetBytes_, "NativeAAD.PlanAdjointBlocks: one lane exceeds minimumScratchBudgetBytes");
            width = std::min(width, *settings.minimumScratchBudgetBytes_ / laneBytes);
        }
        const size_t minimumScratch = CheckedProduct(width, laneBytes, "minimumScratch");
        return {outputs, width, resultBytes, minimumScratch};
    }
} // namespace Dal::AAD
