//
// Created on 2026/10/04.
//

#pragma once

#include <cstdint>
#include <dal/math/aad/aad.hpp>
#include <exception>
#include <thread>

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
        void RequireOwner(const char* operation) const;
        void RequireState(State_ expected, const char* operation) const;
        void RequireMode(const char* operation) const;
        void RequireCheckpoint(const Checkpoint_& checkpoint, const char* operation) const;
        void Apply(Reset_ operation, State_ next, bool reversing = false);
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
        void FinishRecording();
        Checkpoint_ MakeCheckpoint();
        void Restore(const Checkpoint_& checkpoint);
        void ClearAdjoints();
        void Reverse();
        void ReverseSuffix(const Checkpoint_& checkpoint);
        void ReversePrefix(const Checkpoint_& checkpoint);
        void Close();
    };

    std::exception_ptr LastRecordingCleanupFailure();
} // namespace Dal::AAD
