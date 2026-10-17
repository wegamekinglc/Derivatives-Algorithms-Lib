//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <dal/math/aad/linearsolvecoordinateaccuracy.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/platform/platform.hpp>

TEST(AADLinearSolveTest, TestCheckedCoordinateCompositionAndDetachedReports) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::SolveAccuracyReports_ detached;
    AAD::SolveAccuracyEvent_ token;
    {
        AAD::RecordingScope_ scope;
        AAD::Number_ parameter, right;
        scope.RegisterInput(parameter, 1.0);
        scope.RegisterInput(right, 2.0);
        scope.StartRecording();
        AAD::CheckedLinearSolveResult_ checked;
        {
            Vector_<AAD::Number_> parameters{AAD::Number_(3.0), parameter, AAD::Number_(2.0)};
            Matrix_<AAD::Number_> rhs(2, 1);
            rhs(0, 0) = 1.0;
            rhs(1, 0) = right;
            LinearSolveAccuracyPolicy_ policy{0.0, 0.0};
            const auto before = AAD::MeasureTape(*AAD::Tape());
            checked = AAD::LinearSolveWithAccuracy(&scope, LinearSolveCoordinates_::Symmetric(2), parameters, rhs, policy);
            ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_ - before.nodes_, 2);
            ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).reverseEvents_ - before.reverseEvents_, 1);
            parameters.clear();
            rhs.Resize(0, 0);
            policy.transposeBackwardErrorLimit_ = -1.0;
        }
        token = checked.event_;
        AAD::Number_ objective = 2.0 * checked.solution_(0, 0) - checked.solution_(1, 0) + parameter * parameter;
        ASSERT_DOUBLE_EQ(AAD::Value(objective), 0.0);
        scope.FinishRecording();
        for (double weight : {1.0, -2.0, 0.0}) {
            scope.ClearAdjoints();
            AAD::NativeOperations_::SetSeed(objective, weight);
            const auto previous = detached.InvocationId();
            detached = AAD::ReverseWithSolveAccuracy(&scope);
            ASSERT_NE(detached.InvocationId(), previous);
            ASSERT_EQ(detached.Entries().size(), 1);
            ASSERT_EQ(detached.Report(token).transposeBackwardErrors_.Rows(), 1);
            ASSERT_EQ(detached.Report(token).transposeBackwardErrors_.Cols(), 1);
            ASSERT_DOUBLE_EQ(detached.Report(token).transposeBackwardErrors_(0, 0), 0.0);
            ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(parameter), weight);
            ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(right), -weight);
            ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(checked.solution_(0, 0)), 0.0);
        }
        scope.Close();
    }
    ASSERT_EQ(detached.Entries().size(), 1);
    ASSERT_DOUBLE_EQ(detached.Report(token).transposeBackwardErrors_(0, 0), 0.0);
    AAD::Clear(*AAD::Tape());
}
