//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/reverseevent.hpp>
#include <dal/platform/platform.hpp>

namespace {
    class ResetAdjointsEvent_ final : public Dal::AAD::ReverseEvent_ {
    public:
        void Reverse(bool, size_t) override { Dal::AAD::ZeroAdjoints(*Dal::AAD::Tape()); }
    };

    class ActionEvent_ final : public Dal::AAD::ReverseEvent_ {
        void (*action_)();

    public:
        explicit ActionEvent_(void (*action)()) : action_(action) {}
        void Reverse(bool, size_t) override { action_(); }
    };

    void RecordDuringReverse() { Dal::AAD::Number_ additional(1.0); }

    void ClearDuringReverse() { Dal::AAD::Clear(*Dal::AAD::Tape()); }

    void RecurseDuringReverse() { Dal::AAD::PropagateToStart(*Dal::AAD::Tape()); }
} // namespace

TEST(AADReverseEventTest, TestAdjointResetDuringReverseInvalidatesGraph) {
    using namespace Dal::AAD;
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 2.0);
    scope.StartRecording();
    Number_ output = input * input;
    NativeRecordedOperation_::Commit(&scope, std::make_unique<ResetAdjointsEvent_>());
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(output, 1.0);
    ASSERT_THROW(scope.Reverse(), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Dal::Exception_);
    scope.Close();
    Clear(*Tape());
}

TEST(AADReverseEventTest, TestRecordResetAndRecursiveReverseInvalidateGraph) {
    using namespace Dal::AAD;
    for (auto action : {&RecordDuringReverse, &ClearDuringReverse, &RecurseDuringReverse}) {
        Clear(*Tape());
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, 2.0);
        scope.StartRecording();
        Number_ output = input * input;
        NativeRecordedOperation_::Commit(&scope, std::make_unique<ActionEvent_>(action));
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(output, 1.0);
        ASSERT_THROW(scope.Reverse(), Dal::Exception_);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Dal::Exception_);
        scope.Close();
        Clear(*Tape());
    }
    Number_ input(3.0);
    Number_ output = input * input;
    NativeOperations_::SetSeed(output, 1.0);
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 6.0);
    Clear(*Tape());
}
