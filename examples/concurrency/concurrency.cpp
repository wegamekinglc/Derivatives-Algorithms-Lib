//
// Created by wegamekinglc on 22-10-27.
//

#include <dal/math/operators.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/utilities/timer.hpp>
#include <iostream>

using namespace Dal;
using namespace std::chrono_literals;


void LongRunningTask(int n1, int n2) {
    for(int i = n1; i < n2; ++i) {
        NCDF(1. / (Log(static_cast<double>(i))+ 1.0001), true);
    }
}


int main() {
    std::cout << "starting threading test ..." << std::endl;
    const int n = 200000000;

    Timer_ timer;
    LongRunningTask(0, n);

    std::cout << "Elapsed (single threaded): " << timer.Elapsed<milliseconds>() << " ms" << std::endl;

    ThreadPool_* pool = ThreadPool_::GetInstance();
    const int n_packs = 100;
    const int sub_n = n / n_packs;

    timer.Reset();
    std::vector<TaskHandle_> futures;
    for (int k = 0; k < n_packs; ++k) {
        const int n1 = k * sub_n;
        const int n2 = (k + 1) * sub_n;
        futures.push_back(pool->SpawnTask([n1, n2]() {
            LongRunningTask(n1, n2);
            return true;
        }));
    }
    for (auto& future : futures)
        pool->ActiveWait(future);
    std::cout << "Elapsed (multi-threaded): " << timer.Elapsed<milliseconds>() << " ms" << std::endl;

    return 0;
}