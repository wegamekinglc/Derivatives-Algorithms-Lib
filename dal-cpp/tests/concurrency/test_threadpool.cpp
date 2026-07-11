//
// Created by wegam on 2023/1/24.
//

#include <gtest/gtest.h>

#include <dal/concurrency/threadpool.hpp>
#include <dal/utilities/exceptions.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <thread>

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

    void SetThreadCountEnvironment(const char* value) {
#ifdef _WIN32
        ASSERT_EQ(_putenv_s("DAL_NUM_THREADS", value), 0);
#else
        ASSERT_EQ(setenv("DAL_NUM_THREADS", value, 1), 0);
#endif
    }

    void ClearThreadCountEnvironment() {
#ifdef _WIN32
        ASSERT_EQ(_putenv_s("DAL_NUM_THREADS", ""), 0);
#else
        ASSERT_EQ(unsetenv("DAL_NUM_THREADS"), 0);
#endif
    }

    size_t ExpectedThreadCount(size_t requested) {
        const size_t hardware = std::max(1u, std::thread::hardware_concurrency());
        return std::min(requested, hardware);
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
    SetThreadCountEnvironment("2");

    threadPool->Start(0, true);
    ASSERT_EQ(threadPool->NumThreads(), ExpectedThreadCount(2));

    threadPool->Stop();
    ClearThreadCountEnvironment();
    ASSERT_FALSE(threadPool->IsActive());
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
    taskClaimed.get_future().wait();

    auto stopper = std::async(std::launch::async, [threadPool]() { threadPool->Stop(); });
    while (threadPool->IsActive())
        std::this_thread::yield();
    const bool stoppedBeforeClaimedTaskFinished = stopper.wait_for(0s) == std::future_status::ready;

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

    ASSERT_FALSE(stoppedBeforeClaimedTaskFinished);
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
            workerAtClaim.get_future().wait();

            std::promise<void> finished;
            std::thread watchdog([completion = finished.get_future()]() mutable {
                if (completion.wait_for(2s) == std::future_status::timeout)
                    std::_Exit(42);
            });
            std::thread stopper([&]() { threadPool->Stop(); });
            while (threadPool->IsActive())
                std::this_thread::yield();

            bool taskWasCancelledBeforeWorkerRelease = false;
            if (unclaimed.wait_for(0s) == std::future_status::ready) {
                try {
                    static_cast<void>(unclaimed.get());
                } catch (const std::future_error& error) {
                    taskWasCancelledBeforeWorkerRelease = error.code() == std::make_error_code(std::future_errc::broken_promise);
                }
            }
            releaseWorker.set_value();
            stopper.join();
            finished.set_value();
            watchdog.join();
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
            outerClaimed.get_future().wait();
            std::promise<void> finished;
            std::thread watchdog([completion = finished.get_future()]() mutable {
                if (completion.wait_for(2s) == std::future_status::timeout)
                    std::_Exit(42);
            });
            std::thread stopper([&]() { threadPool->Stop(); });
            while (threadPool->IsActive())
                std::this_thread::yield();

            allowNestedWait.set_value();
            stopper.join();
            const bool callerRanTask = caller.get();
            const bool outerObservedCancellation = outer.get();
            finished.set_value();
            watchdog.join();
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
            outerClaimed.get_future().wait();
            nested = threadPool->SpawnTask([&]() {
                ++nestedRuns;
                return true;
            });

            std::promise<void> finished;
            std::thread watchdog([completion = finished.get_future()]() mutable {
                if (completion.wait_for(2s) == std::future_status::timeout)
                    std::_Exit(42);
            });
            std::thread stopper([&]() { threadPool->Stop(); });
            while (threadPool->IsActive())
                std::this_thread::yield();

            allowNestedWait.set_value();
            stopper.join();
            const bool outerObservedCancellation = outer.get();
            finished.set_value();
            watchdog.join();
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

            taskStarted.get_future().wait();
            std::promise<void> finished;
            std::thread watchdog([completion = finished.get_future()]() mutable {
                if (completion.wait_for(2s) == std::future_status::timeout)
                    std::_Exit(42);
            });
            std::thread stopper([&]() { threadPool->Stop(); });
            while (threadPool->IsActive())
                std::this_thread::yield();
            releasePromise.set_value();
            stopper.join();
            const bool taskResult = task.get();
            finished.set_value();
            watchdog.join();
            std::_Exit(taskResult ? 0 : 43);
        },
        ::testing::ExitedWithCode(0), "");
}
