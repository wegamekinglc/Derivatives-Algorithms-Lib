//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <dal-public/src/gsr.hpp>

namespace Dal {
    GSRPriceResult_ PriceGSREuropeanOption(const Handle_<ModelData_>& model, const GSREuropeanOption_& option, const GSRPricingSettings_& settings) {
        REQUIRE(model, "InvalidGSRPricing: model is required");
        if (const auto* data = dynamic_cast<const MultiFactorGSRModelData_*>(model.get()))
            return PriceGSREuropeanOption(*data, option, settings);
        if (const auto* data = dynamic_cast<const GSRModelData_*>(model.get()))
            return PriceGSREuropeanOption(*data, option, settings);
        THROW("InvalidGSRPricing: model must be GSRModelData or MultiFactorGSRModelData");
    }

    GSRCalibrationResult_ CalibrateGSRVolatility(const Handle_<ModelData_>& initial,
                                                 const Vector_<GSRCalibrationQuote_>& quotes,
                                                 const Vector_<GSRCalibrationParameter_>& parameters,
                                                 const GSRCalibrationSettings_& settings) {
        REQUIRE(initial, "InvalidGSRCalibration: initial model is required");
        const auto* data = dynamic_cast<const MultiFactorGSRModelData_*>(initial.get());
        REQUIRE(data, "InvalidGSRCalibration: initial model must be MultiFactorGSRModelData");
        return CalibrateGSRVolatility(*data, quotes, parameters, settings);
    }
} // namespace Dal
