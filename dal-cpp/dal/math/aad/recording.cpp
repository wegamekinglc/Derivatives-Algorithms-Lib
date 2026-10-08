//
// Created on 2026/10/04.
//

#include <dal/platform/strict.hpp>

#include <atomic>
#include <limits>

#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/reverseevent.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::AAD {
#include <dal/auto/MG_AADRecordingState_enum.inc>

    namespace {
        std::uint64_t NewRecordingIdentity() {
            static std::atomic<std::uint64_t> nextIdentity{1};
            auto identity = nextIdentity.load(std::memory_order_relaxed);
            while (identity != 0) {
                const auto next = identity == std::numeric_limits<std::uint64_t>::max() ? 0 : identity + 1;
                if (nextIdentity.compare_exchange_weak(identity, next, std::memory_order_relaxed))
                    return identity;
            }
            THROW("RecordingScope.MakeCheckpoint: recording identities exhausted");
        }
    } // namespace

    NativeRecordingIdentity_ NativeRecordedOperation_::AccuracyRecording(RecordingScope_* recording, bool reverse) {
        REQUIRE(recording != nullptr, "SolveAccuracy: recording scope must not be null");
        recording->RequireState(reverse ? AADRecordingState_::Value_::READY : AADRecordingState_::Value_::RECORDING, "SolveAccuracy");
        if (recording->identity_ == 0)
            recording->identity_ = NewRecordingIdentity();
        return {recording->identity_, recording->multi_, recording->width_};
    }

    struct RecordingScope_::Context_ {
        bool active_ = false;
        bool poisoned_ = false;
        std::exception_ptr failure_;
    };

    RecordingScope_::Context_& RecordingScope_::Context() {
        thread_local Context_ context;
        return context;
    }

    RecordingScope_::RecordingScope_(Tape_* tape) : RecordingScope_(tape, &AAD::Rewind, &AAD::Clear) {}

    RecordingScope_::RecordingScope_(Tape_* tape, Reset_ rewind, Reset_ clear)
        : tape_(tape), context_(nullptr), owner_(std::this_thread::get_id()), rewind_(rewind) {
        REQUIRE(tape_ != nullptr, "RecordingScope: tape must not be null");
        REQUIRE(tape_ == Tape(), "RecordingScope: tape must be the calling thread's default tape");
        context_ = &Context();
        REQUIRE(!context_->active_, "RecordingScope: nested independent recording is unsupported");
        context_->active_ = true;
        try {
            if (context_->poisoned_) {
                clear(*tape_);
                context_->poisoned_ = false;
                context_->failure_ = nullptr;
            }
            AAD::Activate(*tape_);
            rewind_(*tape_);
            multi_ = tape_->multi_;
            width_ = tape_->numAdj_;
        } catch (...) {
            context_->active_ = false;
            context_->poisoned_ = true;
            context_->failure_ = std::current_exception();
            throw;
        }
    }

    const char* RecordingScope_::StateName(State_ state) {
        switch (state) {
        case State_::REGISTERING:
            return "requires input registration";
        case State_::RECORDING:
            return "requires graph recording";
        case State_::READY:
            return "requires ready for reverse";
        case State_::REVERSING:
            return "requires reverse in progress";
        case State_::FAILED:
            return "requires failed state";
        case State_::CLOSED:
            return "requires closed state";
        }
        return "unknown";
    }

    void RecordingScope_::Reject(const char* operation, const char* constraint) { THROW(String_(operation) + ": " + constraint); }

    void RecordingScope_::RetainFailure() {
        state_ = State_::FAILED;
        context_->poisoned_ = true;
        context_->failure_ = std::current_exception();
    }

    void RecordingScope_::RegisterInput(Number_& input, double value) {
        RequireState(State_::REGISTERING, "RecordingScope.RegisterInput");
        try {
            AAD::RegisterIndependent(input, value);
        } catch (...) {
            RetainFailure();
            throw;
        }
    }

    void RecordingScope_::StartRecording() {
        RequireState(State_::REGISTERING, "RecordingScope.StartRecording");
        Apply(AAD::NewRecording, State_::RECORDING);
    }

    void RecordingScope_::ClearAdjoints() {
        RequireOwner("RecordingScope.ClearAdjoints");
        REQUIRE(state_ == State_::RECORDING || state_ == State_::READY,
                "RecordingScope.ClearAdjoints: requires graph recording or ready for reverse");
        RequireMode("RecordingScope.ClearAdjoints");
        Apply(AAD::ZeroAdjoints, state_);
    }

    Checkpoint_ RecordingScope_::MakeCheckpoint() {
        RequireState(State_::RECORDING, "RecordingScope.MakeCheckpoint");
        REQUIRE(checkpointGeneration_ != std::numeric_limits<std::uint64_t>::max(), "RecordingScope.MakeCheckpoint: generation exhausted");
        if (identity_ == 0)
            identity_ = NewRecordingIdentity();
        Apply(AAD::Mark, State_::RECORDING);
        Checkpoint_ result;
        result.recording_ = identity_;
        result.generation_ = ++checkpointGeneration_;
        result.owner_ = owner_;
        result.multi_ = multi_;
        result.width_ = width_;
        return result;
    }

    void RecordingScope_::ReverseUsing(Reset_ reverse) {
        RequireState(State_::READY, "RecordingScope.Reverse");
        Apply(reverse, State_::READY, true);
    }

    void RecordingScope_::Reverse() { ReverseUsing(AAD::PropagateToStart); }

    void RecordingScope_::ReversePrefix(const Checkpoint_& checkpoint) {
        RequireState(State_::READY, "RecordingScope.ReversePrefix");
        RequireCheckpoint(checkpoint, "RecordingScope.ReversePrefix");
        Apply(AAD::PropagateMarkToStart, State_::READY, true);
    }

    void RecordingScope_::Close() {
        RequireOwner("RecordingScope.Close");
        if (state_ == State_::CLOSED)
            return;
        state_ = State_::CLOSED;
        try {
            RequireMode("RecordingScope.Close");
            rewind_(*tape_);
        } catch (...) {
            context_->active_ = false;
            context_->poisoned_ = true;
            context_->failure_ = std::current_exception();
            throw;
        }
        context_->active_ = false;
    }

    RecordingScope_::~RecordingScope_() noexcept {
        if (state_ == State_::CLOSED)
            return;
        if (owner_ != std::this_thread::get_id())
            std::terminate();
        try {
            Close();
        } catch (...) {
            // Close retains the failure and requires rebuilding the context before its next use.
        }
    }

    std::exception_ptr LastRecordingCleanupFailure() { return RecordingScope_::Context().failure_; }

    void RequireRecordingModeChangeAllowed() {
        REQUIRE(!RecordingScope_::Context().active_, "SetNumResultsForAAD: mode must be selected before independent scope entry");
    }
} // namespace Dal::AAD
