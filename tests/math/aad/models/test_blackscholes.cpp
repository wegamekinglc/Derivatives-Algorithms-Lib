//
// Created by wegam on 2021/12/25.
//

#include <dal/math/aad/models/blackscholes.hpp>
#include <dal/math/aad/products/european.hpp>
#include <dal/math/random/quasirandom.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/math/aad/simulation.hpp>
#include <dal/time/date.hpp>
#include <dal/storage/globals.hpp>
#include <gtest/gtest.h>

using namespace Dal;
using namespace Dal::AAD;

TEST(BlackScholesTest, TestBlackScholes) {
    auto global = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    const double strike = 11.0;
    const double spot = 10.0;
    const double vol = 0.20;
    const double rate = 0.034;
    const double div = 0.021;
    const size_t n_paths = 1000000;
    const size_t n_dim = 1;

    European_<double> prd(strike, exerciseDate);
    BlackScholes_<double> mdl(spot, vol, false, rate, div);

    std::unique_ptr<Random_> rand(NewSobol(n_dim, n_paths));
    auto res = MCSimulation(prd, mdl, rand, n_paths);
    auto sum = 0.0;
    for (auto row = 0; row < res.Rows(); ++row)
        sum += res(row, 0);
    auto calculated = sum / static_cast<double>(res.Rows());
    auto expected = 0.806119;
    ASSERT_NEAR(calculated, expected, 1e-5);
}


TEST(BlackScholesTest, TestBlackScholesParallel) {
    auto global = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    const double strike = 11.0;
    const double spot = 10.0;
    const double vol = 0.20;
    const double rate = 0.034;
    const double div = 0.021;
    const size_t n_paths = 40000000;
    const size_t n_dim = 1;

    European_<double> prd(strike, exerciseDate);
    BlackScholes_<double> mdl(spot, vol, false, rate, div);

    std::unique_ptr<Random_> rand(New(RNGType_("MRG32"), 2048, n_dim));
    auto res = MCParallelSimulation(prd, mdl, rand, n_paths);
    auto sum = 0.0;
    for (auto row = 0; row < res.Rows(); ++row)
        sum += res(row, 0);
    auto calculated = sum / static_cast<double>(res.Rows());
    auto expected = 0.806119;
    ASSERT_NEAR(calculated, expected, 1e-3);
}


TEST(BlackScholesTest, TestBlackScholesAAD) {
    auto global = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    const double strike(11.0);
    const Number_ spot(10.0);
    const Number_ vol(0.20);
    const Number_ rate(0.034);
    const Number_ div(0.021);
    const size_t n_paths = 1000000;
    const size_t n_dim = 1;

    European_<Number_> prd(strike, exerciseDate);
    BlackScholes_<Number_> mdl(spot, vol, false, rate, div);

    std::unique_ptr<Random_> rand(NewSobol(n_dim, n_paths));
    auto res = MCSimulationAAD(prd, mdl, rand, n_paths);
    ASSERT_NEAR(res.risks_[0], 0.43986485, 1e-6);
    ASSERT_NEAR(res.risks_[1], 5.38087423, 1e-4);
    ASSERT_NEAR(res.risks_[2], 7.18505725, 1e-4);
    ASSERT_NEAR(res.risks_[3], -8.7972975, 1e-4);
}


TEST(BlackScholesTest, TestBlackScholesAADParallel) {
    auto global = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    const double strike(11.0);
    const Number_ spot(10.0);
    const Number_ vol(0.20);
    const Number_ rate(0.034);
    const Number_ div(0.021);
    const size_t n_paths = 40000000;
    const size_t n_dim = 1;

    European_<Number_> prd(strike, exerciseDate);
    BlackScholes_<Number_> mdl(spot, vol, false, rate, div);

    std::unique_ptr<Random_> rand(New(RNGType_("MRG32"), 1024, n_dim));
    auto res = MCParallelSimulationAAD(prd, mdl, rand, n_paths);
    ASSERT_NEAR(res.risks_[0], 0.43986485, 1e-2);
    ASSERT_NEAR(res.risks_[1], 5.38087423, 1e-2);
    ASSERT_NEAR(res.risks_[2], 7.18505725, 1e-2);
    ASSERT_NEAR(res.risks_[3], -8.7972975, 1e-2);
}