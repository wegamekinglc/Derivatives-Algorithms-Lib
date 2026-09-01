//
// Created by dal-implementer on 2026/7/28.
//

#pragma once

#include <dal/curve/quoteriskaggregation.hpp>
#include <dal/curve/quoteriskprovenance.hpp>
#include <dal/curve/ratecashflowpricing.hpp>

#include <dal-public/src/curvespec.hpp>

namespace Dal {
    const Vector_<RateInstrumentType_>& CurvePricingFamilyRegistry();

    RateQuoteRiskProvenance_ BuildSingleCurveQuoteRiskProvenance(const CurveCalibrationSpec_& spec,
                                                                 const CalibrationResult_& result,
                                                                 const CurveCalibrationOptions_& options,
                                                                 const RatePricingMarket_& boundMarket,
                                                                 const RateQuoteRiskProvenanceConfig_& config);
} // namespace Dal
