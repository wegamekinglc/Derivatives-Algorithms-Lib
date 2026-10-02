//
// Created by ZCode on 2026/10/3.
//

#pragma once

#include <variant>

#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/protocol/optiontype.hpp>
#include <dal/time/date.hpp>

namespace Dal {
    struct FixedCoupon_ {
        Date_ payment_;
        double accrual_ = 0.0;
    };

    struct FloatingCoupon_ {
        Date_ fixing_, start_, end_, payment_;
        double indexAccrual_ = 0.0, couponAccrual_ = 0.0;
        String_ tenor_;
    };

    struct BondOption_ {
        Date_ expiry_, maturity_;
        double strike_ = 1.0;
        OptionType_ type_ = OptionType_("CALL");
    };

    struct Caplet_ {
        Date_ expiry_, start_, end_, payment_;
        double indexAccrual_ = 0.0, couponAccrual_ = 0.0;
        String_ tenor_;
        double strike_ = 0.0;
        OptionType_ type_ = OptionType_("CALL");
    };

    struct Swaption_ {
        Date_ expiry_;
        Vector_<FixedCoupon_> fixed_;
        Vector_<FloatingCoupon_> floating_;
        double strike_ = 0.0;
        OptionType_ type_ = OptionType_("CALL");
    };

    using EuropeanRateOption_ = std::variant<BondOption_, Caplet_, Swaption_>;
} // namespace Dal
