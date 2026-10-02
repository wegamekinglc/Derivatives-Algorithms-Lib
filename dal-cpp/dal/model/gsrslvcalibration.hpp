//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <dal/model/gsrcalibration.hpp>
#include <dal/model/gsrcurverisk.hpp>
#include <dal/model/gsrslvpricing.hpp>

namespace Dal {
    struct GSRSLVCalibrationParameter_ {
        String_ label_;
        double lower_ = 0.0, upper_ = 2.0, scale_ = 1.0;
    };

    struct GSRSLVCalibrationSettings_ {
        GSRCalibrationSettings_ solver_;
        GSRMonteCarloSettings_ pricing_;
        GSRMonteCarloSettings_ validation_{8192, 81173};
        double validationSigma_ = 3.0;
        bool staged_ = true;
    };

    struct GSRSLVCalibrationResult_ {
        Handle_<GSRSLVModelData_> model_;
        Vector_<> modelPrices_, residuals_, standardErrors_, parameters_;
        Vector_<> validationPrices_, validationStandardErrors_, numericalErrors_;
        Vector_<> heldOutPrices_, heldOutResiduals_, heldOutStandardErrors_;
        Vector_<bool> activeBounds_;
        Matrix_<> quoteJacobian_;
        bool converged_ = false, fitWithinTolerance_ = false, numericalValidationPassed_ = false, heldOutWithinTolerance_ = false;
        int iterations_ = 0, evaluations_ = 0, jacobianRank_ = 0;
        double objective_ = 0.0, jacobianConditionEstimate_ = 0.0;
        String_ terminationReason_;
    };

    GSRSLVCalibrationResult_ CalibrateGSRSLV(const GSRSLVModelData_& initial,
                                             const Vector_<GSRCalibrationQuote_>& quotes,
                                             const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                             const GSRSLVCalibrationSettings_& settings = {},
                                             const Vector_<GSRCalibrationQuote_>& heldOut = {});

    struct GSRSLVQuoteRiskSettings_ {
        double relativeBump_ = 0.01;
        double absoluteBump_ = 1e-6;
        double stabilityTolerance_ = 0.05;
    };

    struct GSRSLVQuoteRiskResult_ {
        GSRSLVCalibrationResult_ calibration_;
        Vector_<String_> quoteNames_, quoteUnits_;
        Vector_<> prices_;
        Matrix_<> sensitivities_, refinementErrors_;
        Vector_<bool> stable_, activeSetStable_;
        bool curveRiskIncluded_ = false;
    };

    GSRSLVQuoteRiskResult_ GSRSLVQuoteRisk(const GSRSLVModelData_& initial,
                                           const Vector_<GSRCalibrationQuote_>& quotes,
                                           const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                           const Vector_<GSREuropeanOption_>& targets,
                                           const GSRSLVCalibrationSettings_& calibrationSettings = {},
                                           const GSRSLVQuoteRiskSettings_& riskSettings = {},
                                           const GSRCurveQuoteRisk_* curveRisk = nullptr);
} // namespace Dal
