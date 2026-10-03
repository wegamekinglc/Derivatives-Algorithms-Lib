//
// Created on 2026/10/04.
//

#include <dal/platform/strict.hpp>

#include <dal/math/aad/recording.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::AAD {
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
        } catch (...) {
            context_->active_ = false;
            context_->poisoned_ = true;
            context_->failure_ = std::current_exception();
            throw;
        }
    }

    void RecordingScope_::Close() {
        REQUIRE(owner_ == std::this_thread::get_id(), "RecordingScope.Close: operation must run on the owning thread");
        if (closed_)
            return;
        closed_ = true;
        try {
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
        if (closed_)
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
} // namespace Dal::AAD
