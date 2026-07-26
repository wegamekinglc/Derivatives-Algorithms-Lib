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
    AAD-derived dense Jacobian. Default. Supports every implemented curve
    representation (piecewise-constant forward, piecewise-linear forward,
    and every log-discount interpolation scheme) when the calibration's
    instrument and routing eligibility gates pass. Otherwise falls back to
    BUMPED with a NOTICE. ANALYTIC never throws -- it is a best-effort hint.
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration AnalyticEligibility
    Cached eligibility verdict for the analytic Jacobian path
switchable
alternative UNKNOWN
alternative ELIGIBLE
alternative INELIGIBLE
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration AnalyticIneligibilityReason
    Stable structured analytic-Jacobian ineligibility reason
switchable
alternative DISCOUNT_TARGET_REQUIRED
alternative TEMPLATED_RATE_UNAVAILABLE
alternative PROJECTION_NOT_ALLOWED
alternative PROJECTION_REQUIRED
alternative TRADE_DATE_MISMATCH
alternative LIBOR_BASIS_UNSUPPORTED
alternative DISCOUNT_ROUTE_MISSING
alternative PROJECTION_ROUTE_MISSING
alternative PAIR_CURRENCY_MISMATCH
alternative COUPON_PLAN_EMPTY
alternative NOTIONAL_MODE_UNSUPPORTED
alternative RESET_MAPPING_INVALID
alternative CASHFLOW_PLAN_UNSUPPORTED
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration CurveKnotOriginKind
    Source of a single-curve knot candidate
switchable
alternative INPUT
alternative INSTRUMENT_START
alternative INSTRUMENT_END
alternative SYNTHETIC_ANCHOR
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration CurveKnotCandidateDisposition
    Result of visiting a single-curve knot candidate
switchable
alternative ADDED
alternative DUPLICATE
alternative FILTERED_NOT_AFTER_TODAY
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration CurveFreeParameterComponent
    Representation-aware component of a free curve parameter
switchable
alternative RIGHT_FORWARD
alternative LEFT_FORWARD
alternative ZERO_RATE
alternative LOG_DISCOUNT_FACTOR
-IF-------------------------------------------------------------------------*/

#include <dal/curve/discount.hpp>
#include <dal/curve/logdfscheme.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/platform/platform.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/date.hpp>
#include <dal/time/periodlength.hpp>
#include <map>
#include <memory>
#include <optional>

namespace Dal {
#include <dal/auto/MG_AnalyticEligibility_enum.hpp>
#include <dal/auto/MG_AnalyticIneligibilityReason_enum.hpp>
#include <dal/auto/MG_CurveFreeParameterComponent_enum.hpp>
#include <dal/auto/MG_CurveJacobianMode_enum.hpp>
#include <dal/auto/MG_CurveKnotCandidateDisposition_enum.hpp>
#include <dal/auto/MG_CurveKnotOriginKind_enum.hpp>
#include <dal/auto/MG_CurveKnotPolicy_enum.hpp>
#include <dal/auto/MG_CurveParameterization_enum.hpp>
#include <dal/auto/MG_CurveSolveMode_enum.hpp>

    struct AnalyticEligibilityIssue_ {
        AnalyticIneligibilityReason_ reason_;
        String_ group_;
        int declarationIndex_ = -1;
        int instrumentIndex_ = -1;
        int resetIndex_ = -1;
        String_ nativeMessage_;
    };

    struct AnalyticEligibilityReport_ {
        bool eligible_ = true;
        Vector_<AnalyticEligibilityIssue_> issues_;
    };

    struct CurveKnotOrigin_ {
        CurveKnotOriginKind_ kind_;
        int inputKnotIndex_ = -1;
        int instrumentInputIndex_ = -1;
    };

    struct CurveKnotCandidate_ {
        int ordinal_ = 0;
        Date_ date_;
        CurveKnotOrigin_ origin_;
        CurveKnotCandidateDisposition_ disposition_;
        int resolvedIndex_ = -1;
    };

    struct ResolvedCurveKnotNode_ {
        Date_ date_;
        Vector_<CurveKnotOrigin_> origins_;
    };

    struct CurveFreeParameter_ {
        Date_ date_;
        CurveFreeParameterComponent_ component_;
    };

    struct ResolvedCurveKnotCounts_ {
        int submittedKnots_ = 0;
        int instrumentCandidates_ = 0;
        int resolvedDeclaredNodes_ = 0;
        int storageNodes_ = 0;
        int freeParameters_ = 0;
    };

    struct ResolvedSingleKnotPlan_ {
        int plannerVersion_ = 1;
        CurveKnotPolicy_ requestedPolicy_;
        CurveKnotPolicy_ executionPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        Vector_<Date_> submittedKnotDates_;
        Vector_<CurveKnotCandidate_> candidateTrace_;
        Vector_<ResolvedCurveKnotNode_> resolvedDeclaredNodes_;
        Vector_<ResolvedCurveKnotNode_> storageNodes_;
        Vector_<CurveFreeParameter_> freeParameters_;
        bool anchorAdded_ = false;
        ResolvedCurveKnotCounts_ counts_;
    };

    struct ExecutionSingleKnotCounts_ {
        int resolvedDeclaredNodes_ = 0;
        int storageNodes_ = 0;
        int freeParameters_ = 0;
    };

    struct ExecutionSingleKnotIdentity_ {
        int identityVersion_ = 1;
        CurveKnotPolicy_ executionPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        Date_ today_;
        CurveParameterization_ parameterization_;
        std::optional<LogDfScheme_> logDfScheme_;
        Vector_<Date_> resolvedDeclaredDates_;
        Vector_<Date_> storageDates_;
        Vector_<CurveFreeParameter_> freeParameters_;
        ExecutionSingleKnotCounts_ counts_;
    };

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

        [[nodiscard]] const Date_& Today() const { return today_; }
        [[nodiscard]] CurveKnotPolicy_ KnotPolicy() const { return knotPolicy_; }
        [[nodiscard]] CurveParameterization_ Parameterization() const { return parameterization_; }
        [[nodiscard]] LogDfScheme_ LogDfScheme() const { return logDfScheme_; }
        [[nodiscard]] const Vector_<Date_>& KnotDates() const { return knotDates_; }
    };

    // Solver-side options, not serialized with the spec (the spec is WHAT to calibrate; these are HOW).
    struct CurveCalibrationOptions_ {
        CurveJacobianMode_ jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
        // Exact-solve pseudoinverse used by inverse-Jacobian risk diagnostics.
        bool computeEffJacobianInverse_ = true;
        // Analytic at-solution residual Jacobian; ignored unless ANALYTIC + EXACT + eligible.
        bool computeForwardJacobian_ = true;
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

    std::unique_ptr<Sparse::TriDiagonal_> BuildCurveCalibrationWeights(const Vector_<Date_>& knotDates, int paramsPerKnot, double smoothingWeight);
    Vector_<Date_> BuildCurveCalibrationKnots(const Date_& today,
                                              const Vector_<Handle_<YCInstrument_>>& instruments,
                                              const Vector_<Date_>& inputKnots,
                                              CurveKnotPolicy_ policy);
    [[nodiscard]] ResolvedSingleKnotPlan_ PlanCurveCalibrationKnots(const Date_& today,
                                                                    const Vector_<Handle_<YCInstrument_>>& instruments,
                                                                    const Vector_<Date_>& submittedKnots,
                                                                    CurveKnotPolicy_ requestedPolicy,
                                                                    CurveParameterization_ parameterization);
    [[nodiscard]] ExecutionSingleKnotIdentity_ InspectCurveCalibrationExecutionIdentity(const CurveCalibrationSpec_& finalInputSpec);
    [[nodiscard]] Vector_<> ResolveCurveCalibrationInitialGuess(const CurveCalibrationSpec_& finalSpec);
    [[nodiscard]] AnalyticEligibilityReport_ ValidateSingleCurveAnalyticEligibility(const CurveCalibrationSpec_& spec);
    void ValidateCurveCalibrationSpec(const CurveCalibrationSpec_& spec);
    void ValidatePositiveDiscountFactors(const DiscountCurve_& curve, const Date_& today, const Vector_<Date_>& checkDates);
    CurveCalibrationResult_ CalibrateYieldCurve(const CurveCalibrationSpec_& spec);
    CurveCalibrationResult_ CalibrateYieldCurve(const CurveCalibrationSpec_& spec, const CurveCalibrationOptions_& options);
    MultiCurveCalibrationResult_ CalibrateMultiCurve(const MultiCurveCalibrationSpec_& spec);

} // namespace Dal
