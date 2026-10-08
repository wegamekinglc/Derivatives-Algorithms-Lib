//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <dal/math/aad/implicitroot.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/platform/platform.hpp>

namespace {
    class RecordedQuadraticEquation_ final : public Dal::ImplicitRootEquation_ {
    public:
        [[nodiscard]] Dal::ImplicitRootEvaluation_ Evaluate(const Dal::Vector_<>& theta, const Dal::Vector_<>& inputs) const override {
            return {Dal::Vector_<>{theta[0] * theta[0] - inputs[0]}, Dal::SquareMatrix_<>(1, 2.0 * theta[0]), Dal::Matrix_<>(1, 1, -1.0)};
        }
    };
} // namespace

TEST(AADLinearSolveTest, TestRecordedImplicitRootCompositionAndOwningDiagnostics) {
    using namespace Dal;
    using namespace Dal::AAD;
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    const RecordedQuadraticEquation_ equation;
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 4.0);
    scope.StartRecording();
    const auto root =
        ImplicitRootWithAccuracy(&scope, equation, Vector_<>{2.0}, Vector_<Number_>{input}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
    Number_ objective = 3.0 * root.parameters_[0] + input;
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_DOUBLE_EQ(Value(root.parameters_[0]), 2.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 1.75);
    ASSERT_EQ(reports.Report(root.event_).transposeBackwardErrors_.Rows(), 1);
    ASSERT_EQ(reports.Report(root.event_).transposeBackwardErrors_.Cols(), 1);
    ASSERT_DOUBLE_EQ(reports.Report(root.event_).transposeBackwardErrors_(0, 0), 0.0);
    scope.Close();
    ASSERT_DOUBLE_EQ(root.diagnostics_.residuals_[0], 0.0);
    ASSERT_DOUBLE_EQ(root.diagnostics_.policy_.residualAbsoluteLimits_[0], 0.0);
    ASSERT_DOUBLE_EQ(root.diagnostics_.reciprocalConditionInfinity_, 1.0);
    ASSERT_EQ(reports.Entries().size(), 1);
    Clear(*Tape());
}
