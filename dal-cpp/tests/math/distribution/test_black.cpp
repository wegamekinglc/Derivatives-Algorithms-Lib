//
// Created by wegam on 2022/5/7.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/platform/platform.hpp>

#include <dal/math/distribution/black.hpp>
#include <dal/math/operators.hpp>

using namespace Dal;

namespace {
    double BachelierOracleFromMoneyness(double moneyness, double vol, const OptionType_& type) {
        const double d = moneyness / vol;
        const double cdf = 0.5 * std::erfc(-d / std::sqrt(2.0));
        const double pdf = std::exp(-0.5 * d * d) / 2.5066282746310002;
        const double call = moneyness * cdf + vol * pdf;
        const double put = call - moneyness;
        switch (type.Switch()) {
        case OptionType_::Value_::CALL:
            return call;
        case OptionType_::Value_::PUT:
            return put;
        case OptionType_::Value_::STRADDLE:
            return call + put;
        default:
            return 0.0;
        }
    }

    double BachelierOracle(double forward, double vol, double strike, const OptionType_& type) {
        return BachelierOracleFromMoneyness(forward - strike, vol, type);
    }
} // namespace

TEST(DistributionTest, TestBlackOptionPrice) {
    const auto vol = 0.2;
    const auto T = 2.0;
    const auto forward = 110.0;
    const auto dean_vol = vol * sqrt(T);
    DistributionBlack_ black(forward, dean_vol);

    auto call_price = black.OptionPrice(120.0, OptionType_("Call"));
    ASSERT_NEAR(call_price, 8.53592506, 1e-6);

    auto put_price = black.OptionPrice(120.0, OptionType_("Put"));
    ASSERT_NEAR(put_price, 18.53592506, 1e-6);

    auto straddle_price = black.OptionPrice(120.0, OptionType_("Straddle"));
    ASSERT_NEAR(straddle_price, 27.07185013, 1e-6);
}

TEST(DistributionTest, TestBlackOptionIV) {
    const auto vol = 0.2;
    const auto T = 2.0;
    const auto forward = 110.0;
    const auto strike = 120.0;
    const auto dean_vol = vol * sqrt(T);
    DistributionBlack_ black(forward, dean_vol);

    auto call_price = black.OptionPrice(strike, OptionType_("Call"));
    auto call_iv = Distribution::BlackIV(forward, strike, OptionType_("Call"), call_price);
    ASSERT_NEAR(call_iv, vol * sqrt(T), 1e-6);

    auto put_price = black.OptionPrice(strike, OptionType_("Put"));
    auto put_iv = Distribution::BlackIV(forward, strike, OptionType_("Put"), put_price);
    ASSERT_NEAR(put_iv, vol * sqrt(T), 1e-6);

    auto straddle_price = black.OptionPrice(120.0, OptionType_("Straddle"));
    auto straddle_iv = Distribution::BlackIV(forward, strike, OptionType_("Straddle"), straddle_price);
    ASSERT_NEAR(straddle_iv, vol * sqrt(T), 1e-6);
}

TEST(DistributionTest, TestBlackParameterDerivatives) {
    const auto vol = 0.2;
    const auto T = 2.0;
    const auto forward = 110.0;
    const auto strike = 120.0;
    const auto dean_vol = vol * sqrt(T);

    DistributionBlack_ black(forward, dean_vol);
    auto call_greeks = black.ParameterDerivatives(strike, OptionType_("Call"), {"delta", "vega", "volvega"});
    ASSERT_NEAR(call_greeks["delta"], 0.43399572, 1e-6);
    ASSERT_NEAR(call_greeks["vega"], 43.28165607, 1e-6);
    ASSERT_NEAR(call_greeks["volvega"], 12.241901, 1e-6);

    auto put_greeks = black.ParameterDerivatives(strike, OptionType_("Put"), {"delta", "vega", "volvega"});
    ASSERT_NEAR(put_greeks["delta"], -0.56600428, 1e-6);
    ASSERT_NEAR(put_greeks["vega"], 43.28165607, 1e-6);
    ASSERT_NEAR(put_greeks["volvega"], 12.241901, 1e-6);

    auto straddle_greeks = black.ParameterDerivatives(strike, OptionType_("Straddle"), {"delta", "vega", "volvega"});
    ASSERT_NEAR(straddle_greeks["delta"], call_greeks["delta"] + put_greeks["delta"], 1e-6);
    ASSERT_NEAR(straddle_greeks["vega"], call_greeks["vega"] + put_greeks["vega"], 1e-6);
    ASSERT_NEAR(straddle_greeks["volvega"], call_greeks["volvega"] + put_greeks["volvega"], 1e-6);
}

TEST(DistributionTest, TestBachelierOptionPrice) {
    const auto vol = 22.0;
    const auto T = 2.0;
    const auto forward = 110.0;
    const auto dean_vol = vol * sqrt(T);
    DistributionBachelier_ bachelier(forward, dean_vol);

    auto call_price = bachelier.OptionPrice(120.0, OptionType_("Call"));
    ASSERT_NEAR(call_price, 8.047832538, 1e-6);

    auto put_price = bachelier.OptionPrice(120.0, OptionType_("Put"));
    ASSERT_NEAR(put_price, 18.04783254, 1e-6);

    auto straddle_price = bachelier.OptionPrice(120.0, OptionType_("Straddle"));
    ASSERT_NEAR(straddle_price, 26.09566508, 1e-6);
}

TEST(DistributionTest, TestBachelierOptionIV) {
    const auto vol = 22.0;
    const auto T = 2.0;
    const auto forward = 110.0;
    const auto strike = 120.0;
    const auto dean_vol = vol * sqrt(T);
    DistributionBachelier_ bachelier(forward, dean_vol);

    auto call_price = bachelier.OptionPrice(strike, OptionType_("Call"));
    auto call_iv = Distribution::BachelierIV(forward, strike, OptionType_("Call"), call_price);
    ASSERT_NEAR(call_iv, vol * sqrt(T), 1e-6);

    auto put_price = bachelier.OptionPrice(strike, OptionType_("Put"));
    auto put_iv = Distribution::BachelierIV(forward, strike, OptionType_("Put"), put_price);
    ASSERT_NEAR(put_iv, vol * sqrt(T), 1e-6);

    auto straddle_price = bachelier.OptionPrice(120.0, OptionType_("Straddle"));
    auto straddle_iv = Distribution::BachelierIV(forward, strike, OptionType_("Straddle"), straddle_price);
    ASSERT_NEAR(straddle_iv, vol * sqrt(T), 1e-6);
}

TEST(DistributionTest, TestBachelierRealForwardStrikeRoundTrips) {
    struct Case_ {
        double forward_;
        double strike_;
    };
    const Case_ cases[] = {{110.0, 120.0}, {-110.0, -120.0}, {0.0, 10.0}, {10.0, 0.0}, {-10.0, 10.0}, {10.0, -10.0}};
    const OptionType_ types[] = {OptionType_("Call"), OptionType_("Put"), OptionType_("Straddle")};
    const double vol = 31.0;

    for (const auto& testCase : cases) {
        for (const auto& type : types) {
            const double price = Distribution::BachelierOpt(testCase.forward_, vol, testCase.strike_, type);
            const double expected = BachelierOracle(testCase.forward_, vol, testCase.strike_, type);
            ASSERT_NEAR(price, expected, 1e-12) << "forward=" << testCase.forward_ << " strike=" << testCase.strike_;
            ASSERT_GT(price, type.Payout(testCase.forward_, testCase.strike_));
            ASSERT_NEAR(Distribution::BachelierIV(testCase.forward_, testCase.strike_, type, price), vol, 1e-8);
        }

        const OptionType_ call("Call");
        const double callPrice = Distribution::BachelierOpt(testCase.forward_, vol, testCase.strike_, call);
        ASSERT_NEAR(Distribution::BachelierIV(testCase.forward_, testCase.strike_, call, callPrice, 0.5 * vol), vol, 1e-8);
    }
}

TEST(DistributionTest, TestBachelierIntrinsicHasZeroImpliedVol) {
    const OptionType_ types[] = {OptionType_("Call"), OptionType_("Put"), OptionType_("Straddle")};
    for (const auto& type : types) {
        const double intrinsic = type.Payout(-10.0, 10.0);
        ASSERT_DOUBLE_EQ(Distribution::BachelierIV(-10.0, 10.0, type, intrinsic), 0.0);
        ASSERT_DOUBLE_EQ(Distribution::BachelierIV(-10.0, 10.0, type, intrinsic, 31.0), 0.0);
    }
}

TEST(DistributionTest, TestBachelierIVIsTranslationInvariant) {
    const double vol = 31.0;
    const double moneyness = -10.0;
    const double shifts[] = {0.0, 1.0e9, 1.0e12, 1.0e14};
    const OptionType_ types[] = {OptionType_("Call"), OptionType_("Put"), OptionType_("Straddle")};

    for (const auto& type : types) {
        const double price = BachelierOracleFromMoneyness(moneyness, vol, type);
        for (const double shift : shifts) {
            const double forward = shift + 110.0;
            const double strike = shift + 120.0;
            ASSERT_DOUBLE_EQ(forward - strike, moneyness);

            const double defaultGuess = Distribution::BachelierIV(forward, strike, type, price);
            const double poorGuess = Distribution::BachelierIV(forward, strike, type, price, 100.0 * vol);
            ASSERT_TRUE(std::isfinite(defaultGuess));
            ASSERT_TRUE(std::isfinite(poorGuess));
            ASSERT_GE(defaultGuess, 0.0);
            ASSERT_GE(poorGuess, 0.0);
            ASSERT_NEAR(defaultGuess, vol, 1.0e-8);
            ASSERT_NEAR(poorGuess, vol, 1.0e-8);
        }
    }
}

TEST(DistributionTest, TestBachelierIVRejectsNonFiniteInputs) {
    const OptionType_ call("Call");
    const double price = BachelierOracleFromMoneyness(-10.0, 31.0, call);
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    ASSERT_THROW(Distribution::BachelierIV(inf, 120.0, call, price), Dal::Exception_);
    ASSERT_THROW(Distribution::BachelierIV(nan, 120.0, call, price), Dal::Exception_);
    ASSERT_THROW(Distribution::BachelierIV(110.0, inf, call, price), Dal::Exception_);
    ASSERT_THROW(Distribution::BachelierIV(110.0, nan, call, price), Dal::Exception_);
    ASSERT_THROW(Distribution::BachelierIV(110.0, 120.0, call, inf), Dal::Exception_);
    ASSERT_THROW(Distribution::BachelierIV(110.0, 120.0, call, nan), Dal::Exception_);
    ASSERT_THROW(Distribution::BachelierIV(110.0, 120.0, call, price, inf), Dal::Exception_);
    ASSERT_THROW(Distribution::BachelierIV(110.0, 120.0, call, price, nan), Dal::Exception_);
}

TEST(DistributionTest, TestBachelierIVThrowsWhenFiniteUpperBracketCannotBeFound) {
    const double maxPrice = std::numeric_limits<double>::max();
    ASSERT_THROW(Distribution::BachelierIV(0.0, 0.0, OptionType_("Call"), maxPrice), Dal::Exception_);
}

TEST(DistributionTest, TestBachelierParameterDerivatives) {
    const auto vol = 22.0;
    const auto T = 2.0;
    const auto forward = 110.0;
    const auto strike = 120.0;
    const auto dean_vol = vol * sqrt(T);

    DistributionBachelier_ bachelier(forward, dean_vol);
    auto call_greeks = bachelier.ParameterDerivatives(strike, OptionType_("Call"), {"delta", "vega", "volvega"});
    ASSERT_NEAR(call_greeks["delta"], 0.373949029, 1e-6);
    ASSERT_NEAR(call_greeks["vega"], 0.378858905, 1e-6);
    ASSERT_NEAR(call_greeks["volvega"], dean_vol * 0.378858905, 1e-6);

    auto put_greeks = bachelier.ParameterDerivatives(strike, OptionType_("Put"), {"delta", "vega", "volvega"});
    ASSERT_NEAR(put_greeks["delta"], -0.626050971, 1e-6);
    ASSERT_NEAR(put_greeks["vega"], 0.378858905, 1e-6);
    ASSERT_NEAR(put_greeks["volvega"], dean_vol * 0.378858905, 1e-6);

    auto straddle_greeks = bachelier.ParameterDerivatives(strike, OptionType_("Straddle"), {"delta", "vega", "volvega"});
    ASSERT_NEAR(straddle_greeks["delta"], call_greeks["delta"] + put_greeks["delta"], 1e-6);
    ASSERT_NEAR(straddle_greeks["vega"], call_greeks["vega"] + put_greeks["vega"], 1e-6);
    ASSERT_NEAR(straddle_greeks["volvega"], call_greeks["volvega"] + put_greeks["volvega"], 1e-6);
}
