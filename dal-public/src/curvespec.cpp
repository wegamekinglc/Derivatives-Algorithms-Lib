//
// Created by wegamekinglc on 2026/6/20.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal-public/src/curvespec.hpp>

namespace Dal {

    CurveCalibrationSpec_ CurveCalibrationSpecBuilder_::Build() const {
        CurveCalibrationSpec_ spec;
        spec.today_ = today_;
        spec.ccy_ = ccy_;
        spec.curveName_ = curveName_;
        spec.instruments_ = instruments_;
        spec.knotDates_ = knotDates_;
        spec.discountCurves_ = discountCurves_;
        spec.forwardCurves_ = forwardCurves_;
        spec.baseCurve_ = baseCurve_;
        spec.targetCollateral_ = targetCollateral_;
        spec.targetTenor_ = targetTenor_;
        spec.calibrateDiscountCurve_ = calibrateDiscountCurve_;
        spec.liborBasis_ = liborBasis_;
        spec.smoothingWeight_ = smoothingWeight_;
        spec.tolerance_ = tolerance_;
        spec.fitTolerance_ = fitTolerance_;
        spec.maxEvaluations_ = maxEvaluations_;
        spec.maxRestarts_ = maxRestarts_;
        spec.initialGuess_ = initialGuess_;
        spec.solveMode_ = solveMode_;
        spec.parameterization_ = parameterization_;
        spec.knotPolicy_ = knotPolicy_;
        spec.initialGuessPerNode_ = initialGuessPerNode_;
        spec.logDfScheme_ = logDfScheme_;
        ValidateCurveCalibrationSpec(spec);
        return spec;
    }

    CalibrationResult_ CalibrateSingleCurve(const CurveCalibrationSpec_& spec) {
        return CalibrateSingleCurve(spec, CurveJacobianMode_::Value_::ANALYTIC);
    }

    CalibrationResult_ CalibrateSingleCurve(const CurveCalibrationSpec_& spec,
                                             CurveJacobianMode_ jacobianMode) {
        CurveCalibrationOptions_ options;
        options.jacobianMode_ = jacobianMode;
        auto result = CalibrateYieldCurve(spec, options);
        CalibrationResult_ rtn;
        rtn.curve_ = Handle_<DiscountCurve_>(result.curve_.release());
        rtn.diagnostics_ = std::move(result.diagnostics_);
        return rtn;
    }

    MultiCurveCalibrationResult_ CalibrateMultiCurveBundle(const MultiCurveCalibrationSpec_& spec) {
        return CalibrateMultiCurve(spec);
    }

} // namespace Dal
