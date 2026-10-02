//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <variant>

#include <dal/model/gsrmultidata.hpp>
#include <dal/protocol/optiontype.hpp>

namespace Dal {
    struct GSRBondOption_ {
        Date_ expiry_, maturity_;
        double strike_ = 1.0;
        OptionType_ type_ = OptionType_("CALL");
    };

    struct GSRFloatingCoupon_ {
        Date_ fixing_, start_, end_, payment_;
        double indexAccrual_ = 0.0, couponAccrual_ = 0.0;
        String_ tenor_;
    };

    struct GSRCaplet_ {
        Date_ expiry_, start_, end_, payment_;
        double indexAccrual_ = 0.0, couponAccrual_ = 0.0;
        String_ tenor_;
        double strike_ = 0.0;
        OptionType_ type_ = OptionType_("CALL");
    };

    struct GSRFixedCoupon_ {
        Date_ payment_;
        double accrual_ = 0.0;
    };

    struct GSRSwaption_ {
        Date_ expiry_;
        Vector_<GSRFixedCoupon_> fixed_;
        Vector_<GSRFloatingCoupon_> floating_;
        double strike_ = 0.0;
        OptionType_ type_ = OptionType_("CALL");
    };

    using GSREuropeanOption_ = std::variant<GSRBondOption_, GSRCaplet_, GSRSwaption_>;

    struct GSRPricingSettings_ {
        int quadratureOrder_ = 16;
        bool estimateError_ = true;
    };

    struct GSRPriceResult_ {
        double price_ = 0.0;
        double numericalError_ = 0.0;
    };

    GSRPriceResult_
    PriceGSREuropeanOption(const MultiFactorGSRModelData_& model, const GSREuropeanOption_& option, const GSRPricingSettings_& settings = {});
    GSRPriceResult_ PriceGSREuropeanOption(const GSRModelData_& model, const GSREuropeanOption_& option, const GSRPricingSettings_& settings = {});
} // namespace Dal
