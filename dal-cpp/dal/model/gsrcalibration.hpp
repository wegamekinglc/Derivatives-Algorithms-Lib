//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <dal/model/gsreuropean.hpp>

namespace Dal {
    struct GSRCalibrationQuote_ {
        String_ name_;
        GSREuropeanOption_ option_;
        double price_ = 0.0;
        double priceScale_ = 1.0;
    };

    struct GSRCalibrationParameter_ {
        int factor_ = 0, knot_ = 0;
        double lower_ = 0.0, upper_ = 1.0;
    };

    struct GSRCalibrationSettings_ {
        int maxIterations_ = 100;
        double gradientTolerance_ = 1e-6;
        double stepTolerance_ = 1e-9;
        double finiteDifferenceStep_ = 1e-4;
        double parameterScale_ = 0.01;
        double priorWeight_ = 0.0;
        double smoothingWeight_ = 0.0;
        double numericalErrorFraction_ = 0.25;
        GSRPricingSettings_ pricing_;
    };

    struct GSRCalibrationResult_ {
        Handle_<MultiFactorGSRModelData_> model_;
        Vector_<> modelPrices_, residuals_, numericalErrors_, parameters_;
        Vector_<bool> activeBounds_;
        Matrix_<> quoteJacobian_;
        bool converged_ = false, fitWithinTolerance_ = false, numericalValidationPassed_ = false;
        int iterations_ = 0, evaluations_ = 0, jacobianRank_ = 0;
        double objective_ = 0.0, jacobianConditionEstimate_ = 0.0;
        String_ terminationReason_;
    };

    GSRCalibrationResult_ CalibrateGSRVolatility(const MultiFactorGSRModelData_& initial,
                                                 const Vector_<GSRCalibrationQuote_>& quotes,
                                                 const Vector_<GSRCalibrationParameter_>& parameters,
                                                 const GSRCalibrationSettings_& settings = {});
} // namespace Dal
