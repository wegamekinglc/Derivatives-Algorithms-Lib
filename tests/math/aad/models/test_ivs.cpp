//
// Created by wegam on 2022/9/17.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/math/aad/models/ivs.hpp>

using namespace Dal;
using namespace Dal::AAD;


TEST(AADTest, TestMertonIVS) {
    const auto T = 2.0;
    const auto strike = 110;
    const auto spot = 100;

    MertonIVS_ ivs(spot, 0.15, 0.05, -0.15, 0.1);
    auto implied_vol = ivs.ImpliedVol(strike, T);
    auto expected = ivs.Call(strike, T);

    const auto dean_vol = implied_vol * sqrt(T);
    Dal::DistributionBlack_ black(spot, dean_vol);
    const auto calculated = black.OptionPrice(strike, Dal::OptionType_("Call"));
    ASSERT_NEAR(calculated, expected, 1e-5);
}
