#pragma once

#include <dal/model/gsrbondoption.hpp>

namespace Dal {
    using RateOptionScriptImportSettings_ = BondOptionScriptImportSettings_;

    Handle_<CalibrationInstrument_> NewCapletScriptInstrument(const Caplet_& option, const RateOptionScriptImportSettings_& settings);
    Handle_<CalibrationInstrument_> NewSwaptionScriptInstrument(const Swaption_& option, const RateOptionScriptImportSettings_& settings);
    GSRPriceResult_ PriceGaussianRateOption(const PreparedCalibrationInstrument_& plan,
                                            const Handle_<CalibrationInstrument_>& instrument,
                                            const CalibrationPreparationContext_& context,
                                            const GSRPricingSettings_& settings = {});
} // namespace Dal
