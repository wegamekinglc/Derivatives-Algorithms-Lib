//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <dal/math/aad/implicitroot.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/platform/platform.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    class ElementaryRootEquation_ final : public ImplicitRootEquation_ {
    public:
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& theta, const Vector_<>& inputs) const override {
            return {Vector_<>{theta[0] * theta[0] - inputs[0]}, SquareMatrix_<>(1, 2.0 * theta[0]), Matrix_<>(1, 1, -1.0)};
        }
    };

    class NonsymmetricRootEquation_ final : public ImplicitRootEquation_ {
    public:
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& theta, const Vector_<>& inputs) const override {
            ImplicitRootEvaluation_ result{
                Vector_<>{theta[0] * theta[0] + 2.0 * theta[1] - inputs[0], 3.0 * theta[0] + theta[1] * theta[1] - inputs[1]}, SquareMatrix_<>(2),
                Matrix_<>(2, 2)};
            result.parameterJacobian_(0, 0) = 2.0 * theta[0];
            result.parameterJacobian_(0, 1) = 2.0;
            result.parameterJacobian_(1, 0) = 3.0;
            result.parameterJacobian_(1, 1) = 2.0 * theta[1];
            result.inputJacobian_(0, 0) = result.inputJacobian_(1, 1) = -1.0;
            return result;
        }
    };

    class NoInputRootEquation_ final : public ImplicitRootEquation_ {
    public:
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& theta, const Vector_<>&) const override {
            return {Vector_<>{theta[0] - 2.0}, SquareMatrix_<>(1, 1.0), Matrix_<>(1, 0)};
        }
    };

    void CheckElementaryRootBranch(double sign) {
        SCOPED_TRACE(sign);
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(false, 1);
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, 4.0);
        scope.StartRecording();
        const ElementaryRootEquation_ equation;
        const auto root = ImplicitRootWithAccuracy(&scope, equation, Vector_<>{sign * 2.0}, Vector_<Number_>{input},
                                                   ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
        Number_ difference = root.parameters_[0] - sign * sqrt(input);
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(difference, 1.0);
        const auto reports = ReverseWithSolveAccuracy(&scope);
        ASSERT_DOUBLE_EQ(Value(difference), 0.0);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.0);
        ASSERT_DOUBLE_EQ(reports.Report(root.event_).transposeBackwardErrors_(0, 0), 0.0);
        scope.Close();
        Clear(*Tape());
    }

    void CheckNonsymmetricRoot(bool pivot) {
        SCOPED_TRACE(pivot);
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(false, 1);
        RecordingScope_ scope;
        Vector_<Number_> inputs(2);
        scope.RegisterInput(inputs[0], pivot ? 6.0 : 10.0);
        scope.RegisterInput(inputs[1], pivot ? 9.0 : 15.0);
        scope.StartRecording();
        const NonsymmetricRootEquation_ equation;
        const auto root = ImplicitRootWithAccuracy(&scope, equation, Vector_<>{pivot ? 0.0 : 2.0, 3.0}, inputs,
                                                   ImplicitRootAccuracyPolicy_{Vector_<>{0.0, 0.0}, 1e-14});
        Number_ objective = root.parameters_[0] - 2.0 * root.parameters_[1];
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(objective, 1.0);
        const auto reports = ReverseWithSolveAccuracy(&scope);
        ASSERT_NEAR(NativeOperations_::ReadAdjoint(inputs[0]), pivot ? -2.0 : 2.0 / 3.0, 1e-14);
        ASSERT_NEAR(NativeOperations_::ReadAdjoint(inputs[1]), pivot ? 1.0 / 3.0 : -5.0 / 9.0, 1e-14);
        ASSERT_EQ(reports.Report(root.event_).transposeBackwardErrors_.Rows(), 1);
        ASSERT_LE(reports.Report(root.event_).transposeBackwardErrors_(0, 0), 1e-14);
        scope.Close();
        Clear(*Tape());
    }
} // namespace

TEST(AADLinearSolveTest, TestRecordedImplicitRootMatchesElementaryPositiveAndNegativeBranches) {
    for (double sign : {1.0, -1.0})
        ASSERT_NO_FATAL_FAILURE(CheckElementaryRootBranch(sign));
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootNonsymmetricAndPivotCramerReferences) {
    for (bool pivot : {false, true})
        ASSERT_NO_FATAL_FAILURE(CheckNonsymmetricRoot(pivot));
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootApproximateCandidateKeepsPointAndResidual) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 4.0);
    scope.StartRecording();
    const ElementaryRootEquation_ equation;
    auto root =
        ImplicitRootWithAccuracy(&scope, equation, Vector_<>{1.9}, Vector_<Number_>{input}, ImplicitRootAccuracyPolicy_{Vector_<>{0.4}, 1e-14});
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(root.parameters_[0], 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_DOUBLE_EQ(Value(root.parameters_[0]), 1.9);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(input), 1.0 / 3.8, 1e-14);
    ASSERT_NEAR(root.diagnostics_.residuals_[0], -0.39, 1e-14);
    ASSERT_DOUBLE_EQ(root.diagnostics_.policy_.residualAbsoluteLimits_[0], 0.4);
    ASSERT_LE(reports.Report(root.event_).transposeBackwardErrors_(0, 0), 1e-14);
    scope.Close();
    ASSERT_NEAR(root.diagnostics_.residuals_[0], -0.39, 1e-14);
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootZeroInputsStillExecutesRequestedTranspose) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(true, 4);
    RecordingScope_ scope;
    scope.StartRecording();
    const NoInputRootEquation_ equation;
    auto root = ImplicitRootWithAccuracy(&scope, equation, Vector_<>{2.0}, Vector_<Number_>{}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    for (size_t channel = 0; channel < 4; ++channel)
        NativeOperations_::SetSeed(root.parameters_[0], static_cast<double>(channel), channel);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    const auto& errors = reports.Report(root.event_).transposeBackwardErrors_;
    ASSERT_EQ(root.parameters_.size(), 1);
    ASSERT_EQ(reports.Entries().size(), 1);
    ASSERT_EQ(errors.Rows(), 1);
    ASSERT_EQ(errors.Cols(), 4);
    for (double error : errors)
        ASSERT_DOUBLE_EQ(error, 0.0);
    scope.Close();
    Clear(*Tape());
}
