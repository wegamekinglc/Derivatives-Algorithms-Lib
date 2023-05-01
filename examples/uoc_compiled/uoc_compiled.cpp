//
// Created by wegam on 2020/12/21.
//

#include <iostream>
#include <dal/time/schedules.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/script/event.hpp>
#include <dal/math/aad/models/blackscholes.hpp>
#include <dal/math/aad/models/dupire.hpp>
#include <dal/storage/globals.hpp>
#include <dal/utilities/timer.hpp>
#include <dal/script/simulation.hpp>
#include <iomanip>

using namespace std;
using namespace Dal;
using namespace Dal::Script;
using Dal::AAD::Model_;
using Dal::AAD::Dupire_;


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
    const Date_ start = Global::Dates_().EvaluationDate();

    timer.Reset();
    auto tenor = Date::ParseIncrement("1W");
    const auto schedule = DateGenerate(start, maturity, tenor);

    Vector_<Cell_> eventDates;
    Vector_<String_> events;
    eventDates.push_back(Cell_(schedule[0]));
    events.push_back("alive = 1");
    for (int i = 1; i < schedule.size(); ++i) {
        eventDates.push_back(Cell_(schedule[i]));
        events.push_back("if spot() >= 150:0.5 then alive = 0 endif");
    }
    eventDates.push_back(Cell_(schedule[schedule.size() - 1]));
    events.push_back(String_("K = " + ToString(strike) + "\n call pays alive * MAX(spot() - K, 0.0)"));

    ScriptProduct_ product(eventDates, events);

    auto times = Vector::XRange(0.0, 5.0, 61);
    auto spots = Vector::XRange(50.0, 200.0, 31);
    std::unique_ptr<Model_<Real_>> model = std::make_unique<Dupire_<Real_>>(Real_(spot),
                                                                            Real_(rate),
                                                                            Real_(div),
                                                                            spots,
                                                                            times,
                                                                            Matrix_<Real_>(spots.size(),
                                                                                           times.size(),
                                                                                           Real_(vol)),
                                                                            10.0);
    std::cout << "\nParsing " << std::setprecision(8) << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;

    timer.Reset();
    int max_nested_ifs = product.PreProcess(true, true);
    product.Compile();
    SimResults_<Real_> results = MCSimulation(product, *model, std::pow(2, 20), String_("sobol"), false, max_nested_ifs, 0.01, true);

    auto sum = 0.0;
    for (auto row = 0; row < results.Rows(); ++row)
        sum += results.aggregated_[row];
    auto calculated = sum / static_cast<double>(results.Rows());
    std::cout << "\nEuropean       w. Dupire: price " << std::setprecision(8) << calculated << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    auto vega = 0.0;
    for (int i = 3; i < results.risks_.size(); ++i)
        vega += results.risks_[i];
    std::cout << "                     : delta " << std::setprecision(8) << results.risks_[0] << std::endl;
    std::cout << "                     : vega  " << std::setprecision(8) << vega << std::endl;
    std::cout << "                     : rho   " << std::setprecision(8) << results.risks_[1] << std::endl;

    return 0;
}