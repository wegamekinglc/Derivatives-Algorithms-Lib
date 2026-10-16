//
// Created by Codex on 2026/10/16.
//

#include <gtest/gtest.h>

#include <future>
#include <limits>

#include <dal/math/aad/adjointblockroot.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/aad/weightedroot.hpp>
#include <dal/platform/platform.hpp>

using namespace Dal;
using namespace Dal::AAD;

TEST(PassiveSinkTest, TestConstructorPoolingAndExplicitDistinctInputs) {
    const auto mode = SetNumResultsForAAD();
    RecordingScope_ scope;
    Number_ x, y;
    scope.RegisterInput(x, 2.0);
    scope.RegisterInput(y, 2.0);
    ASSERT_NE(&Adjoint(x), &Adjoint(y));
    const auto before = MeasureTape(*Tape()).nodes_;
    Number_ a(3.0), b(7.0);
#if defined(DAL_EXPERIMENTAL_PASSIVE_SINK)
    ASSERT_EQ(MeasureTape(*Tape()).nodes_, before + 1);
    ASSERT_THROW(Adjoint(a), Exception_);
    ASSERT_THROW(NativeOperations_::ReadAdjoint(b), Exception_);
    ASSERT_THROW(NativeOperations_::SetSeed(a, 1.0), Exception_);
#else
    ASSERT_EQ(MeasureTape(*Tape()).nodes_, before + 2);
    ASSERT_NE(&Adjoint(a), &Adjoint(b));
#endif
    scope.StartRecording();
    Number_ output = x * a + y * b;
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(output, 1.0);
    scope.Reverse();
    ASSERT_DOUBLE_EQ(Value(output), 20.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x), 3.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(y), 7.0);
    PutOnTape(a);
    ASSERT_DOUBLE_EQ(Value(a), 3.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(a), 0.0);
}

TEST(PassiveSinkTest, TestScalarAndVectorWeightedFreshRecordings) {
    for (const size_t width : {size_t{1}, size_t{4}}) {
        const auto mode = SetNumResultsForAAD(width > 1, width);
        for (int round = 0; round < 3; ++round) {
            RecordingScope_ scope;
            Number_ x, y;
            scope.RegisterInput(x, 2.0);
            scope.RegisterInput(y, 3.0);
            scope.StartRecording();
            Number_ a(5.0), b(7.0);
            Number_ first = x * a + y;
            Number_ second = x + y * b;
            Number_ output = WeightedPayoffRoot({first, second}, {2.0, -3.0});
            scope.FinishRecording();
            scope.ClearAdjoints();
            for (size_t lane = 0; lane < width; ++lane)
                NativeOperations_::SetSeed(output, lane + 1.0, lane);
            scope.Reverse();
            ASSERT_DOUBLE_EQ(Value(output), -43.0);
            for (size_t lane = 0; lane < width; ++lane) {
                ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, lane), 7.0 * (lane + 1));
                ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(y, lane), -19.0 * (lane + 1));
            }
            scope.ClearAdjoints();
            for (size_t lane = 0; lane < width; ++lane)
                ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, lane), 0.0);
        }
    }
}

TEST(PassiveSinkTest, TestPrefixAndSuffixSinkRestoration) {
    for (const bool constantBeforeMark : {false, true}) {
        for (const size_t width : {size_t{1}, size_t{4}}) {
            const auto mode = SetNumResultsForAAD(width > 1, width);
            RecordingScope_ scope;
            Number_ x;
            scope.RegisterInput(x, 2.0);
            scope.StartRecording();
            Number_ prefix;
            if (constantBeforeMark)
                prefix = 4.0;
            const auto checkpoint = scope.MakeCheckpoint();
            for (int path = 0; path < 3; ++path) {
                scope.Restore(checkpoint);
                Number_ factor(3.0);
                Number_ output = x * factor + (constantBeforeMark ? prefix : factor);
                scope.FinishRecording();
                for (size_t lane = 0; lane < width; ++lane)
                    NativeOperations_::SetSeed(output, lane + 1.0, lane);
                scope.ReverseSuffix(checkpoint);
            }
            scope.ReversePrefix(checkpoint);
            for (size_t lane = 0; lane < width; ++lane)
                ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, lane), 9.0 * (lane + 1));
        }
    }
}

TEST(PassiveSinkTest, TestPassiveAndAliasedBlockOutputsHaveSeparateRoots) {
    const auto mode = SetNumResultsForAAD(true, 4);
    RecordingScope_ scope;
    Number_ x, zero;
    scope.RegisterInput(x, 2.0);
    scope.RegisterInput(zero, 0.0);
    scope.StartRecording();
    const auto checkpoint = scope.MakeCheckpoint();
    Vector_<Number_> roots;
    roots.reserve(4);
    for (int path = 0; path < 3; ++path) {
        scope.Restore(checkpoint);
        Number_ constant(5.0);
        Number_ output = 3.0 * x;
        const Vector_<Number_> outputs{constant, output, output};
        SeedAdjointBlock(outputs, {0, 3, 4}, zero, &roots);
        scope.FinishRecording();
        scope.ReverseSuffix(checkpoint);
    }
    scope.ReversePrefix(checkpoint);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, 0), 0.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, 1), 9.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, 2), 9.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, 3), 0.0);
}

TEST(PassiveSinkTest, TestOwnerRejectionAndExceptionRecovery) {
    const auto mode = SetNumResultsForAAD();
    {
        RecordingScope_ scope;
        auto rejected = std::async(std::launch::async, [&scope] {
            try {
                scope.StartRecording();
                return false;
            } catch (const Exception_&) {
                return true;
            }
        });
        ASSERT_TRUE(rejected.get());
    }
    ASSERT_THROW(([] {
                     RecordingScope_ scope;
                     Number_ constant(5.0);
                     scope.StartRecording();
                     THROW("business failure");
                 }()),
                 Exception_);
    RecordingScope_ recovered;
    Number_ x;
    recovered.RegisterInput(x, 2.0);
    recovered.StartRecording();
    Number_ output = x * Number_(3.0);
    recovered.FinishRecording();
    NativeOperations_::SetSeed(output, 1.0);
    recovered.Reverse();
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x), 3.0);
}

#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
namespace Dal::AAD {
    struct NativeLifetimeTestAccess_ {
        static std::uint64_t Epoch(const Tape_& tape) { return tape.lifetimeEpoch_; }
        static void SetEpoch(Tape_* tape, std::uint64_t epoch) { tape->lifetimeEpoch_ = epoch; }
    };
} // namespace Dal::AAD

TEST(PassiveSinkTest, TestRejectedFullResetKeepsPassiveBinding) {
    Clear(*Tape());
    Number_ constant(2.0);
    const auto epoch = NativeLifetimeTestAccess_::Epoch(*Tape());
    NativeLifetimeTestAccess_::SetEpoch(Tape(), std::numeric_limits<std::uint64_t>::max());
    ASSERT_THROW(Rewind(*Tape()), Exception_);
    NativeLifetimeTestAccess_::SetEpoch(Tape(), epoch);
#if defined(DAL_EXPERIMENTAL_PASSIVE_SINK)
    ASSERT_THROW(Adjoint(constant), Exception_);
#else
    ASSERT_DOUBLE_EQ(Adjoint(constant), 0.0);
#endif
    Clear(*Tape());
}

TEST(PassiveSinkTest, TestStaleConstantsCannotSurviveRestoreClearOrThreadExit) {
    const auto mode = SetNumResultsForAAD();
    Number_ stale;
    {
        RecordingScope_ scope;
        Number_ x;
        scope.RegisterInput(x, 2.0);
        scope.StartRecording();
        const auto checkpoint = scope.MakeCheckpoint();
        stale = 3.0;
        scope.Restore(checkpoint);
        Number_ fresh(4.0);
        ASSERT_THROW((Number_(x * stale)), Exception_);
        ASSERT_DOUBLE_EQ(Value(fresh), 4.0);
    }
    Clear(*Tape());
    ASSERT_THROW((Number_(stale + 1.0)), Exception_);
    Number_ foreign;
    std::thread worker([&foreign] {
        const auto mode = SetNumResultsForAAD();
        RecordingScope_ scope;
        foreign = 7.0;
    });
    worker.join();
    ASSERT_THROW((Number_(foreign + 1.0)), Exception_);
    RegisterIndependent(stale, Value(stale));
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(stale), 0.0);
}
#endif
