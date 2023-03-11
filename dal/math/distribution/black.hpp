//
// Created by wegam on 2022/5/5.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/distribution/base.hpp>
#include <dal/protocol/optiontype.hpp>

namespace Dal {
    namespace Distribution {
        template <class T_>
        T_ BlackOpt(const T_& fwd, const T_& vol, const T_& strike, const OptionType_& type) {
            if (IsZero(vol) || !IsPositive(fwd * strike))
                return type.Payout(fwd, strike);
            T_ dMinus = log(fwd / strike) / vol - 0.5 * vol;
            T_ dPlus = dMinus + vol;
            switch (type.Switch()) {
            case OptionType_::Value_::CALL:
                return fwd * NCDF(dPlus) - strike * NCDF(dMinus);
            case OptionType_::Value_::PUT:
                return strike * NCDF(T_(-dMinus)) - fwd * NCDF(T_(-dPlus));
            case OptionType_::Value_::STRADDLE:
                return fwd * (1.0 - 2.0 * NCDF(T_(-dPlus))) + strike * (1.0 - 2.0 * NCDF(dMinus));
            default:
                THROW("invalid option type");
            }
        }

        double BlackIV(double fwd, double strike, const OptionType_& type, double price, double guess = 0.0);
        Vector_<> BlackGreeks(double fwd, double vol, double strike, const OptionType_& type);

        template <class T_>
        T_ BachelierOpt(const T_& fwd, const T_& vol, const T_& strike, const OptionType_& type)  {
            if (IsZero(vol) || !IsPositive(fwd * strike))
                return type.Payout(fwd, strike);
            T_ diff = fwd - strike;
            T_ d = diff / vol;
            switch (type.Switch()) {
            case OptionType_::Value_::CALL:
                return diff * NCDF(d) + vol * NPDF(d);
            case OptionType_::Value_::PUT:
                return -diff * NCDF(T_(-d)) + vol * NPDF(d);
            case OptionType_::Value_::STRADDLE:
                return diff * (2.0 * NCDF(d) - 1.0) + 2.0 * vol * NPDF(d);
            default:
                THROW("invalid option type");
            }
        }

        double BachelierIV(double fwd, double strike, const OptionType_& type, double price, double guess = 0.0);
        Vector_<> BachelierGreeks(double fwd, double vol, double strike, const OptionType_& type);

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

    class DistributionBachelier_: public Distribution_ {
        double f_;
        double vol_;

    public:
        DistributionBachelier_(double fwd, double deann_vol): f_(fwd), vol_(deann_vol) {}
        [[nodiscard]] double Forward() const override { return f_; }
        [[nodiscard]] double OptionPrice(double strike, const OptionType_& type) const override {
            return Distribution::BachelierOpt(f_, vol_, strike, type);
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
