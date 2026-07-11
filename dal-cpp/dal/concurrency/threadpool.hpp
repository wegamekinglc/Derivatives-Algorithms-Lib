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

#include <atomic>
#include <condition_variable>
#include <dal/concurrency/concurrentqueue.hpp>
#include <dal/utilities/exceptions.hpp>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace Dal {
    using Task_ = std::packaged_task<bool(void)>;
    using TaskHandle_ = std::future<bool>;

    class ThreadPoolTestAccess_;

    class ThreadPool_ {
        static ThreadPool_ instance_;
        ConcurrentQueue_<Task_> queue_;
        std::vector<std::thread> threads_;
        std::mutex lifecycleMutex_;
        std::mutex transitionMutex_;
        std::condition_variable lifecycleCondition_;
        size_t threadCount_;
        size_t generation_;
        size_t activeCallers_;
        bool active_;
        bool stopping_;
        std::atomic<void (*)()> beforeWorkerClaimHookForTesting_;
        static thread_local size_t tlsNum_;
        static thread_local bool tlsExecutingTask_;

        friend class ThreadPoolTestAccess_;

        void ReleaseActiveCaller(size_t generation);

        class ActiveCallerGuard_ {
            ThreadPool_* owner_;
            size_t generation_;

        public:
            ActiveCallerGuard_(ThreadPool_* owner, size_t generation) : owner_(owner), generation_(generation) {}
            ~ActiveCallerGuard_() { owner_->ReleaseActiveCaller(generation_); }

            ActiveCallerGuard_(const ActiveCallerGuard_&) = delete;
            ActiveCallerGuard_& operator=(const ActiveCallerGuard_&) = delete;
            ActiveCallerGuard_(ActiveCallerGuard_&&) = delete;
            ActiveCallerGuard_& operator=(ActiveCallerGuard_&&) = delete;
        };

        void ThreadFunc(const size_t& num);
        void RunTask(Task_& task);
        static size_t DefaultThreadCount();
        void StartLocked(std::unique_lock<std::mutex>& lock, size_t nThreads);
        void StopLocked(std::unique_lock<std::mutex>& lock);
        void TearDownWorkersLocked(std::unique_lock<std::mutex>& lock, bool waitForCallers);
        ThreadPool_();

    public:
        static ThreadPool_* GetInstance() { return &instance_; }

        [[nodiscard]] size_t NumThreads() {
            std::lock_guard<std::mutex> lock(lifecycleMutex_);
            return threadCount_;
        }

        [[nodiscard]] bool IsActive() {
            std::lock_guard<std::mutex> lock(lifecycleMutex_);
            return active_;
        }

        [[nodiscard]] static size_t ThreadNum() { return tlsNum_; }

        void Start(size_t nThreads = 0, bool restart = false);

        ~ThreadPool_() { Stop(); }

        void Stop();

        ThreadPool_(const ThreadPool_& rhs) = delete;
        ThreadPool_& operator=(const ThreadPool_& rhs) = delete;
        ThreadPool_(ThreadPool_&& rhs) = delete;
        ThreadPool_& operator=(ThreadPool_&& rhs) = delete;

        template <class C_> TaskHandle_ SpawnTask(C_ c) {
            Task_ t(std::move(c));
            TaskHandle_ f = t.get_future();
            std::unique_lock<std::mutex> lock(lifecycleMutex_);
            REQUIRE(!stopping_, "cannot submit a task while the thread pool is stopping");
            if (!active_)
                StartLocked(lock, threadCount_);
            queue_.Push(std::move(t));
            return f;
        }

        bool ActiveWait(const TaskHandle_& f);
    };
} // namespace Dal
