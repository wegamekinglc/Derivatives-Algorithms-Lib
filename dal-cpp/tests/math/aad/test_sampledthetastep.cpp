//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/sampledthetastep.hpp>

TEST(AADSampledThetaStepTest, TestIndependentScalarRateCompositionAndActualReport) {
    Dal::AAD::Clear(*Dal::AAD::Tape());
    auto mode = Dal::AAD::SetNumResultsForAAD(false, 1);
    Dal::PDE::SampledThetaStepInputs_ inputs;
    inputs.x_ = {0.0, 1.0, 2.0};
    inputs.rates_ = {0.1};
    inputs.drifts_ = {0.2};
    inputs.variances_ = {0.4};
    inputs.dt_ = 0.2;
    inputs.theta_ = 0.5;
    inputs.oldValues_ = Dal::Matrix_<>(3, 1);
    for (int row = 0; row < 3; ++row)
        inputs.oldValues_(row, 0) = row + 1.0;
    inputs.externalBoundaries_ = {true, true};
    inputs.externalValues_ = Dal::Matrix_<>(2, 1);
    inputs.externalValues_(0, 0) = 4.0;
    inputs.externalValues_(1, 0) = 5.0;
    Dal::AAD::RecordingScope_ scope;
    Dal::AAD::Number_ rate;
    scope.RegisterInput(rate, 0.1);
    scope.StartRecording();
    Dal::AAD::SampledThetaStepBindings_ active;
    active.rates_ = {rate};
    auto result = Dal::AAD::SampledThetaStepWithAccuracy(&scope, inputs, active, Dal::LinearSolveAccuracyPolicy_{1e-14, 1e-14});
    Dal::AAD::Number_ objective = 2.0 * result.solution_(1, 0) + rate;
    scope.FinishRecording();
    scope.ClearAdjoints();
    Dal::AAD::NativeOperations_::SetSeed(objective, 1.0);
    const auto reports = Dal::AAD::ReverseWithSolveAccuracy(&scope);
    ASSERT_NEAR(Dal::AAD::Value(result.solution_(1, 0)), 73.0 / 35.0, 1e-13);
    ASSERT_NEAR(Dal::AAD::NativeOperations_::ReadAdjoint(rate), 163.0 / 735.0, 1e-13);
    const auto& errors = reports.Report(result.event_).transposeBackwardErrors_;
    ASSERT_EQ(errors.Rows(), 1);
    ASSERT_EQ(errors.Cols(), 1);
    ASSERT_LE(errors(0, 0), 1e-14);
    ASSERT_EQ(result.diagnostics_.forwardBackwardErrors_.size(), 1);
    scope.Close();
    ASSERT_EQ(result.diagnostics_.policy_.forwardBackwardErrorLimit_, 1e-14);
    ASSERT_LE(result.diagnostics_.forwardBackwardErrors_[0], 1e-14);
    ASSERT_LE(reports.Report(result.event_).transposeBackwardErrors_(0, 0), 1e-14);
    Dal::AAD::Clear(*Dal::AAD::Tape());
}
