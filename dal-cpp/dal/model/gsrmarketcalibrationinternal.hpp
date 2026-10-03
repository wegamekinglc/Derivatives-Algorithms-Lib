//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <dal/model/gsrmarketcalibration.hpp>

namespace Dal::GSRSLVCalibrationInternal {
    Vector_<CalibrationQuote_> PriceQuotes(const GSRCurveData_& curve, const Vector_<VolQuote_>& quotes);
}
