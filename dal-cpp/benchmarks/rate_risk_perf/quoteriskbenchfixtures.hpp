//
// Created by dal-implementer on 2026/9/2.
//

#pragma once

#include <memory>

#include <dal/curve/quoteriskaggregation.hpp>

namespace Dal::RateRiskPerf {
    struct QuoteRiskBenchmarkCase_ {
        Vector_<RateTradeDefinition_> trades_;
        RatePricingMarket_ market_;
        Vector_<RateQuoteRiskProvenance_> provenances_;
        std::shared_ptr<void> calibrationLifetime_;
    };

    QuoteRiskBenchmarkCase_ MakeSingleCurveQuoteRiskCase();
    QuoteRiskBenchmarkCase_ MakeJointXccyQuoteRiskCase();
    QuoteRiskBenchmarkCase_ MakeStagedXccyBasisQuoteRiskCase();
} // namespace Dal::RateRiskPerf
