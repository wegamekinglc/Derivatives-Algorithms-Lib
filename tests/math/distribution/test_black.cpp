//
// Created by wegam on 2022/5/7.
//

#include <dal/math/aad/operators.hpp>
#include <dal/math/distribution/black.hpp>
#include <gtest/gtest.h>

using namespace Dal;

TEST(DistributionBlackTest, TestBlackOptionPrice) {
    const auto vol = 0.2;
    const auto T = 2.0;
    const auto forward = 110.0;
    const auto dean_vol = vol * AAD::Sqrt(T);
    DistributionBlack_ black(forward, dean_vol);

    auto call_price = black.OptionPrice(120.0, OptionType_("Call"));
    ASSERT_NEAR(call_price, 8.53592506, 1e-6);

    auto put_price = black.OptionPrice(120.0, OptionType_("Put"));
    ASSERT_NEAR(put_price, 18.53592506, 1e-6);

    auto straddle_price = black.OptionPrice(120.0, OptionType_("Straddle"));
    ASSERT_NEAR(straddle_price, 27.07185013, 1e-6);
}

TEST(DistributionBlackTest, TestBlackOptionIV) {
    const auto vol = 0.2;
    const auto T = 2.0;
    const auto forward = 110.0;
    const auto strike = 120.0;
    const auto dean_vol = vol * AAD::Sqrt(T);
    DistributionBlack_ black(forward, dean_vol);

    auto call_price = black.OptionPrice(strike, OptionType_("Call"));
    auto call_iv = Distribution::BlackIV(forward, strike, OptionType_("Call"), call_price);
    ASSERT_NEAR(call_iv, vol * AAD::Sqrt(T), 1e-6);

    auto put_price = black.OptionPrice(strike, OptionType_("Put"));
    auto put_iv = Distribution::BlackIV(forward, strike, OptionType_("Put"), put_price);
    ASSERT_NEAR(put_iv, vol * AAD::Sqrt(T), 1e-6);

    auto straddle_price = black.OptionPrice(120.0, OptionType_("Straddle"));
    auto straddle_iv = Distribution::BlackIV(forward, strike, OptionType_("Straddle"), straddle_price);
    ASSERT_NEAR(straddle_iv, vol * AAD::Sqrt(T), 1e-6);
}

TEST(DistributionBlackTest, TestBlackParameterDerivatives) {
    const auto vol = 0.2;
    const auto T = 2.0;
    const auto forward = 110.0;
    const auto strike = 120.0;
    const auto dean_vol = vol * AAD::Sqrt(T);

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