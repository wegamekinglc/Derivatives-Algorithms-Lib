//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <dal/model/gsrslvcalibration.hpp>

namespace Dal {
    struct GSRMarketQuote_ {
        String_ name_;
        GSREuropeanOption_ option_;
        double volatility_ = 0.0, priceScale_ = 1.0;
        String_ convention_ = "NORMAL";
        double shift_ = 0.0;
    };

    struct GSRMarketQuoteValue_ {
        double forward_ = 0.0, annuity_ = 0.0, price_ = 0.0, vega_ = 0.0;
    };

    Vector_<GSRMarketQuoteValue_> ConvertGSRMarketQuotes(const GSRCurveData_& curve, const Vector_<GSRMarketQuote_>& quotes);
    GSRSLVCalibrationResult_ CalibrateGSRSLVMarket(const GSRSLVModelData_& initial,
                                                   const Vector_<GSRMarketQuote_>& quotes,
                                                   const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                                   const GSRSLVCalibrationSettings_& settings = {},
                                                   const Vector_<GSRMarketQuote_>& heldOut = {});
    GSRSLVQuoteRiskResult_ GSRSLVMarketQuoteRisk(const GSRSLVModelData_& initial,
                                                 const Vector_<GSRMarketQuote_>& quotes,
                                                 const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                                 const Vector_<GSREuropeanOption_>& targets,
                                                 const GSRSLVCalibrationSettings_& calibrationSettings = {},
                                                 const GSRSLVQuoteRiskSettings_& riskSettings = {},
                                                 const GSRCurveQuoteRisk_* curveRisk = nullptr);
} // namespace Dal
