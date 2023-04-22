//
// Created by wegam on 2023/3/1.
//


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
using Dal::AAD::Model_;
using Dal::AAD::BlackScholes_;


int main() {

    Global::Dates_().SetEvaluationDate(Date_(2023, 3, 1));
    Timer_ timer;

    const double spot = 1.0;
    const double vol = 0.15;
    const double rate = 0.0;
    const double div = 0.0;
    const double ko = 1.00;
    const double ki = 0.88;
    const double coupon = 0.069;
    const Date_ maturity(2025, 3, 1);
    const Date_ start = Global::Dates_().EvaluationDate();
    const int num_path = 500000;

    timer.Reset();
    auto tenor = Date::ParseIncrement("1M");
    const auto schedule = DateGenerate(start, maturity, tenor);

    Vector_<Date_> eventDates;
    Vector_<String_> events;
    eventDates.push_back(schedule[0]);
    events.push_back("alive = 1 ki = 0");
    for (int i = 1; i < schedule.size() - 1; ++i) {
        const double this_coupon = coupon * (schedule[i] - schedule[0]) / 365.0;
        eventDates.push_back(schedule[i]);
        if (i < 4)
            events.push_back("alive = 1");
        else
            events.push_back("if spot() < " + ToString(ki) + ":0.001 then ki = 1 endif\n"
                             "if spot() > " + ToString(ko) + ":0.001 then call pays alive * " + ToString(this_coupon) + " alive = 0  endif");
    }
    const double this_coupon = coupon * (schedule[schedule.size() - 1] - schedule[0]) / 365.0;
    eventDates.push_back(schedule[schedule.size() - 1]);
    events.push_back("if spot() < " + ToString(ki) + ":0.001 then ki = 1 endif\n"
                     "if spot() > " + ToString(ko) + ":0.001 then call pays alive * " + ToString(this_coupon) + " alive = 0  endif\n"
                     "call pays alive * ki * (spot() - " + ToString(spot) + ")");

    // print the events description
    for(auto i = 0; i < eventDates.size(); ++i) {
        std::cout << Date::ToString(eventDates[i]) << std::endl;
        std::cout << events[i] << "\n" << std::endl;
    }

    Vector_<int> widths = {14, 14, 14, 14, 14, 14, 14, 14, 14};

    std::cout << std::setw(widths[0]) << std::left << "Method"
              << std::setw(widths[1]) << std::right << "# of paths"
              << std::setw(widths[2]) << std::right << "# of obs"
              << std::setw(widths[3]) << std::right << "PV"
              << std::setw(widths[4]) << std::right << "delta"
              << std::setw(widths[5]) << std::right << "vega"
              << std::setw(widths[6]) << std::right << "dP/dR"
              << std::setw(widths[7]) << std::right << "dP/dDiv"
              << std::setw(widths[8]) << std::right << "Elapsed (ms)"
              << std::endl;

    {
        ScriptProduct_ product(eventDates, events);
        std::unique_ptr<Model_<double>> model = std::make_unique<BlackScholes_<double>>(spot, vol, rate, div);

        timer.Reset();
        int max_nested_ifs = product.PreProcess(false, false);
        SimResults_<double> results =
            MCSimulation(product, *model, num_path, String_("sobol"), false);

        auto sum = 0.0;
        for (auto row = 0; row < results.Rows(); ++row)
            sum += results.aggregated_[row];
        auto calculated = sum / static_cast<double>(results.Rows());

        std::cout << std::setw(widths[0]) << std::left << "Non-AAD"
                  << std::setw(widths[1]) << std::right << num_path
                  << std::setw(widths[2]) << std::right << events.size()
                  << std::fixed
                  << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << "#NA"
                  << std::setw(widths[5]) << std::right << "#NA"
                  << std::setw(widths[6]) << std::right << "#NA"
                  << std::setw(widths[7]) << std::right << "#NA"
                  << std::setw(widths[8]) << std::right << int(timer.Elapsed<milliseconds>())
                  << std::endl;
    }

    {
        ScriptProduct_ product(eventDates, events);
        std::unique_ptr<Model_<Number_>> model = std::make_unique<BlackScholes_<Number_>>(spot, vol, rate, div);

        timer.Reset();
        int max_nested_ifs = product.PreProcess(true, true);
        SimResults_<Number_> results =
            MCSimulation(product, *model, num_path, String_("sobol"), false, max_nested_ifs);

        auto sum = 0.0;
        for (auto row = 0; row < results.Rows(); ++row)
            sum += results.aggregated_[row];
        auto calculated = sum / static_cast<double>(results.Rows());

        std::cout << std::setw(widths[0]) << std::left << "AAD"
                  << std::setw(widths[1]) << std::right << num_path
                  << std::setw(widths[2]) << std::right << events.size()
                  << std::fixed
                  << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << results.risks_[0]
                  << std::setw(widths[5]) << std::right << results.risks_[1]
                  << std::setw(widths[6]) << std::right << results.risks_[2]
                  << std::setw(widths[7]) << std::right << results.risks_[3]
                  << std::setw(widths[8]) << std::right << int(timer.Elapsed<milliseconds>())
                  << std::endl;
    }

    return 0;
}