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
#include <dal/math/distribution/black.hpp>
#include <iomanip>


using namespace std;
using namespace Dal;
using namespace Dal::Script;
using Dal::AAD::Model_;
using Dal::AAD::BlackScholes_;


int main() {
    RegisterAll_::Init();

    Global::Dates_::SetEvaluationDate(Date_(2022, 9, 25));
    Timer_ timer;

    using Real_ = Number_;

    const double spot = 100.0;
    const double vol = 0.15;
    const double rate = 0.05;
    const double div = 0.03;
    const double strike = 120.0;
    const String_ rsg = "sobol";
    const Date_ maturity(2025, 9, 24);
    const double t = (maturity - Global::Dates_::EvaluationDate()) / 365.0;

    timer.Reset();

    Vector_<Cell_> eventDates(1, Cell_("STRIKE"));
    Vector_<String_> events(1, ToString(strike));
    eventDates.push_back(Cell_(maturity));
    events.push_back("call pays MAX(spot() - STRIKE, 0.0)");

    ScriptProduct_ product(eventDates, events);

    std::unique_ptr<Model_<Real_>> model = std::make_unique<BlackScholes_<Real_>>(spot, vol, rate, div);
    std::cout << "\nParsing " << std::setprecision(8) << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    
    int max_nested = product.PreProcess(false, false);

    Vector_<int> widths = {28, 14, 14, 14, 14, 14, 14, 14};
    double discounts = std::exp(-rate * t);
    double fwd = std::exp((rate - div) * t) * spot;
    double vol_std = std::sqrt(t) * vol;
    const auto benchmark = discounts * Distribution::BlackOpt(fwd, vol_std, strike, OptionType_::Value_::CALL);

    std::cout << std::setw(widths[0]) << std::right << "# of pathes"
              << std::setw(widths[1]) << std::right << "spot"
              << std::setw(widths[2]) << std::right << "price"
              << std::setw(widths[3]) << std::right << "benchmark"
              << std::setw(widths[4]) << std::right << "delta"
//              << std::setw(widths[5]) << std::right << "dStrike"
              << std::setw(widths[6]) << std::right << "Diff (bps)"
              << std::setw(widths[7]) << std::right << "Elapsed (ms)"
              << std::endl;
    for (int i = 12; i <= 25; ++i) {
        timer.Reset();
        int num_paths = std::pow(2, i);
        SimResults_<Real_> results = MCSimulation(product, *model, num_paths, rsg, false, max_nested);

        auto calculated = results.aggregated_ / static_cast<double>(num_paths);
        std::cout << std::setw(widths[0]) << std::right << int(std::pow(2, i))
                  << std::fixed
                  << std::setw(widths[1]) << std::right << spot
                  << std::setprecision(6)
                  << std::setw(widths[2]) << std::right << calculated
                  << std::setw(widths[3]) << std::right << benchmark
                  << std::setw(widths[4]) << std::right << results.risks_[0]
//                  << std::setw(widths[5]) << std::right << results.risks_[1]
                  << std::setw(widths[6]) << std::right << (calculated - benchmark) / benchmark * 10000
                  << std::setw(widths[7]) << std::right << int(timer.Elapsed<milliseconds>())
                  << std::endl;

        for (const auto& res: Zip(results.names_, results.risks_))
            std::cout << res.first << ": " << res.second << std::endl;
    }
    return 0;
}