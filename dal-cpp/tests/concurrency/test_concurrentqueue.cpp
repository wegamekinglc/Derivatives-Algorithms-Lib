//
// Created by wegam on 2021/7/18.
//

#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include <dal/concurrency/concurrentqueue.hpp>

using std::thread;
using std::vector;
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

TEST(ConcurrencyTest, TestConcurrentQueueEmptyOnConstruction) {
    ConcurrentQueue_<int> queue;
    ASSERT_TRUE(queue.Empty());

    int out = -1;
    ASSERT_FALSE(queue.TryPop(out));
    ASSERT_EQ(out, -1);
}

TEST(ConcurrencyTest, TestConcurrentQueueFifoOrderingSingleThreaded) {
    ConcurrentQueue_<int> queue;
    constexpr int count = 100;
    for (int i = 0; i < count; ++i)
        queue.Push(i);
    ASSERT_FALSE(queue.Empty());

    for (int i = 0; i < count; ++i) {
        int out = -1;
        ASSERT_TRUE(queue.TryPop(out));
        ASSERT_EQ(out, i);
    }
    ASSERT_TRUE(queue.Empty());

    int out = -1;
    ASSERT_FALSE(queue.TryPop(out));
}

TEST(ConcurrencyTest, TestConcurrentQueueInterruptUnblocksPop) {
    ConcurrentQueue_<int> queue;

    // Interrupt is latched, so a later Pop on the empty queue returns false immediately
    // instead of blocking; ResetInterrupt then restores normal blocking-pop semantics.
    queue.Interrupt();
    int out = -1;
    ASSERT_FALSE(queue.Pop(out));
    ASSERT_EQ(out, -1);

    queue.ResetInterrupt();
    queue.Push(7);
    ASSERT_TRUE(queue.Pop(out));
    ASSERT_EQ(out, 7);
}

TEST(ConcurrencyTest, TestConcurrentQueueWaitForItemContract) {
    ConcurrentQueue_<int> queue;

    queue.Interrupt();
    ASSERT_FALSE(queue.WaitForItem());

    queue.ResetInterrupt();
    queue.Push(1);
    ASSERT_TRUE(queue.WaitForItem());

    int out = -1;
    ASSERT_TRUE(queue.TryPop(out));
    ASSERT_EQ(out, 1);
}

TEST(ConcurrencyTest, TestConcurrentQueueClearDropsItems) {
    ConcurrentQueue_<int> queue;
    queue.Push(1);
    queue.Push(2);
    ASSERT_FALSE(queue.Empty());

    queue.Clear();
    ASSERT_TRUE(queue.Empty());

    int out = -1;
    ASSERT_FALSE(queue.TryPop(out));
}

TEST(ConcurrencyTest, TestConcurrentQueueSingleProducerSingleConsumer) {
    ConcurrentQueue_<int> queue;
    constexpr int count = 10000;
    vector<int> consumed;
    consumed.reserve(count);

    // The consumer pops exactly `count` items; Pop only fails on Interrupt, which never fires
    // here, so the join-based protocol needs no timing assumptions.
    thread producer([&queue, count]() {
        for (int i = 0; i < count; ++i)
            queue.Push(i);
    });
    thread consumer([&queue, &consumed, count]() {
        for (int i = 0; i < count; ++i) {
            int out = -1;
            if (queue.Pop(out))
                consumed.push_back(out);
        }
    });
    producer.join();
    consumer.join();

    ASSERT_EQ(consumed.size(), static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        ASSERT_EQ(consumed[static_cast<size_t>(i)], i);
}

TEST(ConcurrencyTest, TestConcurrentQueueMultiProducerDrainConservesCount) {
    ConcurrentQueue_<int> queue;
    constexpr int numProducers = 4;
    constexpr int perProducer = 2500;
    constexpr int total = numProducers * perProducer;

    vector<thread> producers;
    producers.reserve(numProducers);
    for (int p = 0; p < numProducers; ++p) {
        producers.emplace_back([&queue, p, perProducer]() {
            for (int i = 0; i < perProducer; ++i)
                queue.Push(p * perProducer + i);
        });
    }
    for (auto& producer : producers)
        producer.join();

    // All producers have joined, so the drain below is single-threaded and timing-free: every
    // pushed value must be delivered exactly once.
    vector<char> seen(total, 0);
    int popped = 0;
    int out = -1;
    while (queue.TryPop(out)) {
        ASSERT_GE(out, 0);
        ASSERT_LT(out, total);
        ASSERT_EQ(seen[static_cast<size_t>(out)], 0) << "duplicate delivery of " << out;
        seen[static_cast<size_t>(out)] = 1;
        ++popped;
    }
    ASSERT_EQ(popped, total);
    ASSERT_TRUE(queue.Empty());
}
