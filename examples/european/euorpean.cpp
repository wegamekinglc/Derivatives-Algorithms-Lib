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

int main() {

    XGLOBAL::SetEvaluationDate(Date_(2022, 6, 12));
    Date_ exerciseDate(2023, 6, 12);
    double strike = 1.0;
    double spot = 1.0;
    double vol = 0.2;
    int seed = 1234;
    double rate = 0.03;
    double div = 0.06;
    size_t n_paths = 10000000;

    European_<double> prd(strike, exerciseDate);
    BlackScholes_<double> mdl(spot, vol, false, rate, div);
    std::unique_ptr<Random_> rand(NewSobol(1, n_paths));

    // single thread simulation
    Timer_ timer;
    auto res = MCSimulation(prd, mdl, rand, n_paths);
    auto sum = 0.0;
    for (auto row = 0; row < res.Rows(); ++row)
        sum += res(row, 0);
    auto calculated = sum / static_cast<double>(res.Rows());
    cout << "Single-threaded: " << setprecision(4) << calculated << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << endl;

    // multi-threads simulation
    // only pseudo random number generator can be used in multithreading situation
    std::unique_ptr<Random_> rand2(New(RNGType_("MRG32"), seed));

    timer.Reset();
    res = MCParallelSimulation(prd, mdl, rand2, n_paths);
    sum = 0.0;
    for (auto row = 0; row < res.Rows(); ++row)
        sum += res(row, 0);
    calculated = sum / static_cast<double>(res.Rows());
    cout << "Multi-threaded: " << setprecision(4) << calculated<< "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << endl;

    return 0;
}
