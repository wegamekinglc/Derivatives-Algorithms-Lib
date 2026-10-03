//
// Created on 2026/10/04.
//

#include <dal/platform/strict.hpp>

#include <atomic>
#include <dal/math/aad/recording.hpp>
#include <dal/utilities/exceptions.hpp>
#include <limits>

namespace Dal::AAD {
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

    struct RecordingScope_::Context_ {
        bool active_ = false;
        bool poisoned_ = false;
        std::exception_ptr failure_;
    };

    RecordingScope_::Context_& RecordingScope_::Context() {
        thread_local Context_ context;
        return context;
    }

    RecordingScope_::RecordingScope_(Tape_* tape) : RecordingScope_(tape, Rewind, Clear) {}

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
            Activate(*tape_);
            rewind_(*tape_);
#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
            multi_ = tape_->multi_;
            width_ = tape_->numAdj_;
#endif
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
            return "input registration";
        case State_::RECORDING:
            return "graph recording";
        case State_::READY:
            return "ready for reverse";
        case State_::REVERSING:
            return "reverse in progress";
        case State_::FAILED:
            return "failed";
        case State_::CLOSED:
            return "closed";
        }
        return "unknown";
    }

    void RecordingScope_::RequireOwner(const char* operation) const {
        REQUIRE(owner_ == std::this_thread::get_id(), String_(operation) + ": operation must run on the owning thread");
    }

    void RecordingScope_::RequireState(State_ expected, const char* operation) const {
        RequireOwner(operation);
        REQUIRE(state_ == expected, String_(operation) + ": requires " + StateName(expected));
        RequireMode(operation);
    }

    void RecordingScope_::RequireMode(const char* operation) const {
#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
        REQUIRE(tape_->multi_ == multi_ && tape_->numAdj_ == width_, String_(operation) + ": recording mode or width changed");
#else
        static_cast<void>(operation);
#endif
    }

    void RecordingScope_::RequireCheckpoint(const Checkpoint_& checkpoint, const char* operation) const {
        REQUIRE(checkpoint.recording_ != 0 && checkpoint.recording_ == identity_ && checkpoint.owner_ == owner_,
                String_(operation) + ": checkpoint belongs to another recording or is invalid");
        REQUIRE(checkpoint.generation_ == checkpointGeneration_, String_(operation) + ": checkpoint has been replaced");
        REQUIRE(checkpoint.multi_ == multi_ && checkpoint.width_ == width_, String_(operation) + ": checkpoint mode or width mismatch");
    }

    void RecordingScope_::RetainFailure() {
        state_ = State_::FAILED;
        context_->poisoned_ = true;
        context_->failure_ = std::current_exception();
    }

    void RecordingScope_::RegisterInput(Number_& input, double value) {
        RequireState(State_::REGISTERING, "RecordingScope.RegisterInput");
        try {
            RegisterIndependent(input, value);
        } catch (...) {
            RetainFailure();
            throw;
        }
    }

    void RecordingScope_::Apply(Reset_ operation, State_ next, bool reversing) {
        if (reversing)
            state_ = State_::REVERSING;
        try {
            operation(*tape_);
            state_ = next;
        } catch (...) {
            RetainFailure();
            throw;
        }
    }

    void RecordingScope_::StartRecording() {
        RequireState(State_::REGISTERING, "RecordingScope.StartRecording");
        Apply(NewRecording, State_::RECORDING);
    }

    void RecordingScope_::FinishRecording() {
        RequireState(State_::RECORDING, "RecordingScope.FinishRecording");
        state_ = State_::READY;
    }

    void RecordingScope_::ClearAdjoints() {
        RequireOwner("RecordingScope.ClearAdjoints");
        REQUIRE(state_ == State_::RECORDING || state_ == State_::READY,
                "RecordingScope.ClearAdjoints: requires graph recording or ready for reverse");
        RequireMode("RecordingScope.ClearAdjoints");
        Apply(ZeroAdjoints, state_);
    }

    Checkpoint_ RecordingScope_::MakeCheckpoint() {
        RequireState(State_::RECORDING, "RecordingScope.MakeCheckpoint");
        REQUIRE(checkpointGeneration_ != std::numeric_limits<std::uint64_t>::max(), "RecordingScope.MakeCheckpoint: generation exhausted");
        if (identity_ == 0)
            identity_ = NewRecordingIdentity();
        Apply(Mark, State_::RECORDING);
        Checkpoint_ result;
        result.recording_ = identity_;
        result.generation_ = ++checkpointGeneration_;
        result.owner_ = owner_;
        result.multi_ = multi_;
        result.width_ = width_;
        return result;
    }

    void RecordingScope_::Restore(const Checkpoint_& checkpoint) {
        RequireOwner("RecordingScope.Restore");
        REQUIRE(state_ == State_::RECORDING || state_ == State_::READY, "RecordingScope.Restore: requires graph recording or ready for reverse");
        RequireMode("RecordingScope.Restore");
        RequireCheckpoint(checkpoint, "RecordingScope.Restore");
        Apply(RewindToMark, State_::RECORDING);
    }

    void RecordingScope_::ReverseUsing(Reset_ reverse) {
        RequireState(State_::READY, "RecordingScope.Reverse");
        Apply(reverse, State_::READY, true);
    }

    void RecordingScope_::Reverse() { ReverseUsing(PropagateToStart); }

    void RecordingScope_::ReverseSuffix(const Checkpoint_& checkpoint) {
        RequireState(State_::READY, "RecordingScope.ReverseSuffix");
        RequireCheckpoint(checkpoint, "RecordingScope.ReverseSuffix");
        Apply(PropagateToMark, State_::READY, true);
    }

    void RecordingScope_::ReversePrefix(const Checkpoint_& checkpoint) {
        RequireState(State_::READY, "RecordingScope.ReversePrefix");
        RequireCheckpoint(checkpoint, "RecordingScope.ReversePrefix");
        Apply(PropagateMarkToStart, State_::READY, true);
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
