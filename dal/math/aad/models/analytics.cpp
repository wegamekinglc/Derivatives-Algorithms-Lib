//
// Created by wegam on 2022/9/11.
//

#include <dal/math/aad/models/analytics.hpp>

namespace Dal::AAD {

    double BlackScholesIVol(double spot, double strike, double prem, double mat) {
        static const OptionType_ type("CALL");
        const auto std_dev = Dal::Distribution::BlackIV(spot, strike, type, prem);
        return std_dev / Sqrt(mat);
    }

    double BlackScholesKO(double spot,
                          double rate,
                          double div,
                          double strike,
                          double barrier,
                          double mat,
                          double vol) {
        const double std = vol * Sqrt(mat);
        const double fwdFact = Exp((rate - div) * mat);
        const double fwd = spot * fwdFact;
        const double disc = Exp(-rate * mat);
        const double v = rate - div - 0.5 * vol * vol;
        const double d2 = (Log(spot / barrier) + v * mat) / std;
        const double d2prime = (Log(barrier / spot) + v * mat) / std;

        const double bar = BlackScholes(fwd, strike, vol, mat) - BlackScholes(fwd, barrier, vol, mat) -
                           (barrier - strike) * NormalCDF(d2) -
                           Pow(barrier / spot, 2 * v / vol / vol) *
                               (BlackScholes(fwdFact * barrier * barrier / spot, strike, vol, mat) -
                                BlackScholes(fwdFact * barrier * barrier / spot, barrier, vol, mat) -
                                (barrier - strike) * NormalCDF(d2prime));

        return disc * bar;
    }
}