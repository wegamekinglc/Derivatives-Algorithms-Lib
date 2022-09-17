//
// Created by wegam on 2022/9/17.
//

#include <gtest/gtest.h>
#include <dal/math/aad/products/european.hpp>

using namespace Dal;
using namespace Dal::AAD;

TEST(EuropeanTest, TestEuropean) {
    auto global = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    const auto strike = 110.0;
    European_<double> prd(strike, exerciseDate);

    ASSERT_EQ(prd.TimeLine().size(), 1);
    ASSERT_EQ(prd.DefLine().size(), 1);

    Scenario_<double> path;
    AllocatePath(prd.DefLine(), path);
    ASSERT_EQ(path.size(), 1);

    Vector_<> payoffs;
    path[0].forwards_.front().front() = 100.0;
    prd.Payoffs(path, &payoffs);
    ASSERT_EQ(payoffs[0], 0.0);
    path[0].forwards_.front().front() = 130.0;
    prd.Payoffs(path, &payoffs);
    ASSERT_EQ(payoffs[0], 20.0);
}