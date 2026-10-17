//
// Created by Codex on 2026/10/17.
//

#include <gtest/gtest.h>

#include <future>

#include <dal/math/aad/adjointblockroot.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/math/aad/weightedroot.hpp>
#include <dal/platform/platform.hpp>

using namespace Dal;
using namespace Dal::AAD;

TEST(PassiveConstantsTest, TestDefaultContractAndExplicitScopedInputs) {
#if !defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
    ASSERT_EQ(sizeof(Number_), 16U);
#endif
    Clear(*Tape());
    Number_ original(2.0), other(2.0);
    ASSERT_FALSE(original.IsPassive());
    ASSERT_NE(&Adjoint(original), &Adjoint(other));
    {
        RecordingScope_ scope;
        scope.EnablePassiveConstants();
        Number_ x, y;
        scope.RegisterInput(x, 2.0);
        scope.RegisterInput(y, 2.0);
        ASSERT_NE(&Adjoint(x), &Adjoint(y));
        const auto before = MeasureTape(*Tape()).nodes_;
        Number_ a(3.0), b(7.0);
        ASSERT_EQ(MeasureTape(*Tape()).nodes_, before + 1);
        ASSERT_TRUE(a.IsPassive());
        ASSERT_THROW(Adjoint(a), Exception_);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(b)), Exception_);
        ASSERT_THROW(NativeOperations_::SetSeed(a, 1.0), Exception_);
        scope.StartRecording();
        Number_ output = x * a + y * b;
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(output, 1.0);
        scope.Reverse();
        ASSERT_DOUBLE_EQ(Value(output), 20.0);
        ASSERT_DOUBLE_EQ(Adjoint(x), 3.0);
        ASSERT_DOUBLE_EQ(Adjoint(y), 7.0);
        PutOnTape(a);
        ASSERT_FALSE(a.IsPassive());
        ASSERT_DOUBLE_EQ(Adjoint(a), 0.0);
    }
    Number_ restored(4.0), separate(4.0);
    ASSERT_FALSE(restored.IsPassive());
    ASSERT_NE(&Adjoint(restored), &Adjoint(separate));
    Clear(*Tape());
}

TEST(PassiveConstantsTest, TestCheckpointsScalarVectorAndPassiveRoots) {
    for (const size_t width : {size_t{1}, size_t{4}}) {
        for (const bool prefixConstant : {false, true}) {
            const auto mode = SetNumResultsForAAD(width > 1, width);
            RecordingScope_ scope;
            scope.EnablePassiveConstants();
            Number_ x, zero;
            scope.RegisterInput(x, 2.0);
            scope.RegisterInput(zero, 0.0);
            scope.StartRecording();
            Number_ prefix;
            if (prefixConstant)
                prefix = 7.0;
            const auto checkpoint = scope.MakeCheckpoint();
            for (int path = 0; path < 3; ++path) {
                scope.Restore(checkpoint);
                Number_ constant(5.0);
                Number_ output = x * Number_(3.0);
                Number_ root = WeightedPayoffRoot({constant, output, prefixConstant ? prefix : constant}, {2.0, 4.0, -1.0});
                root = PayoffRoot(root, zero);
                scope.FinishRecording();
                for (size_t lane = 0; lane < width; ++lane)
                    NativeOperations_::SetSeed(root, lane + 1.0, lane);
                scope.ReverseSuffix(checkpoint);
            }
            scope.ReversePrefix(checkpoint);
            for (size_t lane = 0; lane < width; ++lane)
                ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, lane), 36.0 * (lane + 1));
        }
    }
}

TEST(PassiveConstantsTest, TestPassiveAndAliasedBlockRoots) {
    const auto mode = SetNumResultsForAAD(true, 4);
    RecordingScope_ scope;
    scope.EnablePassiveConstants();
    Number_ x, zero;
    scope.RegisterInput(x, 2.0);
    scope.RegisterInput(zero, 0.0);
    scope.StartRecording();
    const auto checkpoint = scope.MakeCheckpoint();
    Vector_<Number_> roots;
    roots.reserve(4);
    for (int path = 0; path < 3; ++path) {
        scope.Restore(checkpoint);
        Number_ passive(5.0), output = 3.0 * x;
        SeedAdjointBlock({passive, output, output}, {0, 3, 4}, zero, &roots);
        scope.FinishRecording();
        scope.ReverseSuffix(checkpoint);
    }
    scope.ReversePrefix(checkpoint);
    for (size_t lane = 0; lane < 4; ++lane)
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, lane), lane == 1 || lane == 2 ? 9.0 : 0.0);
}

TEST(PassiveConstantsTest, TestResetOwnerFailureAndEventAdmission) {
    {
        RecordingScope_ scope;
        auto foreign = std::async(std::launch::async, [&scope] {
            try {
                scope.EnablePassiveConstants();
                return false;
            } catch (const Exception_&) {
                return true;
            }
        });
        ASSERT_TRUE(foreign.get());
        scope.EnablePassiveConstants();
        Number_ old(2.0);
        Rewind(*Tape());
        Number_ fresh(3.0);
        ASSERT_TRUE(fresh.IsPassive());
        Clear(*Tape());
        Number_ afterClear(4.0);
        ASSERT_TRUE(afterClear.IsPassive());
        scope.StartRecording();
        ASSERT_THROW(scope.EnablePassiveConstants(), Exception_);
        ASSERT_THROW(static_cast<void>(NativeRecordedOperation_::Begin(&scope)), Exception_);
        ASSERT_THROW(NativeRecordedOperation_::Prepare(Tape()), Exception_);
        ASSERT_EQ(Tape()->ReverseEventCount(), 0U);
    }
    ASSERT_THROW(([] {
                     RecordingScope_ scope;
                     scope.EnablePassiveConstants();
                     Number_ passive(3.0);
                     THROW("controlled business failure");
                 }()),
                 Exception_);
    RecordingScope_ recovered;
    Number_ a(2.0), b(2.0);
    ASSERT_FALSE(a.IsPassive());
    ASSERT_NE(&Adjoint(a), &Adjoint(b));
}

TEST(PassiveConstantsTest, TestCapacityBudgetAndLateEnableRemainEnforced) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    {
        TapeCapacityBudget_ budget(initial);
        TapeCapacityScope_ capacity(&budget);
        RecordingScope_ scope;
        scope.EnablePassiveConstants();
        double total = 0.0;
        for (int i = 0; i < 40000; ++i) {
            Number_ value(static_cast<double>(i));
            total += Value(value);
        }
        ASSERT_DOUBLE_EQ(total, 799980000.0);
        ASSERT_EQ(MeasureTape(*Tape()).nodes_, 1U);
        Number_ input;
        scope.RegisterInput(input, 2.0);
        ASSERT_THROW(scope.EnablePassiveConstants(), Exception_);
        ASSERT_THROW(([&] {
                         for (int i = 0; i < 20000; ++i)
                             scope.RegisterInput(input, static_cast<double>(i));
                     }()),
                     Exception_);
        ASSERT_LE(budget.PeakCapacityBytes(), initial);
    }
    RecordingScope_ recovered;
    Number_ a(2.0), b(2.0);
    ASSERT_NE(&Adjoint(a), &Adjoint(b));
}

#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
TEST(PassiveConstantsTest, TestStaleSuffixClearAndThreadBindings) {
    Number_ stale;
    {
        RecordingScope_ scope;
        scope.EnablePassiveConstants();
        scope.StartRecording();
        const auto checkpoint = scope.MakeCheckpoint();
        stale = 3.0;
        scope.Restore(checkpoint);
        Number_ replacement(4.0);
        ASSERT_THROW((Number_(stale + replacement)), Exception_);
    }
    Clear(*Tape());
    ASSERT_THROW((Number_(stale + 1.0)), Exception_);
    std::thread worker([&stale] {
        RecordingScope_ scope;
        scope.EnablePassiveConstants();
        stale = 5.0;
    });
    worker.join();
    ASSERT_THROW((Number_(stale + 1.0)), Exception_);
    RegisterIndependent(stale, Value(stale));
    ASSERT_DOUBLE_EQ(Adjoint(stale), 0.0);
    Clear(*Tape());
}
#endif
