//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <dal/model/gsrslvcalibration.hpp>

namespace Dal::GSRSLVCalibrationInternal {
    GSRSLVCalibrationResult_ FitModel(const GSRSLVModelData_& initial,
                                      const Vector_<CalibrationQuote_>& quotes,
                                      const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                      const GSRSLVCalibrationSettings_& settings);
}
