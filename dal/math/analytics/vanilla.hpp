//
// Created by wegam on 2022/9/11.
//

#pragma once

#include <dal/math/operators.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/platform/platform.hpp>

namespace Dal::AAD {

    template <class T_>
    T_ BlackScholes(const T_& spot, const T_& strike, const T_& vol, const T_& mat) {
        T_ std = vol * sqrt(mat);
        return Distribution::BlackOpt(spot, std, strike, OptionType_("CALL"));
    }

    double BlackScholesIVol(double spot, double strike, double prem, double mat);

    //  Merton, templated
    template <class T_>
    T_ Merton(const T_& spot,
              const T_& strike,
              const T_& vol,
              const T_& mat,
              const T_& intens,
              const T_& meanJmp,
              const T_& stdJmp) {
        const T_ varJmp = stdJmp * stdJmp;
        const T_ mv2 = meanJmp + 0.5 * varJmp;
        const T_ comp = intens * (Dal::exp(mv2) - 1);
        const T_ var = vol * vol;
        const T_ intensT = intens * mat;

        unsigned fact = 1;
        double iT = 1.0;
        const size_t cut = 10;
        T_ result = 0.0;
        for (size_t n = 0; n < cut; ++n) {
            const T_ s = spot * Dal::exp(n * mv2 - comp * mat);
            const T_ v = Dal::sqrt(var + n * varJmp / mat);
            const T_ prob = Dal::exp(-intensT) * iT / fact;
            result += prob * BlackScholes<T_>(s, strike, v, mat);
            fact *= n + 1;
            iT *= intensT;
        }
        return result;
    }

    template <class T_>
    T_ Bachelier(const T_& spot, const T_& strike, const T_& vol, const T_& mat) {
        T_ std = vol * sqrt(mat);
        return Distribution::BachelierOpt(spot, std, strike, OptionType_("CALL"));
    }
} // namespace Dal::AAD
