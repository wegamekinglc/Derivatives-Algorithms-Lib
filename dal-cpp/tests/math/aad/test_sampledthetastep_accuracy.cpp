//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <cmath>
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
    PDE::SampledThetaStepInputs_ AccuracyInputs() {
        auto inputs = IdentityInputs(3, 1, 0.5);
        inputs.rates_ = {0.1};
        inputs.drifts_ = {0.2};
        inputs.variances_ = {0.4};
        inputs.dt_ = 0.2;
        inputs.oldValues_(1, 0) = 2.0;
        inputs.oldValues_(2, 0) = 3.0;
        inputs.externalBoundaries_ = {true, true};
        inputs.externalValues_ = Matrix_<>(2, 1);
        inputs.externalValues_(0, 0) = 4.0;
        inputs.externalValues_(1, 0) = 5.0;
        return inputs;
    }

    Matrix_<> AccuracySeeds() {
        Matrix_<> seeds(3, 1);
        seeds(0, 0) = 0.5;
        seeds(1, 0) = 2.0;
        seeds(2, 0) = -0.3;
        return seeds;
    }

    LinearSolveAccuracyPolicy_ ObservedPolicy(const PDE::SampledThetaStepInputs_& inputs) {
        const PDE::SampledThetaStepPullback_ observed(inputs, LinearSolveAccuracyPolicy_{1.0, 1.0});
        return {observed.ForwardBackwardErrors()[0], observed.Reverse(AccuracySeeds()).transposeBackwardErrors_[0]};
    }

    void SeedStep(CheckedSampledThetaStepResult_* step) {
        const auto seeds = AccuracySeeds();
        for (int row = 0; row < 3; ++row)
            NativeOperations_::SetSeed(step->solution_(row, 0), seeds(row, 0));
    }

    void CheckTransposeRejection(bool collect) {
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(false, 1);
        const auto inputs = AccuracyInputs();
        auto policy = ObservedPolicy(inputs);
        ASSERT_GT(policy.transposeBackwardErrorLimit_, 0.0);
        policy.transposeBackwardErrorLimit_ = std::nextafter(policy.transposeBackwardErrorLimit_, 0.0);
        RecordingScope_ scope;
        Number_ rate;
        scope.RegisterInput(rate, inputs.rates_[0]);
        scope.StartRecording();
        SampledThetaStepBindings_ active;
        active.rates_ = {rate};
        auto step = SampledThetaStepWithAccuracy(&scope, inputs, active, policy);
        scope.FinishRecording();
        scope.ClearAdjoints();
        const auto historical = ReverseWithSolveAccuracy(&scope);
        SeedStep(&step);
        BufferCapacityBudget_ budget(4096);
        BufferCapacityScope_ caller(&budget);
        SolveAccuracyReports_ unpublished;
        if (collect)
            ASSERT_THROW(unpublished = ReverseWithSolveAccuracy(&scope), Exception_);
        else
            ASSERT_THROW(scope.Reverse(), Exception_);
        ASSERT_EQ(unpublished.InvocationId(), 0);
        ASSERT_TRUE(unpublished.Entries().empty());
        ASSERT_EQ(budget.CapacityBytes(), 0);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(rate)), Exception_);
        scope.Close();
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
        ASSERT_EQ(historical.Report(step.event_).transposeBackwardErrors_(0, 0), 0.0);
        ASSERT_EQ(RecoveryRisk(), -0.25);
    }
} // namespace

TEST(AADSampledThetaStepTest, TestInclusivePhysicalForwardAndTransposeLimits) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    const auto inputs = AccuracyInputs();
    const auto policy = ObservedPolicy(inputs);
    ASSERT_GT(policy.forwardBackwardErrorLimit_, 0.0);
    ASSERT_GT(policy.transposeBackwardErrorLimit_, 0.0);
    RecordingScope_ scope;
    Number_ rate;
    scope.RegisterInput(rate, inputs.rates_[0]);
    scope.StartRecording();
    SampledThetaStepBindings_ active;
    active.rates_ = {rate};
    auto step = SampledThetaStepWithAccuracy(&scope, inputs, active, policy);
    ASSERT_EQ(step.diagnostics_.forwardBackwardErrors_[0], policy.forwardBackwardErrorLimit_);
    scope.FinishRecording();
    scope.ClearAdjoints();
    SeedStep(&step);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_EQ(reports.Report(step.event_).transposeBackwardErrors_(0, 0), policy.transposeBackwardErrorLimit_);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(rate), -572.0 / 735.0, 1e-13);
    scope.Close();
    Clear(*Tape());
}

TEST(AADSampledThetaStepTest, TestForwardLimitRejectsBeforePublishingOutputs) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    const auto inputs = AccuracyInputs();
    auto policy = ObservedPolicy(inputs);
    policy.forwardBackwardErrorLimit_ = std::nextafter(policy.forwardBackwardErrorLimit_, 0.0);
    RecordingScope_ scope;
    Number_ rate;
    scope.RegisterInput(rate, inputs.rates_[0]);
    scope.StartRecording();
    SampledThetaStepBindings_ active;
    active.rates_ = {rate};
    const auto before = MeasureTape(*Tape());
    BufferCapacityBudget_ budget(16384);
    BufferCapacityScope_ caller(&budget);
    CheckedSampledThetaStepResult_ unpublished;
    ASSERT_THROW(unpublished = SampledThetaStepWithAccuracy(&scope, inputs, active, policy), Exception_);
    ASSERT_TRUE(unpublished.solution_.Empty());
    ASSERT_EQ(unpublished.event_.EventId(), 0);
    ASSERT_EQ(MeasureTape(*Tape()).nodes_, before.nodes_);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEvents_, before.reverseEvents_);
    ASSERT_EQ(budget.CapacityBytes(), 0);
    ASSERT_THROW(scope.FinishRecording(), Exception_);
    scope.Close();
    ASSERT_EQ(RecoveryRisk(), -0.25);
}

TEST(AADSampledThetaStepTest, TestTransposeLimitRejectsWithAndWithoutReports) {
    for (bool collect : {false, true})
        ASSERT_NO_FATAL_FAILURE(CheckTransposeRejection(collect));
}

TEST(AADSampledThetaStepTest, TestMinimumSubnormalRateRiskSurvivesNativeScatter) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    auto inputs = IdentityInputs();
    inputs.x_ = {0.0, 0.5, 1.0};
    inputs.dt_ = std::numeric_limits<double>::denorm_min();
    RecordingScope_ scope;
    Number_ rate;
    scope.RegisterInput(rate, 0.0);
    scope.StartRecording();
    SampledThetaStepBindings_ active;
    active.rates_ = {rate};
    auto step = SampledThetaStepWithAccuracy(&scope, inputs, active, LinearSolveAccuracyPolicy_{0.0, 0.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(step.solution_(1, 0), 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_EQ(NativeOperations_::ReadAdjoint(rate), -std::numeric_limits<double>::denorm_min());
    ASSERT_EQ(reports.Report(step.event_).transposeBackwardErrors_(0, 0), 0.0);
    scope.Close();
    Clear(*Tape());
}
