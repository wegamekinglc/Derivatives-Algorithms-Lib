//
// Created by Codex on 2026/10/7.
//

#pragma once

#include <cstddef>
#include <memory>

namespace Dal::AAD {
    class Number_;
    class Tape_;
    class RecordingScope_;

    class ReverseEvent_ {
    public:
        virtual ~ReverseEvent_() noexcept = default;
        virtual void Reverse(bool multi, size_t width) = 0;
    };

    struct NativeRecordedOperation_ {
        [[nodiscard]] static Tape_* Begin(RecordingScope_* recording);
        static void ValidateInput(Tape_* tape, const Number_& input);
        static void Commit(RecordingScope_* recording, std::unique_ptr<ReverseEvent_> event);
        static void Fail(RecordingScope_* recording);
    };
} // namespace Dal::AAD
