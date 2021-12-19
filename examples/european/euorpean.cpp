//
// Created by wegam on 2021/8/8.
//

#include <dal/math/aad/models/blackscholes.hpp>
#include <dal/math/aad/products/european.hpp>
#include <dal/math/aad/simulation.hpp>
#include <dal/math/random/random.hpp>
#include <iostream>

using namespace Dal;
using namespace std;

int main() {

    Time_ exerciseDate = 1.0;
    double strike = 1.0;
    double spot = 1.0;
    double vol = 0.2;
    int seed = 1234;
    double rate = 0.03;
    double div = 0.06;
    size_t n_paths = 100000;

    European_<double> prd(strike, exerciseDate);
    BlackScholes_<double> mdl(spot, vol, false, rate, div);
    std::unique_ptr<Random_> rand(New(RNGType_("MRG32"), seed, 1));

    // single thread simulation
    auto res = MCSimulation(prd, mdl, rand, n_paths);
    auto sum = 0.0;
    for (const auto& path : res)
        sum += path[0];
    cout << "serial: " << sum / static_cast<double>(res.size()) << endl;

    // multi-threads simulation
    rand = std::unique_ptr<Random_>(New(RNGType_("MRG32"), seed));
    res = MCParallelSimulation(prd, mdl, rand, n_paths * 8);
    sum = 0.0;
    for (const auto& path : res)
        sum += path[0];
    cout << "parallel: " << sum / static_cast<double>(res.size()) << endl;

    return 0;
}
