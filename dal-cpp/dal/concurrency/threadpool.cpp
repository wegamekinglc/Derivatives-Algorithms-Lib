//
// Created by wegam on 2021/7/18.
//

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <functional>
#include <limits>

#include <dal/concurrency/threadpool.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {

    ThreadPool_ ThreadPool_::instance_;
    thread_local size_t ThreadPool_::tlsNum_ = 0;
    thread_local bool ThreadPool_::tlsExecutingTask_ = false;

    ThreadPool_::ThreadPool_()
        : threadCount_(DefaultThreadCount()), generation_(0), activeCallers_(0), active_(false), stopping_(false),
          beforeWorkerClaimHookForTesting_(nullptr) {}

    size_t ThreadPool_::DefaultThreadCount() {
        const size_t hardware = std::max(1u, std::thread::hardware_concurrency());
        const char* configured = std::getenv("DAL_NUM_THREADS");
        if (configured == nullptr || configured[0] == '\0')
            return hardware;

        char* end = nullptr;
        errno = 0;
        const unsigned long long parsed = std::strtoull(configured, &end, 10);
        if (errno != 0 || end == configured || *end != '\0' || parsed == 0)
            return hardware;
        if (parsed > static_cast<unsigned long long>(std::numeric_limits<size_t>::max()))
            return hardware;
        return std::min(static_cast<size_t>(parsed), hardware);
    }

    void ThreadPool_::ThreadFunc(const size_t& num) {
        tlsNum_ = num;
        while (queue_.WaitForItem()) {
            const auto beforeClaim = beforeWorkerClaimHookForTesting_.load(std::memory_order_acquire);
            if (beforeClaim != nullptr)
                beforeClaim();

            Task_ task;
            bool claimed = false;
            bool stop = false;
            {
                std::lock_guard<std::mutex> lock(lifecycleMutex_);
                stop = !active_ || stopping_;
                if (!stop)
                    claimed = queue_.TryPop(task);
            }
            if (stop)
                break;
            if (claimed)
                RunTask(task);
        }
        tlsNum_ = 0;
    }

    void ThreadPool_::RunTask(Task_& task) {
        const bool wasExecutingTask = tlsExecutingTask_;
        tlsExecutingTask_ = true;
        try {
            task();
        } catch (...) {
            tlsExecutingTask_ = wasExecutingTask;
            throw;
        }
        tlsExecutingTask_ = wasExecutingTask;
    }

    void ThreadPool_::StartLocked(std::unique_lock<std::mutex>& lock, size_t nThreads) {
        ASSERT(lock.owns_lock(), "thread pool lifecycle lock must be held when starting");
        ASSERT(activeCallers_ == 0, "a new thread pool generation cannot start while caller tasks are active");
        ++generation_;
        active_ = true;
        try {
            threads_.reserve(nThreads - 1);
            for (size_t i = 0; i < nThreads - 1; ++i)
                threads_.emplace_back(&ThreadPool_::ThreadFunc, this, i + 1);
        } catch (...) {
            active_ = false;
            stopping_ = true;
            std::vector<std::thread> threads;
            threads.swap(threads_);
            queue_.InterruptAndClear();
            lock.unlock();
            std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
            lock.lock();
            queue_.ResetInterrupt();
            stopping_ = false;
            lifecycleCondition_.notify_all();
            throw;
        }
    }

    void ThreadPool_::StopLocked(std::unique_lock<std::mutex>& lock) {
        ASSERT(lock.owns_lock(), "thread pool lifecycle lock must be held when stopping");
        if (!active_)
            return;

        active_ = false;
        stopping_ = true;
        std::vector<std::thread> threads;
        threads.swap(threads_);
        queue_.InterruptAndClear();
        lock.unlock();
        std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
        lock.lock();
        lifecycleCondition_.wait(lock, [this]() { return activeCallers_ == 0; });
        queue_.ResetInterrupt();
        stopping_ = false;
        lifecycleCondition_.notify_all();
    }

    void ThreadPool_::Start(size_t nThreads, bool restart) {
        REQUIRE(!tlsExecutingTask_, "a thread pool task cannot start or restart the pool");
        const size_t requested = nThreads == 0 ? DefaultThreadCount() : nThreads;
        const size_t hardware = std::max(1u, std::thread::hardware_concurrency());
        const size_t normalized = std::min(requested, hardware);

        std::lock_guard<std::mutex> transitionLock(transitionMutex_);
        std::unique_lock<std::mutex> lock(lifecycleMutex_);
        lifecycleCondition_.wait(lock, [this]() { return !stopping_; });
        if (active_ && !restart)
            return;
        if (active_)
            StopLocked(lock);
        threadCount_ = normalized;
        StartLocked(lock, threadCount_);
    }

    void ThreadPool_::Stop() {
        REQUIRE(!tlsExecutingTask_, "a thread pool task cannot stop the pool");
        std::lock_guard<std::mutex> transitionLock(transitionMutex_);
        std::unique_lock<std::mutex> lock(lifecycleMutex_);
        lifecycleCondition_.wait(lock, [this]() { return !stopping_; });
        StopLocked(lock);
    }

    bool ThreadPool_::ActiveWait(const TaskHandle_& f) {
        Task_ t;
        bool b = false;
        while (f.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            bool popped = false;
            size_t generation = 0;
            {
                std::lock_guard<std::mutex> lock(lifecycleMutex_);
                if (active_ && !stopping_) {
                    popped = queue_.TryPop(t);
                    if (popped) {
                        generation = generation_;
                        ++activeCallers_;
                    }
                }
            }
            if (popped) {
                ActiveCallerGuard_ callerGuard(this, generation);
                RunTask(t);
                b = true;
            } else
                f.wait();
        }
        return b;
    }

    void ThreadPool_::ReleaseActiveCaller(size_t generation) {
        std::lock_guard<std::mutex> lock(lifecycleMutex_);
        ASSERT(generation == generation_, "caller task belongs to a stale thread pool generation");
        ASSERT(activeCallers_ > 0, "thread pool caller task accounting underflow");
        --activeCallers_;
        lifecycleCondition_.notify_all();
    }
} // namespace Dal
