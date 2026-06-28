//
// Created by wegamekinglc on 2026/6/20.
//

#pragma once

#include <map>

#include <dal/curve/calibration.hpp>
#include <dal/curve/logdfscheme.hpp>
#include <dal/platform/platform.hpp>

namespace Dal {

    // Shared solver-tuning knobs and single-curve defaults. Not embedded in the builders: they
    // keep flat fields so dal-python's member-pointer bindings stay source-compatible. The xccy
    // builder/spec override tolerance_ to 1e-10 and initialGuess_ to 0.0.
    struct CurveSolverOptions_ {
        double smoothingWeight_ = 1.0;
        double tolerance_ = 1.0e-8;
        double fitTolerance_ = 1.0e-6;
        double initialGuess_ = 0.05;
        int maxEvaluations_ = 200;
        int maxRestarts_ = 20;
        CurveSolveMode_ solveMode_ = CurveSolveMode_::Value_::EXACT;
    };

    struct CalibrationResult_ {
        Handle_<DiscountCurve_> curve_;
        CurveCalibrationDiagnostics_ diagnostics_;
    };

    struct CurveCalibrationSpecBuilder_ {
        Date_ today_;
        String_ ccy_;
        String_ curveName_ = "calibrated";
        Vector_<Handle_<YCInstrument_>> instruments_;
        Vector_<Date_> knotDates_;
        std::map<CollateralType_, Handle_<DiscountCurve_>> discountCurves_;
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwardCurves_;
        Handle_<DiscountCurve_> baseCurve_;
        CollateralType_ targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        PeriodLength_ targetTenor_;
        bool calibrateDiscountCurve_ = true;
        DayBasis_ liborBasis_ = DayBasis_("ACT_365F");
        // keep in sync with CurveSolverOptions_
        double smoothingWeight_ = 1.0;
        double tolerance_ = 1.0e-8;
        double fitTolerance_ = 1.0e-6;
        int maxEvaluations_ = 200;
        int maxRestarts_ = 20;
        double initialGuess_ = 0.05;
        CurveSolveMode_ solveMode_ = CurveSolveMode_::Value_::EXACT;
        CurveParameterization_ parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
        CurveKnotPolicy_ knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        Vector_<double> initialGuessPerNode_;
        LogDfScheme_ logDfScheme_ = LogDfScheme_::Value_::LOG_LINEAR;

        [[nodiscard]] CurveCalibrationSpec_ Build() const;
    };

    CalibrationResult_ CalibrateSingleCurve(const CurveCalibrationSpec_& spec);
    CalibrationResult_ CalibrateSingleCurve(const CurveCalibrationSpec_& spec,
                                             CurveJacobianMode_ jacobianMode);
    MultiCurveCalibrationResult_ CalibrateMultiCurveBundle(const MultiCurveCalibrationSpec_& spec);

} // namespace Dal
