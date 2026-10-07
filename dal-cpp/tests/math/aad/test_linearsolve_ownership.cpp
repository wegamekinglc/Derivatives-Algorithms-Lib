//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <future>

#include <dal/math/aad/linearsolve.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/statistics.hpp>

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
