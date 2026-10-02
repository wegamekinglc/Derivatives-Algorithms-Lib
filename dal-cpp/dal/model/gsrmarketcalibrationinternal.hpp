//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <dal/model/gsrmarketcalibration.hpp>

namespace Dal::GSRSLVCalibrationInternal {
    Vector_<GSRCalibrationQuote_> PriceQuotes(const GSRCurveData_& curve, const Vector_<GSRMarketQuote_>& quotes);
}
