#pragma once

#include <dal/curve/quoteriskprovenance.hpp>

namespace Dal {
    struct RatePricingMarket_;

    bool SupportsLiveQuoteRiskState(const RateQuoteRiskProvenance_& provenance);
    void RequireLiveQuoteRiskState(const RateQuoteRiskProvenance_& provenance, const char* field);

    RateQuoteRiskComponentState_ CurrentRateQuoteRiskComponentState(const String_& componentKey, const RatePricingMarket_& market);
    RateQuoteRiskComponentState_
    CurrentRateQuoteRiskComponentState(const String_& componentKey, const RatePricingMarket_& market, const String_& scheme);
} // namespace Dal
