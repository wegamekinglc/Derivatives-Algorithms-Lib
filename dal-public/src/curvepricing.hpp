//
// Created by dal-implementer on 2026/7/28.
//

#pragma once

#include <dal/curve/quoteriskaggregation.hpp>
#include <dal/curve/quoteriskprovenance.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccypricing.hpp>

#include <dal-public/src/curvespec.hpp>

namespace Dal {
    Vector_<FixingRequest_> RequiredHistoricalRateTradeFixings(const RateTradeDefinition_& trade, const DateTime_& valuationTime);
    Vector_<FixingRequest_> RequiredHistoricalXccyFixings(const Handle_<CrossCurrencySwap_>& instrument, const DateTime_& valuationTime);
    std::shared_ptr<CrossCurrencyMarket_> NewCrossCurrencyMarket(const Handle_<CurveBlock_>& domesticBlock,
                                                                 const Handle_<CurveBlock_>& foreignBlock,
                                                                 double fxSpot,
                                                                 const DateTime_& valuationTime,
                                                                 const Ccy_& collateralCurrency,
                                                                 const Handle_<MarketFixingSnapshot_>& fixings = {},
                                                                 const Handle_<DiscountCurve_>& basisCurve = {});

    const Vector_<RateInstrumentType_>& CurvePricingFamilyRegistry();

    RateQuoteRiskProvenance_ BuildSingleCurveQuoteRiskProvenance(const CurveCalibrationSpec_& spec,
                                                                 const CalibrationResult_& result,
                                                                 const CurveCalibrationOptions_& options,
                                                                 const RatePricingMarket_& boundMarket,
                                                                 const RateQuoteRiskProvenanceConfig_& config);
} // namespace Dal
