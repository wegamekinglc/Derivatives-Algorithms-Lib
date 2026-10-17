//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <future>
#include <limits>

#include <dal/math/aad/statistics.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/platform/platform.hpp>

#include "sampledthetastepfixture.hpp"

using namespace Dal;
using namespace Dal::AAD;
using DalTest::NativePDE::IdentityInputs;
using DalTest::NativePDE::RecoveryRisk;

namespace {
    template <class F_> void CheckCaptureFailure(const F_& change, double value = 0.0, LinearSolveAccuracyPolicy_ policy = {0.0, 0.0}) {
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(false, 1);
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, value);
        scope.StartRecording();
        auto numeric = IdentityInputs();
        SampledThetaStepBindings_ active;
        active.rates_ = {input};
        change(&numeric, &active);
        const auto before = MeasureTape(*Tape());
        BufferCapacityBudget_ budget(16384);
        BufferCapacityScope_ caller(&budget);
        CheckedSampledThetaStepResult_ unpublished;
        ASSERT_THROW(unpublished = SampledThetaStepWithAccuracy(&scope, numeric, active, policy), Exception_);
        ASSERT_TRUE(unpublished.solution_.Empty());
        ASSERT_EQ(unpublished.event_.EventId(), 0);
        ASSERT_EQ(MeasureTape(*Tape()).nodes_, before.nodes_);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEvents_, before.reverseEvents_);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, before.reverseEventCapacityBytes_);
        ASSERT_EQ(budget.CapacityBytes(), 0);
        ASSERT_THROW(scope.FinishRecording(), Exception_);
        scope.Close();
        ASSERT_EQ(RecoveryRisk(), -0.25);
    }
} // namespace

TEST(AADSampledThetaStepTest, TestFullyPassiveNativeRequestRejectsWithoutPublication) {
    ASSERT_NO_FATAL_FAILURE(CheckCaptureFailure([](auto*, auto* active) { *active = SampledThetaStepBindings_{}; }));
}

TEST(AADSampledThetaStepTest, TestInvalidActiveShapesRejectAndRecover) {
    ASSERT_NO_FATAL_FAILURE(CheckCaptureFailure([](auto*, auto* active) { active->rates_.push_back(active->rates_[0]); }));
    ASSERT_NO_FATAL_FAILURE(CheckCaptureFailure([](auto*, auto* active) { active->oldValues_ = Matrix_<Number_>(2, 1, active->rates_[0]); }));
    ASSERT_NO_FATAL_FAILURE(CheckCaptureFailure([](auto*, auto* active) { active->externalValues_ = Matrix_<Number_>(1, 1, active->rates_[0]); }));
}

TEST(AADSampledThetaStepTest, TestInvalidActiveVarianceTimeAndThetaRejectAndRecover) {
    ASSERT_NO_FATAL_FAILURE(CheckCaptureFailure([](auto*, auto* active) { active->variances_ = active->rates_; }, -1.0));
    ASSERT_NO_FATAL_FAILURE(CheckCaptureFailure([](auto*, auto* active) { active->dt_ = active->rates_[0]; }));
    ASSERT_NO_FATAL_FAILURE(CheckCaptureFailure([](auto*, auto* active) { active->theta_ = active->rates_[0]; }, 2.0));
}

TEST(AADSampledThetaStepTest, TestInvalidPassiveGridAndAccuracyPolicyRejectAndRecover) {
    ASSERT_NO_FATAL_FAILURE(CheckCaptureFailure([](auto* numeric, auto*) { numeric->x_[1] = numeric->x_[0]; }));
    for (const LinearSolveAccuracyPolicy_ policy : {LinearSolveAccuracyPolicy_{-1.0, 0.0}, LinearSolveAccuracyPolicy_{0.0, 2.0}})
        ASSERT_NO_FATAL_FAILURE(CheckCaptureFailure([](auto*, auto*) {}, 0.0, policy));
}

TEST(AADSampledThetaStepTest, TestUnusedExternalActiveValuesStillRequireFiniteInput) {
    for (double value : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
        ASSERT_NO_FATAL_FAILURE(CheckCaptureFailure(
            [](auto*, auto* active) {
                active->externalValues_ = Matrix_<Number_>(2, 1, active->rates_[0]);
                active->rates_.clear();
            },
            value));
}

TEST(AADSampledThetaStepTest, TestNullWrongPhaseRejectBeforeAllocationAndPreserveScope) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 0.0);
    const auto numeric = IdentityInputs();
    SampledThetaStepBindings_ active;
    active.rates_ = {input};
    const auto before = MeasureTape(*Tape());
    {
        BufferCapacityBudget_ budget(0);
        BufferCapacityScope_ caller(&budget);
        ASSERT_THROW(static_cast<void>(SampledThetaStepWithAccuracy(nullptr, numeric, active, LinearSolveAccuracyPolicy_{0.0, 0.0})), Exception_);
        ASSERT_THROW(static_cast<void>(SampledThetaStepWithAccuracy(&scope, numeric, active, LinearSolveAccuracyPolicy_{0.0, 0.0})), Exception_);
        ASSERT_EQ(budget.PeakCapacityBytes(), 0);
    }
    ASSERT_EQ(MeasureTape(*Tape()).nodes_, before.nodes_);
    scope.StartRecording();
    auto result = SampledThetaStepWithAccuracy(&scope, numeric, active, LinearSolveAccuracyPolicy_{0.0, 0.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(result.solution_(1, 0), 1.0);
    scope.Reverse();
    ASSERT_EQ(NativeOperations_::ReadAdjoint(input), -0.25);
    scope.Close();
    Clear(*Tape());
}

TEST(AADSampledThetaStepTest, TestStaleUnoccupiedCoefficientAndUnusedBoundaryReject) {
    for (bool external : {false, true}) {
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(false, 1);
        Number_ stale;
        {
            RecordingScope_ old;
            Number_ dummy;
            old.RegisterInput(dummy, 0.0);
            old.RegisterInput(stale, 0.0);
            old.Close();
        }
        ASSERT_NO_FATAL_FAILURE(CheckCaptureFailure([&](auto*, auto* active) {
            if (external)
                active->externalValues_ = Matrix_<Number_>(2, 1, stale);
            else
                active->rates_ = {stale};
        }));
    }
}

TEST(AADSampledThetaStepTest, TestForeignInputRejectsInWorkerWithoutDamagingOwner) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ owner;
    Number_ input;
    owner.RegisterInput(input, 0.0);
    owner.StartRecording();
    auto worker = std::async(std::launch::async, [input] {
        CheckCaptureFailure([&](auto*, auto* active) { active->rates_ = {input}; });
        return !::testing::Test::HasFatalFailure();
    });
    ASSERT_TRUE(worker.get());
    SampledThetaStepBindings_ active;
    active.rates_ = {input};
    auto step = SampledThetaStepWithAccuracy(&owner, IdentityInputs(), active, LinearSolveAccuracyPolicy_{0.0, 0.0});
    owner.FinishRecording();
    owner.ClearAdjoints();
    NativeOperations_::SetSeed(step.solution_(1, 0), 1.0);
    owner.Reverse();
    ASSERT_EQ(NativeOperations_::ReadAdjoint(input), -0.25);
    owner.Close();
    Clear(*Tape());
}

namespace {
    void CheckReverseFailure(double seed, bool inactiveRange, bool collect) {
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(false, 1);
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, inactiveRange ? 1.0 : 0.0);
        scope.StartRecording();
        auto numeric = IdentityInputs();
        SampledThetaStepBindings_ active;
        if (inactiveRange) {
            numeric.dt_ = 2e-300;
            active.oldValues_ = Matrix_<Number_>(3, 1, input);
        } else
            active.rates_ = {input};
        auto step = SampledThetaStepWithAccuracy(&scope, numeric, active, LinearSolveAccuracyPolicy_{0.0, 0.0});
        scope.FinishRecording();
        scope.ClearAdjoints();
        const auto historical = ReverseWithSolveAccuracy(&scope);
        NativeOperations_::SetSeed(step.solution_(1, 0), seed);
        BufferCapacityBudget_ budget(4096);
        BufferCapacityScope_ caller(&budget);
        SolveAccuracyReports_ unpublished;
        if (collect)
            ASSERT_THROW(unpublished = ReverseWithSolveAccuracy(&scope), Exception_);
        else
            ASSERT_THROW(scope.Reverse(), Exception_);
        ASSERT_TRUE(unpublished.Entries().empty());
        ASSERT_EQ(unpublished.InvocationId(), 0);
        ASSERT_EQ(budget.CapacityBytes(), 0);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Exception_);
        ASSERT_THROW(scope.Reverse(), Exception_);
        scope.Close();
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
        ASSERT_EQ(historical.Report(step.event_).transposeBackwardErrors_(0, 0), 0.0);
        ASSERT_EQ(RecoveryRisk(), -0.25);
    }
} // namespace

TEST(AADSampledThetaStepTest, TestNonfiniteSeedsDiscardReportsAndInvalidateRecording) {
    for (double seed : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
        for (bool collect : {false, true})
            ASSERT_NO_FATAL_FAILURE(CheckReverseFailure(seed, false, collect));
}

TEST(AADSampledThetaStepTest, TestInactiveCoordinateRangeLossStillRejects) {
    for (bool collect : {false, true})
        ASSERT_NO_FATAL_FAILURE(CheckReverseFailure(1e-300, true, collect));
}

TEST(AADSampledThetaStepTest, TestAliasAccumulationOverflowRejectsAfterFiniteNumericReverse) {
    auto numeric = IdentityInputs();
    numeric.oldValues_ = Matrix_<>(3, 1, 0.0);
    const double maximum = std::numeric_limits<double>::max();
    {
        const PDE::SampledThetaStepPullback_ step(numeric, LinearSolveAccuracyPolicy_{0.0, 0.0});
        const auto risk = step.Reverse(Matrix_<>(3, 1, maximum));
        for (double value : risk.oldValues_)
            ASSERT_EQ(value, maximum);
    }
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 0.0);
    scope.StartRecording();
    SampledThetaStepBindings_ active;
    active.oldValues_ = Matrix_<Number_>(3, 1, input);
    auto step = SampledThetaStepWithAccuracy(&scope, numeric, active, LinearSolveAccuracyPolicy_{0.0, 0.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    const auto historical = ReverseWithSolveAccuracy(&scope);
    for (int row = 0; row < 3; ++row)
        NativeOperations_::SetSeed(step.solution_(row, 0), maximum);
    SolveAccuracyReports_ unpublished;
    ASSERT_THROW(unpublished = ReverseWithSolveAccuracy(&scope), Exception_);
    ASSERT_TRUE(unpublished.Entries().empty());
    ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Exception_);
    scope.Close();
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    ASSERT_EQ(historical.Report(step.event_).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_EQ(RecoveryRisk(), -0.25);
}

TEST(AADSampledThetaStepTest, TestLaterChannelFailureDiscardsMixedPartialReportsAndAdjoints) {
    for (bool collect : {false, true}) {
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(true, 2);
        RecordingScope_ scope;
        Number_ rate;
        scope.RegisterInput(rate, 0.0);
        scope.StartRecording();
        SampledThetaStepBindings_ active;
        active.rates_ = {rate};
        auto rejected = SampledThetaStepWithAccuracy(&scope, IdentityInputs(), active, LinearSolveAccuracyPolicy_{0.0, 0.0});
        Matrix_<Number_> rhs(1, 1, rate);
        auto accepted = LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 2.0), rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
        scope.FinishRecording();
        scope.ClearAdjoints();
        const auto historical = ReverseWithSolveAccuracy(&scope);
        ASSERT_EQ(historical.Entries().size(), 2);
        NativeOperations_::SetSeed(accepted.solution_(0, 0), 1.0, 0);
        NativeOperations_::SetSeed(rejected.solution_(1, 0), 1.0, 0);
        NativeOperations_::SetSeed(rejected.solution_(1, 0), std::numeric_limits<double>::infinity(), 1);
        BufferCapacityBudget_ budget(8192);
        BufferCapacityScope_ caller(&budget);
        SolveAccuracyReports_ unpublished;
        if (collect)
            ASSERT_THROW(unpublished = ReverseWithSolveAccuracy(&scope), Exception_);
        else
            ASSERT_THROW(scope.Reverse(), Exception_);
        ASSERT_TRUE(unpublished.Entries().empty());
        ASSERT_EQ(unpublished.InvocationId(), 0);
        ASSERT_EQ(budget.CapacityBytes(), 0);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(rate, 0)), Exception_);
        scope.Close();
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
        ASSERT_EQ(historical.Report(rejected.event_).transposeBackwardErrors_(0, 1), 0.0);
        ASSERT_EQ(historical.Report(accepted.event_).transposeBackwardErrors_(0, 0), 0.0);
        ASSERT_EQ(RecoveryRisk(), -0.25);
    }
}
