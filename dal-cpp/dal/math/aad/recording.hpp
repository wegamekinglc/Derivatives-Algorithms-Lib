//
// Created on 2026/10/04.
//

#pragma once

#include <cstdint>
#include <exception>
#include <thread>
#include <dal/math/aad/backend.hpp>

namespace Dal::AAD {
    class Checkpoint_ {
        std::uint64_t recording_ = 0;
        std::uint64_t generation_ = 0;
        std::thread::id owner_;
        bool multi_ = false;
        size_t width_ = 1;

        friend class RecordingScope_;

    public:
        Checkpoint_() = default;
    };

    class RecordingScope_ {
        struct Context_;
        using Reset_ = void (*)(Tape_&);
        enum class State_ { REGISTERING, RECORDING, READY, REVERSING, FAILED, CLOSED };

        Tape_* tape_;
        Context_* context_;
        std::thread::id owner_;
        Reset_ rewind_;
        State_ state_ = State_::REGISTERING;
        std::uint64_t identity_ = 0;
        std::uint64_t checkpointGeneration_ = 0;
        bool multi_ = false;
        size_t width_ = 1;

        static Context_& Context();
        static const char* StateName(State_ state);
        [[noreturn]] static void Reject(const char* operation, const char* constraint);
        FORCE_INLINE void RequireOwner(const char* operation) const {
            if (owner_ != std::this_thread::get_id())
                Reject(operation, "operation must run on the owning thread");
        }
        FORCE_INLINE void RequireState(State_ expected, const char* operation) const {
            RequireOwner(operation);
            if (state_ != expected)
                Reject(operation, StateName(expected));
            RequireMode(operation);
        }
        FORCE_INLINE void RequireMode(const char* operation) const {
#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
            if (tape_->multi_ != multi_ || tape_->numAdj_ != width_)
                Reject(operation, "recording mode or width changed");
#else
            static_cast<void>(operation);
#endif
        }
        FORCE_INLINE void RequireCheckpoint(const Checkpoint_& checkpoint, const char* operation) const {
            if (checkpoint.recording_ == 0 || checkpoint.recording_ != identity_ || checkpoint.owner_ != owner_)
                Reject(operation, "checkpoint belongs to another recording or is invalid");
            if (checkpoint.generation_ != checkpointGeneration_)
                Reject(operation, "checkpoint has been replaced");
            if (checkpoint.multi_ != multi_ || checkpoint.width_ != width_)
                Reject(operation, "checkpoint mode or width mismatch");
        }
        template <class F_> FORCE_INLINE void Apply(const F_& operation, State_ next, bool reversing = false) {
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
        void ReverseUsing(Reset_ reverse);
        void RetainFailure();
        RecordingScope_(Tape_* tape, Reset_ rewind, Reset_ clear);
        friend struct RecordingScopeTestAccess_;
        friend struct RecordingStateTestAccess_;
        friend std::exception_ptr LastRecordingCleanupFailure();
        friend void RequireRecordingModeChangeAllowed();

    public:
        // Independent ownership; the scope and its destruction stay on the creating thread.
        explicit RecordingScope_(Tape_* tape = Tape());
        ~RecordingScope_() noexcept;
        RecordingScope_(const RecordingScope_&) = delete;
        RecordingScope_& operator=(const RecordingScope_&) = delete;
        RecordingScope_(RecordingScope_&&) = delete;
        RecordingScope_& operator=(RecordingScope_&&) = delete;

        void RegisterInput(Number_& input, double value);
        void StartRecording();
        FORCE_INLINE void FinishRecording() {
            RequireState(State_::RECORDING, "RecordingScope.FinishRecording");
            state_ = State_::READY;
        }
        Checkpoint_ MakeCheckpoint();
        FORCE_INLINE void Restore(const Checkpoint_& checkpoint) {
            RequireOwner("RecordingScope.Restore");
            if (state_ != State_::RECORDING && state_ != State_::READY)
                Reject("RecordingScope.Restore", "requires graph recording or ready for reverse");
            RequireMode("RecordingScope.Restore");
            RequireCheckpoint(checkpoint, "RecordingScope.Restore");
            Apply(BackendAdapter_::RESTORE_SUFFIX, State_::RECORDING);
        }
        void ClearAdjoints();
        void Reverse();
        FORCE_INLINE void ReverseSuffix(const Checkpoint_& checkpoint) {
            RequireState(State_::READY, "RecordingScope.ReverseSuffix");
            RequireCheckpoint(checkpoint, "RecordingScope.ReverseSuffix");
            Apply(BackendAdapter_::REVERSE_SUFFIX, State_::READY, true);
        }
        void ReversePrefix(const Checkpoint_& checkpoint);
        void Close();
    };

    std::exception_ptr LastRecordingCleanupFailure();
} // namespace Dal::AAD
