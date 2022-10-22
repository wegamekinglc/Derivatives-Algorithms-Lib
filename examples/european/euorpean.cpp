//
// Created by wegam on 2021/8/8.
//

#include <dal/math/aad/models/blackscholes.hpp>
#include <dal/math/aad/products/european.hpp>
#include <dal/math/aad/simulation.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/random/quasirandom.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/utilities/timer.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <iostream>
#include <iomanip>

using namespace Dal;
using namespace std;
using namespace Dal::AAD;

auto EuropeanProducts(double strike, const Date_& maturity) {
    std::unique_ptr<Product_<>> prd = std::make_unique<European_<>>(strike, maturity, maturity);
    std::unique_ptr<Product_<Number_>> riskPrd = std::make_unique<European_<Number_>>(strike, maturity, maturity);
    return std::make_pair(std::move(prd), std::move(riskPrd));
}

auto BSModels(double spot, double vol, double rate, double div) {

    std::unique_ptr<Model_<>> mdl = std::make_unique<BlackScholes_<>>(spot, vol, false, rate, div);
    std::unique_ptr<Model_<Number_>> riskMdl = std::make_unique<BlackScholes_<Number_>>(spot, vol, false, rate, div);
    return std::make_pair(std::move(mdl), std::move(riskMdl));
}


int main() {

    XGLOBAL::SetEvaluationDate(Date_(2022, 9, 25));
    Date_ exerciseDate(2025, 9, 25);
    double strike = 120.0;
    double spot = 100.0;
    double vol = 0.15;
    double rate = 0.0;
    double div = 0.0;
    int n;
    std::cout << "Plz input # of paths (power of 2): ";
    std::cin >> n;
    const int n_paths = Pow(2, n);

    auto products = EuropeanProducts(strike, exerciseDate);
    auto bsModels = BSModels(spot, vol, rate, div);

    // single thread simulation
    Timer_ timer;
    auto res = MCSimulation(*products.first, *bsModels.first, "sobol", n_paths);
    auto sum = 0.0;
    for (auto row = 0; row < res.Rows(); ++row)
        sum += res(row, 0);
    auto calculated = sum / static_cast<double>(res.Rows());
    cout << "Single-threaded: " << setprecision(8) << calculated << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << endl;

    // multi-threads simulation
    timer.Reset();
    res = MCParallelSimulation(*products.first, *bsModels.first, "sobol", n_paths);
    sum = 0.0;
    for (auto row = 0; row < res.Rows(); ++row)
        sum += res(row, 0);
    calculated = sum / static_cast<double>(res.Rows());
    cout << "Multi-threaded: " << setprecision(8) << calculated<< "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << endl;

    // single thread simulation (AAD)
    timer.Reset();
    auto resAAD = MCSimulationAAD(*products.second, *bsModels.second, "sobol", n_paths);
    sum = 0.0;
    for (auto row = 0; row < resAAD.Rows(); ++row)
        sum += resAAD.aggregated_[row];
    calculated = sum / static_cast<double>(res.Rows());
    cout << "Single-threaded (AAD): price " << setprecision(8) << calculated << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << endl;
    cout << "                     : delta " << setprecision(8) << resAAD.risks_[0] << endl;
    cout << "                     : vega  " << setprecision(8) << resAAD.risks_[1] << endl;
    cout << "                     : rho   " << setprecision(8) << resAAD.risks_[2] << endl;

    // multi-threads simulation (AAD)
    timer.Reset();
    resAAD = MCParallelSimulationAAD(*products.second, *bsModels.second, "sobol", n_paths);
    sum = 0.0;
    for (auto row = 0; row < resAAD.Rows(); ++row)
        sum += resAAD.aggregated_[row];
    calculated = sum / static_cast<double>(res.Rows());
    cout << "Multi-threaded (AAD) : price " << setprecision(8) << calculated
         << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << endl;
    cout << "                     : delta " << setprecision(8) << resAAD.risks_[0] << endl;
    cout << "                     : vega  " << setprecision(8) << resAAD.risks_[1] << endl;
    cout << "                     : rho   " << setprecision(8) << resAAD.risks_[2] << endl;

    return 0;
}
