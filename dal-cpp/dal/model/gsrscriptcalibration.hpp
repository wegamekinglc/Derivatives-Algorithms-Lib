#pragma once

#include <dal/model/gsrcalibration.hpp>
#include <dal/script/calibrationmc.hpp>

namespace Dal {
    struct GSRScriptCalibrationSettings_ {
        GSRCalibrationSettings_ optimizer_;
        CalibrationMcSettings_ sampling_;
        CalibrationDerivative_ derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
        size_t maxBasketEvaluations_ = 1000;
        double validationSigma_ = 3., epsilonValue_ = 1e-10, epsilonGradient_ = 1e-8;
    };
    struct GSRScriptCalibrationCosts_ {
        size_t preparations_ = 0, refreshes_ = 0, basketEvaluations_ = 0, jacobians_ = 0;
        size_t aadRows_ = 0, doubleRows_ = 0, batchRuns_ = 0, paths_ = 0;
        size_t peakTapeNodes_ = 0, peakTapeBytes_ = 0;
    };
    struct GSRScriptCalibrationResult_ : GSRCalibrationResult_ {
        CalibrationObservationAxes_ axes_, heldOutAxes_;
        Handle_<CalibrationReport_> report_;
        Vector_<String_> parameterLabels_;
        CalibrationDerivative_ derivativeMethod_;
        Vector_<CalibrationMcResult_> fit_, validation_, heldOut_;
        Vector_<CalibrationMcValidation_> validationIndicators_;
        Vector_<> scaledResiduals_, penaltyResiduals_;
        Vector_<bool> oneSidedColumns_;
        double dataObjective_ = 0., penaltyObjective_ = 0.;
        bool heldOutRequested_ = false, heldOutPassed_ = false, quoteRiskAvailable_ = false;
        GSRScriptCalibrationCosts_ costs_;
    };
    GSRScriptCalibrationResult_ CalibrateGSRScriptPrices(const CalibrationPreparationContext_& context,
                                                         const Vector_<Handle_<CalibrationObservation_>>& observations,
                                                         const Vector_<GSRCalibrationParameter_>& parameters,
                                                         const GSRScriptCalibrationSettings_& settings = {},
                                                         const Vector_<Handle_<CalibrationObservation_>>& heldOut = {});
} // namespace Dal
