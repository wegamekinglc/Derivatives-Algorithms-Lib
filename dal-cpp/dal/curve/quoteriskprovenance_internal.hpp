//
// Created by dal-implementer on 2026/9/1.
//

#pragma once

#include <dal/curve/quoteriskprovenance.hpp>

namespace Dal {
    struct RatePricingMarket_;

    RateQuoteRiskComponentState_ CurrentRateQuoteRiskComponentState(const String_& componentKey, const RatePricingMarket_& market);
} // namespace Dal
