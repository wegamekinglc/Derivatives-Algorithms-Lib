//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <algorithm>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/sampledthetastep.hpp>
#include <dal/platform/platform.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    PDE::SampledThetaStepInputs_ ScalarCompositionInputs() {
        PDE::SampledThetaStepInputs_ inputs;
        inputs.x_ = {0.0, 1.0, 2.0};
        inputs.rates_ = {0.1};
        inputs.drifts_ = {0.2};
        inputs.variances_ = {0.4};
        inputs.dt_ = 0.2;
        inputs.theta_ = 0.5;
        inputs.oldValues_ = Matrix_<>(3, 1);
        for (int row = 0; row < 3; ++row)
            inputs.oldValues_(row, 0) = row + 1.0;
        return inputs;
    }

    void CheckAllFieldAliases(size_t width) {
        Clear(*Tape());
        const size_t channels = std::max(size_t(1), width);
        auto mode = SetNumResultsForAAD(width != 0, channels);
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, 0.25);
        scope.StartRecording();
        PDE::SampledThetaStepInputs_ numeric;
        numeric.x_ = {0.0, 1.0, 2.0};
        numeric.externalBoundaries_ = {true, false};
        SampledThetaStepBindings_ active{{input}, {input}, {input}, Matrix_<Number_>(3, 2, input), Matrix_<Number_>(2, 2, input), input, input};
        const auto result = SampledThetaStepWithAccuracy(&scope, numeric, active, LinearSolveAccuracyPolicy_{1e-14, 1e-14});
        Number_ objective = 2.0 * result.solution_(1, 0) - 0.75 * result.solution_(1, 1) + 0.3 * result.solution_(0, 0) -
                            0.4 * result.solution_(0, 1) - 0.7 * result.solution_(2, 0) + 0.2 * result.solution_(2, 1) + input + 2.0 * input -
                            3.0 * input;
        scope.FinishRecording();
        ASSERT_NEAR(Value(objective), 379.0 / 2640.0, 1e-13);
        for (double scale : {1.0, -2.0, 0.0, 1.5}) {
            scope.ClearAdjoints();
            for (size_t channel = 0; channel < channels; ++channel)
                NativeOperations_::SetSeed(objective, scale * (channel + 1.0), channel);
            const auto reports = ReverseWithSolveAccuracy(&scope);
            const auto& errors = reports.Report(result.event_).transposeBackwardErrors_;
            ASSERT_EQ(errors.Rows(), 2);
            ASSERT_EQ(errors.Cols(), static_cast<int>(channels));
            for (size_t channel = 0; channel < channels; ++channel) {
                ASSERT_NEAR(NativeOperations_::ReadAdjoint(input, channel), scale * (channel + 1.0) * 3119.0 / 7260.0, 1e-12);
                if (scale == 0.0) {
                    ASSERT_EQ(NativeOperations_::ReadAdjoint(input, channel), 0.0);
                }
            }
            for (double error : errors)
                ASSERT_LE(error, 1e-14);
        }
        scope.Close();
        Clear(*Tape());
    }
} // namespace

TEST(AADSampledThetaStepTest, TestAllFieldAliasesAndRepeatedChannelSeeds) {
    for (size_t width : {0U, 1U, 4U, 8U})
        ASSERT_NO_FATAL_FAILURE(CheckAllFieldAliases(width));
}

TEST(AADSampledThetaStepTest, TestThreeStepCompositionWithSharedRateAndInitialStateRisk) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ rate;
    scope.RegisterInput(rate, 0.25);
    Matrix_<Number_> state(3, 1);
    for (int row = 0; row < 3; ++row)
        scope.RegisterInput(state(row, 0), row + 1.0);
    const Number_ initial = state(1, 0);
    scope.StartRecording();
    auto numeric = ScalarCompositionInputs();
    numeric.drifts_ = {0.0};
    numeric.variances_ = {0.0};
    numeric.dt_ = 0.5;
    Vector_<SolveAccuracyEvent_> events;
    for (double theta : {0.0, 1.0, 0.5}) {
        numeric.theta_ = theta;
        SampledThetaStepBindings_ active;
        active.rates_ = {rate};
        active.oldValues_ = state;
        const auto step = SampledThetaStepWithAccuracy(&scope, numeric, active, LinearSolveAccuracyPolicy_{1e-14, 1e-14});
        events.push_back(step.event_);
        state = step.solution_;
    }
    Number_ objective = state(1, 0) + rate + initial;
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_NEAR(Value(state(1, 0)), 70.0 / 51.0, 1e-13);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(rate), -8453.0 / 7803.0, 1e-13);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(initial), 86.0 / 51.0, 1e-13);
    ASSERT_EQ(reports.Entries().size(), 3);
    for (const auto& event : events) {
        const auto& errors = reports.Report(event).transposeBackwardErrors_;
        ASSERT_EQ(errors.Rows(), 1);
        ASSERT_EQ(errors.Cols(), 1);
        ASSERT_LE(errors(0, 0), 1e-14);
    }
    scope.Close();
    Clear(*Tape());
}

TEST(AADSampledThetaStepTest, TestVarianceBindingComposesUpstreamVolatilitySquare) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ sigma;
    scope.RegisterInput(sigma, 0.25);
    scope.StartRecording();
    auto numeric = ScalarCompositionInputs();
    numeric.externalBoundaries_ = {true, true};
    numeric.externalValues_ = Matrix_<>(2, 1);
    numeric.externalValues_(0, 0) = 4.0;
    numeric.externalValues_(1, 0) = 5.0;
    SampledThetaStepBindings_ active;
    active.variances_ = {Number_(sigma * sigma)};
    const auto step = SampledThetaStepWithAccuracy(&scope, numeric, active, LinearSolveAccuracyPolicy_{1e-14, 1e-14});
    Number_ objective = step.solution_(1, 0);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_NEAR(Value(step.solution_(1, 0)), 1087.0 / 542.0, 1e-13);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(sigma), 27040.0 / 220323.0, 1e-13);
    ASSERT_LE(reports.Report(step.event_).transposeBackwardErrors_(0, 0), 1e-14);
    scope.Close();
    Clear(*Tape());
}
