//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <dal/math/buffercapacity.hpp>
#include <dal/math/pde/sampledthetastep.hpp>
#include <dal/platform/platform.hpp>

using Dal::BufferCapacityBudget_;
using Dal::BufferCapacityScope_;
using Dal::LinearSolveAccuracyPolicy_;
using Dal::Matrix_;
using Dal::Vector_;
using Dal::PDE::SampledThetaStepInputs_;
using Dal::PDE::SampledThetaStepPullback_;

namespace {
    struct Capacity_ {
        size_t retained_, peak_;
    };

    SampledThetaStepInputs_ CapacityInputs(int n, int layers, double theta) {
        SampledThetaStepInputs_ inputs;
        inputs.x_ = Vector_<>(n);
        for (int row = 0; row < n; ++row)
            inputs.x_[row] = row;
        inputs.rates_ = Vector_<>(n - 2, 0.1);
        inputs.drifts_ = Vector_<>(n - 2, 0.2);
        inputs.variances_ = Vector_<>(n - 2, 0.4);
        inputs.dt_ = 0.2;
        inputs.theta_ = theta;
        inputs.oldValues_ = Matrix_<>(n, layers);
        for (int row = 0; row < n; ++row)
            for (int layer = 0; layer < layers; ++layer)
                inputs.oldValues_(row, layer) = 1.0 + 0.2 * row + 0.1 * layer;
        return inputs;
    }

    size_t CaptureRetained(int n, int layers, double theta) {
        const size_t base = static_cast<size_t>(9 * (n - 2) + 2 * n * layers + layers) * sizeof(double);
        return theta == 0.0 ? base : base + static_cast<size_t>(4 * n - 4) * sizeof(double) + static_cast<size_t>(n - 1) * sizeof(int);
    }

    size_t CapturePeak(int n, int layers, double theta) {
        return CaptureRetained(n, layers, theta) + (theta == 0.0 ? 0 : static_cast<size_t>(n * layers) * sizeof(double));
    }

    size_t ReverseRetained(int n, int layers) { return static_cast<size_t>(n * layers + 3 * layers + 3 * (n - 2)) * sizeof(double); }

    Capacity_ MeasureCapture(int n, int layers, double theta, size_t limit = 1024 * 1024) {
        const auto inputs = CapacityInputs(n, layers, theta);
        BufferCapacityBudget_ budget(limit);
        BufferCapacityScope_ scope(&budget);
        Capacity_ measured;
        {
            const SampledThetaStepPullback_ step(inputs, LinearSolveAccuracyPolicy_{1e-14, 1e-14});
            REQUIRE(step.Solution().Rows() == n && step.Solution().Cols() == layers, "Resource step must retain its solution shape");
            measured = {budget.CapacityBytes(), budget.PeakCapacityBytes()};
        }
        REQUIRE(budget.CapacityBytes() == 0, "Resource step destruction must refund captured buffers");
        return measured;
    }

    Capacity_ MeasureReverse(int n, int layers, double theta, size_t limit = 1024 * 1024) {
        const SampledThetaStepPullback_ step(CapacityInputs(n, layers, theta), LinearSolveAccuracyPolicy_{1e-14, 1e-14});
        const Matrix_<> seeds(n, layers, 1.0);
        BufferCapacityBudget_ budget(limit);
        BufferCapacityScope_ scope(&budget);
        Capacity_ measured;
        {
            const auto risk = step.Reverse(seeds);
            REQUIRE(risk.oldValues_.Rows() == n && risk.oldValues_.Cols() == layers, "Resource reverse must retain its result shape");
            measured = {budget.CapacityBytes(), budget.PeakCapacityBytes()};
        }
        REQUIRE(budget.CapacityBytes() == 0, "Resource reverse destruction must refund result buffers");
        return measured;
    }
} // namespace

TEST(SampledThetaStepTest, TestLinearCacheAndReverseCapacityGrowth) {
    for (int n : {3, 7, 11})
        for (int layers : {1, 2})
            for (double theta : {0.0, 0.5}) {
                SCOPED_TRACE(n);
                SCOPED_TRACE(layers);
                SCOPED_TRACE(theta);
                const auto capture = MeasureCapture(n, layers, theta);
                ASSERT_EQ(capture.retained_, CaptureRetained(n, layers, theta));
                ASSERT_EQ(capture.peak_, CapturePeak(n, layers, theta));
                const auto reverse = MeasureReverse(n, layers, theta);
                ASSERT_EQ(reverse.retained_, ReverseRetained(n, layers));
                ASSERT_EQ(reverse.peak_, ReverseRetained(n, layers) + static_cast<size_t>(n * layers) * sizeof(double));
            }
}

TEST(SampledThetaStepTest, TestExactCaptureCapacityAndOneByteShortRecovery) {
    for (double theta : {0.0, 0.5}) {
        const size_t peak = CapturePeak(3, 2, theta);
        ASSERT_EQ(MeasureCapture(3, 2, theta, peak).peak_, peak);
        ASSERT_THROW(static_cast<void>(MeasureCapture(3, 2, theta, peak - 1)), Dal::Exception_);
        ASSERT_EQ(MeasureCapture(3, 2, theta, peak).peak_, peak);
    }
}

TEST(SampledThetaStepTest, TestExactReverseCapacityAndOneByteShortRecovery) {
    for (double theta : {0.0, 0.5}) {
        const size_t peak = ReverseRetained(7, 2) + 14 * sizeof(double);
        ASSERT_EQ(MeasureReverse(7, 2, theta, peak).peak_, peak);
        ASSERT_THROW(static_cast<void>(MeasureReverse(7, 2, theta, peak - 1)), Dal::Exception_);
        ASSERT_EQ(MeasureReverse(7, 2, theta, peak).peak_, peak);
    }
}

TEST(SampledThetaStepTest, TestFailedCapacityAdmissionRefundsAndPreservesCache) {
    for (double theta : {0.0, 0.5}) {
        const auto inputs = CapacityInputs(7, 2, theta);
        const SampledThetaStepPullback_ step(inputs, LinearSolveAccuracyPolicy_{1e-14, 1e-14});
        const Matrix_<> seeds(7, 2, 1.0);
        {
            BufferCapacityBudget_ budget(CapturePeak(7, 2, theta) - 1);
            BufferCapacityScope_ scope(&budget);
            ASSERT_THROW(static_cast<void>(SampledThetaStepPullback_(inputs, LinearSolveAccuracyPolicy_{1e-14, 1e-14})), Dal::Exception_);
            ASSERT_EQ(budget.CapacityBytes(), 0);
        }
        {
            BufferCapacityBudget_ budget(ReverseRetained(7, 2) + 14 * sizeof(double) - 1);
            BufferCapacityScope_ scope(&budget);
            ASSERT_THROW(static_cast<void>(step.Reverse(seeds)), Dal::Exception_);
            ASSERT_EQ(budget.CapacityBytes(), 0);
        }
        const auto recovered = step.Reverse(seeds);
        ASSERT_EQ(recovered.oldValues_.Rows(), 7);
        for (double error : recovered.transposeBackwardErrors_)
            ASSERT_LE(error, 1e-14);
    }
}
