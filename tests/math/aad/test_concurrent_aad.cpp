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
    Vector_<bool> model_init(n_threads + 1, false);
    Vector_<std::unique_ptr<SimpleModel_>> models(n_threads + 1);
    for (auto& m : models)
        m = std::make_unique<SimpleModel_>(s1, s2);

    Vector_<TaskHandle_> futures;
    futures.reserve(n_rounds / batch_size + 1);
    Vector_<> sim_results;
    sim_results.reserve(n_rounds / batch_size + 1);

    int first_round = 0;
    int rounds_left = n_rounds;

    while (rounds_left > 0) {
        const auto rounds_in_tasks = std::min(rounds_left, batch_size);
        sim_results.emplace_back(0.0);
        auto& sim_result = sim_results.back();

        futures.push_back(pool->SpawnTask([&, rounds_in_tasks]() {
            std::cout << '\0';  // TODO: a trick to make sure thread data is ready
            const size_t n_thread = ThreadPool_::ThreadNum();
            Number_::SetTape(tapes[n_thread]);
            auto& model = models[n_thread];

            if (!model_init[n_thread]) {
                Number_::Tape()->Rewind();
                model->s1_.PutOnTape();
                model->s2_.PutOnTape();
                Number_::Tape()->Mark();
                model_init[n_thread] = true;
            }

            double sum_val = 0.0;
            for (size_t i = 0; i < rounds_in_tasks; ++i) {
                Number_::Tape()->RewindToMark();
                Number_ res = model->s1_ * model->s2_;
                res.PropagateToMark();
                sum_val += res.value();
            }
            sim_result = sum_val;
            return true;
        }));
        rounds_left -= rounds_in_tasks;
        first_round += rounds_in_tasks;
    }

    for (auto& future : futures)
        pool->ActiveWait(future);

    for (size_t i = 0; i < tapes.size(); ++i) {
        if (model_init[i]) {
            Number_::SetTape(tapes[i]);
            Number_::PropagateMarkToStart();
        }
    }

    double aggregated = 0.0;
    for (const auto& s: sim_results)
        aggregated += s;

    Vector_<> greeks(2, 0.0);
    for (auto k = 0; k < model_init.size(); ++k) {
        if (model_init[k]) {
            greeks[0] += models[k]->s1_.Adjoint() / static_cast<double>(n_rounds);
            greeks[1] += models[k]->s2_.Adjoint() / static_cast<double>(n_rounds);
        }
    }

    Number_::SetTape(*mainThreadPtr);
    ASSERT_NEAR(aggregated / n_rounds, 6.0, 1e-8);
    ASSERT_NEAR(greeks[0], 3.0, 1e-8);
    ASSERT_NEAR(greeks[1], 2.0, 1e-8);

}