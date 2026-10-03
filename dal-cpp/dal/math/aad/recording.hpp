//
// Created on 2026/10/04.
//

#pragma once

#include <dal/math/aad/aad.hpp>
#include <exception>
#include <thread>

namespace Dal::AAD {
    class RecordingScope_ {
        struct Context_;
        using Reset_ = void (*)(Tape_&);

        Tape_* tape_;
        Context_* context_;
        std::thread::id owner_;
        Reset_ rewind_;
        bool closed_ = false;

        static Context_& Context();
        RecordingScope_(Tape_* tape, Reset_ rewind, Reset_ clear);
        friend struct RecordingScopeTestAccess_;
        friend std::exception_ptr LastRecordingCleanupFailure();

    public:
        // Independent ownership; the scope and its destruction stay on the creating thread.
        explicit RecordingScope_(Tape_* tape = Tape());
        ~RecordingScope_() noexcept;
        RecordingScope_(const RecordingScope_&) = delete;
        RecordingScope_& operator=(const RecordingScope_&) = delete;
        RecordingScope_(RecordingScope_&&) = delete;
        RecordingScope_& operator=(RecordingScope_&&) = delete;

        void Close();
    };

    std::exception_ptr LastRecordingCleanupFailure();
} // namespace Dal::AAD
