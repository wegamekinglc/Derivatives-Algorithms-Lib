//
// Created by dal-implementer on 2026/9/2.
//

#pragma once

#include <memory>

#include <dal/curve/calibration.hpp>
#include <dal/curve/quoteriskaggregation.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccyjointcalibration.hpp>

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

    // Everything a provenance builder consumes, with the calibration result kept alive for the
    // market's aliased components — so benchmarks can time the Build* call itself rather than
    // the calibration behind it.
    template <class Spec_, class Options_, class Result_> struct ProvenanceMaterials_ {
        Spec_ spec_;
        Options_ options_;
        std::shared_ptr<Result_> calibration_;
        RatePricingMarket_ market_;
        RateQuoteRiskProvenanceConfig_ config_;
    };

    using SingleCurveProvenanceMaterials_ = ProvenanceMaterials_<CurveCalibrationSpec_, CurveCalibrationOptions_, CurveCalibrationResult_>;
    using JointXccyProvenanceMaterials_ = ProvenanceMaterials_<JointXccyCalibrationSpec_, JointXccyCalibrationOptions_, JointXccyCalibrationResult_>;
    using StagedXccyProvenanceMaterials_ = ProvenanceMaterials_<CrossCurrencyCalibrationSpec_, CrossCurrencyCalibrationOptions_, CrossCurrencyCalibrationResult_>;

    SingleCurveProvenanceMaterials_ MakeSingleCurveProvenanceMaterials(int quoteCount, CurveJacobianMode_ mode);
    JointXccyProvenanceMaterials_ MakeJointXccyProvenanceMaterials(int quoteCount, CurveJacobianMode_ mode);
    StagedXccyProvenanceMaterials_ MakeStagedXccyProvenanceMaterials(int quoteCount, CurveJacobianMode_ mode);
} // namespace Dal::RateRiskPerf
