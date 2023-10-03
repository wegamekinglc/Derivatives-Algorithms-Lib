//
// Created by wegam on 2023/2/18.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/vectors.hpp>
#include <dal/concurrency/threadpool.hpp>

using namespace Dal;
using namespace Dal::AAD;

template <class T_>
T_ BlackTest(const T_& fwd, const T_& vol, const T_& numeraire, const T_& strike, const T_& expiry, bool is_call) {
    static const double M_SQRT_2 = 1.4142135623730951;
    const double omega = is_call ? 1.0 : -1.0;
    T_ y(0.0);
    T_ sqrt_var = vol * sqrt(expiry);
    T_ d_minus = log(fwd / strike) / sqrt_var - 0.5 * sqrt_var;
    T_ d_plus = d_minus + sqrt_var;
    y = numeraire * omega * (0.5 * fwd * erfc(-d_plus / M_SQRT_2) - strike * 0.5 * erfc(-d_minus / M_SQRT_2));
    return y;
}


struct TestModel_ {
    Number_ fwd_;
    Number_ vol_;
    Number_ numeraire_;
    Number_ strike_;
    Number_ expiry_;

    TestModel_(double fwd, double vol, double numeraire, double strike, double expiry)
    : fwd_(fwd), vol_(vol), numeraire_(numeraire), strike_(strike), expiry_(expiry) {}
};


auto ModelInit(Tape_& tape, TestModel_& model) {
    tape.reset();
    tape.setActive();
    tape.registerInput(model.fwd_);
    tape.registerInput(model.vol_);
    tape.registerInput(model.numeraire_);
    tape.registerInput(model.strike_);
    tape.registerInput(model.expiry_);
    return tape.getPosition();
}


TEST(AADTest, TestWithCheckpoint) {
    auto& tape = Number_::getTape();
    tape.setActive();

    Number_ s1(1.0);
    Number_ s2(2.0);

    tape.registerInput(s1);
    tape.registerInput(s2);

    Number_ s3 = s1 + s2;
    Position_ begin = tape.getPosition();
    Number_ value = s3 * 2.0;
    value.setGradient(1.0);
    tape.evaluate(tape.getPosition(), begin);

    ASSERT_NEAR(value.value(), 6.0, 1e-10);
    ASSERT_NEAR(s3.getGradient(), 2.0, 1e-10);

    tape.evaluate(begin, tape.getZeroPosition());
    ASSERT_NEAR(s1.getGradient(), 2.0, 1e-10);
}

TEST(AADTest, TestWithCheckpointWithForLoop) {
    auto& tape = Number_::getTape();
    for (int m = 0; m < 3; ++m) {
        tape.reset();
        tape.setActive();

        int n = 10000;
        Number_ s1(1.0);
        Number_ s2(2.0);

        tape.registerInput(s1);
        tape.registerInput(s2);

        Number_ s3 = s1 + s2;
        Position_ begin = tape.getPosition();
        for (int i = 0; i < n; ++i) {
            Number_ value;
            if (i % 2 == 0)
                value = s3 * 1.01;
            else
                value = s3 * 0.99;
            value.setGradient(1.0);
            tape.evaluate(tape.getPosition(), begin);
            if (i % 2 == 0) {
                ASSERT_NEAR(value.value(), 3 * 1.01, 1e-10);
                ASSERT_NEAR(s3.getGradient(), (i + 1) / 2 * 2 + (i + 1) % 2 * 1.01, 1e-10);
            } else {
                ASSERT_NEAR(value.value(), 3 * 0.99, 1e-10);
                ASSERT_NEAR(s3.getGradient(), (i + 1) / 2 * 2 + (i + 1) % 2 * 0.99, 1e-10);
            }
            tape.resetTo(begin);
        }
        tape.evaluate(tape.getPosition(), tape.getZeroPosition());
        ASSERT_NEAR(s1.getGradient(), n, 1e-10);
        ASSERT_NEAR(s2.getGradient(), n, 1e-10);
    }
}

TEST(AADTest, TestWithCheckpointWithMultiThreading) {
    int n_rounds = 1000;
    double fwd = 1.00;
    double vol = 0.20;
    double numeraire = 1.0;
    double strike = 1.20;
    double expiry = 3.0;
    bool is_call = true;

    ThreadPool_* pool = ThreadPool_::GetInstance();
    const size_t n_threads = pool->NumThreads();

    #ifndef USE_AADET
        const int batch_size = std::max(8, static_cast<int>(n_rounds / n_threads + 1));
    #else
        const int batch_size = 8;
    #endif
    Vector_<TaskHandle_> futures;
    futures.reserve(n_rounds / batch_size + 1);
    Vector_<> sim_results;
    sim_results.reserve(n_rounds / batch_size + 1);

    Vector_<AAD::Position_> start_positions(n_threads);
    Vector_<bool> model_init(n_threads, false);
    Vector_<AAD::Tape_*> tapes(n_threads, nullptr);
    Vector_<std::unique_ptr<TestModel_>> models(n_threads);
    for (auto& m : models)
        m = std::make_unique<TestModel_>(fwd, vol, numeraire, strike, expiry);

    int first_round = 0;
    int rounds_left = n_rounds;
    int loop_i = 0;

    Vector_<> greeks(5, 0.0);

    while (rounds_left > 0) {
        auto rounds_in_tasks = std::min(rounds_left, batch_size);
        sim_results.emplace_back(0.0);
        auto& sim_result = sim_results[loop_i];
        loop_i += 1;

        futures.push_back(pool->SpawnTask([&, first_round, rounds_in_tasks]() {
            const size_t n_thread = ThreadPool_::ThreadNum();
            if (!tapes[n_thread])
                tapes[n_thread] = &AAD::Number_::getTape();
            Tape_* tape = tapes[n_thread];
            auto& model = models[n_thread];

            if (!model_init[n_thread]) {
                start_positions[n_thread] = ModelInit(*tape, *model);
                model_init[n_thread] = true;
            }
            auto& pos = start_positions[n_thread];

            double sum_val = 0.0;
            for (size_t i = 0; i < rounds_in_tasks; i++) {
                Number_ res = BlackTest(model->fwd_,
                                        model->vol_,
                                        model->numeraire_,
                                        model->strike_,
                                        model->expiry_,
                                        is_call);
                res.setGradient(1.0);
                tape->evaluate(tape->getPosition(), pos);
                sum_val += res.value();
                tape->resetTo(pos, false);
            }
            sim_result = sum_val;
#ifndef USE_AADET
            greeks[0] += model->fwd_.getGradient() / static_cast<double>(n_rounds);
            greeks[1] += model->vol_.getGradient() / static_cast<double>(n_rounds);
            greeks[2] += model->numeraire_.getGradient() / static_cast<double>(n_rounds);
            greeks[3] += model->strike_.getGradient() / static_cast<double>(n_rounds);
            greeks[4] += model->expiry_.getGradient() / static_cast<double>(n_rounds);
#endif

            return true;
        }));
        rounds_left -= rounds_in_tasks;
        first_round += rounds_in_tasks;
    }

    for (auto& future : futures)
        pool->ActiveWait(future);

    double aggregated = 0.0;
    for (const auto& s: sim_results)
        aggregated += s;

    ASSERT_NEAR(aggregated / n_rounds, 0.0714668, 1e-6);

#ifdef USE_AADET
    for (size_t i = 0; i < models.size(); ++i)
        if (model_init[i])
            greeks[0] += models[i]->fwd_.getGradient() / static_cast<double>(n_rounds);
    

    for (size_t i = 0; i < models.size(); ++i)
        if (model_init[i])
            greeks[1] += models[i]->vol_.getGradient() / static_cast<double>(n_rounds);
    


    for (size_t i = 0; i < models.size(); ++i)
        if (model_init[i])
            greeks[2] += models[i]->numeraire_.getGradient() / static_cast<double>(n_rounds);
    

    for (size_t i = 0; i < models.size(); ++i)
        if (model_init[i])
            greeks[3] += models[i]->strike_.getGradient() / static_cast<double>(n_rounds);
    

    for (size_t i = 0; i < models.size(); ++i)
        if (model_init[i])
            greeks[4] += models[i]->expiry_.getGradient() / static_cast<double>(n_rounds);
#endif

    ASSERT_NEAR(greeks[0], 0.362002, 1e-6);
    ASSERT_NEAR(greeks[1], 0.649225, 1e-6);
    ASSERT_NEAR(greeks[2], 0.0714668, 1e-6);
    ASSERT_NEAR(greeks[3], -0.242113, 1e-6);
    ASSERT_NEAR(greeks[4], 0.0216408, 1e-6);
    
}