//
// Created on 2026/10/04.
//

#include <dal/curve/tapeguard.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>
#include <future>
#include <gtest/gtest.h>
#include <memory>

using namespace Dal;
using namespace Dal::AAD;

namespace Dal::AAD {
    struct RecordingScopeTestAccess_ {
        static std::unique_ptr<RecordingScope_> Create(void (*rewind)(Tape_&), void (*clear)(Tape_&)) {
            return std::unique_ptr<RecordingScope_>(new RecordingScope_(Tape(), rewind, clear));
        }
    };
} // namespace Dal::AAD

namespace {
    thread_local int rewindCalls = 0;

    void FailClose(Tape_& tape) {
        if (++rewindCalls > 1)
            THROW("controlled recording cleanup failure");
        Rewind(tape);
    }

    void FailClear(Tape_&) { THROW("controlled recording rebuild failure"); }

    void RejectNestedDuringReset(Tape_& tape) {
        ASSERT_THROW(RecordingScope_ nested, Exception_);
        Rewind(tape);
    }

    double IndependentSquareGradient(double value) {
        RecordingScope_ scope;
        Number_ input;
        RegisterIndependent(input, value);
        NewRecording(*Tape());
        Number_ output = input * input;
        ZeroAdjoints(*Tape());
        Adjoint(output) = 1.0;
        PropagateToStart(*Tape());
        const double result = AdjointValue(input);
        scope.Close();
        return result;
    }
} // namespace

TEST(AADRecordingTest, TestNestedLegacyGuardRejectsWithoutDiscardingOuterGraph) {
    TapeGuard_ outer(Tape());
    Number_ input;
    RegisterIndependent(input, 3.0);
    NewRecording(*Tape());
    Number_ output = input * input + 2.0;
    ASSERT_THROW(TapeGuard_ inner(Tape()), Exception_);
    ZeroAdjoints(*Tape());
    Adjoint(output) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(Value(output), 11.0);
    ASSERT_DOUBLE_EQ(AdjointValue(input), 6.0);
}

TEST(AADRecordingTest, TestBusinessExceptionReleasesOwnershipForNextRecording) {
    const auto fail = [] {
        TapeGuard_ guard(Tape());
        Number_ input;
        RegisterIndependent(input, 3.0);
        NewRecording(*Tape());
        const Number_ output = input * input;
        ASSERT_DOUBLE_EQ(Value(output), 9.0);
        THROW("recording business failure");
    };
    ASSERT_THROW(fail(), Exception_);
    TapeGuard_ next(Tape());
    Number_ input;
    RegisterIndependent(input, 4.0);
    NewRecording(*Tape());
    Number_ output = input * input;
    ZeroAdjoints(*Tape());
    Adjoint(output) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(AdjointValue(input), 8.0);
}

TEST(AADRecordingTest, TestExplicitCloseFailureRejectsFailedRebuildAndRecovers) {
    rewindCalls = 0;
    auto failed = RecordingScopeTestAccess_::Create(FailClose, Clear);
    ASSERT_THROW(failed->Close(), Exception_);
    ASSERT_TRUE(LastRecordingCleanupFailure());
    ASSERT_THROW(RecordingScopeTestAccess_::Create(Rewind, FailClear), Exception_);
    ASSERT_TRUE(LastRecordingCleanupFailure());
    ASSERT_DOUBLE_EQ(IndependentSquareGradient(4.0), 8.0);
    ASSERT_FALSE(LastRecordingCleanupFailure());
    failed.reset();
    ASSERT_DOUBLE_EQ(IndependentSquareGradient(5.0), 10.0);
}

TEST(AADRecordingTest, TestCleanupDuringUnwindPreservesBusinessException) {
    rewindCalls = 0;
    const auto fail = [] {
        auto scope = RecordingScopeTestAccess_::Create(FailClose, Clear);
        THROW("primary recording business failure");
    };
    try {
        fail();
        FAIL() << "business exception was suppressed";
    } catch (const Exception_& error) {
        ASSERT_NE(std::string(error.what()).find("primary recording business failure"), std::string::npos);
    }
    ASSERT_TRUE(LastRecordingCleanupFailure());
    ASSERT_DOUBLE_EQ(IndependentSquareGradient(6.0), 12.0);
    ASSERT_FALSE(LastRecordingCleanupFailure());
}

TEST(AADRecordingTest, TestCloseIsIdempotentAndCannotCloseANewerRecording) {
    RecordingScope_ old;
    old.Close();
    old.Close();
    RecordingScope_ next;
    Number_ input;
    RegisterIndependent(input, 7.0);
    NewRecording(*Tape());
    Number_ output = input * input;
    old.Close();
    ZeroAdjoints(*Tape());
    Adjoint(output) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(AdjointValue(input), 14.0);
    next.Close();
}

TEST(AADRecordingTest, TestForeignThreadCloseRejectsAndIndependentThreadsProceed) {
    RecordingScope_ scope;
    auto task = std::async(std::launch::async, [&] {
        ASSERT_THROW(scope.Close(), Exception_);
        ASSERT_DOUBLE_EQ(IndependentSquareGradient(8.0), 16.0);
    });
    task.get();
    scope.Close();
    ASSERT_DOUBLE_EQ(IndependentSquareGradient(9.0), 18.0);
}

TEST(AADRecordingTest, TestInvalidTapeRejectsBeforeChangingTheOwnerRecording) {
    RecordingScope_ scope;
    ASSERT_THROW(RecordingScope_ invalid(nullptr), Exception_);
    ASSERT_THROW(RecordingScope_ nested, Exception_);
    Number_ input;
    RegisterIndependent(input, 10.0);
    NewRecording(*Tape());
    Number_ output = input * input;
    ZeroAdjoints(*Tape());
    Adjoint(output) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(AdjointValue(input), 20.0);
}

TEST(AADRecordingTest, TestOwnershipIsClaimedBeforeResetOperations) {
    auto scope = RecordingScopeTestAccess_::Create(RejectNestedDuringReset, Clear);
    scope->Close();
    ASSERT_DOUBLE_EQ(IndependentSquareGradient(11.0), 22.0);
}

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
TEST(AADRecordingTest, TestNormalCloseRetainsNativeVectorCapacity) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(true, 5);
    RecordingScope_ scope;
    Number_ state(1.0);
    for (int i = 0; i < 50000; ++i)
        state = state * 1.0001 + 0.0001;
    const auto recorded = MeasureTape(*Tape());
    ASSERT_GT(recorded.nodes_, BLOCK_SIZE);
    ASSERT_GT(recorded.capacityBytes_, 0);
    scope.Close();
    const auto closed = MeasureTape(*Tape());
    ASSERT_EQ(closed.nodes_, 0);
    ASSERT_EQ(closed.liveBytes_, 0);
    ASSERT_EQ(closed.occupiedBytes_, 0);
    ASSERT_EQ(closed.capacityBytes_, recorded.capacityBytes_);
    Clear(*Tape());
}
#endif
