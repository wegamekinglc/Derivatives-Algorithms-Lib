//
// Created by wegam on 2024/9/1.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/concurrency/threadpool.hpp>

using Dal::ThreadPool_;
using Dal::Vector_;
using Dal::AAD::Tape_;
using Dal::AAD::Number_;
using Dal::TaskHandle_;

struct SimpleModel_ {
    Number_ s1_;
    Number_ s2_;
    SimpleModel_(const double s1, const double s2)
    : s1_(s1), s2_(s2) {}
};


TEST(AADTest, TestAADMutiThread) {
    constexpr int batch_size = 2048;
    constexpr int n_rounds = 100000;
    constexpr double s1 = 2.0;
    constexpr double s2 = 3.0;

    ThreadPool_* pool = ThreadPool_::GetInstance();
    const size_t n_threads = pool->NumThreads();

    Vector_<Tape_> tapes(n_threads + 1);
    Tape_* mainThreadPtr = Number_::Tape();

    Vector_<TaskHandle_> futures;
    futures.reserve(n_rounds / batch_size + 1);

    Vector_<> greeks(3, 0.0);
    Vector_<Vector_<>> final_results(n_threads +1, greeks);

    int first_round = 0;
    int rounds_left = n_rounds;

    while (rounds_left > 0) {
        const auto rounds_in_tasks = std::min(rounds_left, batch_size);
        futures.push_back(pool->SpawnTask([&, rounds_in_tasks]() {
            const size_t n_thread = ThreadPool_::ThreadNum();
            Number_::SetTape(tapes[n_thread]);

            SimpleModel_ model(s1, s2);
            Number_::Tape()->Rewind();

            model.s1_.PutOnTape();
            model.s2_.PutOnTape();
            Number_::Tape()->Mark();

            auto& result = final_results[n_thread];

            double sum_val = 0.0;
            for (size_t i = 0; i < rounds_in_tasks; ++i) {
                Number_::Tape()->RewindToMark();
                Number_ res = model.s1_ * model.s2_;
                res.PropagateToMark();
                sum_val += res.value();
            }
            result[0] += sum_val;
            Number_::PropagateMarkToStart();
            result[1] += model.s1_.Adjoint() / static_cast<double>(n_rounds);
            result[2] += model.s2_.Adjoint() / static_cast<double>(n_rounds);
            return true;
        }));
        rounds_left -= rounds_in_tasks;
        first_round += rounds_in_tasks;
    }

    for (auto& future : futures)
        pool->ActiveWait(future);

    for (const auto& res: final_results)
        for (size_t i = 0; i < greeks.size(); ++i)
        greeks[i] += res[i];

    Number_::SetTape(*mainThreadPtr);
    ASSERT_NEAR(greeks[0] / n_rounds, 6.0, 1e-8);
    ASSERT_NEAR(greeks[1], 3.0, 1e-8);
    ASSERT_NEAR(greeks[2], 2.0, 1e-8);

}