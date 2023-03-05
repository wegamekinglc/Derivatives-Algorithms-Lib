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

    Global::Dates_().SetEvaluationDate(Date_(2022, 9, 25));
    Timer_ timer;

    using Real_ = double;

    const double spot = 120.0;
    const double vol = 0.15;
    const double rate = 0.0;
    const double div = 0.0;
    const double strike = 120.0;
    const String_ rsg = "sobol";
    const Date_ maturity(2025, 9, 25);
    const double t = (maturity - Global::Dates_().EvaluationDate()) / 365.0;

    timer.Reset();

    Vector_<Date_> eventDates;
    Vector_<String_> events;
    eventDates.push_back(maturity);
    events.push_back("call pays MAX(spot() - " + ToString(strike) + ", 0.0)");

    ScriptProduct_ product(eventDates, events);

    std::unique_ptr<Model_<Real_>> model = std::make_unique<BlackScholes_<Real_>>(spot, vol, rate, div);
    std::cout << "\nParsing " << std::setprecision(8) << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;


    product.PreProcess(false, false);
    for (int i = 10; i <= 30; ++i) {
        timer.Reset();
        int num_paths = std::pow(2, i);
        SimResults_<Real_> results =
            MCSimulation(product, *model, num_paths, rsg, false);

        auto sum = 0.0;
        for (auto row = 0; row < results.Rows(); ++row)
            sum += results.aggregated_[row];
        auto calculated = sum / static_cast<double>(results.Rows());

        double discounts = std::exp(-rate * t);
        double vol_std = std::sqrt(t) * vol;
        const double fwd = std::exp(rate * t) * spot;
        const double benchmark_price = discounts * Distribution::BlackOpt(fwd, vol_std, strike, OptionType_::Value_::CALL);
        std::cout << std::setprecision(12) << "2^" << i << "," << spot << "," << calculated
                  << "," << benchmark_price
                  << "," << (calculated - benchmark_price) / benchmark_price * 10000 << "," << timer.Elapsed<milliseconds>() <<std::endl;
    }

    return 0;
}