//
// Created by wegam on 2023/1/24.
//

#include <thread>
#include <gtest/gtest.h>
#include <dal/concurrency/threadpool.hpp>

using namespace Dal;


TEST(ConcurrencyTest, TestThreadPoolStart) {
    ThreadPool_* thread_pool = ThreadPool_::GetInstance();
    ASSERT_EQ(thread_pool->NumThreads(), std::thread::hardware_concurrency() - 1);

    int n_thread = 16;
    thread_pool->Start(n_thread, true);
    ASSERT_EQ(thread_pool->NumThreads(), std::min(static_cast<int>(std::thread::hardware_concurrency() - 1), n_thread));
    thread_pool->Start(0, true);
}