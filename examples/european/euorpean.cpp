//
// Created by wegam on 2020/12/21.
//

#include <iostream>
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
using Dal::AAD::Model_;
using Dal::AAD::BlackScholes_;


int main() {

    Global::Dates_().SetEvaluationDate(Date_(2022, 9, 25));
    Timer_ timer;

    using Real_ = Number_;

    const double spot = 100.0;
    const double vol = 0.15;
    const double rate = 0.0;
    const double div = 0.0;
    const double strike = 120.0;
    const Date_ maturity(2025, 9, 25);

    timer.Reset();

    Vector_<Date_> eventDates;
    Vector_<String_> events;
    eventDates.push_back(maturity);
    events.push_back("call pays MAX(spot() - " + ToString(strike) + ", 0.0)");

    ScriptProduct_ product(eventDates, events);

    std::unique_ptr<Model_<Real_>> model = std::make_unique<BlackScholes_<Real_>>(spot, vol, rate, div);
    std::cout << "\nParsing " << std::setprecision(8) << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;

    timer.Reset();
    int max_nested_ifs = product.PreProcess(true, true);
    SimResults_<Real_> results = MCSimulation(product, *model, std::pow(2, 20), String_("sobol"), false, max_nested_ifs);

    auto sum = 0.0;
    for (auto row = 0; row < results.Rows(); ++row)
        sum += results.aggregated_[row];
    auto calculated = sum / static_cast<double>(results.Rows());
    std::cout << "\nEuropean       w. BS : price " << std::setprecision(8) << calculated << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    std::cout << "                     : delta " << std::setprecision(8) << results.risks_[0] << std::endl;
    std::cout << "                     : vega  " << std::setprecision(8) << results.risks_[1] << std::endl;
    std::cout << "                     : rho   " << std::setprecision(8) << results.risks_[2] << std::endl;

    return 0;
}