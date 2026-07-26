//
// Created by Codex on 2026/7/14.
//

#pragma once

#include <dal/platform/platform.hpp>

#include <dal/curve/jointcalibration.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/indice/fixingsnapshot.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>

namespace Dal {
    struct CalibrationBlockRange_ {
        String_ name_;
        int offset_ = 0;
        int size_ = 0;
    };

    struct JointCurrencyCurveSpec_ {
        Ccy_ ccy_;
        DayBasis_ liborBasis_ = DayBasis_("ACT_365F");
        Vector_<JointCurveDeclaration_> curves_;
    };

    struct XccyBasisCurveDeclaration_ {
        String_ curveName_ = "xccy_basis";
        Vector_<Handle_<CrossCurrencySwap_>> instruments_;
        Vector_<Date_> knotDates_;
        CurveParameterization_ parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        LogDfScheme_ logDfScheme_ = LogDfScheme_::Value_::LOG_LINEAR;
        double smoothingWeight_ = 1.0;
        Vector_<double> initialGuessPerNode_;
    };

    struct JointXccyCalibrationSpec_ {
        DateTime_ valuationTime_;
        CurrencyPair_ pair_;
        Ccy_ collateralCurrency_;
        double fxSpot_ = 0.0;
        JointCurrencyCurveSpec_ domestic_;
        JointCurrencyCurveSpec_ foreign_;
        XccyBasisCurveDeclaration_ basis_;
        Handle_<MarketFixingSnapshot_> fixings_;
        double tolerance_ = 1.0e-8;
        double fitTolerance_ = 1.0e-6;
        double initialGuess_ = 0.0;
        int maxEvaluations_ = 200;
        int maxRestarts_ = 20;
        CurveSolveMode_ solveMode_ = CurveSolveMode_::Value_::EXACT;
    };

    struct JointXccyCalibrationOptions_ {
        CurveJacobianMode_ jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
        bool computeEffJacobianInverse_ = true;
        bool computeForwardJacobian_ = true;
    };

    struct JointXccyCalibrationResult_ {
        Handle_<CurveBlock_> domesticCurveBlock_;
        Handle_<CurveBlock_> foreignCurveBlock_;
        Handle_<DiscountCurve_> basisCurve_;
        CrossCurrencyFxForwardCurve_ fxForwardCurve_;
        Handle_<MarketFixingSnapshot_> fixings_;

        Vector_<JointCurveCalibrationDiagnostics_> domesticDiagnostics_;
        Vector_<JointCurveCalibrationDiagnostics_> foreignDiagnostics_;
        CrossCurrencyCalibrationDiagnostics_ xccyDiagnostics_;
        Vector_<> marketRates_;
        Vector_<> modelRates_;
        Vector_<> residuals_;

        Matrix_<> jacobianAtSolution_;
        Matrix_<> effJacobianInverse_;
        Vector_<CalibrationBlockRange_> parameterRanges_;
        Vector_<CalibrationBlockRange_> residualRanges_;

        double jointMaxAbsResidual_ = 0.0;
        double jointRmsResidual_ = 0.0;
        bool usedApproximateFit_ = false;
        bool converged_ = false;
        int solverEvaluations_ = 0;
    };

    [[nodiscard]] JointXccyCalibrationResult_ CalibrateJointXccyMarket(const JointXccyCalibrationSpec_& spec);
    [[nodiscard]] JointXccyCalibrationResult_ CalibrateJointXccyMarket(const JointXccyCalibrationSpec_& spec,
                                                                       const JointXccyCalibrationOptions_& options);
    [[nodiscard]] AnalyticEligibilityReport_ ValidateJointXccyAnalyticEligibility(const JointXccyCalibrationSpec_& spec);
} // namespace Dal
