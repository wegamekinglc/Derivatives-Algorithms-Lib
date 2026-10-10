//
// Created by Codex on 2026/10/10.
//

#pragma once

#include <dal/curve/rateparameterjacobian.hpp>
#include <dal/math/aad/bumpoveraad.hpp>

namespace Dal::RateCashflowPricingInternal {
    struct RateTradeObjectiveSettings_ {
        Vector_<> weights_;
        Handle_<MarketFixingSnapshot_> fixings_;
        // Raw quote coordinates follow parameters in the curvature callback.
        size_t trailingInputs_ = 0;
    };

    struct RateTradeObjective_ {
        Ccy_ currency_;
        AAD::NativeScalarFunction_ objective_;
    };

    [[nodiscard]] RateTradeObjective_ NewRateTradeObjective(const Vector_<RateTradeDefinition_>& trades,
                                                            const RatePricingMarket_& market,
                                                            const Vector_<RateCurveParameterCoordinate_>& inputAxis,
                                                            const RateTradeObjectiveSettings_& settings = {});
} // namespace Dal::RateCashflowPricingInternal
