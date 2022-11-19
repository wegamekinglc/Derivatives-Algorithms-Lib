//
// Created by wegam on 2020/12/21.
//

#include <iostream>
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

    ScriptProduct_ product;
    std::map<Date_, String_> events;

    events[maturity] = String_("K = " + ToString(strike) + "\n"
                               "call pays MAX(spot() - K, 0.0)");

    product.ParseEvents(events.begin(), events.end());
    int maxNestedIfs = product.PreProcess(false, false);
    std::unique_ptr<Model_<Real_>> model = std::make_unique<BlackScholes_<Real_>>(spot, vol, false, rate, div);

    timer.Reset();

    SimResults_<Real_> results = MCSimulation(product, *model, n_paths, String_(rsg), use_bb);

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