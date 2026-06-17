//
// Created by wegam on 2026/5/9.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/exceptions.hpp>

/*IF--------------------------------------------------------------------------
enumeration CurveSolveMode
    Calibration solve mode selection
switchable
alternative EXACT
alternative APPROXIMATE
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration CurveParameterization
    Forward curve parameterization
switchable
alternative PIECEWISE_LINEAR_FWD
alternative PIECEWISE_CONSTANT_FWD
alternative ZERO_RATE
alternative LOG_DISCOUNT
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration CurveKnotPolicy
    Knot date selection policy
switchable
alternative INPUT
alternative INSTRUMENTS
alternative AUGMENTED
-IF-------------------------------------------------------------------------*/

#include <memory>
#include <map>
#include <dal/platform/platform.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/date.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/logdfscheme.hpp>
#include <dal/curve/ycinstrument.hpp>

namespace Dal {
#include <dal/auto/MG_CurveSolveMode_enum.hpp>
#include <dal/auto/MG_CurveParameterization_enum.hpp>
#include <dal/auto/MG_CurveKnotPolicy_enum.hpp>

    struct CurveCalibrationSpec_ {
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
    };

    struct CurveCalibrationDiagnostics_ {
        String_ curveName_;
        Vector_<String_> instrumentNames_;
        Vector_<> marketRates_;
        Vector_<> modelRates_;
        Vector_<> residuals_;
        Matrix_<> effJacobianInverse_;
        double maxAbsResidual_ = 0.0;
        double rmsResidual_ = 0.0;
        bool usedApproximateFit_ = false;
    };

    struct CurveCalibrationResult_ {
        std::unique_ptr<DiscountCurve_> curve_;
        CurveCalibrationDiagnostics_ diagnostics_;
    };

    struct MultiCurveCalibrationSpec_ {
        String_ name_ = "bundle";
        String_ ccy_;
        Vector_<CurveCalibrationSpec_> stages_;
        DayBasis_ liborBasis_ = DayBasis_("ACT_365F");
    };

    struct MultiCurveCalibrationResult_ {
        std::map<CollateralType_, Handle_<DiscountCurve_>> discountCurves_;
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwardCurves_;
        Vector_<CurveCalibrationDiagnostics_> diagnostics_;
    };

    Sparse::TriDiagonal_* BuildCurveCalibrationWeights(const Vector_<Date_>& knotDates,
                                                       int paramsPerKnot,
                                                       double smoothingWeight);
    Vector_<Date_> BuildCurveCalibrationKnots(const Date_& today,
                                              const Vector_<Handle_<YCInstrument_>>& instruments,
                                              const Vector_<Date_>& inputKnots,
                                              CurveKnotPolicy_ policy);
    void ValidateCurveCalibrationSpec(const CurveCalibrationSpec_& spec);
    void ValidatePositiveDiscountFactors(const DiscountCurve_& curve, const Date_& today, const Vector_<Date_>& checkDates);
    CurveCalibrationResult_ CalibrateYieldCurve(const CurveCalibrationSpec_& spec);
    MultiCurveCalibrationResult_ CalibrateMultiCurve(const MultiCurveCalibrationSpec_& spec);

    namespace TestOnly {
        // Builds the LOG_DISCOUNT calibration Jacobian at the supplied parameter vector x for the
        // supplied spec, using the AAD-tape analytic path. Returns an empty matrix when the spec
        // does not engage the analytic path (parameterization_ != LOG_DISCOUNT, or the calibration
        // is otherwise ineligible for the AAD-tape Jacobian). Exposed for unit-test inspection; not
        // part of the stable public API.
        Matrix_<> AnalyticJacobianAt(const CurveCalibrationSpec_& spec, const Vector_<>& x);
    }

} // namespace Dal
