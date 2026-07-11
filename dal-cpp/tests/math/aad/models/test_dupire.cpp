//
// Created by wegam on 2022/9/17.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/model/dupire.hpp>

using namespace Dal::AAD;

namespace {
    class FlatIVS_ : public IVS_ {
        double vol_;

    public:
        FlatIVS_(double spot, double rate, double repo, double vol) : IVS_(spot, rate, repo), vol_(vol) {}

        [[nodiscard]] double ImpliedVol(double, double) const override { return vol_; }
    };
} // namespace

TEST(AADTest, TestDupireCalib) {
    const auto spot = 100;

    MertonIVS_ ivs(spot, 0.15, 0.05, -0.15, 0.1);
    Dal::Vector_<> incl_spots{ 50.0, 100.0, 200.0 };
    const auto max_ds = 5.0;
    Dal::Vector_<> incl_times{ 5.0 };
    const auto max_dt = 0.08333;
    auto results = DupireCalib(ivs, incl_spots, max_ds, incl_times, max_dt);
    ASSERT_NEAR(results.lVols_(0, 0), 0.187513, 1e-5);
}

TEST(AADTest, TestDupireCalibRateAwareFlatVol) {
    const double vol = 0.25;
    FlatIVS_ ivs(100.0, 0.05, 0.02, vol);
    const Dal::Vector_<> inclSpots{80.0, 100.0, 120.0};
    const Dal::Vector_<> inclTimes{0.5, 1.5};

    const auto results = DupireCalib(ivs, inclSpots, 5.0, inclTimes, 0.25);
    for (const auto& localVol : results.lVols_)
        ASSERT_NEAR(localVol, vol, 2e-6);
}
