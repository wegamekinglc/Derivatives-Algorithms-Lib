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
        int expectedPassivePriceCount_ = 0;
        int expectedPreparationCount_ = 0;
        int expectedSweepCount_ = 0;
    };

    QuoteRiskBenchmarkCase_ MakeSingleCurveQuoteRiskCase();
    QuoteRiskBenchmarkCase_ MakeSingleCurveQuoteRiskCase(int quoteCount, CurveJacobianMode_ mode, int tradeCount);
    QuoteRiskBenchmarkCase_ MakeJointXccyQuoteRiskCase();
    QuoteRiskBenchmarkCase_ MakeJointXccyQuoteRiskCase(int quoteCount, CurveJacobianMode_ mode, int tradeCount);
    QuoteRiskBenchmarkCase_ MakeStagedXccyBasisQuoteRiskCase();
    QuoteRiskBenchmarkCase_ MakeStagedXccyBasisQuoteRiskCase(int quoteCount, CurveJacobianMode_ mode, int tradeCount);
} // namespace Dal::RateRiskPerf
