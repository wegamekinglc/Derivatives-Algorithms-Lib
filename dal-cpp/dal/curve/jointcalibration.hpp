//
// Created by dal-implementer on 2026/6/20.
//

#pragma once

#include <map>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/logdfscheme.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/periodlength.hpp>

namespace Dal {

    // See docs/methodology/yield_curve_jacobian.md §Joint Multi-Curve Analytic Jacobian.
    struct JointCurveDeclaration_ {
        String_ curveName_ = "joint";
        Vector_<Handle_<YCInstrument_>> instruments_;
        Vector_<Date_> knotDates_;
        CollateralType_ targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        PeriodLength_ targetTenor_;            // required iff calibrateDiscountCurve_ == false
        bool calibrateDiscountCurve_ = true;   // true: discount slot; false: forward slot
        bool baseLayeredOverDiscount_ = false; // forward decls only; base = discount curve at targetCollateral_
        CurveParameterization_ parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
        LogDfScheme_ logDfScheme_ = LogDfScheme_::Value_::LOG_LINEAR;
        double smoothingWeight_ = 1.0;        // per-curve; the joint smoother is block-diagonal
        Vector_<double> initialGuessPerNode_; // optional; defaults to spec.initialGuess_ flat
    };

    // The joint spec. Shared fields are hoisted (today_, ccy_, liborBasis_, solver options);
    // per-curve fields live on each declaration. solveMode_, fitTolerance_, tolerance_,
    // maxEvaluations_, maxRestarts_ apply to the ONE joint solve.
    struct JointMultiCurveCalibrationSpec_ {
        Date_ today_;
        String_ ccy_;
        Vector_<JointCurveDeclaration_> curves_;
        DayBasis_ liborBasis_ = DayBasis_("ACT_365F");
        double tolerance_ = 1.0e-8;    // per-residual tol passed to the solver
        double fitTolerance_ = 1.0e-6; // APPROXIMATE fit tol
        double smoothingWeight_ = 1.0; // unused; per-declaration smoothingWeight_ is the active field
        double initialGuess_ = 0.05;   // default per-node initial guess
        int maxEvaluations_ = 200;
        int maxRestarts_ = 20;
        CurveSolveMode_ solveMode_ = CurveSolveMode_::Value_::EXACT;
    };

    // Per-curve + coarse-joint diagnostics. The optional stacked residual Jacobian is carried on
    // JointMultiCurveCalibrationResult_; an effective inverse is not exposed by the joint API.
    struct JointCurveCalibrationDiagnostics_ {
        String_ curveName_;
        int curveIndex_ = 0; // position in spec.curves_ (0-based)
        Vector_<String_> instrumentNames_;
        Vector_<> marketRates_;
        Vector_<> modelRates_;
        Vector_<> residuals_;
        double maxAbsResidual_ = 0.0;
        double rmsResidual_ = 0.0;
        bool usedApproximateFit_ = false; // == (spec.solveMode_ == APPROXIMATE)
    };

    struct JointMultiCurveCalibrationResult_ {
        std::map<CollateralType_, Handle_<DiscountCurve_>> discountCurves_; // one per discount declaration
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwardCurves_;    // one per forward declaration
        Vector_<JointCurveCalibrationDiagnostics_> diagnostics_;            // one per declaration, in order
        double jointMaxAbsResidual_ = 0.0;                                  // max over every residual in the stacked vector
        double jointRmsResidual_ = 0.0;                                     // RMS over every residual in the stacked vector
        // Always true on return: the capability throws on solver non-convergence (mirrors
        // CalibrateYieldCurve). Retained for a future non-throwing overload.
        bool converged_ = false;
        int solverEvaluations_ = 0; // informational
        // See docs/methodology/yield_curve_jacobian.md §Joint Multi-Curve Analytic Jacobian.
        Matrix_<> jacobianAtSolution_;
    };

    // See docs/methodology/yield_curve_jacobian.md §Joint Multi-Curve Analytic Jacobian.
    struct JointMultiCurveCalibrationOptions_ {
        CurveJacobianMode_ jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
        // Analytic at-solution residual Jacobian; ignored unless ANALYTIC + EXACT + eligible.
        bool computeJacobianAtSolution_ = true;
    };

    // Validate inputs and run ONE Underdetermined::Find / Approximate over the concatenated
    // free-parameter vector of every declaration. Throws Dal::Exception_ on validation failure or
    // solver non-convergence (message names the failing solve and residual norm).
    [[nodiscard]] JointMultiCurveCalibrationResult_ CalibrateJointMultiCurve(const JointMultiCurveCalibrationSpec_& spec);

    // Two-arg overload: the options surface carries the per-call Jacobian mode (BUMPED vs
    // ANALYTIC). The single-arg overload above delegates to this with a default-constructed
    // options (-> ANALYTIC), so existing callers exercise the AAD path by default on eligible
    // specs.
    [[nodiscard]] JointMultiCurveCalibrationResult_ CalibrateJointMultiCurve(const JointMultiCurveCalibrationSpec_& spec,
                                                                             const JointMultiCurveCalibrationOptions_& options);

} // namespace Dal
