//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <algorithm>

#include <dal/math/aad/statistics.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/platform/platform.hpp>

#include "sampledthetastepfixture.hpp"

using namespace Dal;
using namespace Dal::AAD;

namespace {
    struct Resources_ {
        size_t retained_, scratch_, tapePeak_, caller_, callerPeak_, nodes_;
    };

    size_t CacheBytes(int n, int layers, double theta) {
        const size_t base = static_cast<size_t>(9 * (n - 2) + 2 * n * layers + layers) * sizeof(double);
        return theta == 0.0 ? base : base + static_cast<size_t>(4 * n - 4) * sizeof(double) + static_cast<size_t>(n - 1) * sizeof(int);
    }

    size_t BindingBytes(int n, int layers) { return static_cast<size_t>(3 * (n - 2) + n * layers + 2 * layers) * sizeof(Number_); }

    size_t DynamicEventBytes(int n, int layers, double theta) {
        return CacheBytes(n, layers, theta) + BindingBytes(n, layers) + static_cast<size_t>(n * layers) * sizeof(Number_);
    }

    size_t ScratchBytes(int n, int layers) { return static_cast<size_t>(3 * n * layers + 3 * layers + 3 * (n - 2)) * sizeof(double); }

    size_t CallerPeak(int n, int layers, double theta, size_t channels) {
        const size_t numeric = static_cast<size_t>(n + 3 * (n - 2) + n * layers + 2 * layers) * sizeof(double);
        const size_t extra = theta == 0.0 ? 0 : static_cast<size_t>(n * layers) * sizeof(double);
        const size_t capture = BindingBytes(n, layers) + numeric + CacheBytes(n, layers, theta) + extra;
        const size_t published = BindingBytes(n, layers) + CacheBytes(n, layers, theta) + static_cast<size_t>(layers) * sizeof(double) +
                                 static_cast<size_t>(n * layers) * sizeof(Number_);
        const size_t retained = static_cast<size_t>(n * layers) * sizeof(Number_) + static_cast<size_t>(layers) * sizeof(double) +
                                sizeof(SolveAccuracyReport_) + layers * channels * sizeof(double);
        return std::max({capture, published, retained + ScratchBytes(n, layers)});
    }

    void SeedOutputs(Matrix_<Number_>* values, size_t channels) {
        for (int row = 0; row < values->Rows(); ++row)
            for (int layer = 0; layer < values->Cols(); ++layer)
                for (size_t channel = 0; channel < channels; ++channel)
                    NativeOperations_::SetSeed((*values)(row, layer), 1.0, channel);
    }

    void CheckResourceRisks(const SampledThetaStepBindings_& inputs, int layers, size_t channels) {
        for (const auto& input : inputs.rates_)
            for (size_t channel = 0; channel < channels; ++channel)
                REQUIRE(NativeOperations_::ReadAdjoint(input, channel) == -0.25 * layers, "Identity PDE rates must aggregate layers");
        for (const auto& input : inputs.oldValues_)
            for (size_t channel = 0; channel < channels; ++channel)
                REQUIRE(NativeOperations_::ReadAdjoint(input, channel) == 1.0, "Identity PDE states must retain each seed");
        for (const auto& input : inputs.externalValues_)
            for (size_t channel = 0; channel < channels; ++channel)
                REQUIRE(NativeOperations_::ReadAdjoint(input, channel) == 0.0, "Unused external PDE bindings must have zero risk");
    }

    Resources_
    MeasureResources(int n, int layers, double theta, size_t width, size_t extraTape = 1024 * 1024, BufferCapacityBudget_* callerBudget = nullptr) {
        Clear(*Tape());
        {
            RecordingScope_ recovery;
            recovery.Close();
        }
        const size_t channels = std::max(size_t(1), width);
        auto mode = SetNumResultsForAAD(width != 0, channels);
        const size_t initial = MeasureTape(*Tape()).capacityBytes_;
        TapeCapacityBudget_ tapeBudget(initial + TapeCleanupCapacityBytes() + extraTape);
        TapeCapacityScope_ tapeCapacity(&tapeBudget, true);
        RecordingScope_ scope;
        auto numeric = DalTest::NativePDE::IdentityInputs(n, layers, theta);
        numeric.externalValues_ = Matrix_<>(2, layers, 1.0);
        const auto active = DalTest::NativePDE::RegisterBindings(&scope, numeric);
        scope.StartRecording();
        const size_t before = Tape()->nodes_.OccupiedSlots();
        BufferCapacityBudget_ fallback(1024 * 1024);
        BufferCapacityScope_ caller(callerBudget == nullptr ? &fallback : callerBudget);
        auto step = SampledThetaStepWithAccuracy(&scope, numeric, active, LinearSolveAccuracyPolicy_{0.0, 0.0});
        const size_t nodes = Tape()->nodes_.OccupiedSlots() - before;
        scope.FinishRecording();
        scope.ClearAdjoints();
        SeedOutputs(&step.solution_, channels);
        const auto reports = ReverseWithSolveAccuracy(&scope);
        CheckResourceRisks(active, layers, channels);
        const auto& report = reports.Report(step.event_);
        REQUIRE(report.transposeBackwardErrors_.Rows() == layers && report.transposeBackwardErrors_.Cols() == static_cast<int>(channels),
                "PDE resource reports must separate layers and channels");
        for (double error : report.transposeBackwardErrors_)
            REQUIRE(error == 0.0, "Identity PDE transpose residual must be exact");
        const auto tape = MeasureTape(*Tape());
        const auto& budget = callerBudget == nullptr ? fallback : *callerBudget;
        const Resources_ result{tape.reverseEventCapacityBytes_, tape.reverseScratchPeakBytes_, tapeBudget.PeakCapacityBytes() - initial,
                                budget.CapacityBytes(),          budget.PeakCapacityBytes(),    nodes};
        scope.Close();
        return result;
    }
} // namespace

TEST(AADSampledThetaStepTest, TestLinearEventCacheAndScratchCapacityAxes) {
    const auto baseline = MeasureResources(3, 1, 0.0, 0);
    const size_t fixed = baseline.retained_ - DynamicEventBytes(3, 1, 0.0);
    for (int n : {3, 5})
        for (int layers : {1, 2})
            for (double theta : {0.0, 0.5})
                for (size_t width : {0U, 1U, 4U, 8U}) {
                    SCOPED_TRACE(n);
                    SCOPED_TRACE(layers);
                    SCOPED_TRACE(theta);
                    SCOPED_TRACE(width);
                    const auto actual = MeasureResources(n, layers, theta, width);
                    ASSERT_EQ(actual.retained_, fixed + DynamicEventBytes(n, layers, theta));
                    ASSERT_EQ(actual.scratch_, ScratchBytes(n, layers));
                    ASSERT_EQ(actual.nodes_, static_cast<size_t>(n * layers));
                    ASSERT_EQ(actual.callerPeak_, CallerPeak(n, layers, theta, std::max(size_t(1), width)));
                }
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    Clear(*Tape());
}

TEST(AADSampledThetaStepTest, TestExactCallerCapacityAndOneByteShortRefund) {
    for (double theta : {0.0, 0.5}) {
        const size_t peak = CallerPeak(3, 2, theta, 8);
        BufferCapacityBudget_ exact(peak);
        const auto accepted = MeasureResources(3, 2, theta, 8, 1024 * 1024, &exact);
        ASSERT_EQ(accepted.callerPeak_, peak);
        ASSERT_EQ(exact.CapacityBytes(), 0);
        BufferCapacityBudget_ shortBudget(peak - 1);
        ASSERT_THROW(static_cast<void>(MeasureResources(3, 2, theta, 8, 1024 * 1024, &shortBudget)), Exception_);
        ASSERT_EQ(shortBudget.CapacityBytes(), 0);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
        ASSERT_NO_THROW(static_cast<void>(MeasureResources(3, 2, theta, 8)));
    }
    Clear(*Tape());
}

TEST(AADSampledThetaStepTest, TestExactTapeCapacityAndOneByteShortRefund) {
    const auto measured = MeasureResources(3, 1, 0.5, 0);
    const auto accepted = MeasureResources(3, 1, 0.5, 0, measured.tapePeak_);
    ASSERT_EQ(accepted.tapePeak_, measured.tapePeak_);
    ASSERT_THROW(static_cast<void>(MeasureResources(3, 1, 0.5, 0, measured.tapePeak_ - 1)), Exception_);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    ASSERT_NO_THROW(static_cast<void>(MeasureResources(3, 1, 0.5, 0)));
    Clear(*Tape());
}
