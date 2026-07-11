//
// Created by wegam on 2022/5/5.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/math/distribution/black.hpp>
#include <dal/math/rootfind.hpp>
#include <dal/math/specialfunctions.hpp>

#include <cmath>
#include <limits>
#include <utility>

namespace Dal {
    namespace Distribution {
        namespace {
            // Black maps the solver coordinate through exp. Bachelier uses a separate finite bracket below.
            template <class Pricer_, class GuessTransform_, class VolTransform_>
            double SolveIV(double fwd,
                           double price,
                           const char* caller,
                           Pricer_ pricer,
                           GuessTransform_ guessTransform,
                           VolTransform_ volTransform,
                           double guess) {
                static const int MAX_ITERATIONS = 30;
                static const double TOL = 1.0e-10;
                Brent_ task(guessTransform(guess, fwd));
                Converged_ done(TOL * max(1.0, std::abs(fwd)), TOL * max(1.0, price));
                for (int i = 0; i < MAX_ITERATIONS; ++i) {
                    const double vol = volTransform(task.NextX());
                    if (done(task, pricer(vol) - price)) {
                        return vol;
                    }
                }
                THROW("exhausted iterations in " + String_(caller));
            }

            double BachelierResidual(double fwd, double strike, const OptionType_& type, double price, double vol) {
                REQUIRE(std::isfinite(vol) && vol >= 0.0, "non-finite or negative volatility in BachelierIV");
                const double optionPrice = BachelierOpt(fwd, vol, strike, type);
                REQUIRE(std::isfinite(optionPrice), "unable to bracket Bachelier volatility: non-finite option price");
                const double value = optionPrice - price;
                REQUIRE(std::isfinite(value), "unable to bracket Bachelier volatility: non-finite residual");
                return value;
            }

            template <class Residual_> std::pair<double, double> HuntBachelierUpperBracket(double upperVol, Residual_ residual) {
                static const int MAX_BRACKET_EXPANSIONS = 1024;
                static const double EXPANSION_FACTOR = 2.0;
                double upperResidual = residual(upperVol);
                int expansion = 0;
                while (upperResidual < 0.0 && expansion < MAX_BRACKET_EXPANSIONS) {
                    if (upperVol > std::numeric_limits<double>::max() / EXPANSION_FACTOR)
                        THROW("unable to bracket Bachelier volatility before endpoint overflow");
                    upperVol *= EXPANSION_FACTOR;
                    upperResidual = residual(upperVol);
                    ++expansion;
                }
                REQUIRE(upperResidual >= 0.0, "unable to bracket Bachelier volatility with a finite upper endpoint");
                return {upperVol, upperResidual};
            }

            double SolveBachelierIV(double fwd, double strike, const OptionType_& type, double price, double guess) {
                static const int MAX_SOLVE_ITERATIONS = 100;
                static const double TOL = 1.0e-10;
                static const double MIN_UPPER_VOL = 0.01;

                REQUIRE(std::isfinite(fwd), "non-finite forward in BachelierIV");
                REQUIRE(std::isfinite(strike), "non-finite strike in BachelierIV");
                REQUIRE(std::isfinite(price), "non-finite price in BachelierIV");
                REQUIRE(std::isfinite(guess), "non-finite guess in BachelierIV");

                const double moneyness = fwd - strike;
                REQUIRE(std::isfinite(moneyness), "non-finite moneyness in BachelierIV");
                const double intrinsic = type.Payout(fwd, strike);
                REQUIRE(std::isfinite(intrinsic), "non-finite intrinsic value in BachelierIV");
                REQUIRE(price >= intrinsic, "value below intrinsic value in BachelierIV");
                if (price == intrinsic)
                    return 0.0;

                auto residual = [&](double vol) { return BachelierResidual(fwd, strike, type, price, vol); };
                const double lowerResidual = intrinsic - price;
                const double invariantScale = max(1.0, max(std::abs(moneyness), price));
                double initialUpperVol = max(MIN_UPPER_VOL, max(std::abs(moneyness), price));
                if (guess > 0.0)
                    initialUpperVol = max(initialUpperVol, guess);
                REQUIRE(std::isfinite(initialUpperVol) && initialUpperVol > 0.0, "unable to bracket Bachelier volatility: invalid upper endpoint");
                const auto [upperVol, upperResidual] = HuntBachelierUpperBracket(initialUpperVol, residual);

                const double volTolerance = TOL * invariantScale;
                const double priceTolerance = TOL * max(1.0, price);
                BracketedBrent_ task({0.0, lowerResidual}, {upperVol, upperResidual}, volTolerance);
                Converged_ done(volTolerance, priceTolerance);
                for (int i = 0; i < MAX_SOLVE_ITERATIONS; ++i) {
                    const double vol = task.NextX();
                    if (done(task, residual(vol)))
                        return vol;
                }
                THROW("exhausted iterations in BachelierIV");
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
            return SolveIV(fwd, price, "BlackIV", pricer, guessTransform, [](double x) { return exp(x); }, guess);
        }

        Vector_<> BlackGreeks(double fwd, double vol, double strike, const OptionType_& type) {
            const double dMinus = log(fwd / strike) / vol - 0.5 * vol;
            const double dPlus = dMinus + vol;
            return GreeksFromD(dPlus, fwd, type);
        }

        double BachelierIV(double fwd, double strike, const OptionType_& type, double price, double guess) {
            return SolveBachelierIV(fwd, strike, type, price, guess);
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

    std::map<String_, double>
    DistributionNormalLike_::ParameterDerivatives(double strike, const OptionType_& type, const Vector_<String_>& to_report) const {
        Vector_<> greeks = Greeks(strike, type);
        std::map<String_, double> ret_val;
        for (const auto& name : to_report) {
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
