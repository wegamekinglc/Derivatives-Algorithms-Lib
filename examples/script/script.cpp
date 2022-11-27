//
// Created by wegam on 2020/12/21.
//

#include <iostream>
#include <dal/time/schedules.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/script/event.hpp>
#include <dal/math/aad/models/blackscholes.hpp>

#include <dal/storage/globals.hpp>
#include <dal/utilities/timer.hpp>
#include <dal/script/simulation.hpp>
#include <iomanip>

using namespace std;
using namespace Dal;
using namespace Dal::Script;
using namespace Dal::AAD;

#define USE_AAD


int main() {

    XGLOBAL::SetEvaluationDate(Date_(2022, 9, 25));
    Timer_ timer;

#ifdef USE_AAD
    using Real_ = Number_;
#else
    using Real_ = double;
#endif

    const double spot = 100.0;
    const double vol = 0.15;
    const double rate = 0.0;
    const double div = 0.0;
    const double strike = 120.0;
    const Date_ maturity(2025, 9, 25);
    const Date_ start = Global::Dates_().EvaluationDate();

    int n;
    std::cout << "Plz input # of paths (power of 2):";
    std::cin >> n;
    const int n_paths = Pow(2, n);

    bool use_bb = false;
    std::cout << "Use brownian bridge?:";
    std::cin >> use_bb;

    std::string rsg = "sobol";
    std::cout << "Type of rsg:";
    std::cin >> rsg;

    timer.Reset();
    auto tenor = Date::ParseIncrement("1W");
    const auto schedule = DateGenerate(start, maturity, tenor);

    Vector_<Date_> eventDates;
    Vector_<String_> events;
    eventDates.push_back(schedule[0]);
    events.push_back("alive = 1");
    for (int i = 1; i < schedule.size(); ++i) {
        eventDates.push_back(schedule[i]);
        events.push_back("if spot() >= 150:0.5 then alive = 0 endif");
    }
    eventDates.push_back(schedule[schedule.size() - 1]);
    events.push_back(String_("K = " + ToString(strike) + "\n call pays alive * MAX(spot() - K, 0.0)"));

    ScriptProduct_ product(eventDates, events);
    int max_nested_ifs = product.PreProcess(true, true);
    std::unique_ptr<Model_<Real_>> model = std::make_unique<BlackScholes_<Real_>>(spot, vol, false, rate, div);
    std::cout << "\nParsing " << std::setprecision(8) << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;

    timer.Reset();
    SimResults_<Real_> results = MCSimulation(product, *model, n_paths, String_(rsg), use_bb, max_nested_ifs);

    auto sum = 0.0;
    for (auto row = 0; row < results.Rows(); ++row)
        sum += results.aggregated_[row];
    auto calculated = sum / static_cast<double>(results.Rows());
    std::cout << "\nEuropean       w. B-S: price " << std::setprecision(8) << calculated << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
#ifdef USE_AAD
    std::cout << "                     : delta " << std::setprecision(8) << results.risks_[0] << std::endl;
    std::cout << "                     : vega  " << std::setprecision(8) << results.risks_[1] << std::endl;
    std::cout << "                     : rho   " << std::setprecision(8) << results.risks_[2] << std::endl;
#endif

    return 0;
}