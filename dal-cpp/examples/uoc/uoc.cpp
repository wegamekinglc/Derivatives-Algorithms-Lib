//
// Created by wegam on 2020/12/21.
//

#include <iostream>
#include <iomanip>
#include <dal/platform/platform.hpp>
#include <dal/time/schedules.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/script/event.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/model/dupire.hpp>
#include <dal/storage/globals.hpp>
#include <dal/utilities/timer.hpp>
#include <dal/script/simulation.hpp>

using namespace std;
using namespace Dal;
using namespace Dal::Script;
using Dal::AAD::Model_;
using Dal::AAD::Dupire_;


int main() {
    Dal::RegisterAll_::Init();

    const Date_ start = Date_(2022, 9, 25);
    const Date_ maturity = Date_(2025, 9, 25);

    Global::Dates_::SetEvaluationDate(start);
    Timer_ timer;

    using Real_ = Number_;

    const double spot = 100.0;
    const double vol = 0.15;
    const double rate = 0.0;
    const double div = 0.0;
    const double strike = 120.0;
    const double barrier = 150.0;
    const String_ freq = "1W";
    const String_ fuzzy = "0.1";
    const int numPath = std::pow(2, 20);

    timer.Reset();

    Vector_<Cell_> eventDates;
    Vector_<String_> events;
    eventDates.push_back(Cell_("STRIKE"));
    events.push_back(ToString(strike));
    eventDates.push_back(Cell_("BARRIER"));
    events.push_back(ToString(barrier));
    eventDates.push_back(Cell_(start));
    events.push_back("alive = 1");
    eventDates.push_back(Cell_("START: " + Date::ToString(start) + " END: " + Date::ToString(maturity) + " FREQ: " + freq));
    events.push_back("if spot() >= BARRIER:" + fuzzy + " then alive = 0 end");
    eventDates.push_back(Cell_(maturity));
    events.push_back(String_("call pays alive * MAX(spot() - STRIKE, 0.0)"));


    const int numObs = freq == "1W" ? 3 * 51 : 3 * 12;

    auto times = Vector::XRange(0.0, 5.0, 61);
    auto spots = Vector::XRange(50.0, 200.0, 31);

    Vector_<int> widths = {14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14};
    std::cout << std::setw(widths[0]) << std::left << "Method"
              << std::setw(widths[1]) << std::right << "# of paths"
              << std::setw(widths[2]) << std::right << "# of obs"
              << std::setw(widths[3]) << std::right << "PV"
              << std::setw(widths[4]) << std::right << "delta"
              << std::setw(widths[5]) << std::right << "dP/dR"
              << std::setw(widths[6]) << std::right << "dP/dDiv"
              << std::setw(widths[7]) << std::right << "vega"
              << std::setw(widths[8]) << std::right << "dP/dB"
              << std::setw(widths[9]) << std::right << "dP/dK"
              << std::setw(widths[10]) << std::right << "Elapsed (ms)"
              << std::endl;
    {
        Handle_<ModelData_> modelData(new DupireModelData_("dupiremodel",
                                                                      spot,
                                                                      rate,
                                                                      div,
                                                                      spots,
                                                                      times,
                                                                      Matrix_<double>(spots.size(),times.size(), vol)));
        timer.Reset();

        ScriptProduct_ product(eventDates, events);
        int maxNestedIfs = product.PreProcess(false, false);
        SimResults_ results = MCSimulation<double>(product, modelData, numPath, String_("sobol"), false, false);

        auto calculated = results.aggregated_ / static_cast<double>(numPath);

        std::cout << std::setw(widths[0]) << std::left << "Non-AAD"
                  << std::setw(widths[1]) << std::right << numPath
                  << std::setw(widths[2]) << std::right << numObs
                  << std::fixed << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << "#NA"
                  << std::setw(widths[5]) << std::right << "#NA"
                  << std::setw(widths[6]) << std::right << "#NA"
                  << std::setw(widths[7]) << std::right << "#NA"
                  << std::setw(widths[8]) << std::right << "#NA"
                  << std::setw(widths[9]) << std::right << "#NA"
                  << std::setw(widths[10]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }

    {
        Handle_<ModelData_> modelData(new DupireModelData_("dupiremodel",
                                                                      spot,
                                                                      rate,
                                                                      div,
                                                                      spots,
                                                                      times,
                                                                      Matrix_<double>(spots.size(),times.size(), vol)));
        timer.Reset();

        ScriptProduct_ product(eventDates, events);
        int maxNestedIfs = product.PreProcess(false, false);
        SimResults_ results = MCSimulation<double>(product, modelData, numPath, String_("sobol"), false, false);
        auto calculated = results.aggregated_ / static_cast<double>(numPath);

        double eps = 0.0001;
        Handle_<ModelData_> modelDataDown(new DupireModelData_("dupiremodel",
                                                                      spot * (1 - eps),
                                                                      rate,
                                                                      div,
                                                                      spots,
                                                                      times,
                                                                      Matrix_<double>(spots.size(),times.size(), vol)));
        product.PreProcess(false, false);
        SimResults_ results_down = MCSimulation<double>(product, modelDataDown, numPath, String_("sobol"), false);
        auto calculatedDown = results_down.aggregated_ / static_cast<double>(numPath);

        Handle_<ModelData_> modelDataUp(new DupireModelData_("dupiremodel",
                                                                      spot * (1 + eps),
                                                                      rate,
                                                                      div,
                                                                      spots,
                                                                      times,
                                                                      Matrix_<double>(spots.size(),times.size(), vol)));
        product.PreProcess(false, false);
        SimResults_ results_up = MCSimulation<double>(product, modelDataUp, numPath, String_("sobol"), false);
        auto calculatedUp = results_up.aggregated_ / static_cast<double>(numPath);
        auto dSpot = (calculatedUp - calculatedDown) / (2 * spot * eps);

        double epsRate = std::abs(rate) > 0 ? abs(rate) * eps : eps;
        modelDataDown.reset(new DupireModelData_("dupiremodel",
                                                                      spot,
                                                                      rate - epsRate,
                                                                      div,
                                                                      spots,
                                                                      times,
                                                                      Matrix_<double>(spots.size(),times.size(), vol)));
        product.PreProcess(false, false);
        results_down = MCSimulation<double>(product, modelDataDown, numPath, String_("sobol"), false);
        calculatedDown = results_down.aggregated_ / static_cast<double>(numPath);

        modelDataUp.reset(new DupireModelData_("dupiremodel",
                                                                      spot,
                                                                      rate + epsRate,
                                                                      div,
                                                                      spots,
                                                                      times,
                                                                      Matrix_<double>(spots.size(),times.size(), vol)));
        product.PreProcess(false, false);
        results_up = MCSimulation<double>(product, modelDataUp, numPath, String_("sobol"), false);
        calculatedUp = results_up.aggregated_ / static_cast<double>(numPath);
        auto dRate = (calculatedUp - calculatedDown) / (2 * epsRate);

        double epsDiv = std::abs(div) > 0 ? abs(rate) * eps : eps;
        modelDataDown.reset(new DupireModelData_("dupiremodel",
                                                                      spot,
                                                                      rate,
                                                                      div - epsDiv,
                                                                      spots,
                                                                      times,
                                                                      Matrix_<double>(spots.size(),times.size(), vol)));
        product.PreProcess(false, false);
        results_down = MCSimulation<double>(product, modelDataDown, numPath, String_("sobol"), false);
        calculatedDown = results_down.aggregated_ / static_cast<double>(numPath);

        modelDataUp.reset(new DupireModelData_("dupiremodel",
                                                                      spot,
                                                                      rate,
                                                                      div + epsDiv,
                                                                      spots,
                                                                      times,
                                                                      Matrix_<double>(spots.size(),times.size(), vol)));
        product.PreProcess(false, false);
        results_up = MCSimulation<double>(product, modelDataUp, numPath, String_("sobol"), false);
        calculatedUp = results_up.aggregated_ / static_cast<double>(numPath);
        auto dDiv = (calculatedUp - calculatedDown) / (2 * epsDiv);

        auto events_down = events;
        events_down[0] = ToString(strike * (1.0 - eps));
        ScriptProduct_ product_down(eventDates, events_down);
        product_down.PreProcess(false, false);
        results_down = MCSimulation<double>(product_down, modelData, numPath, String_("sobol"), false);
        calculatedDown = results_down.aggregated_ / static_cast<double>(numPath);

        auto events_up = events;
        events_up[0] = ToString(strike * (1.0 + eps));
        ScriptProduct_ product_up(eventDates, events_up);
        product_up.PreProcess(false, false);
        results_up = MCSimulation<double>(product_up, modelData, numPath, String_("sobol"), false);
        calculatedUp = results_up.aggregated_ / static_cast<double>(numPath);
        auto dStrike = (calculatedUp - calculatedDown) / (2 * strike * eps);

        events_down = events;
        events_down[1] = ToString(barrier * (1.0 - eps));
        product_down = ScriptProduct_(eventDates, events_down);
        product_down.PreProcess(false, false);
        results_down = MCSimulation<double>(product_down, modelData, numPath, String_("sobol"), false);
        calculatedDown = results_down.aggregated_ / static_cast<double>(numPath);

        events_up = events;
        events_up[1] = ToString(barrier * (1.0 + eps));
        product_up = ScriptProduct_(eventDates, events_up);
        product_up.PreProcess(false, false);
        results_up = MCSimulation<double>(product_up, modelData, numPath, String_("sobol"), false);
        calculatedUp = results_up.aggregated_ / static_cast<double>(numPath);
        auto dBarrier = (calculatedUp - calculatedDown) / (2 * barrier * eps);

        std::cout << std::setw(widths[0]) << std::left << "FDM"
                  << std::setw(widths[1]) << std::right << numPath
                  << std::setw(widths[2]) << std::right << numObs
                  << std::fixed << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << dSpot
                  << std::setw(widths[5]) << std::right << dRate
                  << std::setw(widths[6]) << std::right << dDiv
                  << std::setw(widths[7]) << std::right << "#NA"
                  << std::setw(widths[8]) << std::right << dBarrier
                  << std::setw(widths[9]) << std::right << dStrike
                  << std::setw(widths[10]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }

    {
        Handle_<ModelData_> modelData(new DupireModelData_("dupiremodel",
                                                                      spot,
                                                                      rate,
                                                                      div,
                                                                      spots,
                                                                      times,
                                                                      Matrix_<double>(spots.size(),times.size(), vol)));
        timer.Reset();

        ScriptProduct_ product(eventDates, events);
        int maxNestedIfs = product.PreProcess(true, true);
        SimResults_ results = MCSimulation<Number_>(product, modelData, numPath, String_("sobol"), false, false, maxNestedIfs);

        auto calculated = results.aggregated_ / static_cast<double>(numPath);
        const int volLength = 31 * 61;
        double vega = 0.0;

        for (auto i = 3; i < 3 + volLength; ++i)
            vega += results.risks_[i];

        std::cout << std::setw(widths[0]) << std::left << "AAD"
                  << std::setw(widths[1]) << std::right << numPath
                  << std::setw(widths[2]) << std::right << numObs
                  << std::fixed << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << results.risks_[0]
                  << std::setw(widths[5]) << std::right << results.risks_[1]
                  << std::setw(widths[6]) << std::right << results.risks_[2]
                  << std::setw(widths[7]) << std::right << vega
                  << std::setw(widths[8]) << std::right << results.risks_[3 + volLength]
                  << std::setw(widths[9]) << std::right << results.risks_[3 + volLength + 1]
                  << std::setw(widths[10]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }
    return 0;
}