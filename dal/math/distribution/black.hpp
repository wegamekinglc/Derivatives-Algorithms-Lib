//
// Created by wegam on 2022/5/5.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/distribution/base.hpp>
#include <dal/protocol/optiontype.hpp>

namespace Dal {
    namespace Distribution {
        double BlackOpt(double fwd, double vol, double strike, const OptionType_& type);
        double BlackIV(double fwd, double strike, const OptionType_& type, double price, double guess = 0.0);
        Vector_<> BlackGreeks(double fwd, double vol, double strike, const OptionType_& type);
    }

    class DistributionBlack_: public Distribution_ {
        double f_;
        double vol_;

    public:
        DistributionBlack_(double fwd, double deann_vol): f_(fwd), vol_(deann_vol) {}

        [[nodiscard]] double Forward() const override { return f_; }
        [[nodiscard]] double OptionPrice(double strike, const OptionType_& type) const override {
            return Distribution::BlackOpt(f_, vol_, strike, type);
        }
        double & Vol() override { return vol_; }
        [[nodiscard]] const double& Vol() const override { return vol_; }
        [[nodiscard]] double VolVega(double strike, const OptionType_& type) const override;
        [[nodiscard]] Vector_<String_> ParameterNames() const override {
            Vector_<String_> ret_val;
            ret_val.push_back("forward");
            ret_val.push_back("vol");
            return ret_val;
        }

        [[nodiscard]] std::map<String_, double> ParameterDerivatives(double strike,
                                                                     const OptionType_& type,
                                                                     const Vector_<String_>& to_report) const override;
    };
}
