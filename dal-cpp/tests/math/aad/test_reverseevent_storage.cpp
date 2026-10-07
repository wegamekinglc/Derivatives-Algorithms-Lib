//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/reverseevent.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/aad/tapecapacity.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    class TrackedEvent_ final : public ReverseEvent_ {
        size_t* destroyed_;

    public:
        explicit TrackedEvent_(size_t* destroyed) : destroyed_(destroyed) {}
        ~TrackedEvent_() noexcept override { ++*destroyed_; }
        void Reverse(bool, size_t) override {}
    };

    class ThrowingEvent_ final : public ReverseEvent_ {
    public:
        ThrowingEvent_() { THROW("Controlled event constructor failure"); }
        void Reverse(bool, size_t) override {}
    };
} // namespace

TEST(AADReverseEventTest, TestConstructorFailureRefundsDescriptorReservation) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    TapeCapacityBudget_ budget(initial + sizeof(ThrowingEvent_));
    {
        TapeCapacityScope_ capacity(&budget);
        ASSERT_THROW(static_cast<void>(NativeRecordedOperation_::MakeEvent<ThrowingEvent_>(Tape())), Exception_);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
        ASSERT_EQ(budget.CapacityBytes(), initial);
        ASSERT_EQ(budget.PeakCapacityBytes(), initial + sizeof(ThrowingEvent_));
    }
    Clear(*Tape());
}

TEST(AADReverseEventTest, TestDetachedDescriptorReleaseIsReconciledOnReadmission) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    TapeCapacityBudget_ budget(initial + sizeof(TrackedEvent_));
    size_t destroyed = 0;
    ReverseEventHandle_ event;
    {
        TapeCapacityScope_ capacity(&budget);
        event = NativeRecordedOperation_::MakeEvent<TrackedEvent_>(Tape(), &destroyed);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, sizeof(TrackedEvent_));
        ASSERT_EQ(budget.CapacityBytes(), initial + sizeof(TrackedEvent_));
    }
    event.reset();
    ASSERT_EQ(destroyed, 1);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    ASSERT_EQ(budget.CapacityBytes(), initial + sizeof(TrackedEvent_));
    {
        TapeCapacityScope_ readmission(&budget);
        ASSERT_EQ(budget.CapacityBytes(), initial);
    }
    ASSERT_EQ(destroyed, 1);
    Clear(*Tape());
}

TEST(AADReverseEventTest, TestTableGrowthRejectsBeforeChangingBoundaryAndRefundsPendingDescriptor) {
    Clear(*Tape());
    size_t firstCapacity = 0;
    size_t destroyed = 0;
    {
        RecordingScope_ scope;
        scope.StartRecording();
        NativeRecordedOperation_::Commit(&scope, NativeRecordedOperation_::MakeEvent<TrackedEvent_>(Tape(), &destroyed));
        firstCapacity = MeasureTape(*Tape()).reverseEventCapacityBytes_;
        scope.Close();
    }
    ASSERT_EQ(destroyed, 1);
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    TapeCapacityBudget_ budget(initial + TapeCleanupCapacityBytes() + firstCapacity + sizeof(TrackedEvent_));
    {
        TapeCapacityScope_ capacity(&budget, true);
        RecordingScope_ scope;
        scope.StartRecording();
        NativeRecordedOperation_::Commit(&scope, NativeRecordedOperation_::MakeEvent<TrackedEvent_>(Tape(), &destroyed));
        const auto nodesBefore = Tape()->nodes_.OccupiedSlots();
        auto pending = NativeRecordedOperation_::MakeEvent<TrackedEvent_>(Tape(), &destroyed);
        ASSERT_THROW(NativeRecordedOperation_::Prepare(Tape()), Exception_);
        ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodesBefore);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEvents_, 1);
        ASSERT_EQ(budget.CapacityBytes(), initial + firstCapacity + sizeof(TrackedEvent_));
        pending.reset();
        ASSERT_EQ(budget.CapacityBytes(), initial + firstCapacity);
        scope.Close();
        ASSERT_EQ(budget.CapacityBytes(), initial);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    }
    ASSERT_EQ(destroyed, 3);
    Clear(*Tape());
}
