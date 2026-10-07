//
// Created by Codex on 2026/10/7.
//

#include <dal/platform/platform.hpp>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/reverseevent.hpp>

namespace Dal::AAD {
    Tape_* NativeRecordedOperation_::Begin(RecordingScope_* recording) {
        REQUIRE(recording != nullptr, "LinearSolve: recording scope must not be null");
        recording->RequireState(AADRecordingState_::Value_::RECORDING, "LinearSolve");
        auto* tape = recording->tape_;
        NativeOperations_::ValidateAdjointMode(tape->multi_, tape->numAdj_);
        if (tape->HasReverseEventState())
            tape->RequireReverseEventState("LinearSolve");
        return tape;
    }

    void NativeRecordedOperation_::ValidateInput(Tape_* tape, const Number_& input) {
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        input.ValidateOperands(tape, "LinearSolve.Input");
#endif
        REQUIRE(input.node_ != nullptr && tape->nodes_.Contains(input.node_), "LinearSolve.Input: requires a live slot on the recording tape");
    }

    void NativeRecordedOperation_::Commit(RecordingScope_* recording, std::unique_ptr<ReverseEvent_> event) {
        recording->tape_->AppendReverseEvent(std::move(event));
    }

    void NativeRecordedOperation_::Fail(RecordingScope_* recording) {
        recording->tape_->reverseFailed_ = true;
        recording->RetainFailure();
    }
} // namespace Dal::AAD
