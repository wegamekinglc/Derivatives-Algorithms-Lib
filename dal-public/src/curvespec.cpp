//
// Created by wegamekinglc on 2026/6/20.
//

#include <dal-public/src/curvespec.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {

    CurveCalibrationSpec_ CurveCalibrationSpecBuilder_::Build() const {
        CurveCalibrationSpec_ spec{today_,
                                   ccy_,
                                   curveName_,
                                   instruments_,
                                   knotDates_,
                                   discountCurves_,
                                   forwardCurves_,
                                   baseCurve_,
                                   targetCollateral_,
                                   targetTenor_,
                                   calibrateDiscountCurve_,
                                   liborBasis_,
                                   smoothingWeight_,
                                   tolerance_,
                                   fitTolerance_,
                                   maxEvaluations_,
                                   maxRestarts_,
                                   initialGuess_,
                                   solveMode_,
                                   parameterization_,
                                   knotPolicy_,
                                   initialGuessPerNode_,
                                   logDfScheme_};
        ValidateCurveCalibrationSpec(spec);
        return spec;
    }

    CalibrationResult_ CalibrateSingleCurve(const CurveCalibrationSpec_& spec) { return CalibrateSingleCurve(spec, CurveCalibrationOptions_()); }

    CalibrationResult_ CalibrateSingleCurve(const CurveCalibrationSpec_& spec, CurveJacobianMode_ jacobianMode) {
        CurveCalibrationOptions_ options;
        options.jacobianMode_ = jacobianMode;
        return CalibrateSingleCurve(spec, options);
    }

    CalibrationResult_ CalibrateSingleCurve(const CurveCalibrationSpec_& spec, const CurveCalibrationOptions_& options) {
        auto result = CalibrateYieldCurve(spec, options);
        CalibrationResult_ rtn;
        rtn.curve_ = Handle_<DiscountCurve_>(result.curve_.release());
        rtn.diagnostics_ = std::move(result.diagnostics_);
        return rtn;
    }

    MultiCurveCalibrationResult_ CalibrateMultiCurveBundle(const MultiCurveCalibrationSpec_& spec) { return CalibrateMultiCurve(spec); }

} // namespace Dal
