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

TEST(AADLinearSolveTest, TestCallerContainersCanBeReassignedAndDestroyed) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 2.0);
    scope.StartRecording();
    Matrix_<Number_> solution;
    {
        SquareMatrix_<Number_> matrix(1);
        Matrix_<Number_> rhs(1, 1);
        matrix(0, 0) = input;
        rhs(0, 0) = 3.0;
        solution = LinearSolve(&scope, matrix, rhs);
        matrix(0, 0) = 99.0;
        rhs(0, 0) = -99.0;
    }
    Number_ objective = solution(0, 0) * solution(0, 0);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    scope.Reverse();
    ASSERT_NEAR(Value(solution(0, 0)), 1.5, 1e-10);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(input), -2.25, 1e-10);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestEventAtExactBlockBoundaryAndEmptySuffix) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 2.0);
    scope.StartRecording();
    SquareMatrix_<Number_> matrix(1);
    Matrix_<Number_> rhs(1, 1);
    matrix(0, 0) = input;
    rhs(0, 0) = 3.0;
    while (Tape()->nodes_.OccupiedSlots() < BLOCK_SIZE - 1) {
        Number_ padding(0.0);
    }
    auto solution = LinearSolve(&scope, matrix, rhs);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), BLOCK_SIZE);
    ASSERT_EQ(Tape()->nodes_.AllocatedBlocks(), 2);
    const auto checkpoint = scope.MakeCheckpoint();
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(solution(0, 0), 1.0);
    scope.ReverseSuffix(checkpoint);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(solution(0, 0)), 1.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.0);
    scope.ReversePrefix(checkpoint);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(input), -0.75, 1e-10);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(solution(0, 0)), 0.0);
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(solution(0, 0), -2.0);
    scope.Reverse();
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(input), 1.5, 1e-10);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestOutputSlotsCrossMultipleBlocks) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 2.0);
    scope.StartRecording();
    SquareMatrix_<Number_> matrix(1);
    matrix(0, 0) = input;
    const int columns = static_cast<int>(BLOCK_SIZE + 3);
    Matrix_<Number_> rhs(1, columns);
    for (int column = 0; column < columns; ++column)
        rhs(0, column) = 3.0;
    auto solution = LinearSolve(&scope, matrix, rhs);
    Number_ objective = solution(0, 0) + 2.0 * solution(0, columns - 1);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    scope.Reverse();
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(input), -2.25, 1e-10);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(rhs(0, 0)), 0.5, 1e-10);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(rhs(0, columns - 1)), 1.0, 1e-10);
    for (int column = 0; column < columns; ++column) {
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(solution(0, column)), 0.0);
        if (column != 0 && column != columns - 1)
            ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(rhs(0, column)), 0.0);
    }
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestMissingInputInvalidatesBeforeOutputPublication) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 2.0);
    scope.StartRecording();
    SquareMatrix_<Number_> matrix(1);
    Matrix_<Number_> rhs(1, 1);
    matrix(0, 0) = input;
    const auto nodesBefore = Tape()->nodes_.OccupiedSlots();
    ASSERT_THROW(static_cast<void>(LinearSolve(&scope, matrix, rhs)), Exception_);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodesBefore);
    ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Exception_);
    ASSERT_THROW(NativeOperations_::ClearSeeds(input), Exception_);
    ASSERT_THROW(scope.FinishRecording(), Exception_);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestWrongScopeStateRejectsWithoutDiscardingValidRecording) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 2.0);
    SquareMatrix_<Number_> matrix(1);
    Matrix_<Number_> rhs(1, 1);
    ASSERT_THROW(static_cast<void>(LinearSolve(nullptr, matrix, rhs)), Exception_);
    ASSERT_THROW(static_cast<void>(LinearSolve(&scope, matrix, rhs)), Exception_);
    scope.StartRecording();
    matrix(0, 0) = input;
    rhs(0, 0) = 3.0;
    auto solution = LinearSolve(&scope, matrix, rhs);
    scope.FinishRecording();
    ASSERT_THROW(static_cast<void>(LinearSolve(&scope, matrix, rhs)), Exception_);
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(solution(0, 0), 1.0);
    scope.Reverse();
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(input), -0.75, 1e-10);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestInsufficientCapacityRejectsBeforeOutputPublication) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    TapeCapacityBudget_ budget(initial + TapeCleanupCapacityBytes());
    {
        TapeCapacityScope_ capacity(&budget, true);
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, 2.0);
        scope.StartRecording();
        SquareMatrix_<Number_> matrix(1);
        Matrix_<Number_> rhs(1, 1);
        matrix(0, 0) = input;
        rhs(0, 0) = 3.0;
        const auto nodesBefore = Tape()->nodes_.OccupiedSlots();
        ASSERT_THROW(static_cast<void>(LinearSolve(&scope, matrix, rhs)), Exception_);
        ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodesBefore);
        ASSERT_EQ(budget.CapacityBytes(), initial);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Exception_);
        scope.Close();
        ASSERT_EQ(budget.CapacityBytes(), initial);
    }
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestTinyNonzeroSeedIsNotDiscarded) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 3.0);
    scope.StartRecording();
    SquareMatrix_<> matrix(1);
    matrix(0, 0) = 2.0;
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto solution = LinearSolve(&scope, matrix, rhs);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(solution(0, 0), 1e-40);
    scope.Reverse();
    ASSERT_GT(NativeOperations_::ReadAdjoint(input), 0.0);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(input), 5e-41, 1e-52);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(solution(0, 0)), 0.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestOtherTapeResetDoesNotExposeFailedDefaultGraphAdjoints) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 2.0);
    scope.StartRecording();
    SquareMatrix_<Number_> matrix(1);
    Matrix_<Number_> rhs(1, 1);
    matrix(0, 0) = input;
    ASSERT_THROW(static_cast<void>(LinearSolve(&scope, matrix, rhs)), Exception_);
    Tape_ other;
    Clear(other);
    Rewind(other);
    ASSERT_THROW(static_cast<void>(Adjoint(input)), Exception_);
    scope.Close();
    Number_ recovered(3.0);
    Number_ objective = recovered * recovered;
    Adjoint(objective) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(Adjoint(recovered), 6.0);
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRawAdjointRetainsEventModeValidation) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 3.0);
    scope.StartRecording();
    SquareMatrix_<> matrix(1);
    matrix(0, 0) = 2.0;
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto solution = LinearSolve(&scope, matrix, rhs);
    scope.FinishRecording();
    const auto width = Tape()->numAdj_;
    Tape()->numAdj_ = 2;
    ASSERT_THROW(static_cast<void>(Adjoint(solution(0, 0))), Exception_);
    Tape()->numAdj_ = width;
    scope.ClearAdjoints();
    Adjoint(solution(0, 0)) = 1.0;
    scope.Reverse();
    ASSERT_DOUBLE_EQ(Adjoint(input), 0.5);
    scope.Close();
    Clear(*Tape());
}
