//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <dal/math/pde/sampledthetastep.hpp>
#include <dal/platform/platform.hpp>

using Dal::LinearSolveAccuracyPolicy_;
using Dal::Matrix_;
using Dal::PDE::SampledThetaStepInputs_;
using Dal::PDE::SampledThetaStepPullback_;

TEST(SampledThetaStepTest, TestIndependentAnalyticExternalBoundaries) {
    SampledThetaStepInputs_ inputs;
    inputs.x_ = {0.0, 1.0, 2.0};
    inputs.rates_ = {0.1};
    inputs.drifts_ = {0.2};
    inputs.variances_ = {0.4};
    inputs.dt_ = 0.2;
    inputs.theta_ = 0.5;
    inputs.oldValues_ = Matrix_<>(3, 1);
    for (int row = 0; row < 3; ++row)
        inputs.oldValues_(row, 0) = row + 1.0;
    inputs.externalBoundaries_ = {true, true};
    inputs.externalValues_ = Matrix_<>(2, 1);
    inputs.externalValues_(0, 0) = 4.0;
    inputs.externalValues_(1, 0) = 5.0;
    const LinearSolveAccuracyPolicy_ policy{1e-14, 1e-14};
    const SampledThetaStepPullback_ step(inputs, policy);
    ASSERT_EQ(step.Solution().Rows(), 3);
    ASSERT_EQ(step.Solution().Cols(), 1);
    ASSERT_DOUBLE_EQ(step.Solution()(0, 0), 4.0);
    ASSERT_NEAR(step.Solution()(1, 0), 73.0 / 35.0, 1e-13);
    ASSERT_DOUBLE_EQ(step.Solution()(2, 0), 5.0);
    ASSERT_EQ(step.ForwardBackwardErrors().size(), 1);
    ASSERT_LE(step.ForwardBackwardErrors()[0], policy.forwardBackwardErrorLimit_);

    Matrix_<> seeds(3, 1);
    seeds(0, 0) = 0.5;
    seeds(1, 0) = 2.0;
    seeds(2, 0) = -0.3;
    const auto risk = step.Reverse(seeds);
    ASSERT_EQ(risk.oldValues_.Rows(), 3);
    ASSERT_EQ(risk.oldValues_.Cols(), 1);
    ASSERT_EQ(risk.externalValues_.Rows(), 2);
    ASSERT_EQ(risk.externalValues_.Cols(), 1);
    ASSERT_NEAR(risk.oldValues_(0, 0), 2.0 / 105.0, 1e-13);
    ASSERT_NEAR(risk.oldValues_(1, 0), 38.0 / 21.0, 1e-13);
    ASSERT_NEAR(risk.oldValues_(2, 0), 2.0 / 35.0, 1e-13);
    ASSERT_NEAR(risk.externalValues_(0, 0), 109.0 / 210.0, 1e-13);
    ASSERT_NEAR(risk.externalValues_(1, 0), -17.0 / 70.0, 1e-13);
    ASSERT_EQ(risk.rates_.size(), 1);
    ASSERT_EQ(risk.drifts_.size(), 1);
    ASSERT_EQ(risk.variances_.size(), 1);
    ASSERT_NEAR(risk.rates_[0], -572.0 / 735.0, 1e-13);
    ASSERT_NEAR(risk.drifts_[0], 2.0 / 7.0, 1e-13);
    ASSERT_NEAR(risk.variances_[0], 338.0 / 735.0, 1e-13);
    ASSERT_NEAR(risk.dt_, 40.0 / 49.0, 1e-13);
    ASSERT_NEAR(risk.theta_, 16.0 / 49.0, 1e-13);
    ASSERT_EQ(risk.transposeBackwardErrors_.size(), 1);
    ASSERT_LE(risk.transposeBackwardErrors_[0], policy.transposeBackwardErrorLimit_);
    ASSERT_DOUBLE_EQ(step.Policy().forwardBackwardErrorLimit_, policy.forwardBackwardErrorLimit_);
    ASSERT_DOUBLE_EQ(step.Policy().transposeBackwardErrorLimit_, policy.transposeBackwardErrorLimit_);
}
