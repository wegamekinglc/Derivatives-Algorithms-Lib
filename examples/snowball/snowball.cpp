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
    const int num_path = 8192;

    timer.Reset();
    auto tenor = Date::ParseIncrement("1M");
    const auto schedule = DateGenerate(start, maturity, tenor);

    Vector_<Cell_> eventDates;
    Vector_<String_> events;
    eventDates.emplace_back(start);
    events.push_back("alive = 1 ki = 0");
    eventDates.emplace_back("START: 2023-06-01 END: 2025-03-01 FREQ: 1M");
    auto this_coupon = ToString(coupon) + " * DCF(ACT365F, " + Date::ToString(start) + ", PeriodEnd)";
    events.push_back("if spot() < " + ToString(ki) + ":0.001 then ki = 1 endif\n"
                     "if spot() > " + ToString(ko) + ":0.001 then call pays alive * " + this_coupon + " alive = 0  endif");
    eventDates.emplace_back(maturity);
    this_coupon = ToString(coupon) + " * DCF(ACT365F, " + Date::ToString(start) + ", " + Date::ToString(maturity) + ")";
    events.push_back("if spot() < " + ToString(ki) + ":0.001 then ki = 1 endif\n"
                     "if spot() > " + ToString(ko) + ":0.001 then call pays alive * " + this_coupon + " alive = 0  endif\n"
                     "call pays alive * ki * (spot() - " + ToString(spot) + ") + alive * (1.000000 - ki) * " + this_coupon);
    ScriptProduct_ product(eventDates, events);
    product.Debug();

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