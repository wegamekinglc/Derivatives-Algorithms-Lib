//
// Created by wegam on 2022/5/5.
//

#pragma once

#include <dal/math/distribution/distribution.hpp>
#include <dal/protocol/optiontype.hpp>

namespace Dal {
    namespace Distribution {
        double BlackOpt(double fwd, double vol, double strike, const OptionType_& type);
        double BlackIV(double fwd, double strike, const OptionType_& type, double price, double guess = 0.0);
    }

    class DistributionBlack_: public Distribution_ {
        double f_;
        double vol_;

    public:
        DistributionBlack_(double fwd, double deann_vol): f_(fwd), vol_(deann_vol) {}

        double Forward() const { return f_; }
        double OptionPrice(double strike, const OptionType_& type) const override;
        double & Vol() override { return vol_; }
        const double & Vol() const override { return vol_; }
        double VolVega(double strike, const OptionType_& type) const override;
    };
}
