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
    events.push_back("if spot() >= BARRIER:0.1 then alive = 0 end");
    eventDates.push_back(Cell_(maturity));
    events.push_back(String_("if spot() >= BARRIER:0.1 then alive = 0 end\n call pays alive * MAX(spot() - STRIKE, 0.0)"));

    ScriptProduct_ product(eventDates, events);
    const int num_obs = freq == "1W" ? 3 * 51 : 3 * 12;

    auto times = Vector::XRange(0.0, 5.0, 61);
    auto spots = Vector::XRange(50.0, 200.0, 31);

    Vector_<int> widths = {14, 14, 14, 14, 14, 14, 14, 14, 14, 14};
    std::cout << std::setw(widths[0]) << std::left << "Method"
              << std::setw(widths[1]) << std::right << "# of paths"
              << std::setw(widths[2]) << std::right << "# of obs"
              << std::setw(widths[3]) << std::right << "PV"
              << std::setw(widths[4]) << std::right << "delta"
              << std::setw(widths[5]) << std::right << "dP/dR"
              << std::setw(widths[6]) << std::right << "dP/dDiv"
              << std::setw(widths[7]) << std::right << "vega"
              << std::setw(widths[8]) << std::right << "dP/dK"
              << std::setw(widths[9]) << std::right << "Elapsed (ms)"
              << std::endl;
    {
        std::unique_ptr<Model_<double>> model = std::make_unique<Dupire_<double>>(spot,
                                                                                  rate,
                                                                                  div,
                                                                                  spots,
                                                                                  times,
                                                                                  Matrix_<double>(spots.size(),times.size(),vol),
                                                                                  10.0);
        timer.Reset();

        int max_nested_ifs = product.PreProcess(false, false);
        const int num_path = std::pow(2, 20);
        SimResults_<double> results = MCSimulation(product, *model, num_path, String_("sobol"), false);

        auto calculated = results.aggregated_ / static_cast<double>(num_path);

        std::cout << std::setw(widths[0]) << std::left << "Non-AAD"
                  << std::setw(widths[1]) << std::right << num_path
                  << std::setw(widths[2]) << std::right << num_obs
                  << std::fixed << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << "#NA"
                  << std::setw(widths[5]) << std::right << "#NA"
                  << std::setw(widths[6]) << std::right << "#NA"
                  << std::setw(widths[7]) << std::right << "#NA"
                  << std::setw(widths[8]) << std::right << "#NA"
                  << std::setw(widths[9]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }

    {
        std::unique_ptr<Model_<Real_>> model = std::make_unique<Dupire_<Real_>>(Real_(spot),
                                                                                Real_(rate),
                                                                                Real_(div),
                                                                                spots,
                                                                                times,
                                                                                Matrix_<Real_>(spots.size(),times.size(),Real_(vol)),
                                                                                10.0);
        timer.Reset();

        int max_nested_ifs = product.PreProcess(true, true);
        const int num_path = std::pow(2, 20);
        SimResults_<Real_> results = MCSimulation(product, *model, num_path, String_("sobol"), false, max_nested_ifs);

        auto calculated = results.aggregated_ / static_cast<double>(num_path);
        const int vol_length = 31 * 61;
        double vega = 0.0;
        for (auto i = 3; i < 3 + vol_length; ++i)
            vega += results.risks_[i];

        std::cout << std::setw(widths[0]) << std::left << "AAD"
                  << std::setw(widths[1]) << std::right << num_path
                  << std::setw(widths[2]) << std::right << num_obs
                  << std::fixed << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << results.risks_[0]
                  << std::setw(widths[5]) << std::right << results.risks_[1]
                  << std::setw(widths[6]) << std::right << results.risks_[2]
                  << std::setw(widths[7]) << std::right << vega
                  << std::setw(widths[8]) << std::right << results.risks_[3 + vol_length]
                  << std::setw(widths[9]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }
    return 0;
}