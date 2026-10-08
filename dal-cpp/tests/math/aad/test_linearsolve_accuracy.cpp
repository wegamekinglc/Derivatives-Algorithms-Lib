//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <future>
#include <limits>

#include <dal/math/aad/linearsolveaccuracy.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/platform/platform.hpp>

TEST(AADLinearSolveTest, TestCheckedActualReverseReportsComposeOrdinaryExpressions) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::SolveAccuracyReports_ detached;
    {
        AAD::RecordingScope_ scope;
        AAD::Number_ input;
        scope.RegisterInput(input, 1.0);
        scope.StartRecording();
        SquareMatrix_<AAD::Number_> matrix(2);
        matrix(0, 0) = 2.0;
        matrix(0, 1) = input;
        matrix(1, 0) = 0.0;
        matrix(1, 1) = 4.0;
        Matrix_<AAD::Number_> rhs(2, 1);
        rhs(0, 0) = input + 4.0;
        rhs(1, 0) = 8.0;
        auto checked = AAD::LinearSolveWithAccuracy(&scope, matrix, rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
        AAD::Number_ objective = 3.0 * checked.solution_(0, 0) - checked.solution_(1, 0);
        scope.FinishRecording();
        scope.ClearAdjoints();
        AAD::NativeOperations_::SetSeed(objective, 1.0);
        detached = AAD::ReverseWithSolveAccuracy(&scope);
        const auto& report = detached.Report(checked.event_);
        ASSERT_EQ(report.transposeBackwardErrors_.Rows(), 1);
        ASSERT_EQ(report.transposeBackwardErrors_.Cols(), 1);
        ASSERT_EQ(report.transposeBackwardErrors_(0, 0), 0.0);
        ASSERT_EQ(checked.diagnostics_.componentwiseBackwardErrors_[0], 0.0);
        ASSERT_EQ(AAD::Value(objective), 2.5);
        ASSERT_EQ(AAD::NativeOperations_::ReadAdjoint(input), -1.5);
        ASSERT_EQ(detached.Channels(), 1);
        ASSERT_FALSE(detached.IsMulti());
        const auto firstInvocation = detached.InvocationId();
        scope.ClearAdjoints();
        AAD::NativeOperations_::SetSeed(objective, -2.0);
        const auto repeated = AAD::ReverseWithSolveAccuracy(&scope);
        ASSERT_NE(repeated.InvocationId(), firstInvocation);
        ASSERT_EQ(repeated.Report(checked.event_).transposeBackwardErrors_(0, 0), 0.0);
        ASSERT_EQ(AAD::NativeOperations_::ReadAdjoint(input), 3.0);
        ASSERT_EQ(detached.InvocationId(), firstInvocation);
        scope.Close();
    }
    ASSERT_EQ(detached.Entries().size(), 1);
    ASSERT_EQ(detached.Entries()[0].transposeBackwardErrors_(0, 0), 0.0);
    AAD::Clear(*AAD::Tape());
}

namespace {
    struct ActivitySample_ {
        Dal::SquareMatrix_<Dal::AAD::Number_> activeMatrix_{2};
        Dal::SquareMatrix_<> matrix_{2};
        Dal::Matrix_<> rhs_{2, 1};
        Dal::Matrix_<Dal::AAD::Number_> activeRhs_{2, 1};

        explicit ActivitySample_(const Dal::AAD::Number_& input) {
            matrix_(0, 0) = 2.0;
            matrix_(0, 1) = 1.0;
            matrix_(1, 0) = 0.0;
            matrix_(1, 1) = 4.0;
            for (int row = 0; row < 2; ++row)
                for (int column = 0; column < 2; ++column)
                    activeMatrix_(row, column) = matrix_(row, column);
            activeMatrix_(0, 1) = input;
            rhs_(0, 0) = 5.0;
            rhs_(1, 0) = 8.0;
            activeRhs_(0, 0) = input;
            activeRhs_(1, 0) = 8.0;
        }

        Dal::AAD::CheckedLinearSolveResult_ Solve(Dal::AAD::RecordingScope_* scope, int activity) const {
            const Dal::LinearSolveAccuracyPolicy_ policy{0.0, 0.0};
            return activity == 1   ? Dal::AAD::LinearSolveWithAccuracy(scope, activeMatrix_, rhs_, policy)
                   : activity == 2 ? Dal::AAD::LinearSolveWithAccuracy(scope, matrix_, activeRhs_, policy)
                                   : Dal::AAD::LinearSolveWithAccuracy(scope, activeMatrix_, activeRhs_, policy);
        }
    };

    void CheckActivity(size_t width, int activity) {
        using namespace Dal;
        SCOPED_TRACE(width);
        SCOPED_TRACE(activity);
        AAD::Clear(*AAD::Tape());
        const size_t channels = width == 0 ? 1 : width;
        auto mode = AAD::SetNumResultsForAAD(width != 0, channels);
        AAD::RecordingScope_ scope;
        AAD::Number_ input;
        const double inputs[] = {1.0, 1.0, 5.0};
        scope.RegisterInput(input, inputs[activity]);
        scope.StartRecording();
        const ActivitySample_ sample(input);
        const auto checked = sample.Solve(&scope, activity);
        AAD::Number_ objective = 3.0 * checked.solution_(0, 0) - checked.solution_(1, 0);
        scope.FinishRecording();
        scope.ClearAdjoints();
        const double weights[] = {1.0, -2.0, 0.0, 3.0};
        for (size_t channel = 0; channel < channels; ++channel)
            AAD::NativeOperations_::SetSeed(objective, weights[channel % 4], channel);
        const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
        const auto detached = reports.Report(checked.event_);
        ASSERT_EQ(reports.Channels(), channels);
        ASSERT_EQ(reports.IsMulti(), width != 0);
        ASSERT_EQ(detached.invocationId_, reports.InvocationId());
        ASSERT_EQ(detached.isMulti_, width != 0);
        ASSERT_EQ(detached.transposeBackwardErrors_.Rows(), 1);
        ASSERT_EQ(detached.transposeBackwardErrors_.Cols(), static_cast<int>(channels));
        const double values[] = {-3.5, 2.5, 2.5};
        const double gradients[] = {-1.5, -3.0, 1.5};
        ASSERT_DOUBLE_EQ(AAD::Value(objective), values[activity]);
        for (size_t channel = 0; channel < channels; ++channel) {
            ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input, channel), gradients[activity] * weights[channel % 4]);
            ASSERT_DOUBLE_EQ(detached.transposeBackwardErrors_(0, static_cast<int>(channel)), 0.0);
            ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(checked.solution_(0, 0), channel), 0.0);
        }
        scope.Close();
        ASSERT_EQ(detached.event_, checked.event_);
        ASSERT_EQ(detached.invocationId_, reports.InvocationId());
        AAD::Clear(*AAD::Tape());
    }
} // namespace

TEST(AADLinearSolveTest, TestCheckedActivityAliasesAndChannelAxes) {
    for (size_t width : {0U, 1U, 4U, 8U})
        for (int activity = 0; activity < 3; ++activity) {
            CheckActivity(width, activity);
            ASSERT_FALSE(HasFatalFailure());
        }
}

TEST(AADLinearSolveTest, TestCheckedMultipleRhsComposeAliases) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    auto mode = AAD::SetNumResultsForAAD(true, 4);
    AAD::RecordingScope_ scope;
    AAD::Number_ input;
    scope.RegisterInput(input, 1.0);
    scope.StartRecording();
    SquareMatrix_<AAD::Number_> matrix(2);
    matrix(0, 0) = 2.0;
    matrix(0, 1) = input;
    matrix(1, 0) = 0.0;
    matrix(1, 1) = 4.0;
    Matrix_<AAD::Number_> rhs(2, 2);
    rhs(0, 0) = input + 4.0;
    rhs(1, 0) = 8.0;
    rhs(0, 1) = 10.0;
    rhs(1, 1) = 16.0;
    auto checked = AAD::LinearSolveWithAccuracy(&scope, matrix, rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    AAD::Number_ objective = 3.0 * checked.solution_(0, 0) - checked.solution_(1, 0) + 2.0 * checked.solution_(0, 1) + 4.0 * checked.solution_(1, 1);
    scope.FinishRecording();
    scope.ClearAdjoints();
    const double weights[] = {1.0, -2.0, 0.0, 3.0};
    for (size_t channel = 0; channel < 4; ++channel)
        AAD::NativeOperations_::SetSeed(objective, weights[channel], channel);
    const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
    ASSERT_DOUBLE_EQ(AAD::Value(objective), 24.5);
    const auto& errors = reports.Report(checked.event_).transposeBackwardErrors_;
    ASSERT_EQ(errors.Rows(), 2);
    ASSERT_EQ(errors.Cols(), 4);
    for (size_t channel = 0; channel < 4; ++channel) {
        ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input, channel), -5.5 * weights[channel]);
        ASSERT_DOUBLE_EQ(errors(0, static_cast<int>(channel)), 0.0);
        ASSERT_DOUBLE_EQ(errors(1, static_cast<int>(channel)), 0.0);
    }
    scope.Close();
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedRationalForwardAndActualTransposeLimits) {
    using namespace Dal;
    const double delta = std::ldexp(1.0, -27);
    const double error = std::ldexp(1.0, -55);
    for (double rhsValue : {0.0, 1.0}) {
        AAD::Clear(*AAD::Tape());
        auto mode = AAD::SetNumResultsForAAD(true, 4);
        AAD::RecordingScope_ scope;
        AAD::Number_ matrixInput, rhsInput;
        scope.RegisterInput(matrixInput, 1.0 + delta);
        scope.RegisterInput(rhsInput, rhsValue);
        scope.StartRecording();
        SquareMatrix_<AAD::Number_> matrix(1);
        Matrix_<AAD::Number_> rhs(1, 1);
        matrix(0, 0) = matrixInput;
        rhs(0, 0) = rhsInput;
        auto checked = AAD::LinearSolveWithAccuracy(&scope, matrix, rhs, LinearSolveAccuracyPolicy_{error, error});
        ASSERT_DOUBLE_EQ(checked.diagnostics_.componentwiseBackwardErrors_[0], rhsValue == 0.0 ? 0.0 : error);
        scope.FinishRecording();
        scope.ClearAdjoints();
        const double weights[] = {1.0, -2.0, 0.0, 3.0};
        for (size_t channel = 0; channel < 4; ++channel)
            AAD::NativeOperations_::SetSeed(checked.solution_(0, 0), weights[channel], channel);
        const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
        const auto& observed = reports.Report(checked.event_).transposeBackwardErrors_;
        ASSERT_EQ(observed.Rows(), 1);
        ASSERT_EQ(observed.Cols(), 4);
        for (size_t channel = 0; channel < 4; ++channel) {
            ASSERT_DOUBLE_EQ(observed(0, static_cast<int>(channel)), channel == 2 ? 0.0 : error);
            ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(rhsInput, channel), weights[channel] * (1.0 - delta));
        }
        scope.Close();
        AAD::Clear(*AAD::Tape());
    }
}

TEST(AADLinearSolveTest, TestCheckedTransposeRejectionPoisonsGraphAndKeepsHistoricalReports) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::RecordingScope_ scope;
    AAD::Number_ input;
    scope.RegisterInput(input, 0.0);
    scope.StartRecording();
    const double error = std::ldexp(1.0, -55);
    SquareMatrix_<> matrix(1, 1.0 + std::ldexp(1.0, -27));
    Matrix_<AAD::Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto checked = AAD::LinearSolveWithAccuracy(&scope, matrix, rhs, LinearSolveAccuracyPolicy_{0.0, std::nextafter(error, 0.0)});
    scope.FinishRecording();
    scope.ClearAdjoints();
    auto historical = AAD::ReverseWithSolveAccuracy(&scope);
    ASSERT_DOUBLE_EQ(historical.Report(checked.event_).transposeBackwardErrors_(0, 0), 0.0);
    scope.ClearAdjoints();
    AAD::NativeOperations_::SetSeed(checked.solution_(0, 0), 1.0);
    ASSERT_THROW(static_cast<void>(AAD::ReverseWithSolveAccuracy(&scope)), Exception_);
    ASSERT_THROW(scope.Reverse(), Exception_);
    ASSERT_THROW(static_cast<void>(AAD::NativeOperations_::ReadAdjoint(input)), Exception_);
    ASSERT_DOUBLE_EQ(historical.Report(checked.event_).transposeBackwardErrors_(0, 0), 0.0);
    scope.Close();
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedOrdinaryReverseStillEnforcesPolicy) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::RecordingScope_ scope;
    AAD::Number_ input;
    scope.RegisterInput(input, 0.0);
    scope.StartRecording();
    SquareMatrix_<> matrix(1, 1.0 + std::ldexp(1.0, -27));
    Matrix_<AAD::Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto checked = AAD::LinearSolveWithAccuracy(&scope, matrix, rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    AAD::NativeOperations_::SetSeed(checked.solution_(0, 0), 1.0);
    ASSERT_THROW(scope.Reverse(), Exception_);
    ASSERT_THROW(static_cast<void>(AAD::NativeOperations_::ReadAdjoint(input)), Exception_);
    scope.Close();
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedWindowsRestoreAndEventIdentity) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::RecordingScope_ scope;
    AAD::Number_ input;
    scope.RegisterInput(input, 4.0);
    scope.StartRecording();
    SquareMatrix_<> matrix(1, 2.0);
    Matrix_<AAD::Number_> rhs(1, 1);
    rhs(0, 0) = input;
    const auto prefix = AAD::LinearSolveWithAccuracy(&scope, matrix, rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    const auto checkpoint = scope.MakeCheckpoint();
    scope.ClearAdjoints();
    rhs(0, 0) = prefix.solution_(0, 0) + 2.0;
    matrix(0, 0) = 4.0;
    const auto discarded = AAD::LinearSolveWithAccuracy(&scope, matrix, rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    AAD::Number_ objective = 3.0 * discarded.solution_(0, 0);
    scope.FinishRecording();
    AAD::NativeOperations_::SetSeed(objective, 1.0);
    const auto suffixReports = AAD::ReverseSuffixWithSolveAccuracy(&scope, checkpoint);
    ASSERT_EQ(suffixReports.Entries().size(), 1);
    ASSERT_THROW(static_cast<void>(suffixReports.Report(prefix.event_)), Exception_);
    ASSERT_DOUBLE_EQ(suffixReports.Report(discarded.event_).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input), 0.0);
    ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(prefix.solution_(0, 0)), 0.75);
    scope.Restore(checkpoint);
    scope.FinishRecording();
    const auto prefixReports = AAD::ReversePrefixWithSolveAccuracy(&scope, checkpoint);
    ASSERT_EQ(prefixReports.Entries().size(), 1);
    ASSERT_THROW(static_cast<void>(prefixReports.Report(discarded.event_)), Exception_);
    ASSERT_DOUBLE_EQ(prefixReports.Report(prefix.event_).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input), 0.375);
    scope.Restore(checkpoint);
    rhs(0, 0) = prefix.solution_(0, 0);
    auto replaced = AAD::LinearSolveWithAccuracy(&scope, matrix, rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    ASSERT_NE(replaced.event_.EventId(), discarded.event_.EventId());
    ASSERT_EQ(replaced.event_.RecordingId(), prefix.event_.RecordingId());
    scope.FinishRecording();
    scope.ClearAdjoints();
    AAD::NativeOperations_::SetSeed(replaced.solution_(0, 0), 1.0);
    const auto complete = AAD::ReverseWithSolveAccuracy(&scope);
    ASSERT_EQ(complete.Entries().size(), 2);
    ASSERT_THROW(static_cast<void>(complete.Report(discarded.event_)), Exception_);
    ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input), 0.125);
    scope.Close();
    ASSERT_EQ(suffixReports.Report(discarded.event_).event_, discarded.event_);
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedPassiveMatrixOmitsUnusedOverflowingRisk) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::RecordingScope_ scope;
    AAD::Number_ input;
    scope.RegisterInput(input, 1e150);
    scope.StartRecording();
    SquareMatrix_<> matrix(1, 1e-150);
    Matrix_<AAD::Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto checked = AAD::LinearSolveWithAccuracy(&scope, matrix, rhs, LinearSolveAccuracyPolicy_{1.0, 1.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    AAD::NativeOperations_::SetSeed(checked.solution_(0, 0), 1.0);
    const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
    ASSERT_TRUE(std::isfinite(AAD::Value(checked.solution_(0, 0))));
    ASSERT_TRUE(std::isfinite(AAD::NativeOperations_::ReadAdjoint(input)));
    ASSERT_NEAR(AAD::NativeOperations_::ReadAdjoint(input) / 1e150, 1.0, 1e-10);
    ASSERT_LE(reports.Report(checked.event_).transposeBackwardErrors_(0, 0), 1.0);
    scope.Close();
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedForwardLimitRejectsBeforeOutputPublication) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::RecordingScope_ scope;
    AAD::Number_ input;
    scope.RegisterInput(input, 1.0);
    scope.StartRecording();
    SquareMatrix_<> matrix(1, 1.0 + std::ldexp(1.0, -27));
    Matrix_<AAD::Number_> rhs(1, 1);
    rhs(0, 0) = input;
    const auto before = AAD::MeasureTape(*AAD::Tape());
    ASSERT_THROW(static_cast<void>(
                     AAD::LinearSolveWithAccuracy(&scope, matrix, rhs, LinearSolveAccuracyPolicy_{std::nextafter(std::ldexp(1.0, -55), 0.0), 1.0})),
                 Exception_);
    const auto after = AAD::MeasureTape(*AAD::Tape());
    ASSERT_EQ(after.nodes_, before.nodes_);
    ASSERT_EQ(after.reverseEvents_, before.reverseEvents_);
    ASSERT_EQ(after.reverseEventCapacityBytes_, before.reverseEventCapacityBytes_);
    ASSERT_THROW(scope.FinishRecording(), Exception_);
    scope.Close();
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedLateFailureDiscardsAllPartialReports) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::RecordingScope_ scope;
    AAD::Number_ input;
    scope.RegisterInput(input, 0.0);
    scope.StartRecording();
    Matrix_<AAD::Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto rejected = AAD::LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 1.0 + std::ldexp(1.0, -27)), rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    auto accepted = AAD::LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 2.0), rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    AAD::NativeOperations_::SetSeed(rejected.solution_(0, 0), 1.0);
    AAD::NativeOperations_::SetSeed(accepted.solution_(0, 0), 1.0);
    BufferCapacityBudget_ budget(4096);
    {
        BufferCapacityScope_ caller(&budget);
        AAD::SolveAccuracyReports_ unpublished;
        ASSERT_THROW(unpublished = AAD::ReverseWithSolveAccuracy(&scope), Exception_);
        ASSERT_TRUE(unpublished.Entries().empty());
        ASSERT_EQ(unpublished.InvocationId(), 0);
        ASSERT_EQ(budget.CapacityBytes(), 0);
        ASSERT_GT(budget.PeakCapacityBytes(), sizeof(AAD::SolveAccuracyReport_));
    }
    ASSERT_THROW(static_cast<void>(AAD::NativeOperations_::ReadAdjoint(input)), Exception_);
    scope.Close();
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedRequestedEntryOverflowRejectsNativeGraph) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::RecordingScope_ scope;
    AAD::Number_ input;
    scope.RegisterInput(input, 1e-150);
    scope.StartRecording();
    SquareMatrix_<AAD::Number_> matrix(1);
    matrix(0, 0) = input;
    auto checked = AAD::LinearSolveWithAccuracy(&scope, matrix, Matrix_<>(1, 1, 1e150), LinearSolveAccuracyPolicy_{1.0, 1.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    AAD::NativeOperations_::SetSeed(checked.solution_(0, 0), 1.0);
    ASSERT_THROW(static_cast<void>(AAD::ReverseWithSolveAccuracy(&scope)), Exception_);
    ASSERT_THROW(static_cast<void>(AAD::NativeOperations_::ReadAdjoint(input)), Exception_);
    scope.Close();
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedIllConditionedFiniteRiskIsNotClipped) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    const double epsilon = std::ldexp(1.0, -54);
    AAD::RecordingScope_ scope;
    AAD::Number_ input;
    scope.RegisterInput(input, epsilon);
    scope.StartRecording();
    SquareMatrix_<AAD::Number_> matrix(2);
    matrix(0, 0) = 1.0;
    matrix(0, 1) = 0.0;
    matrix(1, 0) = 0.0;
    matrix(1, 1) = input;
    Matrix_<> rhs(2, 1);
    rhs(0, 0) = 2.0;
    rhs(1, 0) = 3.0;
    auto checked = AAD::LinearSolveWithAccuracy(&scope, matrix, rhs, LinearSolveAccuracyPolicy_{0.0, 0.0}, std::ldexp(1.0, -60));
    scope.FinishRecording();
    scope.ClearAdjoints();
    AAD::NativeOperations_::SetSeed(checked.solution_(1, 0), 1.0);
    const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
    ASSERT_DOUBLE_EQ(checked.diagnostics_.reciprocalConditionInfinity_, epsilon);
    ASSERT_DOUBLE_EQ(AAD::Value(checked.solution_(1, 0)), 3.0 / epsilon);
    ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input), -3.0 / (epsilon * epsilon));
    ASSERT_DOUBLE_EQ(reports.Report(checked.event_).transposeBackwardErrors_(0, 0), 0.0);
    scope.Close();
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedEmptyCollectionAndForeignRecordingLookup) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::SolveAccuracyEvent_ previous;
    {
        AAD::RecordingScope_ scope;
        AAD::Number_ input;
        scope.RegisterInput(input, 3.0);
        scope.StartRecording();
        Matrix_<AAD::Number_> rhs(1, 1);
        rhs(0, 0) = input;
        previous = AAD::LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 2.0), rhs, LinearSolveAccuracyPolicy_{0.0, 0.0}).event_;
        scope.FinishRecording();
        scope.Close();
    }
    {
        auto mode = AAD::SetNumResultsForAAD(true, 1);
        AAD::RecordingScope_ scope;
        AAD::Number_ input;
        scope.RegisterInput(input, 3.0);
        scope.StartRecording();
        AAD::Number_ objective = 2.0 * input;
        scope.FinishRecording();
        scope.ClearAdjoints();
        AAD::NativeOperations_::SetSeed(objective, 1.0);
        const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
        ASSERT_TRUE(reports.Entries().empty());
        ASSERT_NE(reports.InvocationId(), 0);
        ASSERT_TRUE(reports.IsMulti());
        ASSERT_EQ(reports.Channels(), 1);
        ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input), 2.0);
        ASSERT_THROW(static_cast<void>(reports.Report(previous)), Exception_);
        ASSERT_THROW(static_cast<void>(reports.Report(AAD::SolveAccuracyEvent_())), Exception_);
        scope.Close();
    }
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedOwnerAndCheckpointAdmissionPreserveReadyGraph) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::RecordingScope_ scope;
    AAD::Number_ input;
    scope.RegisterInput(input, 3.0);
    scope.StartRecording();
    Matrix_<AAD::Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto checked = AAD::LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 2.0), rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    AAD::NativeOperations_::SetSeed(checked.solution_(0, 0), 1.0);
    auto foreign = std::async(std::launch::async, [&scope] {
        try {
            static_cast<void>(AAD::ReverseWithSolveAccuracy(&scope));
            return false;
        } catch (const Exception_&) {
            return true;
        }
    });
    ASSERT_TRUE(foreign.get());
    ASSERT_THROW(static_cast<void>(AAD::ReverseWithSolveAccuracy(nullptr)), Exception_);
    ASSERT_THROW(static_cast<void>(AAD::ReversePrefixWithSolveAccuracy(&scope, AAD::Checkpoint_())), Exception_);
    ASSERT_THROW(static_cast<void>(AAD::ReverseSuffixWithSolveAccuracy(&scope, AAD::Checkpoint_())), Exception_);
    const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
    ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input), 0.5);
    ASSERT_EQ(reports.Entries().size(), 1);
    scope.Close();
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedCallerReportBudgetIncludesActualScratchAndRefunds) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::RecordingScope_ scope;
    AAD::Number_ input;
    scope.RegisterInput(input, 3.0);
    scope.StartRecording();
    Matrix_<AAD::Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto checked = AAD::LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 2.0), rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    AAD::NativeOperations_::SetSeed(checked.solution_(0, 0), 1.0);
    const size_t retainedReport = sizeof(AAD::SolveAccuracyReport_) + sizeof(double);
    BufferCapacityBudget_ budget(retainedReport + 3 * sizeof(double));
    {
        BufferCapacityScope_ caller(&budget);
        {
            const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
            ASSERT_EQ(budget.CapacityBytes(), retainedReport);
            ASSERT_EQ(budget.PeakCapacityBytes(), retainedReport + 3 * sizeof(double));
            ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).reverseScratchPeakBytes_, 3 * sizeof(double));
            ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input), 0.5);
            scope.Close();
            ASSERT_DOUBLE_EQ(reports.Report(checked.event_).transposeBackwardErrors_(0, 0), 0.0);
            ASSERT_EQ(budget.CapacityBytes(), retainedReport);
        }
        ASSERT_EQ(budget.CapacityBytes(), 0);
    }
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedReportOrScratchOneByteShortPublishesNothing) {
    using namespace Dal;
    const size_t retainedReport = sizeof(AAD::SolveAccuracyReport_) + sizeof(double);
    for (size_t limit : {retainedReport - 1, retainedReport + 3 * sizeof(double) - 1}) {
        SCOPED_TRACE(limit);
        AAD::Clear(*AAD::Tape());
        AAD::RecordingScope_ scope;
        AAD::Number_ input;
        scope.RegisterInput(input, 3.0);
        scope.StartRecording();
        Matrix_<AAD::Number_> rhs(1, 1);
        rhs(0, 0) = input;
        auto checked = AAD::LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 2.0), rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
        scope.FinishRecording();
        scope.ClearAdjoints();
        AAD::NativeOperations_::SetSeed(checked.solution_(0, 0), 1.0);
        const auto before = AAD::MeasureTape(*AAD::Tape()).reverseEventCapacityBytes_;
        BufferCapacityBudget_ budget(limit);
        {
            BufferCapacityScope_ caller(&budget);
            ASSERT_THROW(static_cast<void>(AAD::ReverseWithSolveAccuracy(&scope)), Exception_);
            ASSERT_EQ(budget.CapacityBytes(), 0);
        }
        ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).reverseEventCapacityBytes_, before);
        ASSERT_THROW(static_cast<void>(AAD::NativeOperations_::ReadAdjoint(input)), Exception_);
        scope.Close();
        AAD::Clear(*AAD::Tape());
    }
}

TEST(AADLinearSolveTest, TestCheckedNonfiniteSeedsRejectAndRefundReports) {
    using namespace Dal;
    const double infinity = std::numeric_limits<double>::infinity();
    for (double seed : {std::numeric_limits<double>::quiet_NaN(), infinity, -infinity}) {
        AAD::Clear(*AAD::Tape());
        AAD::RecordingScope_ scope;
        AAD::Number_ input;
        scope.RegisterInput(input, 3.0);
        scope.StartRecording();
        Matrix_<AAD::Number_> rhs(1, 1);
        rhs(0, 0) = input;
        auto checked = AAD::LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 2.0), rhs, LinearSolveAccuracyPolicy_{1.0, 1.0});
        scope.FinishRecording();
        scope.ClearAdjoints();
        AAD::NativeOperations_::SetSeed(checked.solution_(0, 0), seed);
        BufferCapacityBudget_ budget(4096);
        {
            BufferCapacityScope_ caller(&budget);
            ASSERT_THROW(static_cast<void>(AAD::ReverseWithSolveAccuracy(&scope)), Exception_);
            ASSERT_EQ(budget.CapacityBytes(), 0);
        }
        ASSERT_THROW(static_cast<void>(AAD::NativeOperations_::ReadAdjoint(input)), Exception_);
        scope.Close();
        AAD::Clear(*AAD::Tape());
    }
}

TEST(AADLinearSolveTest, TestCheckedInvalidPolicyRefundsCaptureAndRecovers) {
    using namespace Dal;
    const double invalid[] = {-1.0, std::nextafter(1.0, 2.0), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                              -std::numeric_limits<double>::infinity()};
    for (double limit : invalid) {
        for (bool forward : {false, true}) {
            AAD::Clear(*AAD::Tape());
            AAD::RecordingScope_ scope;
            AAD::Number_ input;
            scope.RegisterInput(input, 3.0);
            scope.StartRecording();
            Matrix_<AAD::Number_> rhs(1, 1);
            rhs(0, 0) = input;
            const auto before = AAD::MeasureTape(*AAD::Tape());
            const LinearSolveAccuracyPolicy_ policy = forward ? LinearSolveAccuracyPolicy_{limit, 1.0} : LinearSolveAccuracyPolicy_{1.0, limit};
            ASSERT_THROW(static_cast<void>(AAD::LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 2.0), rhs, policy)), Exception_);
            ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).reverseEventCapacityBytes_, before.reverseEventCapacityBytes_);
            ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, before.nodes_);
            ASSERT_THROW(scope.FinishRecording(), Exception_);
            scope.Close();
            AAD::Clear(*AAD::Tape());
        }
    }
    AAD::RecordingScope_ healthy;
    AAD::Number_ input;
    healthy.RegisterInput(input, 3.0);
    healthy.StartRecording();
    Matrix_<AAD::Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto checked = AAD::LinearSolveWithAccuracy(&healthy, SquareMatrix_<>(1, 2.0), rhs, LinearSolveAccuracyPolicy_{1.0, 1.0});
    healthy.FinishRecording();
    healthy.ClearAdjoints();
    AAD::NativeOperations_::SetSeed(checked.solution_(0, 0), 1.0);
    ASSERT_EQ(AAD::ReverseWithSolveAccuracy(&healthy).Entries().size(), 1);
    ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input), 0.5);
    healthy.Close();
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedCopiedReportsSurviveSourcesGraphAndCollector) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::SolveAccuracyReports_ copy;
    AAD::SolveAccuracyReport_ entry;
    AAD::SolveAccuracyEvent_ token;
    {
        AAD::RecordingScope_ scope;
        AAD::Number_ input;
        scope.RegisterInput(input, 3.0);
        scope.StartRecording();
        AAD::CheckedLinearSolveResult_ checked;
        {
            SquareMatrix_<> matrix(1, 2.0);
            Matrix_<AAD::Number_> rhs(1, 1);
            rhs(0, 0) = input;
            LinearSolveAccuracyPolicy_ policy{0.0, 0.0};
            checked = AAD::LinearSolveWithAccuracy(&scope, matrix, rhs, policy);
            matrix(0, 0) = 9.0;
            rhs(0, 0) = 17.0;
            policy.transposeBackwardErrorLimit_ = -1.0;
        }
        token = checked.event_;
        scope.FinishRecording();
        scope.ClearAdjoints();
        AAD::NativeOperations_::SetSeed(checked.solution_(0, 0), 1.0);
        {
            const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
            copy = reports;
            entry = reports.Report(token);
            ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input), 0.5);
        }
        scope.Close();
    }
    ASSERT_DOUBLE_EQ(copy.Report(token).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_DOUBLE_EQ(entry.transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_EQ(entry.invocationId_, copy.InvocationId());
    ASSERT_EQ(entry.event_, token);
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedIndependentThreadsCollectIndependentInvocations) {
    using namespace Dal;
    auto evaluate = [](size_t width) {
        AAD::Clear(*AAD::Tape());
        auto mode = AAD::SetNumResultsForAAD(true, width);
        AAD::RecordingScope_ scope;
        AAD::Number_ input;
        scope.RegisterInput(input, 3.0);
        scope.StartRecording();
        Matrix_<AAD::Number_> rhs(1, 1);
        rhs(0, 0) = input;
        auto checked = AAD::LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 2.0), rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
        scope.FinishRecording();
        scope.ClearAdjoints();
        for (size_t channel = 0; channel < width; ++channel)
            AAD::NativeOperations_::SetSeed(checked.solution_(0, 0), static_cast<double>(channel), channel);
        auto reports = AAD::ReverseWithSolveAccuracy(&scope);
        for (size_t channel = 0; channel < width; ++channel)
            REQUIRE(AAD::NativeOperations_::ReadAdjoint(input, channel) == 0.5 * channel, "independent analytic thread risk");
        scope.Close();
        AAD::Clear(*AAD::Tape());
        return reports;
    };
    auto first = std::async(std::launch::async, evaluate, 1);
    auto second = std::async(std::launch::async, evaluate, 4);
    auto third = std::async(std::launch::async, evaluate, 8);
    const auto a = first.get(), b = second.get(), c = third.get();
    ASSERT_EQ(a.Channels(), 1);
    ASSERT_EQ(b.Channels(), 4);
    ASSERT_EQ(c.Channels(), 8);
    ASSERT_NE(a.InvocationId(), b.InvocationId());
    ASSERT_NE(b.InvocationId(), c.InvocationId());
    ASSERT_NE(a.Entries()[0].event_.RecordingId(), c.Entries()[0].event_.RecordingId());
    ASSERT_NE(a.Entries()[0].event_.EventId(), b.Entries()[0].event_.EventId());
    ASSERT_DOUBLE_EQ(c.Entries()[0].transposeBackwardErrors_(0, 7), 0.0);
}

namespace {
    double ReferenceComposedObjective(double input) {
        const double matrix00 = 2.0 + input, matrix01 = 1.0 - input;
        const double determinant = 3.0 * matrix00 - 0.5 * matrix01;
        const double rhs[2][2] = {{input * input + 1.0, input + 0.25}, {2.0 * input - 1.0, -0.5 * input}};
        const double weights[2][2] = {{0.5, -2.0}, {1.25, 3.0}};
        double objective = 0.0;
        for (int column = 0; column < 2; ++column) {
            const double first = (3.0 * rhs[0][column] - matrix01 * rhs[1][column]) / determinant;
            const double second = (matrix00 * rhs[1][column] - 0.5 * rhs[0][column]) / determinant;
            objective += weights[0][column] * first + weights[1][column] * second;
        }
        return objective;
    }
} // namespace

TEST(AADLinearSolveTest, TestCheckedIndependentCramerAndThreeStepDifferences) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::RecordingScope_ scope;
    AAD::Number_ input;
    scope.RegisterInput(input, 0.3);
    scope.StartRecording();
    SquareMatrix_<AAD::Number_> matrix(2);
    matrix(0, 0) = 2.0 + input;
    matrix(0, 1) = 1.0 - input;
    matrix(1, 0) = 0.5;
    matrix(1, 1) = 3.0;
    Matrix_<AAD::Number_> rhs(2, 2);
    rhs(0, 0) = input * input + 1.0;
    rhs(0, 1) = input + 0.25;
    rhs(1, 0) = 2.0 * input - 1.0;
    rhs(1, 1) = -0.5 * input;
    auto checked = AAD::LinearSolveWithAccuracy(&scope, matrix, rhs, LinearSolveAccuracyPolicy_{1e-12, 1e-12});
    AAD::Number_ objective =
        0.5 * checked.solution_(0, 0) - 2.0 * checked.solution_(0, 1) + 1.25 * checked.solution_(1, 0) + 3.0 * checked.solution_(1, 1);
    scope.FinishRecording();
    scope.ClearAdjoints();
    AAD::NativeOperations_::SetSeed(objective, 1.0);
    const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
    ASSERT_NEAR(AAD::Value(objective), ReferenceComposedObjective(0.3), 1e-10);
    const double gradient = AAD::NativeOperations_::ReadAdjoint(input);
    const double steps[] = {1e-3, 1e-4, 1e-5};
    const double tolerances[] = {2e-6, 2e-8, 2e-9};
    for (int step = 0; step < 3; ++step) {
        const double reference =
            (ReferenceComposedObjective(0.3 + steps[step]) - ReferenceComposedObjective(0.3 - steps[step])) / (2.0 * steps[step]);
        ASSERT_NEAR(gradient, reference, tolerances[step]);
    }
    ASSERT_LE(reports.Report(checked.event_).transposeBackwardErrors_(0, 0), 1e-12);
    ASSERT_LE(reports.Report(checked.event_).transposeBackwardErrors_(1, 0), 1e-12);
    scope.Close();
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedFiniteContributionAccumulationOverflowRejects) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::RecordingScope_ scope;
    AAD::Number_ input;
    scope.RegisterInput(input, 0.0);
    scope.StartRecording();
    Matrix_<AAD::Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto checked = AAD::LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 1.0), rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    const double maximum = std::numeric_limits<double>::max();
    AAD::NativeOperations_::SetSeed(input, maximum);
    AAD::NativeOperations_::SetSeed(checked.solution_(0, 0), maximum);
    BufferCapacityBudget_ budget(4096);
    {
        BufferCapacityScope_ caller(&budget);
        ASSERT_THROW(static_cast<void>(AAD::ReverseWithSolveAccuracy(&scope)), Exception_);
        ASSERT_EQ(budget.CapacityBytes(), 0);
    }
    ASSERT_THROW(static_cast<void>(AAD::NativeOperations_::ReadAdjoint(input)), Exception_);
    scope.Close();
    AAD::Clear(*AAD::Tape());
}

namespace {
    Dal::AAD::CheckedLinearSolveResult_ CaptureBudgetFixture(Dal::AAD::RecordingScope_* scope, Dal::AAD::Number_* input) {
        scope->RegisterInput(*input, 3.0);
        scope->StartRecording();
        Dal::Matrix_<Dal::AAD::Number_> rhs(1, 1);
        rhs(0, 0) = *input;
        auto checked = Dal::AAD::LinearSolveWithAccuracy(scope, Dal::SquareMatrix_<>(1, 2.0), rhs, Dal::LinearSolveAccuracyPolicy_{0.0, 0.0});
        scope->FinishRecording();
        scope->ClearAdjoints();
        Dal::AAD::NativeOperations_::SetSeed(checked.solution_(0, 0), 1.0);
        return checked;
    }
} // namespace

TEST(AADLinearSolveTest, TestCheckedExactTapeScratchBudgetAndOneByteShort) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    const auto initial = AAD::MeasureTape(*AAD::Tape()).capacityBytes_;
    size_t retained = 0;
    {
        AAD::RecordingScope_ scope;
        AAD::Number_ input;
        const auto checked = CaptureBudgetFixture(&scope, &input);
        retained = AAD::MeasureTape(*AAD::Tape()).capacityBytes_;
        scope.Close();
    }
    for (bool accept : {false, true}) {
        AAD::Clear(*AAD::Tape());
        const size_t scratch = 3 * sizeof(double);
        AAD::TapeCapacityBudget_ budget(retained + AAD::TapeCleanupCapacityBytes() + scratch - (accept ? 0 : 1));
        {
            AAD::TapeCapacityScope_ capacity(&budget, true);
            AAD::RecordingScope_ scope;
            AAD::Number_ input;
            const auto checked = CaptureBudgetFixture(&scope, &input);
            ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).capacityBytes_, retained);
            if (accept) {
                const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
                ASSERT_EQ(budget.PeakCapacityBytes(), std::max(initial + AAD::TapeCleanupCapacityBytes(), retained + scratch));
                ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).reverseScratchPeakBytes_, scratch);
                ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input), 0.5);
                ASSERT_DOUBLE_EQ(reports.Report(checked.event_).transposeBackwardErrors_(0, 0), 0.0);
            } else {
                ASSERT_THROW(static_cast<void>(AAD::ReverseWithSolveAccuracy(&scope)), Exception_);
                ASSERT_THROW(static_cast<void>(AAD::NativeOperations_::ReadAdjoint(input)), Exception_);
            }
            ASSERT_EQ(budget.CapacityBytes(), retained);
            scope.Close();
            ASSERT_EQ(budget.CapacityBytes(), AAD::MeasureTape(*AAD::Tape()).capacityBytes_);
        }
        AAD::Clear(*AAD::Tape());
    }
}

TEST(AADLinearSolveTest, TestCheckedDetachedCopyBudgetRejectsAndRefundsWithoutGraphFailure) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    AAD::RecordingScope_ scope;
    AAD::Number_ input;
    const auto checked = CaptureBudgetFixture(&scope, &input);
    const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
    const size_t reportBytes = sizeof(AAD::SolveAccuracyReport_) + sizeof(double);
    BufferCapacityBudget_ shortBudget(reportBytes - 1);
    {
        BufferCapacityScope_ caller(&shortBudget);
        ASSERT_THROW(static_cast<void>(AAD::SolveAccuracyReports_(reports)), Exception_);
        ASSERT_EQ(shortBudget.CapacityBytes(), 0);
    }
    ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input), 0.5);
    BufferCapacityBudget_ exactBudget(reportBytes);
    {
        BufferCapacityScope_ caller(&exactBudget);
        {
            const auto copied = reports;
            ASSERT_EQ(exactBudget.CapacityBytes(), reportBytes);
            ASSERT_EQ(copied.InvocationId(), reports.InvocationId());
            scope.Close();
            ASSERT_DOUBLE_EQ(copied.Report(checked.event_).transposeBackwardErrors_(0, 0), 0.0);
        }
        ASSERT_EQ(exactBudget.CapacityBytes(), 0);
    }
    AAD::Clear(*AAD::Tape());
}

TEST(AADLinearSolveTest, TestCheckedEmptyCollectionRejectsInvalidNativeWidth) {
    using namespace Dal;
    AAD::Clear(*AAD::Tape());
    auto mode = AAD::SetNumResultsForAAD(false, 1);
    AAD::Tape()->numAdj_ = 0;
    {
        AAD::RecordingScope_ scope;
        scope.StartRecording();
        scope.FinishRecording();
        bool rejected = false;
        try {
            static_cast<void>(AAD::ReverseWithSolveAccuracy(&scope));
        } catch (const Exception_&) {
            rejected = true;
        }
        scope.Close();
        AAD::Tape()->numAdj_ = 1;
        ASSERT_TRUE(rejected);
    }
    AAD::Clear(*AAD::Tape());
}
