//
// Created by wegam on 2021/12/25.
//

#include <dal/math/aad/models/blackscholes.hpp>
#include <dal/storage/globals.hpp>
#include <dal/script/event.hpp>
#include <dal/script/simulation.hpp>
#include <gtest/gtest.h>

using namespace Dal;
using namespace Dal::AAD;
using namespace Dal::Script;

TEST(ModelsTest, TestBlackScholes) {
    auto global = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    const double strike = 11.0;
    const double spot = 10.0;
    const double vol = 0.20;
    const double rate = 0.034;
    const double div = 0.021;
    const String_ rsg = "sobol";
    const size_t num_paths = 10000000;

    Vector_<Cell_> eventDates(1, Cell_("STRIKE"));
    Vector_<String_> events(1, ToString(strike));
    eventDates.push_back(Cell_(exerciseDate));
    events.push_back("call pays MAX(spot() - STRIKE, 0.0)");

    ScriptProduct_ product(eventDates, events);
    std::unique_ptr<Model_<>> model = std::make_unique<BlackScholes_<>>(spot, vol, rate, div);

    product.PreProcess(false, false);
    SimResults_<> results = MCSimulation(product, *model, num_paths, rsg, false, false);

    auto expected = 0.806119;
    ASSERT_NEAR(results.aggregated_ / num_paths, expected, 1e-5);
}

TEST(BlackScholesTest, TestBlackScholesAAD) {
    auto global = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    const double strike = 11.0;
    const double spot = 10.0;
    const double vol = 0.20;
    const double rate = 0.034;
    const double div = 0.021;
    const String_ rsg = "sobol";
    const size_t num_paths = 10000000;

    Vector_<Cell_> eventDates(1, Cell_("STRIKE"));
    Vector_<String_> events(1, ToString(strike));
    eventDates.push_back(Cell_(exerciseDate));
    events.push_back("call pays MAX(spot() - STRIKE, 0.0)");

    ScriptProduct_ product(eventDates, events);
    std::unique_ptr<Model_<Number_>> model = std::make_unique<BlackScholes_<Number_>>(spot, vol, rate, div);

    product.PreProcess(false, false);
    SimResults_<Number_> results = MCSimulation(product, *model, num_paths, rsg, false, false);

    ASSERT_NEAR(results.risks_[0], 0.43986485, 1e-6);
    ASSERT_NEAR(results.risks_[1], 5.38087423, 1e-4);
    ASSERT_NEAR(results.risks_[2], 7.18505725, 1e-4);
    ASSERT_NEAR(results.risks_[3], -8.7972975, 1e-4);
}