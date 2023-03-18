//
// Created by wegam on 2022/9/17.
//

#include <gtest/gtest.h>
#include <dal/math/aad/models/dupire.hpp>

using namespace Dal::AAD;

TEST(ModelsTest, TestDupireCalib) {
    const auto spot = 100;

    MertonIVS_ ivs(spot, 0.15, 0.05, -0.15, 0.1);
    Dal::Vector_<> incl_spots{ 50.0, 100.0, 200.0 };
    const auto max_ds = 5.0;
    Dal::Vector_<> incl_times{ 5.0 };
    const auto max_dt = 0.08333;
    auto results = DupireCalib(ivs, incl_spots, max_ds, incl_times, max_dt);
    ASSERT_NEAR(results.lVols_(0, 0), 0.187513, 1e-5);
}
