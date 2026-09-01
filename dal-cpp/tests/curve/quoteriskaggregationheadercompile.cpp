//
// Created by dal-implementer on 2026/9/1.
//

#include <type_traits>

#include <dal/curve/quoteriskaggregation.hpp>

namespace {
    using Aggregate_ = Dal::RatePortfolioQuoteRisk_ (*)(const Dal::Vector_<Dal::RateTradeDefinition_>&,
                                                        const Dal::RatePricingMarket_&,
                                                        const Dal::Vector_<Dal::RateQuoteRiskProvenance_>&);

    static_assert(std::is_same_v<decltype(&Dal::AggregateRatePortfolioQuoteRisk), Aggregate_>);
} // namespace
