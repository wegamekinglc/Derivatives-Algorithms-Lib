//
// Created by wegam on 2022/5/5.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/math/rootfind.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/platform/platform.hpp>

namespace Dal {
    namespace Distribution {
        namespace {
            // Shared Brent-on-log-vol implied-vol solver. The iteration sequence
            // (NextX -> exp -> pricer -> done) matches the original BlackIV/BachelierIV
            // implementations exactly, so floating-point results are byte-identical.
            template <class Pricer_, class GuessTransform_>
            double SolveIV(double fwd,
                           double price,
                           const char* caller,
                           Pricer_ pricer,
                           GuessTransform_ guessTransform,
                           double guess) {
                static const int MAX_ITERATIONS = 30;
                static const double TOL = 1.0e-10;
                Brent_ task(guessTransform(guess, fwd));
                Converged_ done(TOL * max(1.0, fwd), TOL * max(1.0, price));
                for (int i = 0; i < MAX_ITERATIONS; ++i) {
                    const double vol = exp(task.NextX());
                    if (done(task, pricer(vol) - price)) {
                        return vol;
                    }
                }
                THROW("exhausted iterations in " + String_(caller));
            }

            // Shared OptionType_ switch + push_back scaffolding for the (delta, vega) pair.
            // vegaScale is `fwd` for Black and `1.0` for Bachelier, reproducing the original
            // `fwd * NPDF(dPlus)` / `NPDF(d)` expressions verbatim.
            Vector_<> GreeksFromD(double d, double vegaScale, const OptionType_& type) {
                double fwdDelta = 0.0;
                double vega = 0.0;
                switch (type.Switch()) {
                case OptionType_::Value_::CALL:
                    fwdDelta = NCDF(d);
                    vega = vegaScale * NPDF(d);
                    break;
                case OptionType_::Value_::PUT:
                    fwdDelta = -NCDF(-d);
                    vega = vegaScale * NPDF(d);
                    break;
                case OptionType_::Value_::STRADDLE:
                    fwdDelta = NCDF(d) - NCDF(-d);
                    vega = 2.0 * vegaScale * NPDF(d);
                    break;
                default:
                    THROW("invalid option type");
                }
                Vector_<> ret_val;
                ret_val.push_back(fwdDelta);
                ret_val.push_back(vega);
                return ret_val;
            }
        } // namespace

        double BlackIV(double fwd, double strike, const OptionType_& type, double price, double guess) {
            REQUIRE(price >= type.Payout(fwd, strike), "value below intrinsic value in BlackIV");
            auto pricer = [&](double vol) { return BlackOpt(fwd, vol, strike, type); };
            auto guessTransform = [](double g, double) { return g > 0.0 ? log(g) : -1.5; };
            return SolveIV(fwd, price, "BlackIV", pricer, guessTransform, guess);
        }

        Vector_<> BlackGreeks(double fwd, double vol, double strike, const OptionType_& type) {
            const double dMinus = log(fwd / strike) / vol - 0.5 * vol;
            const double dPlus = dMinus + vol;
            return GreeksFromD(dPlus, fwd, type);
        }

        double BachelierIV(double fwd, double strike, const OptionType_& type, double price, double guess) {
            REQUIRE(price >= type.Payout(fwd, strike), "value below intrinsic value in BachelierIV");
            auto pricer = [&](double vol) { return BachelierOpt(fwd, vol, strike, type); };
            auto guessTransform = [](double g, double fwdIn) { return g > 0.0 ? g : -1.5 * fwdIn; };
            return SolveIV(fwd, price, "BachelierIV", pricer, guessTransform, guess);
        }

        Vector_<> BachelierGreeks(double fwd, double vol, double strike, const OptionType_& type) {
            const double diff = fwd - strike;
            const double d = diff / vol;
            return GreeksFromD(d, 1.0, type);
        }
    } // namespace Distribution

    double DistributionNormalLike_::VolVega(double strike, const OptionType_& type) const {
        Vector_<> greeks = Greeks(strike, type);
        return vol_ * greeks[1];
    }

    std::map<String_, double> DistributionNormalLike_::ParameterDerivatives(double strike,
                                                                             const OptionType_& type,
                                                                             const Vector_<String_>& to_report) const {
        Vector_<> greeks = Greeks(strike, type);
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
} // namespace Dal
