//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <dal/model/gsrcalibration.hpp>

namespace Dal {
    GSRPriceResult_
    PriceGSREuropeanOption(const Handle_<ModelData_>& model, const GSREuropeanOption_& option, const GSRPricingSettings_& settings = {});
    GSRCalibrationResult_ CalibrateGSRVolatility(const Handle_<ModelData_>& initial,
                                                 const Vector_<GSRCalibrationQuote_>& quotes,
                                                 const Vector_<GSRCalibrationParameter_>& parameters,
                                                 const GSRCalibrationSettings_& settings = {});
} // namespace Dal
