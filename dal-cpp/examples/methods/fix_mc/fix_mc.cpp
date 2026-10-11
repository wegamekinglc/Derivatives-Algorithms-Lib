//
// Created by Kimi on 2026/9/19.
//

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

#include <dal/indice/fixingsnapshot.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/consts.hpp>
#include <dal/platform/initall.hpp>
#include <dal/script/event.hpp>
#include <dal/script/settings.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/date.hpp>
#include <dal/time/datetime.hpp>

using namespace std;
using namespace Dal;
using namespace Dal::Script;

namespace {
    // One "today" for the whole example: fixings before this date are historical
    // (known values, frozen once from a fixing snapshot at preparation time);
    // fixings after it are future (simulated by the Monte Carlo model).
    const Date_ EVAL_DATE(2026, 9, 19);
    const Date_ PAST_FIXING(2026, 9, 11);    // already fixed: historical
    const Date_ FUTURE_FIXING(2026, 10, 19); // not yet fixed: simulated
    const Date_ MATURITY(2026, 11, 19);      // payment date of every payoff below

    const String_ INDEX("EQ[STOCK]");

    const double SPOT = 100.0;
    const double VOL = 0.20;
    const double RATE = 0.05;
    const double PAST_VALUE = 95.0;     // observed fixing on PAST_FIXING
    const double SNAPSHOT_VALUE = 90.0; // same date, different source: explicit snapshot wins
    const double TODAY_VALUE = 99.5;    // observed fixing on EVAL_DATE

    const size_t N_PATHS = 1 << 16;

    double Discount(const Date_& payDate) { return std::exp(-RATE * (payDate - EVAL_DATE) / DAYS_PER_YEAR); }

    // expectation of a simulated future fixing under Black-Scholes (no dividend)
    double Forward(const Date_& fixingDate) { return SPOT * std::exp(RATE * (fixingDate - EVAL_DATE) / DAYS_PER_YEAR); }

    // future FIX(...) observations are managed by index name alone: the engine
    // binds the model's spot output to the script's own index
    ScriptProductData_ PayFixing(const Date_& fixingDate) {
        return {"", {Cell_(MATURITY)}, {"pay PAYS FIX(" + INDEX + ", " + Date::ToString(fixingDate) + ")"}};
    }

    void StoreFixing(const Date_& fixingDate, double value) {
        FixHistory_ history;
        history.vals_ = {{DateTime_(fixingDate, 0.0), value}};
        XGLOBAL::StoreFixings(INDEX, history, false);
    }

    void Run(const String_& scenario,
             const ScriptProductData_& product,
             const ScriptValuationSettings_& settings,
             const Handle_<MarketFixingSnapshot_>& snapshot,
             double expected) {
        const Handle_<ModelData_> modelData(new BSModelData_("bsmodel", SPOT, VOL, RATE));
        const SimResults_ results = MCSimulation<double>(product, modelData, N_PATHS, settings, {}, snapshot);
        const double price = results.aggregated_ / static_cast<double>(N_PATHS);
        std::cout << std::setw(52) << std::left << scenario << std::setw(14) << std::right << std::fixed << std::setprecision(6) << price
                  << std::setw(14) << std::right << expected << std::setw(12) << std::right << (price - expected) / expected * 1.0e4 << std::endl;
    }
} // namespace

int main() {
    RegisterAll_::Init();
    Global::Dates_::SetEvaluationDate(EVAL_DATE);

    std::cout << '\n' << std::string(70, '=') << "\n  Fixing Monte Carlo scenarios\n"
              << std::string(70, '=') << "\n\n"
              << "evaluation date: " << Date::ToString(EVAL_DATE) << "  (fixings before this date are historical)\n\n";
    std::cout << std::setw(52) << std::left << "Scenario" << std::setw(14) << std::right << "MC price" << std::setw(14) << std::right << "expected"
              << std::setw(12) << std::right << "Diff (bps)" << std::endl;
    std::cout << std::string(92, '-') << '\n';

    // 1) future fixing: the value comes from the simulated model paths
    Run("future fixing (simulated model paths)", PayFixing(FUTURE_FIXING), ScriptValuationSettings_(), {}, Forward(FUTURE_FIXING) * Discount(MATURITY));

    // 2) historical fixing: the value is frozen from the global fixings store
    StoreFixing(PAST_FIXING, PAST_VALUE);
    Run("historical fixing (global fixings store)", PayFixing(PAST_FIXING), ScriptValuationSettings_(), {}, PAST_VALUE * Discount(MATURITY));

    // 3) mixed: one leg frozen from history, one leg simulated, in a single payoff
    const ScriptProductData_ mixed(
        "", {Cell_(MATURITY)},
        {"pay PAYS 0.5 * (FIX(" + INDEX + ", " + Date::ToString(PAST_FIXING) + ") + FIX(" + INDEX + ", " + Date::ToString(FUTURE_FIXING) + "))"});
    Run("mixed historical + future fixings", mixed, ScriptValuationSettings_(), {}, 0.5 * (PAST_VALUE + Forward(FUTURE_FIXING)) * Discount(MATURITY));

    // 4) historical fixing from an explicit snapshot, which takes precedence
    //    over the global fixings store for this valuation
    const Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_({{INDEX, {{DateTime_(PAST_FIXING, 0.0), SNAPSHOT_VALUE}}}}));
    Run("historical fixing (explicit snapshot)", PayFixing(PAST_FIXING), ScriptValuationSettings_(), snapshot, SNAPSHOT_VALUE * Discount(MATURITY));

    // 5) fixing dated exactly on the evaluation date: the valuation setting
    //    todayFixingPolicy_ chooses the source
    Run("today fixing, policy = MODEL (simulated)", PayFixing(EVAL_DATE), ScriptValuationSettings_(), {}, SPOT * Discount(MATURITY));

    StoreFixing(EVAL_DATE, TODAY_VALUE);
    ScriptValuationSettings_ requireHistorical;
    requireHistorical.todayFixingPolicy_ = TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
    Run("today fixing, policy = REQUIREHISTORICAL", PayFixing(EVAL_DATE), requireHistorical, {}, TODAY_VALUE * Discount(MATURITY));

    std::cout << std::string(92, '-') << "\n\n";
    return 0;
}
