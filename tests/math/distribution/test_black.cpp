//
// Created by wegam on 2022/5/7.
//

#include <gtest/gtest.h>
#include <dal/math/distribution/black.hpp>

using namespace Dal;

TEST(DistributionBlackTest, TestBlackOptionPrice) {
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