#pragma once

#include <dal/model/gsrcalibration.hpp>
#include <dal/script/calibrationpreparation.hpp>

namespace Dal {
    struct BondOptionScriptImportSettings_ {
        String_ instrumentId_;
        Ccy_ currency_;
        String_ discountRoute_;
        double notional_ = 0.;
    };

    Handle_<CalibrationInstrument_> NewBondOptionScriptInstrument(const BondOption_& option, const BondOptionScriptImportSettings_& settings);
    GSRPriceResult_ PriceGaussianBondOption(const PreparedCalibrationInstrument_& plan,
                                            const Handle_<CalibrationInstrument_>& instrument,
                                            const CalibrationPreparationContext_& context);

    struct GaussianBondOptionCosts_ {
        size_t models_ = 0, preparations_ = 0, fastPrices_ = 0, monteCarloPaths_ = 0;
    };
    struct GaussianBondOptionCalibrationResult_ : GSRCalibrationResult_ {
        CalibrationObservationAxes_ axes_;
        Handle_<CalibrationReport_> report_;
        Vector_<String_> parameterLabels_, planIdentities_;
        Vector_<> scaledResiduals_;
        Vector_<bool> oneSidedColumns_;
        bool nativeJacobianAvailable_ = true;
        GaussianBondOptionCosts_ costs_;
        String_ numeraire_ = "dal.native-gsr/1";
    };
    GaussianBondOptionCalibrationResult_ CalibrateGaussianBondOptions(const CalibrationPreparationContext_& context,
                                                                      const Vector_<Handle_<CalibrationObservation_>>& observations,
                                                                      const Vector_<GSRCalibrationParameter_>& parameters,
                                                                      const GSRCalibrationSettings_& settings = {});
} // namespace Dal
