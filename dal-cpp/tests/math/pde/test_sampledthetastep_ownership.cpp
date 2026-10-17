//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <future>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <dal/math/buffercapacity.hpp>
#include <dal/math/pde/sampledthetastep.hpp>
#include <dal/platform/platform.hpp>

using Dal::LinearSolveAccuracyPolicy_;
using Dal::Matrix_;
using Dal::PDE::SampledThetaStepAdjoints_;
using Dal::PDE::SampledThetaStepInputs_;
using Dal::PDE::SampledThetaStepPullback_;

namespace {
    SampledThetaStepInputs_ OwnershipInputs() {
        SampledThetaStepInputs_ inputs;
        inputs.x_ = {0.0, 0.7, 1.8, 3.0, 4.5};
        inputs.rates_ = {0.05, 0.06, 0.03};
        inputs.drifts_ = {-0.1, 0.2, -0.05};
        inputs.variances_ = {0.3, 0.4, 0.5};
        inputs.dt_ = 0.2;
        inputs.theta_ = 0.5;
        inputs.oldValues_ = Matrix_<>(5, 2);
        for (int row = 0; row < 5; ++row)
            for (int layer = 0; layer < 2; ++layer)
                inputs.oldValues_(row, layer) = 1.0 + 0.3 * row - 0.2 * layer;
        inputs.externalBoundaries_ = {true, false};
        inputs.externalValues_ = Matrix_<>(2, 2, 1.6);
        return inputs;
    }

    Matrix_<> OwnershipSeeds(double scale = 1.0) {
        Matrix_<> seeds(5, 2);
        for (int row = 0; row < 5; ++row)
            for (int layer = 0; layer < 2; ++layer)
                seeds(row, layer) = scale * (0.5 + 0.1 * row - 0.2 * layer);
        return seeds;
    }

    void CheckRiskScale(const SampledThetaStepAdjoints_& actual, const SampledThetaStepAdjoints_& expected, double scale = 1.0) {
        const std::pair<const Matrix_<>*, const Matrix_<>*> matrixPairs[] = {{&actual.oldValues_, &expected.oldValues_},
                                                                             {&actual.externalValues_, &expected.externalValues_}};
        for (const auto& matrices : matrixPairs) {
            ASSERT_EQ(matrices.first->Rows(), matrices.second->Rows());
            ASSERT_EQ(matrices.first->Cols(), matrices.second->Cols());
            for (int row = 0; row < matrices.first->Rows(); ++row)
                for (int column = 0; column < matrices.first->Cols(); ++column)
                    ASSERT_NEAR((*matrices.first)(row, column), scale * (*matrices.second)(row, column), 1e-12);
        }
        const std::pair<const Dal::Vector_<>*, const Dal::Vector_<>*> vectorPairs[] = {
            {&actual.rates_, &expected.rates_}, {&actual.drifts_, &expected.drifts_}, {&actual.variances_, &expected.variances_}};
        for (const auto& vectors : vectorPairs) {
            ASSERT_EQ(vectors.first->size(), vectors.second->size());
            for (size_t row = 0; row < vectors.first->size(); ++row)
                ASSERT_NEAR((*vectors.first)[row], scale * (*vectors.second)[row], 1e-12);
        }
        ASSERT_NEAR(actual.dt_, scale * expected.dt_, 1e-12);
        ASSERT_NEAR(actual.theta_, scale * expected.theta_, 1e-12);
        ASSERT_EQ(actual.transposeBackwardErrors_.size(), 2);
        for (double error : actual.transposeBackwardErrors_)
            ASSERT_LE(error, 1e-14);
    }
} // namespace

TEST(SampledThetaStepTest, TestOwnsInputsPolicyAndDetachedResults) {
    SampledThetaStepAdjoints_ expected, detached;
    const auto seeds = OwnershipSeeds();
    {
        auto inputs = OwnershipInputs();
        LinearSolveAccuracyPolicy_ policy{1e-14, 1e-14};
        const SampledThetaStepPullback_ step(inputs, policy);
        expected = step.Reverse(seeds);
        const Matrix_<> solution = step.Solution();
        inputs.x_.clear();
        inputs.rates_.clear();
        inputs.drifts_.clear();
        inputs.variances_.clear();
        inputs.oldValues_.Clear();
        inputs.externalValues_.Clear();
        inputs.externalBoundaries_ = {false, true};
        inputs.dt_ = inputs.theta_ = std::numeric_limits<double>::quiet_NaN();
        policy = {0.0, 0.0};
        ASSERT_DOUBLE_EQ(step.Policy().forwardBackwardErrorLimit_, 1e-14);
        ASSERT_DOUBLE_EQ(step.Policy().transposeBackwardErrorLimit_, 1e-14);
        for (int row = 0; row < 5; ++row)
            for (int column = 0; column < 2; ++column)
                ASSERT_DOUBLE_EQ(step.Solution()(row, column), solution(row, column));
        detached = step.Reverse(seeds);
        CheckRiskScale(detached, expected);
    }
    CheckRiskScale(detached, expected);
}

TEST(SampledThetaStepTest, TestCopiesAndMovesOwnIndependentBuffers) {
    const SampledThetaStepPullback_ original(OwnershipInputs(), LinearSolveAccuracyPolicy_{1e-14, 1e-14});
    auto copy = original;
    ASSERT_NE(copy.Solution().Data(), original.Solution().Data());
    ASSERT_NE(copy.ForwardBackwardErrors().data(), original.ForwardBackwardErrors().data());
    const auto seeds = OwnershipSeeds();
    const auto expected = original.Reverse(seeds);
    auto moved = std::move(copy);
    CheckRiskScale(moved.Reverse(seeds), expected);
    copy = original;
    CheckRiskScale(copy.Reverse(seeds), expected);
    CheckRiskScale(original.Reverse(seeds), expected);
}

TEST(SampledThetaStepTest, TestFailedCopyAssignmentPreservesOriginalSolutionAndRisks) {
    for (double theta : {0.0, 0.5}) {
        SCOPED_TRACE(theta);
        auto inputs = OwnershipInputs();
        inputs.theta_ = theta;
        SampledThetaStepPullback_ destination(inputs, LinearSolveAccuracyPolicy_{1e-14, 1e-14});
        const Matrix_<> solution(destination.Solution());
        const auto seeds = OwnershipSeeds();
        const auto expected = destination.Reverse(seeds);
        inputs.rates_ = {0.4, 0.5, 0.6};
        inputs.oldValues_ = Matrix_<>(5, 3, 1.9);
        inputs.externalValues_ = Matrix_<>(2, 3, 1.6);
        const SampledThetaStepPullback_ source(inputs, LinearSolveAccuracyPolicy_{1e-14, 1e-14});
        const size_t copyBytes = 60 * sizeof(double) + (theta == 0.0 ? 0 : 16 * sizeof(double) + 4 * sizeof(int));
        for (size_t limit : {size_t(0), copyBytes - 1}) {
            SCOPED_TRACE(limit);
            Dal::BufferCapacityBudget_ budget(limit);
            Dal::BufferCapacityScope_ scope(&budget);
            ASSERT_NO_THROW(destination = destination);
            ASSERT_THROW(destination = source, Dal::Exception_);
            ASSERT_EQ(budget.CapacityBytes(), 0);
        }
        ASSERT_EQ(destination.Solution().Rows(), solution.Rows());
        ASSERT_EQ(destination.Solution().Cols(), solution.Cols());
        for (int row = 0; row < solution.Rows(); ++row)
            for (int layer = 0; layer < solution.Cols(); ++layer)
                ASSERT_EQ(destination.Solution()(row, layer), solution(row, layer));
        const auto recovered = destination.Reverse(seeds);
        ASSERT_EQ(recovered.dt_, expected.dt_);
        ASSERT_NO_FATAL_FAILURE(CheckRiskScale(recovered, expected));
    }
}

TEST(SampledThetaStepTest, TestRepeatedSeedsAreLinearAndZeroSeedsAreExact) {
    const SampledThetaStepPullback_ step(OwnershipInputs(), LinearSolveAccuracyPolicy_{1e-14, 1e-14});
    const auto expected = step.Reverse(OwnershipSeeds());
    CheckRiskScale(step.Reverse(OwnershipSeeds(-2.0)), expected, -2.0);
    CheckRiskScale(step.Reverse(OwnershipSeeds(3.0)), expected, 3.0);
    CheckRiskScale(step.Reverse(OwnershipSeeds()), expected);
    const auto zero = step.Reverse(Matrix_<>(5, 2));
    for (double value : zero.oldValues_)
        ASSERT_EQ(value, 0.0);
    for (double value : zero.externalValues_)
        ASSERT_EQ(value, 0.0);
    for (const auto* values : {&zero.rates_, &zero.drifts_, &zero.variances_, &zero.transposeBackwardErrors_})
        for (double value : *values)
            ASSERT_EQ(value, 0.0);
    ASSERT_EQ(zero.dt_, 0.0);
    ASSERT_EQ(zero.theta_, 0.0);
}

TEST(SampledThetaStepTest, TestConcurrentConstReverseUsesIndependentRequests) {
    const SampledThetaStepPullback_ step(OwnershipInputs(), LinearSolveAccuracyPolicy_{1e-14, 1e-14});
    const auto expected = step.Reverse(OwnershipSeeds());
    std::vector<std::future<SampledThetaStepAdjoints_>> futures;
    const double scales[] = {1.0, -2.0, 0.0, 3.0};
    for (double scale : scales)
        futures.emplace_back(std::async(std::launch::async, [&step, scale] { return step.Reverse(OwnershipSeeds(scale)); }));
    for (size_t index = 0; index < futures.size(); ++index)
        CheckRiskScale(futures[index].get(), expected, scales[index]);
    CheckRiskScale(step.Reverse(OwnershipSeeds()), expected);
}

TEST(SampledThetaStepTest, TestInvalidSeedsRejectBeforeAllocationAndCacheRecovers) {
    const SampledThetaStepPullback_ step(OwnershipInputs(), LinearSolveAccuracyPolicy_{1e-14, 1e-14});
    for (const Matrix_<>& seeds : {Matrix_<>(4, 2), Matrix_<>(5, 0), Matrix_<>(5, 1), Matrix_<>(5, 2, std::numeric_limits<double>::infinity()),
                                   Matrix_<>(5, 2, std::numeric_limits<double>::quiet_NaN())}) {
        Dal::BufferCapacityBudget_ budget(0);
        Dal::BufferCapacityScope_ scope(&budget);
        try {
            static_cast<void>(step.Reverse(seeds));
            FAIL() << "Invalid sampled theta-step seeds were accepted";
        } catch (const Dal::Exception_& error) {
            ASSERT_NE(std::string(error.what()).find("seed"), std::string::npos);
        }
        ASSERT_EQ(budget.CapacityBytes(), 0);
        ASSERT_EQ(budget.PeakCapacityBytes(), 0);
    }
    const auto recovered = step.Reverse(OwnershipSeeds());
    ASSERT_EQ(recovered.oldValues_.Rows(), 5);
    ASSERT_EQ(recovered.transposeBackwardErrors_.size(), 2);
}

TEST(SampledThetaStepTest, TestTridiagonalLateScalingPreservesMinimumRhs) {
    const Dal::PDE::SampledThetaDetail::TridiagonalFactors_ factors({0.0}, {1.0, 1e300}, {0.0}, 1e-310);
    Matrix_<> rhs(2, 1);
    rhs(0, 0) = std::numeric_limits<double>::denorm_min();
    for (bool transpose : {false, true}) {
        const auto solution = factors.Solve(rhs, transpose);
        ASSERT_EQ(solution(0, 0), std::numeric_limits<double>::denorm_min());
        ASSERT_EQ(solution(1, 0), 0.0);
    }
}
