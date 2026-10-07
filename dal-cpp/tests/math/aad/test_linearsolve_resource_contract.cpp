//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <dal/math/aad/linearsolve.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/aad/tapecapacity.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    struct ResourceRun_ {
        size_t retained_;
        size_t scratch_;
        size_t peak_;
    };

    ResourceRun_ RunResourceSolve(bool matrixActive, bool rhsActive, size_t extraLimit = 1024 * 1024) {
        Clear(*Tape());
        const auto initial = MeasureTape(*Tape()).capacityBytes_;
        TapeCapacityBudget_ budget(initial + TapeCleanupCapacityBytes() + extraLimit);
        ResourceRun_ result{};
        TapeCapacityScope_ capacity(&budget, true);
        RecordingScope_ scope;
        SquareMatrix_<> matrix(2);
        Matrix_<> rhs(2, 2);
        SquareMatrix_<Number_> activeMatrix(2);
        Matrix_<Number_> activeRhs(2, 2);
        const double matrixValues[2][2] = {{2.0, 1.0}, {1.0, 3.0}};
        const double rhsValues[2][2] = {{4.0, 1.0}, {5.0, -1.0}};
        for (int row = 0; row < 2; ++row)
            for (int column = 0; column < 2; ++column) {
                matrix(row, column) = matrixValues[row][column];
                rhs(row, column) = rhsValues[row][column];
                scope.RegisterInput(activeMatrix(row, column), matrix(row, column));
                scope.RegisterInput(activeRhs(row, column), rhs(row, column));
            }
        scope.StartRecording();
        auto solution = matrixActive ? (rhsActive ? LinearSolve(&scope, activeMatrix, activeRhs) : LinearSolve(&scope, activeMatrix, rhs))
                                     : LinearSolve(&scope, matrix, activeRhs);
        result.retained_ = MeasureTape(*Tape()).reverseEventCapacityBytes_;
        scope.FinishRecording();
        scope.ClearAdjoints();
        for (int row = 0; row < 2; ++row)
            for (int column = 0; column < 2; ++column)
                NativeOperations_::SetSeed(solution(row, column), 1.0);
        scope.Reverse();
        result.scratch_ = MeasureTape(*Tape()).reverseScratchPeakBytes_;
        result.peak_ = budget.PeakCapacityBytes() - initial;
        scope.Close();
        return result;
    }
} // namespace

TEST(AADLinearSolveTest, TestPassiveInputsOmitBindingStorageAndMatrixReverseScratch) {
    const auto both = RunResourceSolve(true, true);
    const auto passiveMatrix = RunResourceSolve(false, true);
    const auto passiveRhs = RunResourceSolve(true, false);
    ASSERT_EQ(both.retained_ - passiveMatrix.retained_, 4 * sizeof(Number_));
    ASSERT_EQ(both.retained_ - passiveRhs.retained_, 4 * sizeof(Number_));
    ASSERT_EQ(both.scratch_, 12 * sizeof(double));
    ASSERT_EQ(passiveMatrix.scratch_, 8 * sizeof(double));
    ASSERT_EQ(passiveRhs.scratch_, both.scratch_);
    ASSERT_GT(both.peak_, both.retained_);
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestExactPeakBudgetSucceedsAndOneByteLessRejects) {
    const auto measured = RunResourceSolve(true, true);
    const auto exact = RunResourceSolve(true, true, measured.peak_);
    ASSERT_EQ(exact.retained_, measured.retained_);
    ASSERT_EQ(exact.peak_, measured.peak_);
    ASSERT_THROW(static_cast<void>(RunResourceSolve(true, true, measured.peak_ - 1)), Exception_);
    const auto recovered = RunResourceSolve(true, true, measured.peak_);
    ASSERT_EQ(recovered.retained_, measured.retained_);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEvents_, 0);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRestoreReleasesSuffixStorageAndRetainsTableCapacity) {
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
        auto prefix = LinearSolve(&scope, matrix, rhs);
        const auto checkpoint = scope.MakeCheckpoint();
        const auto prefixCapacity = MeasureTape(*Tape()).capacityBytes_;
        scope.ClearAdjoints();
        rhs(0, 0) = prefix(0, 0);
        auto suffix = LinearSolve(&scope, matrix, rhs);
        const auto bothCapacity = MeasureTape(*Tape()).capacityBytes_;
        ASSERT_EQ(MeasureTape(*Tape()).reverseEvents_, 2);
        scope.FinishRecording();
        NativeOperations_::SetSeed(suffix(0, 0), 1.0);
        scope.ReverseSuffix(checkpoint);
        scope.Restore(checkpoint);
        const auto restoredCapacity = MeasureTape(*Tape()).capacityBytes_;
        ASSERT_GE(restoredCapacity, prefixCapacity);
        ASSERT_LT(restoredCapacity, bothCapacity);
        ASSERT_EQ(budget.CapacityBytes(), restoredCapacity);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEvents_, 1);
        auto replacement = LinearSolve(&scope, matrix, rhs);
        ASSERT_EQ(MeasureTape(*Tape()).capacityBytes_, bothCapacity);
        scope.FinishRecording();
        NativeOperations_::SetSeed(replacement(0, 0), 1.0);
        scope.ReverseSuffix(checkpoint);
        scope.Restore(checkpoint);
        ASSERT_EQ(MeasureTape(*Tape()).capacityBytes_, restoredCapacity);
        scope.FinishRecording();
        scope.ReversePrefix(checkpoint);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.5);
        scope.Close();
        ASSERT_EQ(budget.CapacityBytes(), initial);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
        ASSERT_EQ(MeasureTape(*Tape()).reverseScratchPeakBytes_, 0);
    }
    Clear(*Tape());
}
