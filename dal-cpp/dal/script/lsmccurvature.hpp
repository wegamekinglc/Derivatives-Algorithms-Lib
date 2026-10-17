//
// Created by Codex on 2026/10/10.
//

#pragma once

#include <memory>
#include <optional>
#include <utility>

#include <dal/math/aad/bumpoveraad.hpp>
#include <dal/script/lsmc.hpp>
#include <dal/script/preparation.hpp>

namespace Dal::Script {
    struct LsmcCurvatureExecution_ {
        String_ method_;
        size_t gradientEvaluations_ = 0;
        size_t pathsPerReplicate_ = 0;
        size_t trainingPaths_ = 0;
        size_t validationPaths_ = 0;
        size_t pricingReplicates_ = 1;
        size_t numericPayloadBytes_ = 0;
        size_t maxBatchTapeBytes_ = 0;
        size_t maxBatchCleanupReserveBytes_ = 0;
        std::optional<size_t> recordingCapacityBudgetBytes_;
    };

    class BlackScholesLsmcCurvatureResult_ {
        double value_;
        Vector_<> gradient_;
        Vector_<> point_;
        AAD::BumpOverAADRequest_ bumps_;
        Matrix_<> products_;
        std::shared_ptr<const PreparedScript_> prepared_;
        Vector_<ExerciseRegression_> policy_;
        LsmcCurvatureExecution_ execution_;

    public:
        BlackScholesLsmcCurvatureResult_(double value,
                                         Vector_<> gradient,
                                         Vector_<> point,
                                         AAD::BumpOverAADRequest_ bumps,
                                         Matrix_<> products,
                                         std::shared_ptr<const PreparedScript_> prepared,
                                         Vector_<ExerciseRegression_> policy,
                                         LsmcCurvatureExecution_ execution)
            : value_(value), gradient_(std::move(gradient)), point_(std::move(point)), bumps_(std::move(bumps)), products_(std::move(products)),
              prepared_(std::move(prepared)), policy_(std::move(policy)), execution_(std::move(execution)) {
            REQUIRE2(prepared_, "LsmcCurvatureResult: prepared script must be present", ScriptError_);
        }
        [[nodiscard]] double Value() const { return value_; }
        [[nodiscard]] const Vector_<>& Gradient() const { return gradient_; }
        [[nodiscard]] const Vector_<>& Point() const { return point_; }
        [[nodiscard]] const Matrix_<>& Directions() const { return bumps_.directions_; }
        [[nodiscard]] const Vector_<>& Steps() const { return bumps_.steps_; }
        [[nodiscard]] const Matrix_<>& HessianProducts() const { return products_; }
        [[nodiscard]] const PreparedScript_& Prepared() const { return *prepared_; }
        [[nodiscard]] const Vector_<ExerciseRegression_>& BasePolicy() const { return policy_; }
        [[nodiscard]] const LsmcCurvatureExecution_& Execution() const { return execution_; }
    };

    [[nodiscard]] BlackScholesLsmcCurvatureResult_ EvaluateBlackScholesLsmcCurvature(std::shared_ptr<const PreparedScript_> prepared,
                                                                                     const Vector_<>& parameters,
                                                                                     size_t pathCount,
                                                                                     const AAD::BumpOverAADRequest_& bumps);
} // namespace Dal::Script
