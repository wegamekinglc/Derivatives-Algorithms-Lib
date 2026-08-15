//
// Created by wegam on 2023/1/24.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include <dal/concurrency/threadpool.hpp>
#include <dal/utilities/exceptions.hpp>

using namespace Dal;

namespace Dal {
    class ThreadPoolTestAccess_ {
    public:
        static void SetBeforeWorkerClaimHook(ThreadPool_* threadPool, void (*hook)()) {
            threadPool->beforeWorkerClaimHookForTesting_.store(hook, std::memory_order_release);
        }
    };
} // namespace Dal

namespace {

    std::promise<void>* workerReadyToClaim = nullptr;
    const std::shared_future<void>* releaseWorkerClaim = nullptr;

    void PauseWorkerBeforeClaim() {
        workerReadyToClaim->set_value();
        releaseWorkerClaim->wait();
    }

    int SetThreadCountEnvironmentRaw(const char* value) {
#ifdef _WIN32
        return static_cast<int>(_putenv_s("DAL_NUM_THREADS", value));
#else
        return setenv("DAL_NUM_THREADS", value, 1);
#endif
    }

    int ClearThreadCountEnvironmentRaw() {
#ifdef _WIN32
        return static_cast<int>(_putenv_s("DAL_NUM_THREADS", ""));
#else
        return unsetenv("DAL_NUM_THREADS");
#endif
    }

    // RAII save/restore of DAL_NUM_THREADS. Every test in this binary shares the process and the
    // ThreadPool_ singleton, so an override left behind by a failing assertion would silently
    // change the thread count other tests observe.
    class ScopedThreadCountEnvironment_ {
        bool hadValue_;
        std::string savedValue_;

    public:
        explicit ScopedThreadCountEnvironment_(const char* value) {
            const char* current = std::getenv("DAL_NUM_THREADS");
            hadValue_ = current != nullptr;
            if (hadValue_)
                savedValue_ = current;
            EXPECT_EQ(SetThreadCountEnvironmentRaw(value), 0);
        }

        ~ScopedThreadCountEnvironment_() {
            if (hadValue_)
                SetThreadCountEnvironmentRaw(savedValue_.c_str());
            else
                ClearThreadCountEnvironmentRaw();
        }
    };

    size_t ExpectedThreadCount(size_t requested) {
        const size_t hardware = std::max(1u, std::thread::hardware_concurrency());
        return std::min(requested, hardware);
    }

    // Bounded poll for Stop() having begun teardown (active_ flips false under the same critical
    // section that latches stopping_). Used only where no task future can observe the transition;
    // the timeout turns a production deadlock into a test failure instead of an infinite spin.
    bool WaitUntilPoolInactive(ThreadPool_* threadPool, std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (threadPool->IsActive()) {
            if (std::chrono::steady_clock::now() >= deadline)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

} // namespace

TEST(ConcurrencyTest, TestThreadPoolStartsLazily) {
    ThreadPool_* threadPool = ThreadPool_::GetInstance();

    ASSERT_FALSE(threadPool->IsActive());
    ASSERT_GE(threadPool->NumThreads(), 1u);
    ASSERT_FALSE(threadPool->IsActive());

    auto future = threadPool->SpawnTask([]() { return true; });
    ASSERT_TRUE(threadPool->IsActive());
    threadPool->ActiveWait(future);
    ASSERT_TRUE(future.get());

    threadPool->Stop();
    ASSERT_FALSE(threadPool->IsActive());
}

TEST(ConcurrencyTest, TestThreadPoolStartStopLifecycle) {
    ThreadPool_* threadPool = ThreadPool_::GetInstance();
    constexpr size_t requested = 3;

    for (int iteration = 0; iteration < 3; ++iteration) {
        threadPool->Start(requested, true);
        ASSERT_TRUE(threadPool->IsActive());
        ASSERT_EQ(threadPool->NumThreads(), ExpectedThreadCount(requested));

        std::atomic<int> completed{0};
        std::vector<TaskHandle_> futures;
        for (int task = 0; task < 8; ++task) {
            futures.push_back(threadPool->SpawnTask([&completed]() {
                ++completed;
                return true;
            }));
        }
        for (auto& future : futures) {
            threadPool->ActiveWait(future);
            ASSERT_TRUE(future.get());
        }
        ASSERT_EQ(completed.load(), 8);

        threadPool->Stop();
        ASSERT_FALSE(threadPool->IsActive());
    }
}

TEST(ConcurrencyTest, TestThreadPoolUsesEnvironmentLimit) {
    ThreadPool_* threadPool = ThreadPool_::GetInstance();
    threadPool->Stop();

    const char* preexisting = std::getenv("DAL_NUM_THREADS");
    const std::string preexistingValue = preexisting != nullptr ? preexisting : "";
    {
        const ScopedThreadCountEnvironment_ envOverride("2");
        threadPool->Start(0, true);
        ASSERT_EQ(threadPool->NumThreads(), ExpectedThreadCount(2));
        threadPool->Stop();
    }
    ASSERT_FALSE(threadPool->IsActive());

    // The guard must restore the exact prior environment, whatever the outcome above.
    const char* restored = std::getenv("DAL_NUM_THREADS");
    if (preexisting != nullptr) {
        ASSERT_TRUE(restored != nullptr);
        ASSERT_STREQ(restored, preexistingValue.c_str());
    } else {
        ASSERT_TRUE(restored == nullptr || restored[0] == '\0');
    }
}

TEST(ConcurrencyTest, TestThreadPoolTaskCannotStopOrRestartPool) {
    ThreadPool_* threadPool = ThreadPool_::GetInstance();
    threadPool->Start(1, true);

    auto stopFuture = threadPool->SpawnTask([threadPool]() {
        threadPool->Stop();
        return true;
    });
    threadPool->ActiveWait(stopFuture);
    ASSERT_THROW(stopFuture.get(), Dal::Exception_);
    ASSERT_TRUE(threadPool->IsActive());

    auto restartFuture = threadPool->SpawnTask([threadPool]() {
        threadPool->Start(1, true);
        return true;
    });
    threadPool->ActiveWait(restartFuture);
    ASSERT_THROW(restartFuture.get(), Dal::Exception_);
    ASSERT_TRUE(threadPool->IsActive());

    threadPool->Stop();
}

TEST(ConcurrencyTest, TestThreadPoolStopDrainsClaimedCallerAndCancelsQueuedTasks) {
    using namespace std::chrono_literals;

    ThreadPool_* threadPool = ThreadPool_::GetInstance();
    threadPool->Start(1, true);

    std::promise<void> taskClaimed;
    std::promise<void> releaseTask;
    const std::shared_future<void> release = releaseTask.get_future().share();
    std::atomic<int> sentinelRuns{0};

    auto claimed = threadPool->SpawnTask([&]() {
        taskClaimed.set_value();
        release.wait();
        return true;
    });
    auto cancelled = threadPool->SpawnTask([&]() {
        ++sentinelRuns;
        return true;
    });
    auto waiter = std::async(std::launch::async, [&]() { return threadPool->ActiveWait(claimed); });
    // The caller in `waiter` pops tasks FIFO, so it claims `claimed` and sets taskClaimed.
    // Bounded waits act as the hang guard throughout: a broken handshake fails within seconds.
    ASSERT_EQ(taskClaimed.get_future().wait_for(10s), std::future_status::ready)
        << "caller never claimed the first queued task";

    auto stopper = std::async(std::launch::async, [threadPool]() { threadPool->Stop(); });
    // The claimed task is still blocked on `release`, so Stop() must stay blocked waiting for its
    // active caller to drain. Stop() can never complete here, so this bounded negative wait cannot
    // false-fail the way an IsActive() spin + wait_for(0s) instant check could mis-order.
    ASSERT_EQ(stopper.wait_for(50ms), std::future_status::timeout)
        << "Stop() returned while a claimed caller task was still blocked";

    releaseTask.set_value();
    ASSERT_TRUE(waiter.get());
    stopper.get();

    bool queuedTaskWasCancelled = false;
    try {
        static_cast<void>(cancelled.get());
    } catch (const std::future_error& error) {
        queuedTaskWasCancelled = error.code() == std::make_error_code(std::future_errc::broken_promise);
    }

    threadPool->Start(1, true);
    auto restarted = threadPool->SpawnTask([&]() {
        ++sentinelRuns;
        return true;
    });
    ASSERT_TRUE(threadPool->ActiveWait(restarted));
    ASSERT_TRUE(restarted.get());
    threadPool->Stop();

    ASSERT_TRUE(queuedTaskWasCancelled);
    ASSERT_EQ(sentinelRuns.load(), 1);
}

TEST(ConcurrencyTest, TestThreadPoolStopPreventsWorkerClaimAfterStoppingBegins) {
    if (ExpectedThreadCount(2) < 2)
        GTEST_SKIP() << "requires one pool worker in addition to the caller";

    ASSERT_EXIT(
        {
            using namespace std::chrono_literals;

            ThreadPool_* threadPool = ThreadPool_::GetInstance();
            threadPool->Start(2, true);
            std::promise<void> workerAtClaim;
            std::promise<void> releaseWorker;
            const std::shared_future<void> release = releaseWorker.get_future().share();
            workerReadyToClaim = &workerAtClaim;
            releaseWorkerClaim = &release;
            ThreadPoolTestAccess_::SetBeforeWorkerClaimHook(threadPool, &PauseWorkerBeforeClaim);

            std::atomic<int> taskRuns{0};
            auto unclaimed = threadPool->SpawnTask([&]() {
                ++taskRuns;
                return true;
            });
            // Bounded waits replace the old 2s watchdog thread and the IsActive() spin: the worker
            // is parked before its claim and no other thread pops, so `unclaimed` becomes ready
            // only when Stop() clears the queue and breaks the task promise. That makes the future
            // itself a deterministic "stop has begun" handshake.
            if (workerAtClaim.get_future().wait_for(10s) != std::future_status::ready)
                std::_Exit(45);

            std::thread stopper([threadPool]() { threadPool->Stop(); });
            if (unclaimed.wait_for(10s) != std::future_status::ready)
                std::_Exit(46);

            bool taskWasCancelledBeforeWorkerRelease = false;
            try {
                static_cast<void>(unclaimed.get());
            } catch (const std::future_error& error) {
                taskWasCancelledBeforeWorkerRelease = error.code() == std::make_error_code(std::future_errc::broken_promise);
            }
            releaseWorker.set_value();
            stopper.join();
            std::_Exit(taskWasCancelledBeforeWorkerRelease && taskRuns.load() == 0 ? 0 : 43);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(ConcurrencyTest, TestThreadPoolStopCancelsQueueBeforeDrainingNestedCallerWait) {
    ASSERT_EXIT(
        {
            using namespace std::chrono_literals;

            ThreadPool_* threadPool = ThreadPool_::GetInstance();
            threadPool->Start(1, true);
            std::promise<void> outerClaimed;
            std::promise<void> allowNestedWait;
            const std::shared_future<void> nestedWaitAllowed = allowNestedWait.get_future().share();
            TaskHandle_ nested;

            auto outer = threadPool->SpawnTask([&]() {
                outerClaimed.set_value();
                nestedWaitAllowed.wait();
                const bool nestedWasRun = threadPool->ActiveWait(nested);
                bool nestedWasCancelled = false;
                try {
                    static_cast<void>(nested.get());
                } catch (const std::future_error& error) {
                    nestedWasCancelled = error.code() == std::make_error_code(std::future_errc::broken_promise);
                }
                return !nestedWasRun && nestedWasCancelled;
            });
            nested = threadPool->SpawnTask([]() { return true; });

            auto caller = std::async(std::launch::async, [&]() { return threadPool->ActiveWait(outer); });
            if (outerClaimed.get_future().wait_for(10s) != std::future_status::ready)
                std::_Exit(45);

            std::thread stopper([threadPool]() { threadPool->Stop(); });
            // The caller is busy running `outer` and a 1-thread pool has no workers, so `nested`
            // becomes ready only when Stop() clears the queue. Waiting on the future is the
            // deterministic "stop has begun" handshake; no watchdog or IsActive() spin is needed.
            if (nested.wait_for(10s) != std::future_status::ready)
                std::_Exit(46);

            allowNestedWait.set_value();
            stopper.join();
            const bool callerRanTask = caller.get();
            const bool outerObservedCancellation = outer.get();
            std::_Exit(callerRanTask && outerObservedCancellation ? 0 : 43);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(ConcurrencyTest, TestThreadPoolStopCancelsQueueBeforeJoiningNestedWorkerWait) {
    if (ExpectedThreadCount(2) < 2)
        GTEST_SKIP() << "requires one pool worker in addition to the caller";

    ASSERT_EXIT(
        {
            using namespace std::chrono_literals;

            ThreadPool_* threadPool = ThreadPool_::GetInstance();
            threadPool->Start(2, true);
            std::promise<void> outerClaimed;
            std::promise<void> allowNestedWait;
            const std::shared_future<void> nestedWaitAllowed = allowNestedWait.get_future().share();
            TaskHandle_ nested;
            std::atomic<int> nestedRuns{0};

            auto outer = threadPool->SpawnTask([&]() {
                outerClaimed.set_value();
                nestedWaitAllowed.wait();
                const bool nestedWasRun = threadPool->ActiveWait(nested);
                bool nestedWasCancelled = false;
                try {
                    static_cast<void>(nested.get());
                } catch (const std::future_error& error) {
                    nestedWasCancelled = error.code() == std::make_error_code(std::future_errc::broken_promise);
                }
                return !nestedWasRun && nestedWasCancelled && nestedRuns.load() == 0;
            });
            if (outerClaimed.get_future().wait_for(10s) != std::future_status::ready)
                std::_Exit(45);
            nested = threadPool->SpawnTask([&]() {
                ++nestedRuns;
                return true;
            });

            std::thread stopper([threadPool]() { threadPool->Stop(); });
            // The single worker is busy running `outer`, so `nested` becomes ready only when
            // Stop() clears the queue: a deterministic "stop has begun" handshake that replaces
            // the old watchdog thread and IsActive() spin.
            if (nested.wait_for(10s) != std::future_status::ready)
                std::_Exit(46);

            allowNestedWait.set_value();
            stopper.join();
            const bool outerObservedCancellation = outer.get();
            std::_Exit(outerObservedCancellation ? 0 : 43);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(ConcurrencyTest, TestThreadPoolStopAllowsNestedQueriesAndRejectsSubmission) {
    if (ExpectedThreadCount(2) < 2)
        GTEST_SKIP() << "requires one pool worker in addition to the caller";

    ASSERT_EXIT(
        {
            using namespace std::chrono_literals;

            ThreadPool_* threadPool = ThreadPool_::GetInstance();
            threadPool->Start(2, true);
            std::promise<void> taskStarted;
            std::promise<void> releasePromise;
            std::shared_future<void> releaseTask = releasePromise.get_future().share();

            auto task = threadPool->SpawnTask([&]() {
                taskStarted.set_value();
                releaseTask.wait();
                const size_t count = threadPool->NumThreads();
                const bool active = threadPool->IsActive();
                bool submissionRejected = false;
                try {
                    threadPool->SpawnTask([]() { return true; });
                } catch (const Dal::Exception_&) {
                    submissionRejected = true;
                }
                return count == 2 && !active && submissionRejected;
            });

            if (taskStarted.get_future().wait_for(10s) != std::future_status::ready)
                std::_Exit(45);
            std::thread stopper([threadPool]() { threadPool->Stop(); });
            // No task future can observe stopping_ here (the probing task is itself blocked), so
            // poll IsActive() with a bounded timeout instead of an unbounded yield spin; the
            // timeout, not a watchdog thread, is the hang guard.
            if (!WaitUntilPoolInactive(threadPool, 10s))
                std::_Exit(46);
            releasePromise.set_value();
            stopper.join();
            const bool taskResult = task.get();
            std::_Exit(taskResult ? 0 : 43);
        },
        ::testing::ExitedWithCode(0), "");
}
