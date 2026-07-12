"""Tests for yield curve calibration."""

import dal
import pytest

S = dal.String_


def _today():
    return dal.Date_(2025, 6, 20)


def _spot():
    return _today().AddDays(2)


def _fixed_6m():
    return dal.RateLegConvention_New(dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_365F"))


def _float_3m():
    return dal.RateLegConvention_New(dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360"))


def _libor_3m():
    return dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS()
    )


def _overnight_index():
    return dal.RateIndexConvention_New(
        dal.PeriodLength_New("12M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS()
    )


def _make_ois_instruments():
    """Build OIS instruments for a flat 4% market."""
    instruments = []
    knot_dates = []
    for y in [2, 5, 10]:
        maturity = _spot().AddDays(y * 365)
        knot_dates.append(maturity)
        inst = dal.OISSwap_New(_today(), _spot(), maturity, 0.04, _fixed_6m(), _overnight_index(), _float_3m())
        instruments.append(inst)
    return instruments, knot_dates


# ---- CurveCalibrationSpecBuilder_ ----

def test_builder_defaults():
    """CurveCalibrationSpecBuilder_ has sensible defaults."""
    builder = dal.CurveCalibrationSpecBuilder_()
    assert "calibrated" in repr(builder.curveName_)  # nosec B101 - pytest assertions are intentional
    assert builder.calibrateDiscountCurve_ is True  # nosec B101 - pytest assertions are intentional
    assert builder.tolerance_ == 1.0e-8  # nosec B101 - pytest assertions are intentional
    assert builder.fitTolerance_ == 1.0e-6  # nosec B101 - pytest assertions are intentional
    assert builder.maxEvaluations_ == 200  # nosec B101 - pytest assertions are intentional
    assert builder.maxRestarts_ == 20  # nosec B101 - pytest assertions are intentional
    assert builder.initialGuess_ == 0.05  # nosec B101 - pytest assertions are intentional
    assert builder.smoothingWeight_ == 1.0  # nosec B101 - pytest assertions are intentional


def test_builder_solve_mode_default():
    """Default solve mode is EXACT."""
    builder = dal.CurveCalibrationSpecBuilder_()
    assert builder.solveMode_ == dal.CurveSolveMode.EXACT  # nosec B101 - pytest assertions are intentional


def test_builder_parameterization_default():
    """Default parameterization is PIECEWISE_LINEAR_FWD."""
    builder = dal.CurveCalibrationSpecBuilder_()
    assert builder.parameterization_ == dal.CurveParameterization.PIECEWISE_LINEAR_FWD  # nosec B101 - pytest assertions are intentional


def test_builder_can_set_fields():
    """Builder fields can be set before Build."""
    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = _today()
    builder.ccy_ = S("USD")
    builder.curveName_ = S("test_curve")
    builder.tolerance_ = 1.0e-10
    builder.maxEvaluations_ = 100
    builder.solveMode_ = dal.CurveSolveMode.APPROXIMATE
    builder.parameterization_ = dal.CurveParameterization.PIECEWISE_CONSTANT_FWD
    assert builder.tolerance_ == 1.0e-10  # nosec B101 - pytest assertions are intentional
    assert builder.maxEvaluations_ == 100  # nosec B101 - pytest assertions are intentional
    assert builder.solveMode_ == dal.CurveSolveMode.APPROXIMATE  # nosec B101 - pytest assertions are intentional
    assert builder.parameterization_ == dal.CurveParameterization.PIECEWISE_CONSTANT_FWD  # nosec B101 - pytest assertions are intentional


# ---- Single-curve calibration ----

def test_calibrate_single_curve_ois():
    """CalibrateSingleCurve calibrates a simple OIS curve."""
    instruments, knot_dates = _make_ois_instruments()

    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = _today()
    builder.ccy_ = S("USD")
    builder.curveName_ = S("ois")
    builder.calibrateDiscountCurve_ = True
    builder.tolerance_ = 1.0e-8
    builder.initialGuess_ = 0.04
    builder.instruments_ = instruments
    builder.knotDates_ = knot_dates

    spec = builder.Build()
    result = dal.CalibrateSingleCurve(spec)

    assert result.curve_ is not None  # nosec B101 - pytest assertions are intentional
    diag = result.diagnostics_
    assert len(diag.marketRates_) > 0  # nosec B101 - pytest assertions are intentional
    assert len(diag.marketRates_) == len(diag.modelRates_)  # nosec B101 - pytest assertions are intentional
    assert diag.maxAbsResidual_ < 1.0e-6  # nosec B101 - pytest assertions are intentional
    assert diag.rmsResidual_ < 1.0e-6  # nosec B101 - pytest assertions are intentional


def test_calibrate_single_curve_with_analytic_jacobian():
    """CalibrateSingleCurve with ANALYTIC Jacobian mode returns a Jacobian matrix."""
    instruments = []
    knot_dates = [_today()]  # anchor knot required for LOG_DISCOUNT
    for y in [2, 5, 10]:
        maturity = _spot().AddDays(y * 365)
        knot_dates.append(maturity)
        inst = dal.OISSwap_New(_today(), _spot(), maturity, 0.04, _fixed_6m(), _overnight_index(), _float_3m())
        instruments.append(inst)

    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = _today()
    builder.ccy_ = S("USD")
    builder.curveName_ = S("ois_analytic")
    builder.calibrateDiscountCurve_ = True
    builder.initialGuess_ = 0.04
    builder.parameterization_ = dal.CurveParameterization.LOG_DISCOUNT
    builder.instruments_ = instruments
    builder.knotDates_ = knot_dates

    spec = builder.Build()
    result = dal.CalibrateSingleCurve(spec, dal.CurveJacobianMode.ANALYTIC)

    assert result.curve_ is not None  # nosec B101 - pytest assertions are intentional
    diag = result.diagnostics_
    assert diag.maxAbsResidual_ < 1.0e-6  # nosec B101 - pytest assertions are intentional
    assert diag.jacobian_ is not None  # nosec B101 - pytest assertions are intentional


def test_calibrate_single_zero_rate_curve_with_analytic_jacobian():
    """ZERO_RATE calibration exposes the AAD analytical Jacobian in Python."""
    instruments, knot_dates = _make_ois_instruments()

    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = _today()
    builder.ccy_ = S("USD")
    builder.curveName_ = S("ois_zero_analytic")
    builder.calibrateDiscountCurve_ = True
    builder.initialGuess_ = 0.04
    builder.parameterization_ = dal.CurveParameterization.ZERO_RATE
    builder.instruments_ = instruments
    builder.knotDates_ = knot_dates

    result = dal.CalibrateSingleCurve(builder.Build(), dal.CurveJacobianMode.ANALYTIC)

    assert result.curve_ is not None  # nosec B101 - pytest assertions are intentional
    assert result.diagnostics_.maxAbsResidual_ < 1.0e-6  # nosec B101 - pytest assertions are intentional
    assert result.diagnostics_.jacobian_.Rows() == len(instruments)  # nosec B101 - pytest assertions are intentional
    assert result.diagnostics_.jacobian_.Cols() == len(knot_dates)  # nosec B101 - pytest assertions are intentional


def test_calibrate_single_curve_with_bumped_jacobian():
    """CalibrateSingleCurve with BUMPED Jacobian mode completes successfully."""
    instruments, knot_dates = _make_ois_instruments()

    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = _today()
    builder.ccy_ = S("USD")
    builder.curveName_ = S("ois_bumped")
    builder.calibrateDiscountCurve_ = True
    builder.initialGuess_ = 0.04
    builder.solveMode_ = dal.CurveSolveMode.APPROXIMATE
    builder.instruments_ = instruments
    builder.knotDates_ = knot_dates

    spec = builder.Build()
    result = dal.CalibrateSingleCurve(spec, dal.CurveJacobianMode.BUMPED)

    assert result.curve_ is not None  # nosec B101 - pytest assertions are intentional
    assert result.diagnostics_.maxAbsResidual_ < 1.0e-6  # nosec B101 - pytest assertions are intentional


# ---- api.calibrate_curve convenience wrapper ----

def test_api_calibrate_curve():
    """api.calibrate_curve high-level wrapper works."""
    instruments, knot_dates = _make_ois_instruments()

    result = dal.api.calibrate_curve(
        _today(), "USD", instruments, knot_dates,
        settings=dict(curve_name=S("api_test"), tolerance=1e-8, initial_guess=0.04),
    )
    assert result.curve_ is not None  # nosec B101 - pytest assertions are intentional
    assert result.diagnostics_.maxAbsResidual_ < 1.0e-6  # nosec B101 - pytest assertions are intentional


def test_api_calibrate_curve_with_jacobian():
    """api.calibrate_curve accepts jacobian_mode."""
    instruments = []
    knot_dates = [_today()]  # anchor for LOG_DISCOUNT
    for y in [2, 5, 10]:
        maturity = _spot().AddDays(y * 365)
        knot_dates.append(maturity)
        inst = dal.OISSwap_New(_today(), _spot(), maturity, 0.04, _fixed_6m(), _overnight_index(), _float_3m())
        instruments.append(inst)

    result = dal.api.calibrate_curve(
        _today(), "USD", instruments, knot_dates,
        settings=dict(curve_name=S("api_jac"), tolerance=1e-8, initial_guess=0.04,
                      parameterization=dal.CurveParameterization.LOG_DISCOUNT),
        jacobian_mode=dal.CurveJacobianMode.ANALYTIC,
    )
    assert result.diagnostics_.jacobian_ is not None  # nosec B101 - pytest assertions are intentional


def test_api_calibrate_zero_rate_curve_with_base():
    """api.calibrate_curve appends optional base support without moving positional arguments."""
    instruments, knot_dates = _make_ois_instruments()
    base = dal.DiscountZeroRate_New(
        "base",
        "USD",
        _today(),
        knot_dates,
        [0.01] * len(knot_dates),
    )
    settings = dict(
        curve_name=S("api_zero_spread"),
        tolerance=1.0e-8,
        initial_guess=0.03,
        parameterization=dal.CurveParameterization.ZERO_RATE,
    )

    total_result = dal.api.calibrate_curve(
        _today(),
        "USD",
        instruments,
        knot_dates,
        settings,
        dal.CurveJacobianMode.ANALYTIC,
    )
    result = dal.api.calibrate_curve(
        _today(),
        "USD",
        instruments,
        knot_dates,
        settings,
        dal.CurveJacobianMode.ANALYTIC,
        base_curve=base,
    )

    assert isinstance(result.curve_, dal.DiscountZeroRate_)  # nosec B101 - pytest assertions are intentional
    assert isinstance(total_result.curve_, dal.DiscountZeroRate_)  # nosec B101 - pytest assertions are intentional
    assert result.diagnostics_.maxAbsResidual_ < 1.0e-6  # nosec B101 - pytest assertions are intentional
    assert result.diagnostics_.jacobian_.Rows() == len(instruments)  # nosec B101 - pytest assertions are intentional
    assert result.curve_(_today(), knot_dates[1]) == pytest.approx(  # nosec B101 - pytest assertions are intentional
        total_result.curve_(_today(), knot_dates[1]), abs=1.0e-9
    )
    for spread_rate, total_rate in zip(result.curve_.zero_rates, total_result.curve_.zero_rates):
        assert spread_rate == pytest.approx(total_rate - 0.01, abs=1.0e-9)  # nosec B101 - intentional


# ---- Multi-curve calibration ----

def test_calibrate_multi_curve_bundle():
    """CalibrateMultiCurveBundle calibrates OIS discount in one stage."""
    instruments, knot_dates = _make_ois_instruments()

    stage1 = dal.CurveCalibrationSpecBuilder_()
    stage1.today_ = _today()
    stage1.ccy_ = S("USD")
    stage1.curveName_ = S("ois_dc")
    stage1.calibrateDiscountCurve_ = True
    stage1.solveMode_ = dal.CurveSolveMode.APPROXIMATE
    stage1.initialGuess_ = 0.04
    stage1.instruments_ = instruments
    stage1.knotDates_ = knot_dates

    multi = dal.MultiCurveCalibrationSpec_()
    multi.name_ = S("usd_multi")
    multi.ccy_ = S("USD")
    multi.liborBasis_ = dal.DayBasis_New("ACT_365F")
    multi.stages_ = [stage1.Build()]

    result = dal.CalibrateMultiCurveBundle(multi)

    assert len(result.discountCurves_) == 1  # nosec B101 - pytest assertions are intentional
    assert len(result.diagnostics_) == 1  # nosec B101 - pytest assertions are intentional

    diag = result.diagnostics_[0]
    assert diag.maxAbsResidual_ < 1.0e-6  # nosec B101 - pytest assertions are intentional
    assert diag.rmsResidual_ < 1.0e-6  # nosec B101 - pytest assertions are intentional
