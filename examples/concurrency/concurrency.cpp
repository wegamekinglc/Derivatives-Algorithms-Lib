//
// Created by wegamekinglc on 22-10-27.
//

#include <thread>
#include <chrono>
#include <iostream>
#include <dal/concurrency/threadpool.hpp>
#include <dal/utilities/timer.hpp>

using namespace Dal;
using namespace std::chrono_literals;

void LongRunningTask(int n) {
    int sum = 0;
    for(int i = 0; i < n; ++i) {
        sum += i;
        std::this_thread::sleep_for(2ms);
    }
}


int main() {
    const int n = 5000;

    Timer_ timer;
    LongRunningTask(n);

    std::cout << "Elapsed (single threaded): " << timer.Elapsed<milliseconds>() << " ms" << std::endl;

    ThreadPool_* pool = ThreadPool_::GetInstance();
    const int nThread = pool->NumThreads();

    const int sub_n = n / nThread;

    timer.Reset();
    Vector_<TaskHandle_> futures;
    futures.push_back(pool->SpawnTask([&]() {LongRunningTask(sub_n); return true; }));
    for (auto& future : futures)
        pool->ActiveWait(future);
    std::cout << "Elapsed (multi-threaded): " << timer.Elapsed<milliseconds>() << " ms" << std::endl;

    return 0;
}