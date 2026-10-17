//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <future>

#include <dal/math/aad/linearsolve.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/reverseevent.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/aad/tapecapacity.hpp>

using namespace Dal;
using namespace Dal::AAD;

TEST(AADLinearSolveTest, TestWrongThreadRejectsBeforeTouchingTheOwnerRecording) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 3.0);
    scope.StartRecording();
    SquareMatrix_<> matrix(1);
    matrix(0, 0) = 2.0;
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    const auto nodesBefore = Tape()->nodes_.OccupiedSlots();
    auto rejected = std::async(std::launch::async, [&] {
        try {
            static_cast<void>(LinearSolve(&scope, matrix, rhs));
            return false;
        } catch (const Exception_&) {
            return true;
        }
    });
    ASSERT_TRUE(rejected.get());
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodesBefore);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    auto solution = LinearSolve(&scope, matrix, rhs);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(solution(0, 0), 1.0);
    scope.Reverse();
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.5);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestForeignLiveInputRejectsWithoutPublishingOutputs) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 3.0);
    scope.StartRecording();
    SquareMatrix_<Number_> matrix(1);
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    std::promise<void> ready, release;
    auto readyFuture = ready.get_future();
    auto releaseFuture = release.get_future();
    auto owner = std::async(std::launch::async, [&] {
        Number_ foreign(2.0);
        matrix(0, 0) = foreign;
        ready.set_value();
        releaseFuture.wait();
    });
    readyFuture.wait();
    const auto nodesBefore = Tape()->nodes_.OccupiedSlots();
    std::exception_ptr failure;
    try {
        static_cast<void>(LinearSolve(&scope, matrix, rhs));
    } catch (...) {
        failure = std::current_exception();
    }
    release.set_value();
    owner.get();
    ASSERT_TRUE(failure != nullptr);
    ASSERT_THROW(std::rethrow_exception(failure), Exception_);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodesBefore);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Exception_);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCapturedInputIndexCoversLiveBlocksAndRefundsTemporaryCapacity) {
    Clear(*Tape());
    const Number_ first(1.0);
    Mark(*Tape());
    Number_ middle, last;
    for (size_t index = 1; index < 2 * BLOCK_SIZE + 3; ++index) {
        Number_ input(static_cast<double>(index));
        if (index == BLOCK_SIZE)
            middle = input;
        if (index == 2 * BLOCK_SIZE + 2)
            last = input;
    }
    ASSERT_EQ(Tape()->nodes_.AllocatedBlocks(), 3);
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    TapeCapacityBudget_ budget(initial + TapeCleanupCapacityBytes() + 1024);
    {
        TapeCapacityScope_ capacity(&budget, true);
        const auto before = budget.CapacityBytes();
        {
            const NativeInputSlots_ slots(Tape());
            NativeRecordedOperation_::ValidateInput(slots, first);
            NativeRecordedOperation_::ValidateInput(slots, middle);
            NativeRecordedOperation_::ValidateInput(slots, last);
            ASSERT_GT(budget.CapacityBytes(), before);
            ASSERT_EQ(budget.CapacityBytes(), MeasureTape(*Tape()).capacityBytes_);
            ASSERT_THROW(NativeRecordedOperation_::ValidateInput(slots, Number_()), Exception_);
        }
        ASSERT_EQ(budget.CapacityBytes(), before);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
        RewindToMark(*Tape());
        {
            const NativeInputSlots_ slots(Tape());
            NativeRecordedOperation_::ValidateInput(slots, first);
            ASSERT_THROW(NativeRecordedOperation_::ValidateInput(slots, middle), Exception_);
            ASSERT_THROW(NativeRecordedOperation_::ValidateInput(slots, last), Exception_);
        }
        ASSERT_EQ(budget.CapacityBytes(), before);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    }
    TapeCapacityBudget_ shortBudget(initial + 2 * sizeof(const TapNode_*) - 1);
    {
        TapeCapacityScope_ capacity(&shortBudget);
        ASSERT_THROW(static_cast<void>(NativeInputSlots_(Tape())), Exception_);
        ASSERT_EQ(shortBudget.CapacityBytes(), initial);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    }
    Clear(*Tape());
}
