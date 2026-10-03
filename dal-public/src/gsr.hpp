//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <dal/model/gsrcalibration.hpp>
#include <dal/model/gsrslvcalibration.hpp>
#include <dal/model/gsrmarketcalibration.hpp>

namespace Dal {
    Vector_<VolQuoteValue_> ConvertVolQuotes(const Handle_<Storable_>& snapshot, const Vector_<VolQuote_>& quotes);
    GSRSLVCalibrationResult_ CalibrateGSRSLVMarket(const Handle_<ModelData_>& initial,
                                                   const Vector_<VolQuote_>& quotes,
                                                   const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                                   const GSRSLVCalibrationSettings_& settings = {},
                                                   const Vector_<VolQuote_>& heldOut = {});
    GSRSLVQuoteRiskResult_ GSRSLVMarketQuoteRisk(const Handle_<ModelData_>& initial,
                                                 const Vector_<VolQuote_>& quotes,
                                                 const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                                 const Vector_<EuropeanRateOption_>& targets,
                                                 const GSRSLVCalibrationSettings_& calibrationSettings = {},
                                                 const GSRSLVQuoteRiskSettings_& riskSettings = {},
                                                 const GSRCurveQuoteRisk_* curveRisk = nullptr);
    GSRPriceResult_
    PriceGSREuropeanOption(const Handle_<ModelData_>& model, const EuropeanRateOption_& option, const GSRPricingSettings_& settings = {});
    GSRCalibrationResult_ CalibrateGSRVolatility(const Handle_<ModelData_>& initial,
                                                 const Vector_<CalibrationQuote_>& quotes,
                                                 const Vector_<GSRCalibrationParameter_>& parameters,
                                                 const GSRCalibrationSettings_& settings = {});
    Vector_<GSRMonteCarloPrice_> PriceGSRSLVEuropeanOptions(const Handle_<ModelData_>& model,
                                                            const Vector_<EuropeanRateOption_>& options,
                                                            const GSRMonteCarloSettings_& settings = {});
    GSRSLVCalibrationResult_ CalibrateGSRSLV(const Handle_<ModelData_>& initial,
                                             const Vector_<CalibrationQuote_>& quotes,
                                             const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                             const GSRSLVCalibrationSettings_& settings = {},
                                             const Vector_<CalibrationQuote_>& heldOut = {});
    GSRSLVQuoteRiskResult_ GSRSLVQuoteRisk(const Handle_<ModelData_>& initial,
                                           const Vector_<CalibrationQuote_>& quotes,
                                           const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                           const Vector_<EuropeanRateOption_>& targets,
                                           const GSRSLVCalibrationSettings_& calibrationSettings = {},
                                           const GSRSLVQuoteRiskSettings_& riskSettings = {},
                                           const GSRCurveQuoteRisk_* curveRisk = nullptr);
    GSRCurveQuoteRisk_ BuildGSRCurveQuoteRisk(const Handle_<Storable_>& snapshot,
                                              const RatePricingMarket_& market,
                                              const RateQuoteRiskProvenance_& provenance,
                                              const String_& discountComponent,
                                              const Vector_<String_>& projectionComponents = {});
} // namespace Dal
