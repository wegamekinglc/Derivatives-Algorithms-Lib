//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/reverseevent.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    struct EventCounts_ {
        size_t reversed_ = 0;
        size_t destroyed_ = 0;
    };

    class CountedEvent_ final : public ReverseEvent_ {
        EventCounts_* counts_;

    public:
        explicit CountedEvent_(EventCounts_* counts) : counts_(counts) {}
        ~CountedEvent_() noexcept override { ++counts_->destroyed_; }
        void Reverse(bool, size_t) override { ++counts_->reversed_; }
    };

    void RecordCountedEvent(RecordingScope_* scope, EventCounts_* counts) {
        Number_ boundary(1.0);
        NativeRecordedOperation_::Commit(scope, std::make_unique<CountedEvent_>(counts));
    }
} // namespace

TEST(AADReverseEventTest, TestRestoreReleasesOnlySuffixAndCloseReleasesPrefixOnce) {
    Clear(*Tape());
    EventCounts_ prefix, suffix;
    RecordingScope_ scope;
    scope.StartRecording();
    RecordCountedEvent(&scope, &prefix);
    const auto checkpoint = scope.MakeCheckpoint();
    RecordCountedEvent(&scope, &suffix);
    scope.FinishRecording();
    scope.ReverseSuffix(checkpoint);
    ASSERT_EQ(prefix.reversed_, 0);
    ASSERT_EQ(suffix.reversed_, 1);
    scope.Restore(checkpoint);
    ASSERT_EQ(prefix.destroyed_, 0);
    ASSERT_EQ(suffix.destroyed_, 1);
    RecordCountedEvent(&scope, &suffix);
    scope.FinishRecording();
    scope.ReverseSuffix(checkpoint);
    scope.ReversePrefix(checkpoint);
    ASSERT_EQ(prefix.reversed_, 1);
    ASSERT_EQ(suffix.reversed_, 2);
    scope.Close();
    ASSERT_EQ(prefix.destroyed_, 1);
    ASSERT_EQ(suffix.destroyed_, 2);
    scope.Close();
    Clear(*Tape());
    ASSERT_EQ(prefix.destroyed_, 1);
    ASSERT_EQ(suffix.destroyed_, 2);
}

TEST(AADReverseEventTest, TestFullResetReleasesOwnedEventsExactlyOnce) {
    using Reset_ = void (*)(Tape_&);
    for (auto reset : {static_cast<Reset_>(&Rewind), static_cast<Reset_>(&Clear)}) {
        Clear(*Tape());
        EventCounts_ counts;
        RecordingScope_ scope;
        scope.StartRecording();
        RecordCountedEvent(&scope, &counts);
        scope.FinishRecording();
        reset(*Tape());
        ASSERT_EQ(counts.destroyed_, 1);
        ASSERT_EQ(counts.reversed_, 0);
        scope.Close();
        Clear(*Tape());
        ASSERT_EQ(counts.destroyed_, 1);
    }
}

TEST(AADReverseEventTest, TestRawSweepsUseTheSameEventIntervals) {
    Clear(*Tape());
    EventCounts_ prefix, suffix;
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 2.0);
    scope.StartRecording();
    Number_ prefixOutput = input * input;
    RecordCountedEvent(&scope, &prefix);
    scope.MakeCheckpoint();
    Number_ suffixOutput = prefixOutput * prefixOutput;
    RecordCountedEvent(&scope, &suffix);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(suffixOutput, 1.0);
    PropagateToMark(*Tape());
    ASSERT_EQ(prefix.reversed_, 0);
    ASSERT_EQ(suffix.reversed_, 1);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(prefixOutput), 8.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.0);
    PropagateMarkToStart(*Tape());
    ASSERT_EQ(prefix.reversed_, 1);
    ASSERT_EQ(suffix.reversed_, 1);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 32.0);
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(suffixOutput, 1.0);
    PropagateToStart(*Tape());
    ASSERT_EQ(prefix.reversed_, 2);
    ASSERT_EQ(suffix.reversed_, 2);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 32.0);
    scope.Close();
    Clear(*Tape());
}
