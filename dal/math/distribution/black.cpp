//
// Created by wegam on 2022/5/5.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/math/aad/operators.hpp>
#include <dal/math/rootfind.hpp>
#include <dal/math/specialfunctions.hpp>


namespace Dal {
    namespace Distribution {
        double BlackOpt(double fwd, double vol, double strike, const OptionType_& type) {
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

        double BlackIV(double fwd, double strike, const OptionType_& type, double price, double guess) {
            static const int MAX_ITERATIONS = 30;
            static const double TOL = 1.0e-10;
            REQUIRE(price >= type.Payout(fwd, strike), "value below intrinsic value in BlackIV");
            Brent_ task(guess ? Log(guess) : -1.5);
            Converged_ done(TOL * Max(1.0, fwd), TOL * Max(1.0, price));
            for (int i = 0; i < MAX_ITERATIONS; ++i) {
                const double vol = Exp(task.NextX());
                if (done(task, BlackOpt(fwd, vol, strike, type) - price))
                    return vol;
            }
            THROW("exhausted iterations in BlackIV");
        }

        Vector_<> BlackGreeks(double fwd, double vol, double strike, const OptionType_& type) {
            const double dMinus = Log(fwd / strike) / vol - 0.5 * vol;
            const double dPlus = dMinus + vol;
            double fwdDelta = 0.0;
            double vega = 0.0;
            switch (type.Switch()) {
            case OptionType_::Value_::CALL:
                fwdDelta = NCDF(dPlus);
                vega = fwd * NPDF(dPlus);
                break;
            case OptionType_::Value_::PUT:
                fwdDelta = -NCDF(-dPlus);
                vega = fwd * NPDF(dPlus);
                break;
            case OptionType_::Value_::STRADDLE:
                fwdDelta = NCDF(dPlus) - NCDF(-dPlus);
                vega = 2.0 * fwd * NPDF(dPlus);
                break;
            default:
                THROW("invalid option type");
            }
            Vector_<> ret_val;
            ret_val.push_back(fwdDelta);
            ret_val.push_back(vega);
            return ret_val;
        }
    }

    double DistributionBlack_::VolVega(double strike, const OptionType_& type) const {
        Vector_<> greeks = Distribution::BlackGreeks(f_, vol_, strike, type);
        return vol_ * greeks[1];
    }

    std::map<String_, double> DistributionBlack_::ParameterDerivatives(double strike,
                                                                       const OptionType_& type,
                                                                       const Vector_<String_>& to_report) const {
        Vector_<> greeks = Distribution::BlackGreeks(f_, vol_, strike, type);
        std::map<String_, double> ret_val;
        for (const auto& name: to_report) {
            if (name == "delta")
                ret_val.insert({String_("delta"), greeks[0]});
            else if (name == "vega")
                ret_val.insert({String_("vega"), greeks[1]});
            else if (name == "volvega")
                ret_val.insert({String_("volvega"), vol_ * greeks[1]});
            else
                THROW("not known greek name " + name);
        }
        return ret_val;
    }
}