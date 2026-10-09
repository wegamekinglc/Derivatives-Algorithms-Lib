//
// Created by Codex on 2026/10/09.
//

#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include <dal/script/segmentedpath.hpp>

namespace Dal::Script {
    struct SegmentedMonteCarloSettings_ {
        String_ rsg_ = "sobol";
        bool useBb_ = false;
        size_t firstPath_ = 0;
        std::optional<std::uint64_t> scrambleKey_;
        AAD::SegmentedPathSettings_ path_;
    };

    struct SegmentedMonteCarloExecution_ {
        size_t pathCount_ = 0;
        size_t firstPath_ = 0;
        size_t batchSize_ = 32;
        size_t batches_ = 0;
        size_t lanes_ = 1;
        size_t segmentSteps_ = 0;
        String_ rsg_;
        bool useBb_ = false;
        std::optional<std::uint64_t> scrambleKey_;
        size_t maxPathTapeBytes_ = 0;
        size_t maxPathCheckpointBytes_ = 0;
        size_t maxPathCleanupReserveBytes_ = 0;
    };

    class SegmentedMonteCarloResult_ {
        double meanValue_;
        Vector_<> meanGradient_;
        Vector_<String_> labels_;
        SegmentedMonteCarloExecution_ execution_;

    public:
        SegmentedMonteCarloResult_(double value, Vector_<> gradient, Vector_<String_> labels, SegmentedMonteCarloExecution_ execution)
            : meanValue_(value), meanGradient_(std::move(gradient)), labels_(std::move(labels)), execution_(std::move(execution)) {}
        [[nodiscard]] double MeanValue() const { return meanValue_; }
        [[nodiscard]] const Vector_<>& MeanGradient() const { return meanGradient_; }
        [[nodiscard]] const Vector_<String_>& ParameterLabels() const { return labels_; }
        [[nodiscard]] const SegmentedMonteCarloExecution_& Execution() const { return execution_; }
    };

    [[nodiscard]] SegmentedMonteCarloResult_ EvaluateBlackScholesSegmentedMonteCarlo(const BlackScholesSegmentedPath_& kernel,
                                                                                     const Vector_<>& parameters,
                                                                                     size_t pathCount,
                                                                                     const SegmentedMonteCarloSettings_& settings = {});
} // namespace Dal::Script
