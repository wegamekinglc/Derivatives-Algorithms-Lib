//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal/math/aad/linearsolve.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/math/buffercapacity.hpp>

using namespace Dal;
using namespace Dal::AAD;

TEST(AADLinearSolveTest, TestFiniteCapacityIncludesCacheAndResetReturnsToBaseline) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    TapeCapacityBudget_ budget(initial + TapeCleanupCapacityBytes() + 1024 * 1024);
    {
        TapeCapacityScope_ capacity(&budget, true);
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, 3.0);
        scope.StartRecording();
        SquareMatrix_<> matrix(1);
        matrix(0, 0) = 2.0;
        Matrix_<Number_> rhs(1, 1);
        rhs(0, 0) = input;
        auto solution = LinearSolve(&scope, matrix, rhs);
        const auto retained = MeasureTape(*Tape()).capacityBytes_;
        ASSERT_GT(retained, initial);
        ASSERT_EQ(budget.CapacityBytes(), retained);
        scope.FinishRecording();
        scope.ClearAdjoints();
        ASSERT_EQ(MeasureTape(*Tape()).capacityBytes_, retained);
        NativeOperations_::SetSeed(solution(0, 0), 1.0);
        scope.Reverse();
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.5);
        ASSERT_EQ(MeasureTape(*Tape()).capacityBytes_, retained);
        ASSERT_EQ(budget.CapacityBytes(), retained);
        scope.Close();
        ASSERT_EQ(MeasureTape(*Tape()).capacityBytes_, initial);
        ASSERT_EQ(budget.CapacityBytes(), initial);
    }
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCallerBufferBudgetExcludesCacheAndIncludesReverseScratch) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    TapeCapacityBudget_ tapeBudget(initial + TapeCleanupCapacityBytes() + 1024 * 1024);
    BufferCapacityBudget_ buffers(sizeof(Number_) + 2 * sizeof(double));
    TapeCapacityScope_ capacity(&tapeBudget, true);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 3.0);
    scope.StartRecording();
    SquareMatrix_<> matrix(1);
    matrix(0, 0) = 2.0;
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    {
        BufferCapacityScope_ caller(&buffers);
        {
            auto solution = LinearSolve(&scope, matrix, rhs);
            const auto retained = MeasureTape(*Tape()).capacityBytes_;
            ASSERT_EQ(buffers.CapacityBytes(), sizeof(Number_));
            ASSERT_EQ(tapeBudget.CapacityBytes(), retained);
            scope.FinishRecording();
            scope.ClearAdjoints();
            NativeOperations_::SetSeed(solution(0, 0), 1.0);
            scope.Reverse();
            ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.5);
            ASSERT_EQ(buffers.CapacityBytes(), sizeof(Number_));
            ASSERT_EQ(buffers.PeakCapacityBytes(), sizeof(Number_) + 2 * sizeof(double));
            ASSERT_EQ(tapeBudget.CapacityBytes(), retained);
            ASSERT_EQ(MeasureTape(*Tape()).reverseScratchPeakBytes_, 2 * sizeof(double));
            scope.Close();
            ASSERT_EQ(tapeBudget.CapacityBytes(), initial);
            ASSERT_EQ(buffers.CapacityBytes(), sizeof(Number_));
        }
        ASSERT_EQ(buffers.CapacityBytes(), 0);
    }
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestReverseBufferRejectionRefundsScratchAndHidesPartialGradients) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    TapeCapacityBudget_ tapeBudget(initial + TapeCleanupCapacityBytes() + 1024 * 1024);
    BufferCapacityBudget_ buffers(sizeof(Number_) + sizeof(double));
    TapeCapacityScope_ capacity(&tapeBudget, true);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 3.0);
    scope.StartRecording();
    SquareMatrix_<> matrix(1);
    matrix(0, 0) = 2.0;
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    {
        BufferCapacityScope_ caller(&buffers);
        {
            auto solution = LinearSolve(&scope, matrix, rhs);
            const auto retained = MeasureTape(*Tape()).capacityBytes_;
            scope.FinishRecording();
            scope.ClearAdjoints();
            NativeOperations_::SetSeed(solution(0, 0), 1.0);
            ASSERT_THROW(scope.Reverse(), Exception_);
            ASSERT_EQ(tapeBudget.CapacityBytes(), retained);
            ASSERT_EQ(buffers.CapacityBytes(), sizeof(Number_));
            ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Exception_);
            ASSERT_THROW(scope.Reverse(), Exception_);
            scope.Close();
            ASSERT_EQ(tapeBudget.CapacityBytes(), initial);
        }
        ASSERT_EQ(buffers.CapacityBytes(), 0);
    }
    RecordingScope_ recovered;
    recovered.RegisterInput(input, 3.0);
    recovered.StartRecording();
    rhs(0, 0) = input;
    auto solution = LinearSolve(&recovered, matrix, rhs);
    recovered.FinishRecording();
    NativeOperations_::SetSeed(solution(0, 0), 1.0);
    recovered.Reverse();
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.5);
    recovered.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestFiniteCapacityVectorChannelsReuseBoundedScratch) {
    for (size_t width : {1U, 2U, 3U, 4U, 8U}) {
        SCOPED_TRACE(width);
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(true, width);
        const auto initial = MeasureTape(*Tape()).capacityBytes_;
        TapeCapacityBudget_ budget(initial + TapeCleanupCapacityBytes() + 1024 * 1024);
        TapeCapacityScope_ capacity(&budget, true);
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, 3.0);
        scope.StartRecording();
        SquareMatrix_<> matrix(1);
        matrix(0, 0) = 2.0;
        Matrix_<Number_> rhs(1, 1);
        rhs(0, 0) = input;
        auto solution = LinearSolve(&scope, matrix, rhs);
        const auto retained = MeasureTape(*Tape()).capacityBytes_;
        scope.FinishRecording();
        scope.ClearAdjoints();
        for (size_t channel = 0; channel < width; ++channel)
            NativeOperations_::SetSeed(solution(0, 0), static_cast<double>(channel + 1), channel);
        scope.Reverse();
        for (size_t channel = 0; channel < width; ++channel)
            ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input, channel), 0.5 * static_cast<double>(channel + 1));
        ASSERT_EQ(MeasureTape(*Tape()).reverseScratchPeakBytes_, 2 * sizeof(double));
        ASSERT_EQ(budget.CapacityBytes(), retained);
        scope.Close();
        ASSERT_EQ(budget.CapacityBytes(), initial);
        Clear(*Tape());
    }
}

TEST(AADLinearSolveTest, TestNumericConstructionRejectionRefundsAllOwnedStorage) {
    for (double value : {0.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        SCOPED_TRACE(value);
        Clear(*Tape());
        const auto initial = MeasureTape(*Tape()).capacityBytes_;
        TapeCapacityBudget_ budget(initial + TapeCleanupCapacityBytes() + 1024 * 1024);
        TapeCapacityScope_ capacity(&budget, true);
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, 3.0);
        scope.StartRecording();
        SquareMatrix_<> matrix(1);
        matrix(0, 0) = value;
        Matrix_<Number_> rhs(1, 1);
        rhs(0, 0) = input;
        const auto nodesBefore = Tape()->nodes_.OccupiedSlots();
        ASSERT_THROW(static_cast<void>(LinearSolve(&scope, matrix, rhs)), Exception_);
        ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodesBefore);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
        ASSERT_EQ(budget.CapacityBytes(), initial);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Exception_);
        scope.Close();
        Clear(*Tape());
    }
}
