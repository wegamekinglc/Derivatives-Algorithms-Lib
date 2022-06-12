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

TEST(BlackScholesTest, TestBlackScholes) {
    XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 12));
    Date_ exerciseDate(2024, 6, 11);
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


TEST(BlackScholesTest, TestBlackScholesAAD) {

}