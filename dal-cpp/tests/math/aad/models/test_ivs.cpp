//
// Created by wegam on 2022/9/17.
//

#include <gtest/gtest.h>

#include <cmath>

#include <dal/platform/platform.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/model/ivs.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    class FlatIVS_ : public IVS_ {
        double vol_;

    public:
        FlatIVS_(double spot, double rate, double repo, double vol) : IVS_(spot, rate, repo), vol_(vol) {}

        [[nodiscard]] double ImpliedVol(double, double) const override { return vol_; }
    };
} // namespace

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

TEST(AADTest, TestRateAwareFlatVolLocalVol) {
    const double spot = 100.0;
    const double rate = 0.05;
    const double repo = 0.02;
    const double vol = 0.25;
    const double mat = 1.7;
    FlatIVS_ ivs(spot, rate, repo, vol);

    const double strike = 105.0;
    const double forward = spot * std::exp((rate - repo) * mat);
    const double stdDev = vol * std::sqrt(mat);
    const double dMinus = std::log(forward / strike) / stdDev - 0.5 * stdDev;
    const double expectedCall = std::exp(-rate * mat) * (forward * NCDF(dMinus + stdDev) - strike * NCDF(dMinus));
    ASSERT_NEAR(ivs.Call(strike, mat), expectedCall, 1e-10);

    for (double localStrike : {80.0, 100.0, 120.0})
        ASSERT_NEAR(ivs.LocalVol(localStrike, mat), vol, 2e-6) << "strike=" << localStrike;
}
