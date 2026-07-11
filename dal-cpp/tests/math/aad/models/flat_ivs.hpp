//
// Created by wegam on 2026/7/11.
//

#pragma once

#include <dal/model/ivs.hpp>

namespace Dal::AAD {
    // Constant-implied-vol IVS_ fixture shared by the IVS and Dupire tests.
    class FlatIVS_ : public IVS_ {
        double vol_;

    public:
        FlatIVS_(double spot, double rate, double repo, double vol) : IVS_(spot, rate, repo), vol_(vol) {}

        [[nodiscard]] double ImpliedVol(double, double) const override { return vol_; }
    };
} // namespace Dal::AAD
