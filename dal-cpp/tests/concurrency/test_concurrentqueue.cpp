//
// Created by wegam on 2021/7/18.
//

#include <gtest/gtest.h>
#include <dal/concurrency/concurrentqueue.hpp>
#include <thread>

using std::thread;
using Dal::ConcurrentQueue_;

TEST(ConcurrencyTest, TestConcurrentQueuePushAndPop) {
    ConcurrentQueue_<int> queue;
    auto push_func = [](int i, ConcurrentQueue_<int>* queue) {
        queue->Push(i);
    };

    auto t1 = thread(push_func, 1, &queue);
    auto t2 = thread(push_func, 2, &queue);
    t1.join();
    t2.join();

    auto pop_func = [](int* i, ConcurrentQueue_<int>* queue) {
      queue->Pop(*i);
    };

    int pop1 = 0;
    int pop2 = 0;
    auto t3 = thread(pop_func, &pop1, &queue);
    auto t4 = thread(pop_func, &pop2, &queue);
    t3.join();
    t4.join();
    ASSERT_EQ(pop1 + pop2, 3);
}