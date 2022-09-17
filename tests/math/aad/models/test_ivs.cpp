//
// Created by wegam on 2022/9/17.
//

#include <gtest/gtest.h>
#include <dal/math/distribution/black.hpp>
#include <dal/math/aad/models/ivs.hpp>

using namespace Dal::AAD;


TEST(IVSTest, TestMertonIVS) {
    const auto T = 2.0;
    const auto strike = 110;
    const auto spot = 100;

    MertonIVS_ ivs(spot, 0.2, 0.1, 0.1, 0.2);
    auto implied_vol = ivs.ImpliedVol(strike, T);
    auto expected = ivs.Call(strike, T);
    std::cout << implied_vol << std::endl;

    const auto dean_vol = implied_vol * Sqrt(T);
    Dal::DistributionBlack_ black(spot, dean_vol);
    const auto calculated = black.OptionPrice(strike, Dal::OptionType_("Call"));

    ASSERT_NEAR(calculated, expected, 1e-5);
}
