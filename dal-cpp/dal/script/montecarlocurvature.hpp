//
// Created by Codex on 2026/10/09.
//

#pragma once

#include <memory>
#include <optional>
#include <utility>

#include <dal/math/aad/bumpoveraad.hpp>
#include <dal/script/segmentedmontecarlo.hpp>

namespace Dal::Script {
    struct MonteCarloCurvatureExecution_ {
        String_ method_ = "BumpOverSegmentedNativeAAD";
        size_t gradientEvaluations_ = 0;
        size_t numericPayloadBytes_ = 0;
        size_t maxPathTapeBytes_ = 0;
        size_t maxPathCheckpointBytes_ = 0;
        size_t maxPathCleanupReserveBytes_ = 0;
        std::optional<size_t> recordingCapacityBudgetBytes_;
    };

    class MonteCarloCurvatureResult_ {
        SegmentedMonteCarloResult_ base_;
        Vector_<> point_;
        AAD::BumpOverAADRequest_ bumps_;
        SegmentedMonteCarloSettings_ settings_;
        std::shared_ptr<const PreparedScript_> prepared_;
        Matrix_<> products_;
        MonteCarloCurvatureExecution_ execution_;

    public:
        MonteCarloCurvatureResult_(SegmentedMonteCarloResult_ base,
                                   Vector_<> point,
                                   AAD::BumpOverAADRequest_ bumps,
                                   SegmentedMonteCarloSettings_ settings,
                                   std::shared_ptr<const PreparedScript_> prepared,
                                   Matrix_<> products,
                                   MonteCarloCurvatureExecution_ execution)
            : base_(std::move(base)), point_(std::move(point)), bumps_(std::move(bumps)), settings_(std::move(settings)),
              prepared_(std::move(prepared)), products_(std::move(products)), execution_(std::move(execution)) {}
        [[nodiscard]] const SegmentedMonteCarloResult_& Base() const { return base_; }
        [[nodiscard]] const Vector_<>& Point() const { return point_; }
        [[nodiscard]] const Matrix_<>& Directions() const { return bumps_.directions_; }
        [[nodiscard]] const Vector_<>& Steps() const { return bumps_.steps_; }
        [[nodiscard]] const SegmentedMonteCarloSettings_& Settings() const { return settings_; }
        [[nodiscard]] const PreparedScript_& Prepared() const { return *prepared_; }
        [[nodiscard]] const Matrix_<>& HessianProducts() const { return products_; }
        [[nodiscard]] const MonteCarloCurvatureExecution_& Execution() const { return execution_; }
    };

    [[nodiscard]] MonteCarloCurvatureResult_ EvaluateBlackScholesMonteCarloCurvature(const BlackScholesSegmentedPath_& kernel,
                                                                                     const Vector_<>& parameters,
                                                                                     size_t pathCount,
                                                                                     const AAD::BumpOverAADRequest_& bumps,
                                                                                     const SegmentedMonteCarloSettings_& settings = {});
} // namespace Dal::Script
