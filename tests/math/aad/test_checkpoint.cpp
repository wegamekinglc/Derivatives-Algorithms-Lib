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
    Reset(&tape);
    SetActive(&tape);

    tape.registerInput(model.fwd_);
    tape.registerInput(model.vol_);
    tape.registerInput(model.numeraire_);
    tape.registerInput(model.strike_);
    tape.registerInput(model.expiry_);

    NewRecording(&tape);
    return tape.getPosition();
}


TEST(AADTest, TestWithCheckpoint) {
#ifndef USE_XAD
    auto& tape = GetTape();
#else
    auto tape = GetTape();
#endif
    Reset(&tape);
    SetActive(&tape);

    Number_ s1(1.0);
    Number_ s2(2.0);

    tape.registerInput(s1);
    tape.registerInput(s2);
    NewRecording(&tape);

    Number_ s3 = s1 + s2;
    Position_ begin = tape.getPosition();
    Number_ value = s3 * 2.0;
    SetGradient(value, 1.0);
    Evaluate(&tape, begin);

    ASSERT_NEAR(value.value(), 6.0, 1e-10);
    ASSERT_NEAR(GetGradient(s3), 2.0, 1e-10);

    ResetToPos(&tape, begin);
    Evaluate(&tape);
    ASSERT_NEAR(GetGradient(s1), 2.0, 1e-10);
}

TEST(AADTest, TestWithCheckpointWithForLoop) {
#ifndef USE_XAD
    auto& tape = GetTape();
#else
    auto tape = GetTape();
#endif
    for (int m = 0; m < 3; ++m) {
        Reset(&tape);
        SetActive(&tape);

        int n = 10000;
        Number_ s1(1.0);
        Number_ s2(2.0);

        tape.registerInput(s1);
        tape.registerInput(s2);

        NewRecording(&tape);

        Number_ s3 = s1 + s2;
        Position_ begin = tape.getPosition();
        for (int i = 0; i < n; ++i) {
            Number_ value;
            if (i % 2 == 0)
                value = s3 * 1.01;
            else
                value = s3 * 0.99;
            SetGradient(value, 1.0);
            Evaluate(&tape, begin);
            if (i % 2 == 0) {
                ASSERT_NEAR(value.value(), 3 * 1.01, 1e-10);
                ASSERT_NEAR(GetGradient(s3), (i + 1) / 2 * 2 + (i + 1) % 2 * 1.01, 1e-10);
            } else {
                ASSERT_NEAR(value.value(), 3 * 0.99, 1e-10);
                ASSERT_NEAR(GetGradient(s3), (i + 1) / 2 * 2 + (i + 1) % 2 * 0.99, 1e-10);
            }
            tape.resetTo(begin);
        }
        Evaluate(&tape);
        ASSERT_NEAR(GetGradient(s1), n, 1e-10);
        ASSERT_NEAR(GetGradient(s2), n, 1e-10);
    }
}

TEST(AADTest, TestWithCheckpointWithMultiThreading) {
    int n_rounds = 100000;
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

    Vector_<Position_> start_positions(n_threads);
    Vector_<bool> model_init(n_threads, false);
    Vector_<Tape_*> tapes(n_threads, nullptr);
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
#ifndef USE_XAD
            tapes[n_thread]= &GetTape();
            Tape_* tape = tapes[n_thread];
#else
            auto this_tap = GetTape();
            Tape_* tape = &this_tap;
#endif
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
                SetGradient(res, 1.0);
                Evaluate(tape, pos);
                sum_val += res.value();
                ResetToPos(tape, pos);
            }
            sim_result = sum_val;
#ifndef USE_AADET
            greeks[0] += GetGradient(model->fwd_) / static_cast<double>(n_rounds);
            greeks[1] += GetGradient(model->vol_) / static_cast<double>(n_rounds);
            greeks[2] += GetGradient(model->numeraire_) / static_cast<double>(n_rounds);
            greeks[3] += GetGradient(model->strike_) / static_cast<double>(n_rounds);
            greeks[4] += GetGradient(model->expiry_) / static_cast<double>(n_rounds);
            Reset(tape);
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
            greeks[0] += GetGradient(models[i]->fwd_) / static_cast<double>(n_rounds);

    for (size_t i = 0; i < models.size(); ++i)
        if (model_init[i])
            greeks[1] += GetGradient(models[i]->vol_) / static_cast<double>(n_rounds);

    for (size_t i = 0; i < models.size(); ++i)
        if (model_init[i])
            greeks[2] += GetGradient(models[i]->numeraire_) / static_cast<double>(n_rounds);

    for (size_t i = 0; i < models.size(); ++i)
        if (model_init[i])
            greeks[3] += GetGradient(models[i]->strike_) / static_cast<double>(n_rounds);

    for (size_t i = 0; i < models.size(); ++i)
        if (model_init[i])
            greeks[4] += GetGradient(models[i]->expiry_) / static_cast<double>(n_rounds);
#endif

    ASSERT_NEAR(greeks[0], 0.362002, 1e-6);
    ASSERT_NEAR(greeks[1], 0.649225, 1e-6);
    ASSERT_NEAR(greeks[2], 0.0714668, 1e-6);
    ASSERT_NEAR(greeks[3], -0.242113, 1e-6);
    ASSERT_NEAR(greeks[4], 0.0216408, 1e-6);
}