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


auto ModelInit(TestModel_& model) {
    AAD::Tape_& tape = *Number_::tape_;
    tape.Rewind();
    tape.registerInput(model.fwd_);
    tape.registerInput(model.vol_);
    tape.registerInput(model.numeraire_);
    tape.registerInput(model.strike_);
    tape.registerInput(model.expiry_);
    tape.Mark();
}


TEST(AADTest, TestWithCheckpoint) {
    Number_::tape_->Clear();

    Number_ s1(1.0);
    Number_ s2(2.0);

    Number_::tape_->registerInput(s1);
    Number_::tape_->registerInput(s2);

    Number_ s3 = s1 + s2;
    Position_ begin = Number_::tape_->getPosition();
    Number_ value = s3 * 2.0;
    SetGradient(value, 1.0);
    Evaluate(Number_::tape_, begin);

    ASSERT_NEAR(value.value(), 6.0, 1e-10);
    ASSERT_NEAR(GetGradient(s3), 2.0, 1e-10);

    ResetToPos(Number_::tape_, begin);
    Evaluate(Number_::tape_);
    ASSERT_NEAR(GetGradient(s1), 2.0, 1e-10);
}

TEST(AADTest, TestWithCheckpointWithForLoop) {
    Number_::tape_->Clear();

    for (int m = 0; m < 3; ++m) {

        int n = 10000;
        Number_ s1(1.0);
        Number_ s2(2.0);

        Number_::tape_->registerInput(s1);
        Number_::tape_->registerInput(s2);

        Number_ s3 = s1 + s2;
        Position_ begin = Number_::tape_->getPosition();
        for (int i = 0; i < n; ++i) {
            Number_ value;
            if (i % 2 == 0)
                value = s3 * 1.01;
            else
                value = s3 * 0.99;
            SetGradient(value, 1.0);
            Evaluate(Number_::tape_, begin);
            if (i % 2 == 0) {
                ASSERT_NEAR(value.value(), 3 * 1.01, 1e-10);
                ASSERT_NEAR(GetGradient(s3), (i + 1) / 2 * 2 + (i + 1) % 2 * 1.01, 1e-10);
            } else {
                ASSERT_NEAR(value.value(), 3 * 0.99, 1e-10);
                ASSERT_NEAR(GetGradient(s3), (i + 1) / 2 * 2 + (i + 1) % 2 * 0.99, 1e-10);
            }
            Number_::tape_->resetTo(begin);
        }
        Evaluate(Number_::tape_);
        ASSERT_NEAR(GetGradient(s1), n, 1e-10);
        ASSERT_NEAR(GetGradient(s2), n, 1e-10);
    }
}

TEST(AADTest, TestWithCheckpointWithMultiThreading) {
    int n_rounds = 100000;
    const int batch_size = 8196;
    double fwd = 1.00;
    double vol = 0.20;
    double numeraire = 1.0;
    double strike = 1.20;
    double expiry = 3.0;
    bool is_call = true;

    ThreadPool_* pool = ThreadPool_::GetInstance();
    const size_t n_threads = pool->NumThreads();

    Vector_<TaskHandle_> futures;
    futures.reserve(n_rounds / batch_size + 1);
    Vector_<> sim_results;
    sim_results.reserve(n_rounds / batch_size + 1);

    Vector_<bool> model_init(n_threads + 1, false);
    Vector_<AAD::Tape_> tapes(n_threads);
    Vector_<std::unique_ptr<TestModel_>> models(n_threads + 1);
    for (auto& m : models)
        m = std::make_unique<TestModel_>(fwd, vol, numeraire, strike, expiry);

    Number_::tape_->Clear();
    ModelInit(*models[0]);
    model_init[0] = true;

    int first_round = 0;
    int rounds_left = n_rounds;
    int loop_i = 0;

    Vector_<> greeks(5, 0.0);

    while (rounds_left > 0) {
        auto rounds_in_tasks = std::min(rounds_left, batch_size);
        sim_results.emplace_back(0.0);
        auto& sim_result = sim_results[loop_i];
        loop_i += 1;

        futures.push_back(pool->SpawnTask([&, rounds_in_tasks]() {
            const size_t n_thread = ThreadPool_::ThreadNum();
            if (n_thread > 0)
                Number_::tape_ = &tapes[n_thread - 1];

            auto& model = models[n_thread];

            if (!model_init[n_thread]) {
                ModelInit(*model);
                model_init[n_thread] = true;
            }

            double sum_val = 0.0;
            for (size_t i = 0; i < rounds_in_tasks; ++i) {
                Number_::tape_->RewindToMark();
                Number_ res = BlackTest(model->fwd_,
                                        model->vol_,
                                        model->numeraire_,
                                        model->strike_,
                                        model->expiry_,
                                        is_call);
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

    Number_::PropagateMarkToStart();
    //  And on the worker thread's tapes
    Tape_* mainThreadPtr = Number_::tape_;
    for (size_t i = 0; i < n_threads; ++i) {
        if (model_init[i + 1]) {
            Number_::tape_ = &tapes[i];
            Number_::PropagateMarkToStart();
        }
    }

    double aggregated = 0.0;
    for (const auto& s: sim_results)
        aggregated += s;

    for (auto k = 0; k < model_init.size(); ++k) {
        if (model_init[k]) {
            greeks[0] += GetGradient(models[k]->fwd_) / static_cast<double>(n_rounds);
            greeks[1] += GetGradient(models[k]->vol_) / static_cast<double>(n_rounds);
            greeks[2] += GetGradient(models[k]->numeraire_) / static_cast<double>(n_rounds);
            greeks[3] += GetGradient(models[k]->strike_) / static_cast<double>(n_rounds);
            greeks[4] += GetGradient(models[k]->expiry_) / static_cast<double>(n_rounds);
        }
    }

    Number_::tape_ = mainThreadPtr;
    Number_::tape_->Clear();

    ASSERT_NEAR(aggregated / n_rounds, 0.0714668, 1e-6);
    ASSERT_NEAR(greeks[0], 0.362002, 1e-6);
    ASSERT_NEAR(greeks[1], 0.649225, 1e-6);
    ASSERT_NEAR(greeks[2], 0.0714668, 1e-6);
    ASSERT_NEAR(greeks[3], -0.242113, 1e-6);
    ASSERT_NEAR(greeks[4], 0.0216408, 1e-6);

}