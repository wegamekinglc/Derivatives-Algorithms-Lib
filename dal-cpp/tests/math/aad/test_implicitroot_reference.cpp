//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include <dal/math/aad/implicitroot.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/platform/platform.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    class CoupledRootEquation_ final : public ImplicitRootEquation_ {
    public:
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& theta, const Vector_<>& inputs) const override {
            ImplicitRootEvaluation_ result{Vector_<>{theta[0] * theta[0] + theta[1] - inputs[0], theta[0] + theta[1] * theta[1] - inputs[1]},
                                           SquareMatrix_<>(2), Matrix_<>(2, 2)};
            result.parameterJacobian_(0, 0) = 2.0 * theta[0];
            result.parameterJacobian_(0, 1) = result.parameterJacobian_(1, 0) = 1.0;
            result.parameterJacobian_(1, 1) = 2.0 * theta[1];
            result.inputJacobian_(0, 0) = result.inputJacobian_(1, 1) = -1.0;
            return result;
        }
    };

    class AliasedRootEquation_ final : public ImplicitRootEquation_ {
    public:
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& theta, const Vector_<>& inputs) const override {
            Matrix_<> inputJacobian(1, 2);
            inputJacobian(0, 0) = -1.0;
            inputJacobian(0, 1) = -2.0;
            return {Vector_<>{theta[0] - inputs[0] - 2.0 * inputs[1]}, SquareMatrix_<>(1, 1.0), std::move(inputJacobian)};
        }
    };

    class MutableSourceRootEquation_ final : public ImplicitRootEquation_ {
        Number_* source_;
        std::shared_ptr<int> calls_;

    public:
        MutableSourceRootEquation_(Number_* source, std::shared_ptr<int> calls) : source_(source), calls_(std::move(calls)) {}
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& theta, const Vector_<>& inputs) const override {
            ++*calls_;
            *source_ = 9.0;
            return {Vector_<>{theta[0] * theta[0] - inputs[0]}, SquareMatrix_<>(1, 2.0 * theta[0]), Matrix_<>(1, 1, -1.0)};
        }
    };

    class StationaryRootEquation_ final : public ImplicitRootEquation_ {
    public:
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& theta, const Vector_<>& inputs) const override {
            const double parameter = theta[0], residual = parameter * parameter - inputs[0];
            Matrix_<> inputJacobian(1, 2);
            inputJacobian(0, 0) = -2.0 * parameter;
            inputJacobian(0, 1) = -1.0;
            return {Vector_<>{2.0 * parameter * residual + parameter - inputs[1]},
                    SquareMatrix_<>(1, 4.0 * parameter * parameter + 2.0 * residual + 1.0), std::move(inputJacobian)};
        }
    };

    double ChannelWeight(size_t channel) {
        const double weights[] = {1.0, -2.0, 0.0, 3.0};
        return weights[channel % 4];
    }

    void CheckCoupledRootChannels(size_t width) {
        SCOPED_TRACE(width);
        Clear(*Tape());
        const size_t channels = std::max(size_t(1), width);
        auto mode = SetNumResultsForAAD(width != 0, channels);
        RecordingScope_ scope;
        Vector_<Number_> inputs(2);
        scope.RegisterInput(inputs[0], 7.0);
        scope.RegisterInput(inputs[1], 11.0);
        scope.StartRecording();
        const CoupledRootEquation_ equation;
        const auto root =
            ImplicitRootWithAccuracy(&scope, equation, Vector_<>{2.0, 3.0}, inputs, ImplicitRootAccuracyPolicy_{Vector_<>{0.0, 0.0}, 1e-14});
        Number_ objective = root.parameters_[0] - 2.0 * root.parameters_[1] + 5.0 * inputs[0] - inputs[1];
        scope.FinishRecording();
        scope.ClearAdjoints();
        for (size_t channel = 0; channel < channels; ++channel)
            NativeOperations_::SetSeed(objective, ChannelWeight(channel), channel);
        const auto reports = ReverseWithSolveAccuracy(&scope);
        const auto& report = reports.Report(root.event_);
        ASSERT_EQ(report.transposeBackwardErrors_.Rows(), 1);
        ASSERT_EQ(report.transposeBackwardErrors_.Cols(), static_cast<int>(channels));
        ASSERT_EQ(reports.IsMulti(), width != 0);
        ASSERT_DOUBLE_EQ(Value(objective), 20.0);
        for (size_t channel = 0; channel < channels; ++channel) {
            ASSERT_NEAR(NativeOperations_::ReadAdjoint(inputs[0], channel), (5.0 + 8.0 / 23.0) * ChannelWeight(channel), 1e-13);
            ASSERT_NEAR(NativeOperations_::ReadAdjoint(inputs[1], channel), (-1.0 - 9.0 / 23.0) * ChannelWeight(channel), 1e-13);
            ASSERT_GE(report.transposeBackwardErrors_(0, static_cast<int>(channel)), 0.0);
            ASSERT_LE(report.transposeBackwardErrors_(0, static_cast<int>(channel)), 1e-14);
        }
        scope.Close();
        Clear(*Tape());
    }
} // namespace

TEST(AADLinearSolveTest, TestRecordedImplicitRootIndependentChannelsAndReportAxes) {
    for (size_t width : {0U, 1U, 4U, 8U})
        ASSERT_NO_FATAL_FAILURE(CheckCoupledRootChannels(width));
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootAliasedInputsAndDirectTerms) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 2.0);
    scope.StartRecording();
    const AliasedRootEquation_ equation;
    const auto root =
        ImplicitRootWithAccuracy(&scope, equation, Vector_<>{6.0}, Vector_<Number_>{input, input}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
    Number_ objective = root.parameters_[0] + input;
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_DOUBLE_EQ(Value(objective), 8.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 4.0);
    ASSERT_DOUBLE_EQ(reports.Report(root.event_).transposeBackwardErrors_(0, 0), 0.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootCapturesBindingsBeforeEquationMutatesSources) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Vector_<Number_> inputs(1);
    scope.RegisterInput(inputs[0], 4.0);
    const Number_ original = inputs[0];
    const auto calls = std::make_shared<int>(0);
    scope.StartRecording();
    CheckedImplicitRootResult_ root;
    {
        const MutableSourceRootEquation_ equation(&inputs[0], calls);
        root = ImplicitRootWithAccuracy(&scope, equation, Vector_<>{2.0}, inputs, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
    }
    ASSERT_EQ(*calls, 1);
    ASSERT_DOUBLE_EQ(Value(inputs[0]), 9.0);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(root.parameters_[0], 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(original), 0.25);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(inputs[0]), 0.0);
    ASSERT_DOUBLE_EQ(root.diagnostics_.residuals_[0], 0.0);
    ASSERT_DOUBLE_EQ(reports.Report(root.event_).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_EQ(*calls, 1);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootFullStationarityJacobian) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Vector_<Number_> inputs(2);
    scope.RegisterInput(inputs[0], 0.0);
    scope.RegisterInput(inputs[1], 3.0);
    scope.StartRecording();
    const StationaryRootEquation_ equation;
    auto root = ImplicitRootWithAccuracy(&scope, equation, Vector_<>{1.0}, inputs, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 1e-14});
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(root.parameters_[0], 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(inputs[0]), 2.0 / 7.0, 1e-14);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(inputs[1]), 1.0 / 7.0, 1e-14);
    ASSERT_LE(reports.Report(root.event_).transposeBackwardErrors_(0, 0), 1e-14);
    ASSERT_EQ(root.diagnostics_.residuals_.size(), 1);
    ASSERT_DOUBLE_EQ(root.diagnostics_.residuals_[0], 0.0);
    scope.Close();
    Clear(*Tape());
}
