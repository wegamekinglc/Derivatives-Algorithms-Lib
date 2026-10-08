//
// Created by Codex on 2026/10/7.
//

#include <dal/platform/platform.hpp>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/reverseevent.hpp>

#include <algorithm>
#include <functional>
#include <limits>

namespace Dal::AAD {
    EventStorageTicket_::EventStorageTicket_(Tape_* tape, size_t bytes) : tape_(tape), bytes_(bytes) {
        NativeRecordedOperation_::ReserveStorage(tape_, bytes_);
    }

    EventStorageTicket_::~EventStorageTicket_() noexcept {
        if (tape_ != nullptr)
            NativeRecordedOperation_::ReleaseStorage(tape_, bytes_);
    }

    void ReverseEventDeleter_::operator()(ReverseEvent_* event) const noexcept {
        delete event;
        NativeRecordedOperation_::ReleaseStorage(tape_, bytes_);
    }

    void NativeRecordedOperation_::ReserveStorage(Tape_* tape, size_t bytes) {
        REQUIRE(bytes <= std::numeric_limits<size_t>::max() - tape->eventCapacityBytes_, "Tape event storage: capacity extent overflow");
        BlockAllocationTicket_ admission(tape, bytes);
        tape->eventCapacityBytes_ += bytes;
        admission.Commit();
    }

    void NativeRecordedOperation_::ReleaseStorage(Tape_* tape, size_t bytes) noexcept {
        if (bytes > tape->eventCapacityBytes_)
            std::terminate();
        ReleaseBlockAllocation(tape, bytes);
        tape->eventCapacityBytes_ -= bytes;
    }

    void NativeRecordedOperation_::ObserveScratch(Tape_* tape, size_t bytes) noexcept {
        tape->eventScratchPeakBytes_ = std::max(tape->eventScratchPeakBytes_, bytes);
    }

    Tape_* NativeRecordedOperation_::Begin(RecordingScope_* recording) {
        REQUIRE(recording != nullptr, "RecordedOperation: recording scope must not be null");
        recording->RequireState(AADRecordingState_::Value_::RECORDING, "RecordedOperation");
        auto* tape = recording->tape_;
        NativeOperations_::ValidateAdjointMode(tape->multi_, tape->numAdj_);
        if (tape->HasReverseEventState())
            tape->RequireReverseEventState("RecordedOperation");
        return tape;
    }

    NativeInputSlots_::NativeInputSlots_(Tape_* tape) : tape_(tape), ranges_(ReverseEventAllocator_<Range_>(tape)) {
        tape->nodes_.ForEachLiveRange([&](const TapNode_* first, const TapNode_* last) { ranges_.emplace_back(first, last); });
        std::sort(ranges_.begin(), ranges_.end(),
                  [](const Range_& left, const Range_& right) { return std::less<const TapNode_*>()(left.first, right.first); });
    }

    void NativeRecordedOperation_::ValidateInput(const NativeInputSlots_& slots, const Number_& input) {
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        input.ValidateOperands(slots.tape_, "RecordedOperation.Input");
#endif
        REQUIRE(input.node_ != nullptr, "RecordedOperation.Input: requires a live slot on the recording tape");
        const auto range = std::upper_bound(
            slots.ranges_.begin(), slots.ranges_.end(), input.node_,
            [](const TapNode_* node, const NativeInputSlots_::Range_& candidate) { return std::less<const TapNode_*>()(node, candidate.first); });
        REQUIRE(range != slots.ranges_.begin() && std::less<const TapNode_*>()(input.node_, std::prev(range)->second),
                "RecordedOperation.Input: requires a live slot on the recording tape");
    }

    void NativeRecordedOperation_::Prepare(Tape_* tape) { tape->PrepareReverseEvent(); }

    void NativeRecordedOperation_::Commit(RecordingScope_* recording, ReverseEventHandle_ event) {
        recording->tape_->AppendReverseEvent(std::move(event));
    }

    void NativeRecordedOperation_::Fail(RecordingScope_* recording) {
        recording->tape_->reverseFailed_ = true;
        recording->tape_->UpdateThreadEventValidation();
        recording->RetainFailure();
    }
} // namespace Dal::AAD
