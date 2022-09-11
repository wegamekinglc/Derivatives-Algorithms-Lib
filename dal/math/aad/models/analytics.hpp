//
// Created by wegam on 2022/9/11.
//

#pragma once

#include <dal/math/aad/models/gaussians.hpp>
#include <dal/platform/platform.hpp>
#include <dal/math/distribution/black.hpp>

namespace Dal::AAD {

    template <class T_, class U_, class V_, class W_>
    T_ BlackScholes(const U_& spot, const V_& strike, const T_& vol, const W_& mat) {
        const auto std = vol * Sqrt(mat);
        if (std <= EPSILON)
            return Max(T_(0.0), T_(spot - strike));
        const auto d2 = Log(spot / strike) / std - 0.5 * std;
        const auto d1 = d2 + std;
        return spot * NormalCDF(d1) - strike * NormalCDF(d2);
    }

    inline double BlackScholesIVol(double spot, double strike, double prem, double mat) {
        const auto std_dev = Dal::Distribution::BlackIV(spot, strike, OptionType_("CALL"), prem);
        return std_dev / Sqrt(mat);
    }

    //  Merton, templated
    template <class T_, class U_, class V_, class W_, class X_>
    T_ Merton(const U_& spot, const V_& strike, const T_& vol, const W_& mat, const X_& intens, const X_& meanJmp, const X_& stdJmp) {
        const auto varJmp = stdJmp * stdJmp;
        const auto mv2 = meanJmp + 0.5 * varJmp;
        const auto comp = intens * (Exp(mv2) - 1);
        const auto var = vol * vol;
        const auto intensT = intens * mat;

        unsigned fact = 1;
        X_ iT = 1.0;
        const size_t cut = 10;
        T_ result = 0.0;
        for (size_t n = 0; n < cut; ++n) {
            const auto s = spot * Exp(n * mv2 - comp * mat);
            const auto v = Sqrt(var + n * varJmp / mat);
            const auto prob = Exp(-intensT) * iT / fact;
            result += prob * BlackScholes(s, strike, v, mat);
            fact *= n + 1;
            iT *= intensT;
        }

        return result;
    }

    //	Up and out call in Black-Scholes, untemplated
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

} // namespace Dal::AAD
