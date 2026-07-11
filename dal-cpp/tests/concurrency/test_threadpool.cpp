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

namespace {

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
            std::atomic<bool> finished{false};
            std::thread watchdog([&]() {
                std::this_thread::sleep_for(2s);
                if (!finished.load())
                    std::_Exit(42);
            });
            std::thread stopper([&]() { threadPool->Stop(); });
            std::this_thread::sleep_for(50ms);
            releasePromise.set_value();
            stopper.join();
            const bool taskResult = task.get();
            finished = true;
            watchdog.join();
            std::_Exit(taskResult ? 0 : 43);
        },
        ::testing::ExitedWithCode(0), "");
}
