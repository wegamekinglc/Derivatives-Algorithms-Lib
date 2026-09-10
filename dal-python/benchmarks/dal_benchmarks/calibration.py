"""Public-interface curve fixtures; no imports from the unit-test suite."""

import math

import dal

from .harness import Workload, require


def leg(months=12, unadjusted=True):
    value = dal.RateLegConvention_New(
        dal.PeriodLength_New(f"{months}M"), dal.DayBasis_New("ACT_365F")
    )
    value.payment_lag = 0
    if unadjusted:
        value.business_day_convention = dal.BizDayConvention_.UNADJUSTED
        value.payment_convention = dal.BizDayConvention_.UNADJUSTED
    return value


def index(projected=False, months=3, unadjusted=True):
    value = dal.RateIndexConvention_New(
        dal.PeriodLength_New(f"{months}M"),
        dal.DayBasis_New("ACT_365F"),
        dal.CollateralType_OIS(),
        projected,
    )
    value.fixing_lag = 0
    value.spot_lag = 0
    if unadjusted:
        value.business_day_convention = dal.BizDayConvention_.UNADJUSTED
    return value


def single_spec(representation, approximate=False):
    """The 23 swaps and 24 future knots of curve_calibration_perf."""
    today = dal.Date_(2022, 1, 1)
    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = today
    builder.ccy_ = dal.String_("USD")
    builder.curveName_ = dal.String_("curve_calibration_perf")
    builder.targetCollateral_ = dal.CollateralType_OIS()
    builder.calibrateDiscountCurve_ = True
    builder.liborBasis_ = dal.DayBasis_New("ACT_365F")
    builder.tolerance_ = 1e-10
    builder.fitTolerance_ = 1e-8
    builder.initialGuess_ = 0.03
    builder.smoothingWeight_ = 1.0
    builder.knot_policy = dal.CurveKnotPolicy.INPUT
    builder.solveMode_ = (
        dal.CurveSolveMode.APPROXIMATE if approximate else dal.CurveSolveMode.EXACT
    )
    if representation in ("PWC", "PWL"):
        builder.parameterization_ = (
            dal.CurveParameterization.PIECEWISE_CONSTANT_FWD
            if representation == "PWC"
            else dal.CurveParameterization.PIECEWISE_LINEAR_FWD
        )
        builder.knotDates_ = [dal.Date_(2022 + y, 7, 1) for y in range(23)] + [
            dal.Date_(2045, 1, 1)
        ]
    else:
        builder.parameterization_ = dal.CurveParameterization.LOG_DISCOUNT
        builder.logDfScheme_ = getattr(dal.LogDfScheme, representation)
        builder.knotDates_ = [dal.Date_(2022 + y, 1, 1) for y in range(25)]
    builder.instruments_ = [
        dal.Swap_New(
            today,
            today,
            dal.Date_(2022 + y, 1, 1),
            (1.0 + 2.5 * (y - 1) / 22) / 100,
            leg(),
            index(months=12),
            leg(),
        )
        for y in range(1, 24)
    ]
    return builder.Build()


def calibration_options(mode, diagnostics=True, kind="single"):
    constructors = {
        "single": dal.CurveCalibrationOptions_,
        "staged": dal.CrossCurrencyCalibrationOptions_,
        "joint": dal.JointXccyCalibrationOptions_,
    }
    options = constructors[kind]()
    options.jacobian_mode = getattr(dal.CurveJacobianMode, mode)
    options.compute_eff_jacobian_inverse = diagnostics
    options.compute_forward_jacobian = diagnostics
    return options


def check_single(result, count, diagnostics, approximate=False, mode="ANALYTIC"):
    diagnostic = result.diagnostics_
    require(len(diagnostic.modelRates_) == count, "calibration residual width changed")
    require(
        all(math.isfinite(value) for value in diagnostic.modelRates_),
        "nonfinite model rate",
    )
    require(math.isfinite(diagnostic.maxAbsResidual_), "nonfinite calibration residual")
    if not approximate:
        require(
            diagnostic.maxAbsResidual_ < 1e-7, "exact calibration residual too large"
        )
    require(
        diagnostic.jacobian_.rows()
        == (count if diagnostics and mode == "ANALYTIC" and not approximate else 0),
        "forward Jacobian retention changed",
    )
    if diagnostics and not approximate:
        require(
            diagnostic.effJacobianInverse_.cols() == count,
            "inverse quote width changed",
        )
    else:
        require(
            diagnostic.effJacobianInverse_.rows() == 0, "solve-only retained an inverse"
        )


def prepare_calibration(representation, mode, diagnostics, approximate=False):
    spec = single_spec(representation, approximate)
    options = calibration_options(mode, diagnostics)
    return Workload(
        lambda: dal.CalibrateSingleCurve(spec, options),
        lambda result: check_single(result, 23, diagnostics, approximate, mode),
    )
