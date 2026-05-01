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
T_ BlackTest(const T_& fwd, const T_& vol, const T_& numeraire, const T_& strike, const T_& expiry, bool isCall) {
    static const double M_SQRT_2 = 1.4142135623730951;
    const double omega = isCall ? 1.0 : -1.0;
    T_ y(0.0);
    T_ sqrtVar = vol * sqrt(expiry);
    T_ dMinus = log(fwd / strike) / sqrtVar - 0.5 * sqrtVar;
    T_ dPlus = dMinus + sqrtVar;
    y = numeraire * omega * (0.5 * fwd * erfc(-dPlus / M_SQRT_2) - strike * 0.5 * erfc(-dMinus / M_SQRT_2));
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
    Rewind(*Tape());
    PutOnTape(model.fwd_);
    PutOnTape(model.vol_);
    PutOnTape(model.numeraire_);
    PutOnTape(model.strike_);
    PutOnTape(model.expiry_);
    NewRecording(*Tape());
    Mark(*Tape());
}


TEST(AADTest, TestWithCheckpoint) {
    Clear(*Tape());

    Number_ s1(1.0);
    Number_ s2(2.0);

    PutOnTape(s1);
    PutOnTape(s2);
    NewRecording(*Tape());

    Number_ s3 = s1 + s2;
    Mark(*Tape());
    Number_ value = s3 * 2.0;
    Adjoint(value) = 1.0;
    PropagateToMark(*Tape());

    ASSERT_NEAR(Value(value), 6.0, 1e-10);
    ASSERT_NEAR(Adjoint(s3), 2.0, 1e-10);
    PropagateMarkToStart(*Tape());
    ASSERT_NEAR(Adjoint(s1), 2.0, 1e-10);
}

TEST(AADTest, TestWithCheckpointWithForLoop) {
    Clear(*Tape());

    for (int m = 0; m < 3; ++m) {

        int n = 10000;
        Number_ s1(1.0);
        Number_ s2(2.0);

        PutOnTape(s1);
        PutOnTape(s2);
        NewRecording(*Tape());

        Number_ s3 = s1 + s2;
        Mark(*Tape());
        for (int i = 0; i < n; ++i) {
            RewindToMark(*Tape());
            Number_ value;
            if (i % 2 == 0)
                value = s3 * 1.01;
            else
                value = s3 * 0.99;
            Adjoint(value) = 1.0;
            PropagateToMark(*Tape());
            if (i % 2 == 0) {
                ASSERT_NEAR(Value(value), 3 * 1.01, 1e-10);
                ASSERT_NEAR(Adjoint(s3), (i + 1) / 2 * 2 + (i + 1) % 2 * 1.01, 1e-10);
            } else {
                ASSERT_NEAR(Value(value), 3 * 0.99, 1e-10);
                ASSERT_NEAR(Adjoint(s3), (i + 1) / 2 * 2 + (i + 1) % 2 * 0.99, 1e-10);
            }
        }
        PropagateMarkToStart(*Tape());
        ASSERT_NEAR(Adjoint(s1), n, 1e-10);
        ASSERT_NEAR(Adjoint(s2), n, 1e-10);
    }
}

TEST(AADTest, TestWithCheckpointWithMultiThreading) {
    int nRounds = 100000;
    int batchSize = 2048;
    double fwd = 1.00;
    double vol = 0.20;
    double numeraire = 1.0;
    double strike = 1.20;
    double expiry = 3.0;
    bool isCall = true;

    ThreadPool_* pool = ThreadPool_::GetInstance();
    const size_t nThreads = pool->NumThreads();

    batchSize = std::max(batchSize, static_cast<int>(nRounds / nThreads) + 1);
    Vector_<TaskHandle_> futures;
    futures.reserve(nRounds / batchSize + 1);

    int roundsLeft = nRounds;

    Vector_<> greeks(6, 0.0);
    Vector_<Vector_<>> finalResults(nThreads, greeks);

    while (roundsLeft > 0) {
        auto roundsInTasks = std::min(roundsLeft, batchSize);

        futures.push_back(pool->SpawnTask([&, roundsInTasks]() {
            const size_t nThread = ThreadPool_::ThreadNum();
            Clear(*Tape());
            std::unique_ptr<TestModel_> model = std::make_unique<TestModel_>(fwd, vol, numeraire, strike, expiry);
            ModelInit(*model);
            auto& results = finalResults[nThread];

            double sumVal = 0.0;
            for (size_t i = 0; i < roundsInTasks; ++i) {
                RewindToMark(*Tape());
                Number_ res = BlackTest(model->fwd_,
                                        model->vol_,
                                        model->numeraire_,
                                        model->strike_,
                                        model->expiry_,
                                        isCall);
                Adjoint(res) = 1.0;
                PropagateToMark(*Tape());
                sumVal += Value(res);
            }

            PropagateMarkToStart(*Tape());
            results[0] += sumVal;
            results[1] += Adjoint(model->fwd_) / static_cast<double>(nRounds);
            results[2] += Adjoint(model->vol_) / static_cast<double>(nRounds);
            results[3] += Adjoint(model->numeraire_) / static_cast<double>(nRounds);
            results[4] += Adjoint(model->strike_) / static_cast<double>(nRounds);
            results[5] += Adjoint(model->expiry_) / static_cast<double>(nRounds);
            return true;
        }));
        roundsLeft -= roundsInTasks;
    }

    for (auto& future: futures)
        pool->ActiveWait(future);

    for (const auto& res: finalResults)
        for (size_t i = 0; i < greeks.size(); ++i)
            greeks[i] += res[i];

    ASSERT_NEAR(greeks[0] / static_cast<double>(nRounds), 0.0714668, 1e-6);
    ASSERT_NEAR(greeks[1], 0.362002, 1e-6);
    ASSERT_NEAR(greeks[2], 0.649225, 1e-6);
    ASSERT_NEAR(greeks[3], 0.0714668, 1e-6);
    ASSERT_NEAR(greeks[4], -0.242113, 1e-6);
    ASSERT_NEAR(greeks[5], 0.0216408, 1e-6);

}
