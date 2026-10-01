//
// Created by dal-implementer on 2026/7/28.
//

#include <dal-public/src/curvepricing.hpp>

namespace Dal {
    Vector_<FixingRequest_> RequiredHistoricalRateTradeFixings(const RateTradeDefinition_& trade, const DateTime_& valuationTime) {
        return BuildRateCashflowPlan(trade, valuationTime).requiredHistoricalFixings_;
    }

    Vector_<FixingRequest_> RequiredHistoricalXccyFixings(const Handle_<CrossCurrencySwap_>& instrument, const DateTime_& valuationTime) {
        REQUIRE(instrument, "instrument must be a non-null CrossCurrencySwap handle");
        const auto span = instrument->TimeSpan();
        return RequiredHistoricalFixings(BuildXccyCashflowPlan(span.first, span.second, instrument->Config()), valuationTime);
    }

    std::shared_ptr<CrossCurrencyMarket_> NewCrossCurrencyMarket(const Handle_<CurveBlock_>& domesticBlock,
                                                                 const Handle_<CurveBlock_>& foreignBlock,
                                                                 double fxSpot,
                                                                 const DateTime_& valuationTime,
                                                                 const Ccy_& collateralCurrency,
                                                                 const Handle_<MarketFixingSnapshot_>& fixings,
                                                                 const Handle_<DiscountCurve_>& basisCurve) {
        auto result = std::make_shared<CrossCurrencyMarket_>(domesticBlock, foreignBlock, fxSpot, valuationTime, collateralCurrency, fixings);
        if (basisCurve)
            result->SetBasisCurve(basisCurve);
        return result;
    }

    const Vector_<RateInstrumentType_>& CurvePricingFamilyRegistry() {
        static const Vector_<RateInstrumentType_> result = RateInstrumentTypeListAll();
        return result;
    }

    RateQuoteRiskProvenance_ BuildSingleCurveQuoteRiskProvenance(const CurveCalibrationSpec_& spec,
                                                                 const CalibrationResult_& result,
                                                                 const CurveCalibrationOptions_& options,
                                                                 const RatePricingMarket_& boundMarket,
                                                                 const RateQuoteRiskProvenanceConfig_& config) {
        REQUIRE(result.curve_, "QUOTE_RISK_CALIBRATION_RESULT_CURVE_EMPTY");
        std::unique_ptr<YCComponent_> cloned = result.curve_->Clone(result.curve_->Name(), {});
        auto* curve = dynamic_cast<DiscountCurve_*>(cloned.get());
        REQUIRE(curve, "QUOTE_RISK_CALIBRATION_RESULT_CURVE_INVALID");

        CurveCalibrationResult_ coreResult;
        coreResult.curve_.reset(static_cast<DiscountCurve_*>(cloned.release()));
        coreResult.diagnostics_ = result.diagnostics_;
        return BuildSingleCurveQuoteRiskProvenance(spec, coreResult, options, boundMarket, config);
    }
} // namespace Dal
