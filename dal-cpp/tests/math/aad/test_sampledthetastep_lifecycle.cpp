//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <future>
#include <limits>
#include <vector>

#include <dal/math/aad/linearsolvecoordinateaccuracy.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/platform/platform.hpp>

#include "sampledthetastepfixture.hpp"

using namespace Dal;
using namespace Dal::AAD;
using DalTest::NativePDE::IdentityInputs;
using DalTest::NativePDE::RecoveryRisk;

TEST(AADSampledThetaStepTest, TestOwnsCapturedBindingsValuesPolicyAndDetachedDiagnostics) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 0.0);
    const Number_ captured = input;
    scope.StartRecording();
    CheckedSampledThetaStepResult_ step;
    {
        auto numeric = IdentityInputs();
        SampledThetaStepBindings_ active;
        active.rates_ = {input};
        LinearSolveAccuracyPolicy_ policy{0.0, 0.0};
        step = SampledThetaStepWithAccuracy(&scope, numeric, active, policy);
        input = 7.0;
        active.rates_.clear();
        numeric.x_.clear();
        numeric.rates_.clear();
        numeric.drifts_.clear();
        numeric.variances_.clear();
        numeric.oldValues_.Clear();
        numeric.dt_ = numeric.theta_ = std::numeric_limits<double>::quiet_NaN();
        policy = {1.0, 1.0};
    }
    const auto detached = step.diagnostics_;
    step.diagnostics_.forwardBackwardErrors_[0] = 1.0;
    step.diagnostics_.policy_ = {1.0, 1.0};
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(step.solution_(1, 0), 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_EQ(Value(step.solution_(1, 0)), 1.0);
    ASSERT_EQ(NativeOperations_::ReadAdjoint(captured), -0.25);
    ASSERT_EQ(NativeOperations_::ReadAdjoint(input), 0.0);
    scope.Close();
    ASSERT_EQ(detached.policy_.forwardBackwardErrorLimit_, 0.0);
    ASSERT_EQ(detached.forwardBackwardErrors_[0], 0.0);
    ASSERT_EQ(reports.Report(step.event_).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    Clear(*Tape());
}

TEST(AADSampledThetaStepTest, TestConcurrentIndependentRecordingsUseOwnedCaches) {
    std::vector<std::future<double>> futures;
    for (int worker = 0; worker < 4; ++worker)
        futures.emplace_back(std::async(std::launch::async, [] { return RecoveryRisk(); }));
    for (auto& future : futures)
        ASSERT_EQ(future.get(), -0.25);
}

TEST(AADSampledThetaStepTest, TestWrongOwnerThreadRejectsBeforePublicationAndOwnerRecovers) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 0.0);
    scope.StartRecording();
    const auto numeric = IdentityInputs();
    SampledThetaStepBindings_ active;
    active.rates_ = {input};
    const auto before = MeasureTape(*Tape());
    auto rejected = std::async(std::launch::async, [&] {
        try {
            static_cast<void>(SampledThetaStepWithAccuracy(&scope, numeric, active, LinearSolveAccuracyPolicy_{0.0, 0.0}));
            return false;
        } catch (const Exception_&) {
            return true;
        }
    });
    ASSERT_TRUE(rejected.get());
    ASSERT_EQ(MeasureTape(*Tape()).nodes_, before.nodes_);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEvents_, before.reverseEvents_);
    auto step = SampledThetaStepWithAccuracy(&scope, numeric, active, LinearSolveAccuracyPolicy_{0.0, 0.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(step.solution_(1, 0), 1.0);
    scope.Reverse();
    ASSERT_EQ(NativeOperations_::ReadAdjoint(input), -0.25);
    scope.Close();
    Clear(*Tape());
}

TEST(AADSampledThetaStepTest, TestCheckpointMixedSolveReportsAndSuffixCacheRelease) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 0.0);
    scope.StartRecording();
    SampledThetaStepBindings_ active;
    active.rates_ = {input};
    auto step = SampledThetaStepWithAccuracy(&scope, IdentityInputs(), active, LinearSolveAccuracyPolicy_{0.0, 0.0});
    const auto checkpoint = scope.MakeCheckpoint();
    scope.ClearAdjoints();
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = step.solution_(1, 0) + 1.0;
    const auto warmup = LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 2.0), rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    scope.Restore(checkpoint);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEvents_, 1);
    const auto retained = MeasureTape(*Tape()).reverseEventCapacityBytes_;
    rhs(0, 0) = step.solution_(1, 0) + 1.0;
    auto discarded = LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 2.0), rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    ASSERT_NE(discarded.event_.EventId(), warmup.event_.EventId());
    Number_ objective = 3.0 * discarded.solution_(0, 0);
    scope.FinishRecording();
    NativeOperations_::SetSeed(objective, 1.0);
    const auto suffix = ReverseSuffixWithSolveAccuracy(&scope, checkpoint);
    ASSERT_EQ(suffix.Entries().size(), 1);
    ASSERT_THROW(static_cast<void>(suffix.Report(step.event_)), Exception_);
    ASSERT_EQ(NativeOperations_::ReadAdjoint(step.solution_(1, 0)), 1.5);
    ASSERT_EQ(NativeOperations_::ReadAdjoint(input), 0.0);
    scope.Restore(checkpoint);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, retained);
    scope.FinishRecording();
    const auto prefix = ReversePrefixWithSolveAccuracy(&scope, checkpoint);
    ASSERT_EQ(prefix.Entries().size(), 1);
    ASSERT_NE(prefix.InvocationId(), suffix.InvocationId());
    ASSERT_EQ(NativeOperations_::ReadAdjoint(input), -0.375);
    scope.Restore(checkpoint);
    rhs(0, 0) = step.solution_(1, 0);
    auto replacement =
        LinearSolveWithAccuracy(&scope, LinearSolveCoordinates_::Symmetric(1), Vector_<>{2.0}, rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    ASSERT_NE(replacement.event_.EventId(), discarded.event_.EventId());
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(replacement.solution_(0, 0), 1.0);
    const auto full = ReverseWithSolveAccuracy(&scope);
    ASSERT_EQ(full.Entries().size(), 2);
    ASSERT_THROW(static_cast<void>(full.Report(discarded.event_)), Exception_);
    ASSERT_EQ(NativeOperations_::ReadAdjoint(input), -0.125);
    scope.Close();
    ASSERT_EQ(suffix.Report(discarded.event_).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_EQ(prefix.Report(step.event_).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    Clear(*Tape());
}

TEST(AADSampledThetaStepTest, TestModeChangeRejectsAndRawMarkWindowsCompose) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ rate;
    scope.RegisterInput(rate, 0.0);
    scope.StartRecording();
    SampledThetaStepBindings_ active;
    active.rates_ = {rate};
    auto prefix = SampledThetaStepWithAccuracy(&scope, IdentityInputs(), active, LinearSolveAccuracyPolicy_{0.0, 0.0});
    Mark(*Tape());
    active.oldValues_ = prefix.solution_;
    auto suffix = SampledThetaStepWithAccuracy(&scope, IdentityInputs(), active, LinearSolveAccuracyPolicy_{0.0, 0.0});
    ASSERT_THROW(static_cast<void>(SetNumResultsForAAD(true, 4)), Exception_);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(suffix.solution_(1, 0), 1.0);
    PropagateToMark(*Tape());
    ASSERT_EQ(NativeOperations_::ReadAdjoint(prefix.solution_(1, 0)), 1.0);
    ASSERT_EQ(NativeOperations_::ReadAdjoint(rate), -0.25);
    PropagateMarkToStart(*Tape());
    ASSERT_EQ(NativeOperations_::ReadAdjoint(rate), -0.5);
    ASSERT_EQ(NativeOperations_::ReadAdjoint(prefix.solution_(1, 0)), 0.0);
    scope.Close();
    Clear(*Tape());
}
