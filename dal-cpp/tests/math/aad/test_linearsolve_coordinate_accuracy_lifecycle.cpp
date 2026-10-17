//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/math/aad/linearsolveaccuracy.hpp>
#include <dal/math/aad/linearsolvecoordinateaccuracy.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/platform/platform.hpp>

using Dal::LinearSolveAccuracyPolicy_;
using Dal::LinearSolveCoordinates_;
using Dal::Matrix_;
using Dal::SquareMatrix_;
using Dal::Vector_;
using Dal::AAD::Clear;
using Dal::AAD::LinearSolveWithAccuracy;
using Dal::AAD::NativeOperations_;
using Dal::AAD::Number_;
using Dal::AAD::RecordingScope_;
using Dal::AAD::ReverseWithSolveAccuracy;
using Dal::AAD::Tape;

TEST(AADLinearSolveTest, TestCheckedCoordinateAndDenseWindowsShareCollectorAndIdentity) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 4.0);
    scope.StartRecording();
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    const auto layout = LinearSolveCoordinates_::Symmetric(1);
    const LinearSolveAccuracyPolicy_ policy{0.0, 0.0};
    const auto prefix = LinearSolveWithAccuracy(&scope, layout, Vector_<>{2.0}, rhs, policy);
    const auto checkpoint = scope.MakeCheckpoint();
    scope.ClearAdjoints();
    rhs(0, 0) = prefix.solution_(0, 0) + 2.0;
    const auto discarded = LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 4.0), rhs, policy);
    Number_ objective = 3.0 * discarded.solution_(0, 0);
    scope.FinishRecording();
    NativeOperations_::SetSeed(objective, 1.0);
    const auto suffixReports = Dal::AAD::ReverseSuffixWithSolveAccuracy(&scope, checkpoint);
    ASSERT_EQ(suffixReports.Entries().size(), 1);
    ASSERT_THROW(static_cast<void>(suffixReports.Report(prefix.event_)), Dal::Exception_);
    ASSERT_DOUBLE_EQ(suffixReports.Report(discarded.event_).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(prefix.solution_(0, 0)), 0.75);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.0);
    scope.Restore(checkpoint);
    scope.FinishRecording();
    const auto prefixReports = Dal::AAD::ReversePrefixWithSolveAccuracy(&scope, checkpoint);
    ASSERT_NE(prefixReports.InvocationId(), suffixReports.InvocationId());
    ASSERT_EQ(prefixReports.Entries().size(), 1);
    ASSERT_DOUBLE_EQ(prefixReports.Report(prefix.event_).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.375);
    scope.Restore(checkpoint);
    rhs(0, 0) = prefix.solution_(0, 0);
    const auto replaced = LinearSolveWithAccuracy(&scope, layout, Vector_<>{4.0}, rhs, policy);
    ASSERT_NE(replaced.event_.EventId(), discarded.event_.EventId());
    ASSERT_EQ(replaced.event_.RecordingId(), discarded.event_.RecordingId());
    scope.FinishRecording();
    scope.ClearAdjoints();
    Number_ output = replaced.solution_(0, 0);
    NativeOperations_::SetSeed(output, 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_EQ(reports.Entries().size(), 2);
    ASSERT_THROW(static_cast<void>(reports.Report(discarded.event_)), Dal::Exception_);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.125);
    scope.Close();
    ASSERT_DOUBLE_EQ(suffixReports.Report(discarded.event_).transposeBackwardErrors_(0, 0), 0.0);
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCheckedCoordinateTransposePolicyAlsoAppliesWithoutCollector) {
    const double error = std::ldexp(1.0, -55);
    for (bool collect : {false, true}) {
        Clear(*Tape());
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, 0.0);
        scope.StartRecording();
        Matrix_<Number_> rhs(1, 1);
        rhs(0, 0) = input;
        auto checked = LinearSolveWithAccuracy(&scope, LinearSolveCoordinates_::Banded(1, 0, 0), Vector_<>{1.0 + std::ldexp(1.0, -27)}, rhs,
                                               LinearSolveAccuracyPolicy_{0.0, std::nextafter(error, 0.0)});
        scope.FinishRecording();
        scope.ClearAdjoints();
        const auto historical = ReverseWithSolveAccuracy(&scope);
        ASSERT_DOUBLE_EQ(historical.Report(checked.event_).transposeBackwardErrors_(0, 0), 0.0);
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(checked.solution_(0, 0), 1.0);
        if (collect)
            ASSERT_THROW(static_cast<void>(ReverseWithSolveAccuracy(&scope)), Dal::Exception_);
        else
            ASSERT_THROW(scope.Reverse(), Dal::Exception_);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Dal::Exception_);
        ASSERT_THROW(scope.Reverse(), Dal::Exception_);
        ASSERT_DOUBLE_EQ(historical.Report(checked.event_).transposeBackwardErrors_(0, 0), 0.0);
        scope.Close();
        Clear(*Tape());
    }
}

TEST(AADLinearSolveTest, TestCheckedCoordinateLateFailureDiscardsMixedPartialCollection) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 0.0);
    scope.StartRecording();
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto rejected = LinearSolveWithAccuracy(&scope, LinearSolveCoordinates_::Symmetric(1), Vector_<>{1.0 + std::ldexp(1.0, -27)}, rhs,
                                            LinearSolveAccuracyPolicy_{0.0, 0.0});
    auto accepted = LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 2.0), rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    const auto historical = ReverseWithSolveAccuracy(&scope);
    ASSERT_EQ(historical.Entries().size(), 2);
    NativeOperations_::SetSeed(rejected.solution_(0, 0), 1.0);
    NativeOperations_::SetSeed(accepted.solution_(0, 0), 1.0);
    Dal::BufferCapacityBudget_ budget(4096);
    {
        Dal::BufferCapacityScope_ caller(&budget);
        Dal::AAD::SolveAccuracyReports_ unpublished;
        ASSERT_THROW(unpublished = ReverseWithSolveAccuracy(&scope), Dal::Exception_);
        ASSERT_TRUE(unpublished.Entries().empty());
        ASSERT_EQ(unpublished.InvocationId(), 0);
        ASSERT_EQ(budget.CapacityBytes(), 0);
        ASSERT_GT(budget.PeakCapacityBytes(), sizeof(Dal::AAD::SolveAccuracyReport_));
    }
    ASSERT_DOUBLE_EQ(historical.Report(rejected.event_).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_DOUBLE_EQ(historical.Report(accepted.event_).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Dal::Exception_);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCheckedCoordinateForwardLimitRejectsBeforeOutputs) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 1.0);
    scope.StartRecording();
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    const auto before = Dal::AAD::MeasureTape(*Tape());
    const double error = std::ldexp(1.0, -55);
    ASSERT_THROW(static_cast<void>(LinearSolveWithAccuracy(&scope, LinearSolveCoordinates_::Symmetric(1), Vector_<>{1.0 + std::ldexp(1.0, -27)}, rhs,
                                                           LinearSolveAccuracyPolicy_{std::nextafter(error, 0.0), 1.0})),
                 Dal::Exception_);
    const auto after = Dal::AAD::MeasureTape(*Tape());
    ASSERT_EQ(after.nodes_, before.nodes_);
    ASSERT_EQ(after.reverseEvents_, before.reverseEvents_);
    ASSERT_EQ(after.reverseEventCapacityBytes_, before.reverseEventCapacityBytes_);
    ASSERT_THROW(scope.FinishRecording(), Dal::Exception_);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCheckedCoordinateLargeFiniteRiskUsesExplicitLegalPivot) {
    Clear(*Tape());
    const double epsilon = std::ldexp(1.0, -54);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, epsilon);
    scope.StartRecording();
    Vector_<Number_> parameters{Number_(1.0), input};
    Matrix_<> rhs(2, 1);
    rhs(0, 0) = 2.0;
    rhs(1, 0) = 3.0;
    auto checked = LinearSolveWithAccuracy(&scope, LinearSolveCoordinates_::Banded(2, 0, 0), parameters, rhs, LinearSolveAccuracyPolicy_{0.0, 0.0},
                                           std::ldexp(1.0, -60));
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(checked.solution_(1, 0), 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_DOUBLE_EQ(checked.diagnostics_.reciprocalConditionInfinity_, epsilon);
    ASSERT_DOUBLE_EQ(Dal::AAD::Value(checked.solution_(1, 0)), 3.0 / epsilon);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), -3.0 / (epsilon * epsilon));
    ASSERT_DOUBLE_EQ(reports.Report(checked.event_).transposeBackwardErrors_(0, 0), 0.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCheckedCoordinateNonfiniteSeedsRefundReportsAndFailGraph) {
    const double infinity = std::numeric_limits<double>::infinity();
    for (double seed : {std::numeric_limits<double>::quiet_NaN(), infinity, -infinity}) {
        Clear(*Tape());
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, 3.0);
        scope.StartRecording();
        Matrix_<Number_> rhs(1, 1);
        rhs(0, 0) = input;
        auto checked =
            LinearSolveWithAccuracy(&scope, LinearSolveCoordinates_::Symmetric(1), Vector_<>{2.0}, rhs, LinearSolveAccuracyPolicy_{1.0, 1.0});
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(checked.solution_(0, 0), seed);
        Dal::BufferCapacityBudget_ budget(4096);
        {
            Dal::BufferCapacityScope_ caller(&budget);
            ASSERT_THROW(static_cast<void>(ReverseWithSolveAccuracy(&scope)), Dal::Exception_);
            ASSERT_EQ(budget.CapacityBytes(), 0);
        }
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Dal::Exception_);
        scope.Close();
        Clear(*Tape());
    }
}

TEST(AADLinearSolveTest, TestCheckedCoordinateInvalidCapturePublishesNothingAndRecovers) {
    for (int problem = 0; problem < 4; ++problem) {
        SCOPED_TRACE(problem);
        Clear(*Tape());
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, 3.0);
        scope.StartRecording();
        Vector_<> parameters{2.0};
        Matrix_<Number_> rhs(problem == 1 ? 2 : 1, 1);
        rhs(0, 0) = input;
        LinearSolveAccuracyPolicy_ policy{0.0, 0.0};
        if (problem == 0)
            parameters.clear();
        if (problem == 2)
            policy.forwardBackwardErrorLimit_ = -1.0;
        if (problem == 3)
            parameters[0] = std::numeric_limits<double>::infinity();
        const auto before = Dal::AAD::MeasureTape(*Tape());
        ASSERT_THROW(static_cast<void>(LinearSolveWithAccuracy(&scope, LinearSolveCoordinates_::Symmetric(1), parameters, rhs, policy)),
                     Dal::Exception_);
        const auto after = Dal::AAD::MeasureTape(*Tape());
        ASSERT_EQ(after.nodes_, before.nodes_);
        ASSERT_EQ(after.reverseEvents_, before.reverseEvents_);
        ASSERT_EQ(after.reverseEventCapacityBytes_, before.reverseEventCapacityBytes_);
        ASSERT_THROW(scope.FinishRecording(), Dal::Exception_);
        scope.Close();
    }
    Clear(*Tape());
    RecordingScope_ healthy;
    Number_ input;
    healthy.RegisterInput(input, 3.0);
    healthy.StartRecording();
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto checked =
        LinearSolveWithAccuracy(&healthy, LinearSolveCoordinates_::Symmetric(1), Vector_<>{2.0}, rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    healthy.FinishRecording();
    healthy.ClearAdjoints();
    NativeOperations_::SetSeed(checked.solution_(0, 0), 1.0);
    ASSERT_EQ(ReverseWithSolveAccuracy(&healthy).Entries().size(), 1);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.5);
    healthy.Close();
    Clear(*Tape());
}
