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

/*IF--------------------------------------------------------------------------
enumeration CurveJacobianMode
    Jacobian construction mode for curve calibration
switchable
alternative BUMPED
    Finite-difference bumping of each free node. Byte-for-byte identical to
    the pre-analytic path. Always available.
alternative ANALYTIC
    AAD-derived dense Jacobian. Default. Engages only when
    EligibleForAnalyticJacobian() is true (LOG_DISCOUNT + DISCOUNT-target +
    forecast==discount + vanilla swap/deposit/FRA/Future + tradeDate ==
    anchor); otherwise falls back to BUMPED with a NOTICE. ANALYTIC never
    throws -- it is a best-effort hint.
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
#include <dal/auto/MG_CurveJacobianMode_enum.hpp>

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

    // Solver-side options, NOT serialized with the spec: the spec describes WHAT to calibrate,
    // these describe HOW to solve. The default is ANALYTIC: eligible calibrations engage the AAD
    // Jacobian, ineligible ones fall back to bumped with a NOTICE (ANALYTIC never throws), so the
    // default is byte-for-byte identical to the bumped path only for ineligible specs.
    struct CurveCalibrationOptions_ {
        CurveJacobianMode_ jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    };

    struct CurveCalibrationDiagnostics_ {
        String_ curveName_;
        Vector_<String_> instrumentNames_;
        Vector_<> marketRates_;
        Vector_<> modelRates_;
        Vector_<> residuals_;
        Matrix_<> effJacobianInverse_;
        // Unscaled at-solution forward Jacobian; see docs/methodology/yield_curve_jacobian.md §"The Forward Jacobian, Two Ways".
        Matrix_<> jacobian_;
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
    CurveCalibrationResult_ CalibrateYieldCurve(const CurveCalibrationSpec_& spec, const CurveCalibrationOptions_& options);
    MultiCurveCalibrationResult_ CalibrateMultiCurve(const MultiCurveCalibrationSpec_& spec);

} // namespace Dal
