//
// Created by wegam on 2024/9/1.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>

using Dal::TaskHandle_;
using Dal::ThreadPool_;
using Dal::Vector_;
using Dal::AAD::Number_;
using Dal::AAD::Tape_;

struct SimpleModel_ {
    Number_ s1_;
    Number_ s2_;
    SimpleModel_(const double s1, const double s2) : s1_(s1), s2_(s2) {}
};

#ifdef DAL_USE_CODIPACK_AAD
namespace {
    struct CoDiThreadResult_ {
        const void* tape_;
        double value_;
        double firstAdjoint_;
        double secondAdjoint_;
    };

    std::vector<CoDiThreadResult_> RunCoDiThreadWave(size_t nThreads) {
        std::vector<CoDiThreadResult_> results(nThreads);
        std::vector<std::thread> threads;
        threads.reserve(nThreads);

        std::mutex startMutex;
        std::condition_variable startCondition;
        size_t nReady = 0;
        bool start = false;
        bool tapesAreDistinct = false;

        for (size_t i = 0; i < nThreads; ++i) {
            threads.emplace_back([&, i]() {
                results[i].tape_ = &Dal::AAD::Tape()->tape_;

                {
                    std::unique_lock<std::mutex> lock(startMutex);
                    ++nReady;
                    startCondition.notify_all();
                    startCondition.wait(lock, [&]() { return start; });
                }

                if (!tapesAreDistinct)
                    return;

                Dal::AAD::Clear(*Dal::AAD::Tape());
                Number_ first(2.0);
                Number_ second(3.0);
                Dal::AAD::PutOnTape(first);
                Dal::AAD::PutOnTape(second);
                Dal::AAD::NewRecording(*Dal::AAD::Tape());

                Number_ value = first * second;
                Dal::AAD::Adjoint(value) = 1.0;
                Dal::AAD::PropagateToStart(*Dal::AAD::Tape());

                results[i].value_ = Dal::AAD::Value(value);
                results[i].firstAdjoint_ = Dal::AAD::Adjoint(first);
                results[i].secondAdjoint_ = Dal::AAD::Adjoint(second);
            });
        }

        {
            std::unique_lock<std::mutex> lock(startMutex);
            startCondition.wait(lock, [&]() { return nReady == nThreads; });
            std::vector<std::uintptr_t> tapeAddresses;
            tapeAddresses.reserve(nThreads);
            for (const auto& result : results)
                tapeAddresses.push_back(reinterpret_cast<std::uintptr_t>(result.tape_));
            std::sort(tapeAddresses.begin(), tapeAddresses.end());
            tapeAddresses.erase(std::unique(tapeAddresses.begin(), tapeAddresses.end()), tapeAddresses.end());
            tapesAreDistinct = tapeAddresses.size() == nThreads;
            start = true;
        }
        startCondition.notify_all();

        for (auto& thread : threads)
            thread.join();

        return results;
    }
} // namespace

TEST(AADTest, TestCoDiPackTapeIsPerThreadAcrossThreadLifetimes) {
    constexpr size_t N_THREADS = 8;
    constexpr size_t N_WAVES = 8;

    for (size_t wave = 0; wave < N_WAVES; ++wave) {
        const auto results = RunCoDiThreadWave(N_THREADS);
        std::vector<std::uintptr_t> tapeAddresses;
        tapeAddresses.reserve(N_THREADS);

        for (const auto& result : results)
            tapeAddresses.push_back(reinterpret_cast<std::uintptr_t>(result.tape_));

        std::sort(tapeAddresses.begin(), tapeAddresses.end());
        tapeAddresses.erase(std::unique(tapeAddresses.begin(), tapeAddresses.end()), tapeAddresses.end());
        ASSERT_EQ(tapeAddresses.size(), N_THREADS);

        for (const auto& result : results) {
            ASSERT_NEAR(result.value_, 6.0, 1e-10);
            ASSERT_NEAR(result.firstAdjoint_, 3.0, 1e-10);
            ASSERT_NEAR(result.secondAdjoint_, 2.0, 1e-10);
        }
    }
}
#endif

TEST(AADTest, TestAADMutiThread) {
    constexpr int batch_size = 2048;
    constexpr int n_rounds = 100000;
    constexpr double s1 = 2.0;
    constexpr double s2 = 3.0;

    ThreadPool_* pool = ThreadPool_::GetInstance();
    const size_t n_threads = pool->NumThreads();

    Vector_<TaskHandle_> futures;
    futures.reserve(n_rounds / batch_size + 1);

    Vector_<> greeks(3, 0.0);
    Vector_<Vector_<>> final_results(n_threads + 1, greeks);

    int first_round = 0;
    int rounds_left = n_rounds;

    while (rounds_left > 0) {
        const auto rounds_in_tasks = std::min(rounds_left, batch_size);
        futures.push_back(pool->SpawnTask([&, rounds_in_tasks]() {
            const size_t n_thread = ThreadPool_::ThreadNum();
            Dal::AAD::Clear(*Dal::AAD::Tape());

            SimpleModel_ model(s1, s2);
            Dal::AAD::Rewind(*Dal::AAD::Tape());

            Dal::AAD::PutOnTape(model.s1_);
            Dal::AAD::PutOnTape(model.s2_);
            Dal::AAD::NewRecording(*Dal::AAD::Tape());
            Dal::AAD::Mark(*Dal::AAD::Tape());

            auto& result = final_results[n_thread];

            double sum_val = 0.0;
            for (size_t i = 0; i < rounds_in_tasks; ++i) {
                Dal::AAD::RewindToMark(*Dal::AAD::Tape());
                Number_ res = model.s1_ * model.s2_;
                Dal::AAD::Adjoint(res) = 1.0;
                Dal::AAD::PropagateToMark(*Dal::AAD::Tape());
                sum_val += Dal::AAD::Value(res);
            }
            result[0] += sum_val;
            Dal::AAD::PropagateMarkToStart(*Dal::AAD::Tape());
            result[1] += Dal::AAD::Adjoint(model.s1_) / static_cast<double>(n_rounds);
            result[2] += Dal::AAD::Adjoint(model.s2_) / static_cast<double>(n_rounds);
            return true;
        }));
        rounds_left -= rounds_in_tasks;
        first_round += rounds_in_tasks;
    }

    for (auto& future : futures)
        pool->ActiveWait(future);

    for (const auto& res : final_results)
        for (size_t i = 0; i < greeks.size(); ++i)
            greeks[i] += res[i];

    ASSERT_NEAR(greeks[0] / n_rounds, 6.0, 1e-8);
    ASSERT_NEAR(greeks[1], 3.0, 1e-8);
    ASSERT_NEAR(greeks[2], 2.0, 1e-8);
}
