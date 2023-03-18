//
// Created by wegam on 2022/5/7.
//

#include <dal/math/operators.hpp>
#include <dal/math/distribution/black.hpp>
#include <gtest/gtest.h>

using namespace Dal;

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