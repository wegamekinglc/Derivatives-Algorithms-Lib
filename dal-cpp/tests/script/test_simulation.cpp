//
// Created by wegamekinglc on 2026/8/15.
//
// Coverage for dal/script/simulation.hpp: determinism under fixed-seed RNGs,
// payoff aggregation against analytic expectations, and error/edge paths.

#include <gtest/gtest.h>

#include <cmath>

#include <dal/platform/platform.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/storage/globals.hpp>
#include <dal/script/event.hpp>
#include <dal/script/simulation.hpp>
#include <dal/utilities/exceptions.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    ScriptProduct_ VanillaCallProduct(const Date_& exerciseDate, double strike) {
        Vector_<Cell_> eventDates{Cell_(String_("STRIKE")), Cell_(exerciseDate)};
        Vector_<String_> events{ToString(strike), "call pays MAX(spot() - STRIKE, 0.0)"};
        return ScriptProduct_(eventDates, events);
    }

    Handle_<ModelData_> StandardBSModel() {
        return Handle_<ModelData_>(new BSModelData_("bsmodel", 10.0, 0.20, 0.034, 0.021));
    }
} // namespace

TEST(SimulationTest, TestDeterministicWithFixedSeed) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    const SimResults_ first = MCSimulation<double>(product, model, 4096, "sobol", false, false);
    const SimResults_ second = MCSimulation<double>(product, model, 4096, "sobol", false, false);

    ASSERT_DOUBLE_EQ(first.aggregated_, second.aggregated_);
}

TEST(SimulationTest, TestDeterministicWithMrg32) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    const SimResults_ first = MCSimulation<double>(product, model, 4096, "mrg32", false, false);
    const SimResults_ second = MCSimulation<double>(product, model, 4096, "mrg32", false, false);

    ASSERT_DOUBLE_EQ(first.aggregated_, second.aggregated_);
}

TEST(SimulationTest, TestAggregatesTowardsAnalyticBlackScholes) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    const size_t nPaths = 65536;
    const SimResults_ results = MCSimulation<double>(product, model, nPaths, "sobol", false, false);

    // Closed-form Black-Scholes PV for spot=10, strike=11, vol=0.20,
    // rate=0.034, div=0.021, maturity~2y (same golden value as TestBlackScholes).
    ASSERT_NEAR(results.aggregated_ / static_cast<double>(nPaths), 0.806119, 1e-3);
}

TEST(SimulationTest, TestCompiledAggregationMatchesAnalyticToo) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    const size_t nPaths = 65536;
    const SimResults_ results = MCSimulation<double>(product, model, nPaths, "sobol", false, true);

    ASSERT_NEAR(results.aggregated_ / static_cast<double>(nPaths), 0.806119, 1e-3);
}

TEST(SimulationTest, TestZeroPathsReturnsZeroAggregate) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    const SimResults_ results = MCSimulation<double>(product, model, 0, "sobol", false, false);
    ASSERT_DOUBLE_EQ(results.aggregated_, 0.0);
}

TEST(SimulationTest, TestSinglePathIsDeterministicAndFinite) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    const SimResults_ first = MCSimulation<double>(product, model, 1, "sobol", false, false);
    const SimResults_ second = MCSimulation<double>(product, model, 1, "sobol", false, false);

    ASSERT_TRUE(std::isfinite(first.aggregated_));
    ASSERT_DOUBLE_EQ(first.aggregated_, second.aggregated_);
}

TEST(SimulationTest, TestMismatchedDatesAndEventsThrow) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 6, 21)), Cell_(Date_(2024, 6, 21))};
    Vector_<String_> events{"call pays spot()"};

    ASSERT_THROW(ScriptProduct_ product(eventDates, events), Dal::ScriptError_);
}

TEST(SimulationTest, TestInvalidRngNameThrows) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    ASSERT_THROW(MCSimulation<double>(product, model, 64, "not_a_rng", false, false), Dal::Exception_);
}

TEST(SimulationTest, TestEmptyScriptPaysNothing) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Vector_<Cell_> eventDates{Cell_(Date_(2024, 6, 21))};
    Vector_<String_> events{"call pays 0"};
    ScriptProduct_ product(eventDates, events, "call");
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    const SimResults_ results = MCSimulation<double>(product, model, 1024, "sobol", false, false);
    ASSERT_DOUBLE_EQ(results.aggregated_, 0.0);
}
