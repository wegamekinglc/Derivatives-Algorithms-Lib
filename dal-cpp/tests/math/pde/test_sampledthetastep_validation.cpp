//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

#include <dal/math/buffercapacity.hpp>
#include <dal/math/pde/sampledthetastep.hpp>
#include <dal/platform/platform.hpp>

using Dal::LinearSolveAccuracyPolicy_;
using Dal::Matrix_;
using Dal::Vector_;
using Dal::PDE::SampledThetaStepInputs_;
using Dal::PDE::SampledThetaStepPullback_;

namespace {
    SampledThetaStepInputs_ BasicInputs(double theta = 0.5) {
        SampledThetaStepInputs_ inputs;
        inputs.x_ = {0.0, 1.0, 2.0};
        inputs.rates_ = {0.1};
        inputs.drifts_ = {0.2};
        inputs.variances_ = {0.4};
        inputs.dt_ = 0.2;
        inputs.theta_ = theta;
        inputs.oldValues_ = Matrix_<>(3, 1, 1.0);
        return inputs;
    }

    template <class Mutation_> void CheckInvalidInputs(const Mutation_& mutation, const char* message) {
        auto inputs = BasicInputs();
        mutation(&inputs);
        Dal::BufferCapacityBudget_ budget(0);
        Dal::BufferCapacityScope_ scope(&budget);
        try {
            static_cast<void>(SampledThetaStepPullback_(inputs, LinearSolveAccuracyPolicy_{1e-14, 1e-14}));
            FAIL() << "Invalid sampled theta-step input was accepted";
        } catch (const Dal::Exception_& error) {
            ASSERT_NE(std::string(error.what()).find(message), std::string::npos);
        }
        ASSERT_EQ(budget.CapacityBytes(), 0);
        ASSERT_EQ(budget.PeakCapacityBytes(), 0);
    }

    void CheckInvalidPolicy(double value, bool forward) {
        const auto inputs = BasicInputs(0.0);
        LinearSolveAccuracyPolicy_ policy{1e-14, 1e-14};
        if (forward)
            policy.forwardBackwardErrorLimit_ = value;
        else
            policy.transposeBackwardErrorLimit_ = value;
        Dal::BufferCapacityBudget_ budget(0);
        Dal::BufferCapacityScope_ scope(&budget);
        try {
            static_cast<void>(SampledThetaStepPullback_(inputs, policy));
            FAIL() << "Invalid sampled theta-step accuracy policy was accepted";
        } catch (const Dal::Exception_& error) {
            ASSERT_NE(std::string(error.what()).find(forward ? "forward backward-error limit" : "transpose backward-error limit"), std::string::npos);
        }
        ASSERT_EQ(budget.CapacityBytes(), 0);
        ASSERT_EQ(budget.PeakCapacityBytes(), 0);
    }

    Matrix_<> InteriorSeed() {
        Matrix_<> seed(3, 1);
        seed(1, 0) = 1.0;
        return seed;
    }
} // namespace

TEST(SampledThetaStepTest, TestInvalidGridRejectsBeforeCapture) {
    for (const Vector_<>& grid :
         {Vector_<>{0.0, 1.0}, Vector_<>{0.0, 1.0, 1.0}, Vector_<>{0.0, 2.0, 1.0}, Vector_<>{0.0, std::numeric_limits<double>::quiet_NaN(), 2.0},
          Vector_<>{0.0, 1.0, std::numeric_limits<double>::infinity()}})
        CheckInvalidInputs([&](auto* inputs) { inputs->x_ = grid; }, "grid");
}

TEST(SampledThetaStepTest, TestInvalidCoefficientsAndValuesRejectBeforeCapture) {
    CheckInvalidInputs([](auto* inputs) { inputs->rates_.clear(); }, "coefficients");
    CheckInvalidInputs([](auto* inputs) { inputs->drifts_.clear(); }, "coefficients");
    CheckInvalidInputs([](auto* inputs) { inputs->variances_.clear(); }, "coefficients");
    CheckInvalidInputs([](auto* inputs) { inputs->variances_[0] = -0.1; }, "nonnegative");
    CheckInvalidInputs([](auto* inputs) { inputs->rates_[0] = std::numeric_limits<double>::infinity(); }, "rates");
    CheckInvalidInputs([](auto* inputs) { inputs->drifts_[0] = std::numeric_limits<double>::quiet_NaN(); }, "drifts");
    CheckInvalidInputs([](auto* inputs) { inputs->variances_[0] = std::numeric_limits<double>::quiet_NaN(); }, "variances");
    CheckInvalidInputs([](auto* inputs) { inputs->oldValues_ = Matrix_<>(2, 1); }, "old values");
    CheckInvalidInputs([](auto* inputs) { inputs->oldValues_ = Matrix_<>(3, 0); }, "old values");
    CheckInvalidInputs([](auto* inputs) { inputs->oldValues_(1, 0) = std::numeric_limits<double>::quiet_NaN(); }, "old values");
    CheckInvalidInputs([](auto* inputs) { inputs->externalBoundaries_[0] = true; }, "external values");
    CheckInvalidInputs([](auto* inputs) { inputs->externalValues_ = Matrix_<>(1, 1); }, "external values");
    CheckInvalidInputs([](auto* inputs) { inputs->externalValues_ = Matrix_<>(2, 2); }, "external values");
    CheckInvalidInputs([](auto* inputs) { inputs->externalValues_ = Matrix_<>(2, 1, std::numeric_limits<double>::infinity()); }, "external values");
}

TEST(SampledThetaStepTest, TestInvalidTimeThetaAndAccuracyRejectBeforeCapture) {
    for (double value : {-1.0, std::nextafter(1.0, 2.0), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        CheckInvalidInputs([value](auto* inputs) { inputs->theta_ = value; }, "theta");
        CheckInvalidPolicy(value, true);
        CheckInvalidPolicy(value, false);
    }
    for (double value : {-1.0, 0.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        CheckInvalidInputs([value](auto* inputs) { inputs->dt_ = value; }, "dt");
}

TEST(SampledThetaStepTest, TestInvalidPivotToleranceRejectsBeforeExplicitAndImplicitCapture) {
    for (double theta : {0.0, 0.5}) {
        const auto inputs = BasicInputs(theta);
        for (double tolerance : {-1.0, 0.0, 1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            Dal::BufferCapacityBudget_ budget(0);
            Dal::BufferCapacityScope_ scope(&budget);
            try {
                static_cast<void>(SampledThetaStepPullback_(inputs, LinearSolveAccuracyPolicy_{1e-14, 1e-14}, tolerance));
                FAIL() << "Invalid sampled theta-step pivot tolerance was accepted";
            } catch (const Dal::Exception_& error) {
                ASSERT_NE(std::string(error.what()).find("pivot tolerance"), std::string::npos);
            }
            ASSERT_EQ(budget.CapacityBytes(), 0);
            ASSERT_EQ(budget.PeakCapacityBytes(), 0);
        }
    }
}

TEST(SampledThetaStepTest, TestTridiagonalPivotAndNumericalRangeAdmission) {
    using Dal::PDE::SampledThetaDetail::TridiagonalFactors_;
    ASSERT_THROW(static_cast<void>(TridiagonalFactors_({}, {}, {}, 1e-14)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(TridiagonalFactors_({}, {0.0}, {}, 1e-14)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(TridiagonalFactors_({}, {std::numeric_limits<double>::infinity()}, {}, 1e-14)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(TridiagonalFactors_({0.0}, {0.25, 1.0}, {0.0}, 0.25)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(TridiagonalFactors_({0.0}, {1.0, 0.25}, {0.0}, 0.25)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(TridiagonalFactors_({1.0}, {1.0, 1.0}, {1.0}, 1e-14)), Dal::Exception_);
    const TridiagonalFactors_ admitted({0.0}, {1.0, 0.25}, {0.0}, std::nextafter(0.25, 0.0));
    for (bool transpose : {false, true}) {
        const auto result = admitted.Solve(Matrix_<>(2, 1, 1.0), transpose);
        ASSERT_DOUBLE_EQ(result(0, 0), 1.0);
        ASSERT_DOUBLE_EQ(result(1, 0), 4.0);
    }
    ASSERT_THROW(static_cast<void>(TridiagonalFactors_({0.0}, {1e-300, 1e300}, {0.0}, 1e-310)), Dal::Exception_);
    for (double scale : {1e-300, 1e300}) {
        const TridiagonalFactors_ factors({}, {scale}, {}, 1e-14);
        ASSERT_THROW(static_cast<void>(factors.Solve(Matrix_<>(1, 1, 1.0 / scale), false)), Dal::Exception_);
        ASSERT_THROW(static_cast<void>(factors.Solve(Matrix_<>(1, 1, 1.0 / scale), true)), Dal::Exception_);
        ASSERT_DOUBLE_EQ(factors.Solve(Matrix_<>(1, 1, scale), false)(0, 0), 1.0);
    }
}

TEST(SampledThetaStepTest, TestBoundaryProvenanceIsIndependentOfEqualValues) {
    const auto oldInputs = BasicInputs();
    auto externalInputs = oldInputs;
    externalInputs.externalBoundaries_ = {true, true};
    externalInputs.externalValues_ = Matrix_<>(2, 1, 1.0);
    const SampledThetaStepPullback_ oldStep(oldInputs, LinearSolveAccuracyPolicy_{1e-14, 1e-14});
    const SampledThetaStepPullback_ externalStep(externalInputs, LinearSolveAccuracyPolicy_{1e-14, 1e-14});
    const auto oldRisk = oldStep.Reverse(InteriorSeed());
    const auto externalRisk = externalStep.Reverse(InteriorSeed());
    for (int row = 0; row < 3; ++row)
        ASSERT_DOUBLE_EQ(oldStep.Solution()(row, 0), externalStep.Solution()(row, 0));
    for (int side = 0; side < 2; ++side) {
        const int row = side == 0 ? 0 : 2;
        ASSERT_DOUBLE_EQ(oldRisk.externalValues_(side, 0), 0.0);
        ASSERT_GT(externalRisk.externalValues_(side, 0), 0.0);
        ASSERT_NEAR(oldRisk.oldValues_(row, 0) - externalRisk.oldValues_(row, 0), externalRisk.externalValues_(side, 0), 1e-13);
    }
}

TEST(SampledThetaStepTest, TestThetaEndpointsRetainCompleteDerivatives) {
    const SampledThetaStepPullback_ explicitStep(BasicInputs(0.0), LinearSolveAccuracyPolicy_{0.0, 0.0});
    const auto explicitRisk = explicitStep.Reverse(InteriorSeed());
    ASSERT_NEAR(explicitStep.Solution()(1, 0), 0.98, 1e-13);
    ASSERT_NEAR(explicitRisk.theta_, 0.002, 1e-13);
    ASSERT_NEAR(explicitRisk.dt_, -0.1, 1e-13);
    ASSERT_NEAR(explicitRisk.rates_[0], -0.2, 1e-13);
    ASSERT_NEAR(explicitRisk.oldValues_(0, 0), 0.02, 1e-13);
    ASSERT_NEAR(explicitRisk.oldValues_(1, 0), 0.9, 1e-13);
    ASSERT_NEAR(explicitRisk.oldValues_(2, 0), 0.06, 1e-13);
    ASSERT_DOUBLE_EQ(explicitStep.ForwardBackwardErrors()[0], 0.0);
    ASSERT_DOUBLE_EQ(explicitRisk.transposeBackwardErrors_[0], 0.0);
    const SampledThetaStepPullback_ implicitStep(BasicInputs(1.0), LinearSolveAccuracyPolicy_{1e-14, 1e-14});
    const auto implicitRisk = implicitStep.Reverse(InteriorSeed());
    ASSERT_NEAR(implicitStep.Solution()(1, 0), 54.0 / 55.0, 1e-13);
    ASSERT_NEAR(implicitRisk.oldValues_(0, 0), 1.0 / 55.0, 1e-13);
    ASSERT_NEAR(implicitRisk.oldValues_(1, 0), 10.0 / 11.0, 1e-13);
    ASSERT_NEAR(implicitRisk.oldValues_(2, 0), 3.0 / 55.0, 1e-13);
}

TEST(SampledThetaStepTest, TestInclusiveForwardAndTransposeAccuracyLimits) {
    auto inputs = BasicInputs();
    inputs.oldValues_(1, 0) = 2.0;
    inputs.oldValues_(2, 0) = 3.0;
    inputs.externalBoundaries_ = {true, true};
    inputs.externalValues_ = Matrix_<>(2, 1);
    inputs.externalValues_(0, 0) = 4.0;
    inputs.externalValues_(1, 0) = 5.0;
    Matrix_<> seeds(3, 1);
    seeds(0, 0) = 0.5;
    seeds(1, 0) = 2.0;
    seeds(2, 0) = -0.3;
    const SampledThetaStepPullback_ observed(inputs, LinearSolveAccuracyPolicy_{1.0, 1.0});
    const auto observedRisk = observed.Reverse(seeds);
    const double forward = observed.ForwardBackwardErrors()[0];
    const double transpose = observedRisk.transposeBackwardErrors_[0];
    ASSERT_GT(forward, 0.0);
    ASSERT_GT(transpose, 0.0);
    const SampledThetaStepPullback_ inclusive(inputs, LinearSolveAccuracyPolicy_{forward, transpose});
    ASSERT_DOUBLE_EQ(inclusive.Reverse(seeds).transposeBackwardErrors_[0], transpose);
    ASSERT_THROW(static_cast<void>(SampledThetaStepPullback_(inputs, LinearSolveAccuracyPolicy_{std::nextafter(forward, 0.0), 1.0})),
                 Dal::Exception_);
    const SampledThetaStepPullback_ strict(inputs, LinearSolveAccuracyPolicy_{1.0, std::nextafter(transpose, 0.0)});
    ASSERT_THROW(static_cast<void>(strict.Reverse(seeds)), Dal::Exception_);
    const auto recovered = strict.Reverse(Matrix_<>(3, 1));
    ASSERT_DOUBLE_EQ(recovered.transposeBackwardErrors_[0], 0.0);
}

TEST(SampledThetaStepTest, TestNonzeroRiskUnderflowRejectsAndCacheRecovers) {
    auto inputs = BasicInputs(0.0);
    inputs.rates_[0] = inputs.drifts_[0] = inputs.variances_[0] = 0.0;
    inputs.dt_ = 2e-300;
    const SampledThetaStepPullback_ step(inputs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    auto tinySeed = InteriorSeed();
    tinySeed(1, 0) = 1e-300;
    ASSERT_THROW(static_cast<void>(step.Reverse(tinySeed)), Dal::Exception_);
    const auto risk = step.Reverse(InteriorSeed());
    ASSERT_DOUBLE_EQ(risk.rates_[0], -2e-300);
    ASSERT_DOUBLE_EQ(step.Reverse(Matrix_<>(3, 1)).rates_[0], 0.0);
}

TEST(SampledThetaStepTest, TestMinimumSubnormalRiskIsPreserved) {
    auto inputs = BasicInputs(0.0);
    inputs.x_ = {0.0, 0.5, 1.0};
    inputs.rates_[0] = inputs.drifts_[0] = inputs.variances_[0] = 0.0;
    inputs.dt_ = std::numeric_limits<double>::denorm_min();
    const SampledThetaStepPullback_ step(inputs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    const auto risk = step.Reverse(InteriorSeed());
    ASSERT_DOUBLE_EQ(risk.rates_[0], -std::numeric_limits<double>::denorm_min());
    ASSERT_DOUBLE_EQ(risk.drifts_[0], 0.0);
    ASSERT_DOUBLE_EQ(risk.variances_[0], 0.0);
}

TEST(SampledThetaStepTest, TestSingularCaptureRefundsAndRecovers) {
    auto inputs = BasicInputs(1.0);
    inputs.rates_[0] = -5.0;
    inputs.drifts_[0] = inputs.variances_[0] = 0.0;
    Dal::BufferCapacityBudget_ budget(1024 * 1024);
    Dal::BufferCapacityScope_ scope(&budget);
    ASSERT_THROW(static_cast<void>(SampledThetaStepPullback_(inputs, LinearSolveAccuracyPolicy_{1e-14, 1e-14})), Dal::Exception_);
    ASSERT_EQ(budget.CapacityBytes(), 0);
    {
        const SampledThetaStepPullback_ recovered(BasicInputs(), LinearSolveAccuracyPolicy_{1e-14, 1e-14});
        ASSERT_NEAR(recovered.Solution()(1, 0), 103.0 / 105.0, 1e-13);
    }
    ASSERT_EQ(budget.CapacityBytes(), 0);
}
