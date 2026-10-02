//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <dal/model/gsrslvcalibration.hpp>
#include <dal/protocol/volquote.hpp>

namespace Dal {
    Vector_<VolQuoteValue_> ConvertVolQuotes(const GSRCurveData_& curve, const Vector_<VolQuote_>& quotes);
    GSRSLVCalibrationResult_ CalibrateGSRSLVMarket(const GSRSLVModelData_& initial,
                                                   const Vector_<VolQuote_>& quotes,
                                                   const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                                   const GSRSLVCalibrationSettings_& settings = {},
                                                   const Vector_<VolQuote_>& heldOut = {});
    GSRSLVQuoteRiskResult_ GSRSLVMarketQuoteRisk(const GSRSLVModelData_& initial,
                                                 const Vector_<VolQuote_>& quotes,
                                                 const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                                 const Vector_<EuropeanRateOption_>& targets,
                                                 const GSRSLVCalibrationSettings_& calibrationSettings = {},
                                                 const GSRSLVQuoteRiskSettings_& riskSettings = {},
                                                 const GSRCurveQuoteRisk_* curveRisk = nullptr);
} // namespace Dal
