//
// Created by dal-implementer on 2026/9/1.
//

#pragma once

#include <map>

#include <dal/curve/quoteriskprovenance.hpp>
#include <dal/curve/ratecashflowpricing.hpp>

namespace Dal {
    struct RateQuoteRiskBucket_ {
        String_ calibrationId_;
        String_ axisFingerprint_;
        String_ quoteKey_;
        String_ quoteName_;
        String_ residualBlock_;
        int quoteOrdinal_ = 0;
        Ccy_ actualPvCcy_;
        double dPvDDecimalQuote_ = 0.0;
        double dv01_ = 0.0;
    };

    struct RatePortfolioQuoteRiskMetaEntry_ {
        String_ instrumentId_;
        String_ calibrationId_;
        bool eligible_ = false;
        bool structuralZero_ = false;
        String_ reason_;
        String_ failingComponentKey_;
        String_ originalNodeRiskReason_;
        Ccy_ actualPvCcy_;
        double pv_ = 0.0;
    };

    struct RateQuoteRiskProvenanceFailure_ {
        String_ calibrationId_;
        String_ reason_;
        String_ componentKey_;
        String_ expectedStateFingerprint_;
        String_ actualStateFingerprint_;
    };

    struct RatePortfolioQuoteRisk_ {
        String_ policy_ = "UnconvertedByActualPvCcy";
        Vector_<RateQuoteRiskBucket_> buckets_;
        std::map<String_, double> pvByActualPvCcy_;
        Vector_<RatePortfolioQuoteRiskMetaEntry_> meta_;
        Vector_<RateQuoteRiskProvenanceFailure_> provenanceFailures_;
    };

    RatePortfolioQuoteRisk_ AggregateRatePortfolioQuoteRisk(const Vector_<RateTradeDefinition_>& trades,
                                                            const RatePricingMarket_& market,
                                                            const Vector_<RateQuoteRiskProvenance_>& provenances);
} // namespace Dal
