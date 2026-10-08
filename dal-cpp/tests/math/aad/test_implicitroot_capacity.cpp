//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <algorithm>

#include <dal/math/aad/implicitroot.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/platform/platform.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    class ResourceRootEquation_ final : public ImplicitRootEquation_ {
    public:
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& theta, const Vector_<>& inputs) const override {
            const int n = static_cast<int>(theta.size());
            ImplicitRootEvaluation_ result{Vector_<>(n), SquareMatrix_<>(n), Matrix_<>(n, n)};
            for (int row = 0; row < n; ++row) {
                result.residuals_[row] = theta[row] - inputs[row];
                result.parameterJacobian_(row, row) = 1.0;
                result.inputJacobian_(row, row) = -1.0;
            }
            return result;
        }
    };

    struct RootResources_ {
        size_t retained_, scratch_, tapePeak_, caller_, callerPeak_, publishedNodes_;
    };

    size_t CallerCapturePeak(size_t n) {
        const size_t numeric = (6 * n * n + 6 * n + 1) * sizeof(double) + n * sizeof(int);
        const size_t retained = (3 * n * n + 5 * n + 1) * sizeof(double) + n * sizeof(int);
        const size_t publication = retained + 2 * n * (sizeof(Number_) + sizeof(double));
        return std::max(numeric + n * (sizeof(Number_) + sizeof(double)), publication);
    }

    void SeedResourceRoots(Vector_<Number_>* outputs, size_t channels) {
        for (auto& output : *outputs)
            for (size_t channel = 0; channel < channels; ++channel)
                NativeOperations_::SetSeed(output, 1.0, channel);
    }

    void VerifyResourceRisk(const Vector_<Number_>& inputs, const SolveAccuracyReport_& report, size_t channels) {
        for (const auto& input : inputs)
            for (size_t channel = 0; channel < channels; ++channel)
                REQUIRE(NativeOperations_::ReadAdjoint(input, channel) == 1.0, "Resource root input risk must equal one");
        REQUIRE(report.transposeBackwardErrors_.Rows() == 1 && report.transposeBackwardErrors_.Cols() == static_cast<int>(channels),
                "Resource root report must have one RHS and the requested channels");
        for (double error : report.transposeBackwardErrors_)
            REQUIRE(error == 0.0, "Identity root transpose must be exact");
    }

    RootResources_ RunRootResources(int n, size_t width, size_t extraTapeLimit = 1024 * 1024, BufferCapacityBudget_* callerBudget = nullptr) {
        Clear(*Tape());
        {
            RecordingScope_ recovery;
            recovery.Close();
        }
        const size_t channels = std::max(size_t(1), width);
        auto mode = SetNumResultsForAAD(width != 0, channels);
        const size_t initial = MeasureTape(*Tape()).capacityBytes_;
        TapeCapacityBudget_ tapeBudget(initial + TapeCleanupCapacityBytes() + extraTapeLimit);
        TapeCapacityScope_ tapeCapacity(&tapeBudget, true);
        RecordingScope_ scope;
        Vector_<Number_> inputs(n);
        for (auto& input : inputs)
            scope.RegisterInput(input, 1.0);
        const Vector_<> candidate(n, 1.0);
        const ImplicitRootAccuracyPolicy_ policy{Vector_<>(n, 0.0), 0.0};
        const ResourceRootEquation_ equation;
        scope.StartRecording();
        const size_t beforeNodes = Tape()->nodes_.OccupiedSlots();
        BufferCapacityBudget_ fallback(1024 * 1024);
        BufferCapacityScope_ caller(callerBudget == nullptr ? &fallback : callerBudget);
        auto root = ImplicitRootWithAccuracy(&scope, equation, candidate, inputs, policy);
        const size_t published = Tape()->nodes_.OccupiedSlots() - beforeNodes;
        scope.FinishRecording();
        scope.ClearAdjoints();
        SeedResourceRoots(&root.parameters_, channels);
        const auto reports = ReverseWithSolveAccuracy(&scope);
        VerifyResourceRisk(inputs, reports.Report(root.event_), channels);
        const auto tape = MeasureTape(*Tape());
        const auto& budget = callerBudget == nullptr ? fallback : *callerBudget;
        const RootResources_ result{tape.reverseEventCapacityBytes_, tape.reverseScratchPeakBytes_, tapeBudget.PeakCapacityBytes() - initial,
                                    budget.CapacityBytes(),          budget.PeakCapacityBytes(),    published};
        scope.Close();
        return result;
    }
} // namespace

TEST(AADLinearSolveTest, TestRecordedImplicitRootResourceAxesAndStorageDifferences) {
    const auto scalar = RunRootResources(1, 0);
    const auto coupled = RunRootResources(2, 0);
    ASSERT_EQ(coupled.retained_ - scalar.retained_, 116 + 2 * sizeof(Number_));
    ASSERT_EQ(scalar.scratch_, 6 * sizeof(double));
    ASSERT_EQ(coupled.scratch_, 10 * sizeof(double));
    ASSERT_EQ(scalar.publishedNodes_, 1);
    ASSERT_EQ(coupled.publishedNodes_, 2);
    for (size_t width : {1U, 4U, 8U}) {
        const auto multi = RunRootResources(2, width);
        ASSERT_EQ(multi.retained_, coupled.retained_);
        ASSERT_EQ(multi.scratch_, coupled.scratch_);
        ASSERT_EQ(multi.caller_, 2 * (sizeof(Number_) + 2 * sizeof(double)) + sizeof(SolveAccuracyReport_) + width * sizeof(double));
        ASSERT_EQ(multi.callerPeak_, std::max(multi.caller_ + multi.scratch_, CallerCapturePeak(2)));
        ASSERT_EQ(multi.publishedNodes_, 2);
    }
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootExactCallerCapacityAndOneByteShortRefund) {
    for (size_t n : {1U, 2U}) {
        const size_t retained = n * (sizeof(Number_) + 2 * sizeof(double)) + sizeof(SolveAccuracyReport_) + 4 * sizeof(double);
        const size_t expected = std::max(retained + (4 * n + 2) * sizeof(double), CallerCapturePeak(n));
        BufferCapacityBudget_ exact(expected);
        const auto accepted = RunRootResources(static_cast<int>(n), 4, 1024 * 1024, &exact);
        ASSERT_EQ(accepted.caller_, retained);
        ASSERT_EQ(exact.CapacityBytes(), 0);
        ASSERT_EQ(exact.PeakCapacityBytes(), expected);
        BufferCapacityBudget_ shortBudget(expected - 1);
        ASSERT_THROW(static_cast<void>(RunRootResources(static_cast<int>(n), 4, 1024 * 1024, &shortBudget)), Exception_);
        ASSERT_EQ(shortBudget.CapacityBytes(), 0);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
        ASSERT_NO_THROW(static_cast<void>(RunRootResources(static_cast<int>(n), 4)));
    }
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootExactTapeCapacityAndOneByteShortRefund) {
    const auto observed = RunRootResources(1, 0);
    ASSERT_GT(observed.tapePeak_, 0);
    const size_t extra = observed.tapePeak_;
    const auto exact = RunRootResources(1, 0, extra);
    ASSERT_EQ(exact.tapePeak_, observed.tapePeak_);
    ASSERT_THROW(static_cast<void>(RunRootResources(1, 0, extra - 1)), Exception_);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    ASSERT_NO_THROW(static_cast<void>(RunRootResources(1, 0)));
    Clear(*Tape());
}
