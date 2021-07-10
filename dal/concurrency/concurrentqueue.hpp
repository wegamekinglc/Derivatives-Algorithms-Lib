/*
 * Modified by wegamekinglc on 2021/3/10.
 * Written by Antoine Savine in 2018
 * This code is the strict IP of Antoine Savine
 * License to use and alter this code for personal and commercial applications
 * is freely granted to any person or company who purchased a copy of the book
 * Modern Computational Finance: AAD and Parallel Simulations
 * Antoine Savine
 * Wiley, 2018
 * As long as this comment is preserved at the top of the file
*/

#pragma once

#include <dal/platform/platform.hpp>
#include <queue>
#include <mutex>


namespace Dal {

    template <class T_> class ConcurrentQueue_ {
        std::queue<T_> queue_;
        mutable mutex mutex_;
        std::condition_variable cv_;
        bool interrupt_;

    public:
        ConcurrentQueue_(): interrupt_(false) {}
        ~ConcurrentQueue_() { Interrupt(); }

        bool Empty() const {
            std::lock_guard<std::mutex> lk(mutex_);
            return queue_.empty();
        }

        bool TryPop(T_& t) {
            std::lock_guard<std::mutex> lk(mutex_);
            if (queue_.empty())
                return false;
            t = move(queue_.front());
            queue_.pop();
            return true;
        }

        void Push(T_ t) {
            {
                std::lock_guard<std::mutex> lk(mutex_);
                queue_.push(move(t));
            }
            cv_.notify_one();
        }

        void Pop(T_& t) {
            std::unique_lock<std::mutex> lk(mutex_);
            while (!interrupt_ && queue_.empty())
                cv_.wait(lk);
            if (interrupt_)
                return false;
            t = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        void Interrupt() {
            {
                std::lock_guard<std::mutex> lk(mutex_);
                interrupt_ = true;
            }
            cv_.notify_all();
        }

        void ResetInterrupt() {
            interrupt_ = false;
        }

        void Clear() {
            std::queue<T_> empty;
            std::swap(queue_, empty);
        }
    };
}
