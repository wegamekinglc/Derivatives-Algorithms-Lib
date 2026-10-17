#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

#include <dal-public/src/types.hpp>

namespace Dal::Python {
    class GilTestBarrier_ {
        std::mutex mutex_;
        std::condition_variable condition_;
        bool entered_ = false;
        bool released_ = false;

    public:
        void Arm(bool* armed) {
            std::lock_guard<std::mutex> lock(mutex_);
            entered_ = released_ = false;
            *armed = true;
        }

        void Wait(bool* armed, const char* timeoutMessage) {
            if (!*armed)
                return;
            *armed = false;
            std::unique_lock<std::mutex> lock(mutex_);
            entered_ = true;
            REQUIRE(condition_.wait_for(lock, std::chrono::seconds(5), [this] { return released_; }), timeoutMessage);
        }

        bool Release() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!entered_)
                return false;
            released_ = true;
            condition_.notify_all();
            return true;
        }
    };
} // namespace Dal::Python
