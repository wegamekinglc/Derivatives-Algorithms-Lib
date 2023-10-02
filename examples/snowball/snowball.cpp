//
// Created by wegam on 2020/12/21.
//

#include <iostream>

#include <iomanip>
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/math/aad/models/blackscholes.hpp>
#include <dal/storage/globals.hpp>
#include <dal/utilities/timer.hpp>
#include <dal/script/simulation.hpp>

using namespace std;
using namespace Dal;
using namespace Dal::Script;
using Dal::AAD::Model_;
using Dal::AAD::BlackScholes_;


int main() {
    Dal::RegisterAll_::Init();

    Global::Dates_::SetEvaluationDate(Date_(2023, 3, 1));
    Timer_ timer;

    const double spot = 1.0;
    const double vol = 0.15;
    const double rate = 0.0;
    const double div = 0.0;
    const double ko = 1.00;
    const double ki = 0.88;
    const double coupon = 0.069;
    const String_ freq = "1M";
    const Date_ maturity(2025, 3, 1);
    const Date_ start = Global::Dates_::EvaluationDate();
    const int num_path = std::pow(2, 20);

    timer.Reset();

    Vector_<Cell_> eventDates;
    Vector_<String_> events;

    // parameters
    eventDates.emplace_back("KI");
    events.push_back(ToString(ki));
    eventDates.emplace_back("KO");
    events.push_back(ToString(ko));
    eventDates.emplace_back("STRIKE");
    events.push_back(ToString(spot));
    eventDates.emplace_back("COUPON");
    events.push_back(ToString(coupon));

    // monitor
    eventDates.emplace_back(start);
    events.push_back("alive = 1 is_ki = 0");
    eventDates.emplace_back("START: 2023-06-01 END: 2025-02-01 FREQ: " + freq);
    auto dcf = "DCF(ACT365F, " + Date::ToString(start) + ", PeriodEnd)";
    events.push_back("if spot() < KI:0.001 then is_ki = 1 end\n"
                     "if spot() > KO:0.001 then call pays alive * COUPON * " + dcf + " alive = 0 end");
    eventDates.emplace_back(maturity);
    dcf = "DCF(ACT365F, " + Date::ToString(start) + ", " + Date::ToString(maturity) + ")";
    events.push_back("if spot() < KI:0.001 then is_ki = 1 end\n"
                     "if spot() > KO:0.001 then call pays alive * COUPON * " + dcf + " alive = 0  end\n"
                     "call pays alive * is_ki * (spot() - STRIKE) + alive * (1.000000 - is_ki) * COUPON * " + dcf);
    ScriptProduct_ product(eventDates, events, "call");
    const int num_obs = freq == "1W" ? 3 * 51 : 3 * 12;

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

        auto calculated = results.aggregated_ / static_cast<double>(num_path);

        std::cout << std::setw(widths[0]) << std::left << "Non-AAD"
                  << std::setw(widths[1]) << std::right << num_path
                  << std::setw(widths[2]) << std::right << num_obs
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

        auto calculated = results.aggregated_ / static_cast<double>(num_path);

        std::cout << std::setw(widths[0]) << std::left << "AAD"
                  << std::setw(widths[1]) << std::right << num_path
                  << std::setw(widths[2]) << std::right << num_obs
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