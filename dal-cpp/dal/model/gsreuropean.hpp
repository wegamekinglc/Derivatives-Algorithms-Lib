//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <dal/model/gsrmultidata.hpp>
#include <dal/protocol/rateoption.hpp>

namespace Dal {
    struct GSRPricingSettings_ {
        int quadratureOrder_ = 16;
        bool estimateError_ = true;
    };

    struct GSRPriceResult_ {
        double price_ = 0.0;
        double numericalError_ = 0.0;
    };

    GSRPriceResult_
    PriceGSREuropeanOption(const MultiFactorGSRModelData_& model, const EuropeanRateOption_& option, const GSRPricingSettings_& settings = {});
    GSRPriceResult_ PriceGSREuropeanOption(const GSRModelData_& model, const EuropeanRateOption_& option, const GSRPricingSettings_& settings = {});
} // namespace Dal
