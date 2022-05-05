//
// Created by wegam on 2022/5/5.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/distribution/distributionblack.hpp>
#include <dal/math/aad/operators.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/math/rootfind.hpp>


namespace Dal {
    double Distribution::BlackOpt(double fwd, double vol, double strike, const OptionType_& type) {
        if (IsZero(vol) || !IsPositive(fwd * strike))
            return type.Payout(fwd, strike);
        const double dMinus = Log(fwd / strike) / vol - 0.5 * vol;
        const double dPlus = dMinus + vol;
        switch (type.Switch()) {
        case OptionType_::Value_::CALL:
            return fwd * NCDF(dPlus) - strike * NCDF(dMinus);
        case OptionType_::Value_::PUT:
            return strike * NCDF(-dMinus) - fwd * NCDF(-dPlus);
        case OptionType_::Value_::STRADDLE:
            return fwd * (1.0 - 2.0 * NCDF(-dPlus)) + strike * (1.0 - 2.0 * NCDF(dMinus));
        default:
            THROW("invalid option type");
        }
    }

    double Distribution::BlackIV(double fwd, double strike, const OptionType_& type, double price, double guess) {
        static const int MAX_ITERATIONS = 30;
        static const double TOL = 1.0e-10;
        REQUIRE(price >= type.Payout(fwd, strike), "value below intrinsic value in BlackIV");
        Brent_ task(guess ? Log(guess) : - 1.5);
        Converged_ done(TOL * Max(1.0 , fwd), TOL * Max(1.0, price));
        for (int i = 0; i < MAX_ITERATIONS; ++ i) {
            const double vol = Exp(task.NextX());
            if (done(task, BlackOpt(fwd, vol, strike, type) - price))
                return vol;
        }
        THROW("exhausted iterations in BlackIV");
    }
}