//
// Created by Codex on 2026/10/06.
//

#pragma once

#include <optional>

#include <dal/math/aad/aad.hpp>

namespace Dal::AAD {
    struct AdjointBlockSettings_ {
        size_t maxWidth_ = 1;
        size_t concurrentWorkers_ = 1;
        size_t batchResultSlots_ = 1;
        std::optional<size_t> numericResultBudgetBytes_;
        std::optional<size_t> minimumScratchBudgetBytes_;
    };

    struct AdjointBlock_ {
        size_t firstOutput_;
        size_t outputs_;
        size_t width_;
    };

    class AdjointBlockPlan_ {
        size_t outputs_;
        size_t width_;
        size_t numericResultBytes_;
        size_t minimumScratchBytes_;

        AdjointBlockPlan_(size_t outputs, size_t width, size_t numericResultBytes, size_t minimumScratchBytes)
            : outputs_(outputs), width_(width), numericResultBytes_(numericResultBytes), minimumScratchBytes_(minimumScratchBytes) {}
        friend AdjointBlockPlan_ PlanAdjointBlocks(size_t, size_t, const AdjointBlockSettings_&);

    public:
        [[nodiscard]] size_t Width() const { return width_; }
        [[nodiscard]] size_t BlockCount() const { return outputs_ / width_ + (outputs_ % width_ != 0); }
        [[nodiscard]] AdjointBlock_ Block(size_t index) const;
        [[nodiscard]] size_t NumericResultBytes() const { return numericResultBytes_; }
        [[nodiscard]] size_t MinimumScratchBytes() const { return minimumScratchBytes_; }
    };

    [[nodiscard]] AdjointBlockPlan_ PlanAdjointBlocks(size_t outputs, size_t inputs, const AdjointBlockSettings_& settings = {});
} // namespace Dal::AAD
