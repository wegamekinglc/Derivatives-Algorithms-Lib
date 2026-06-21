from . import dal as _bindings


def Product_New(events_dates: list, events: list[str]):
    wrapped = [d if isinstance(d, _bindings.Cell_) else _bindings.Cell_(d) for d in events_dates]
    return _bindings.Product_New(wrapped, events)


def _apply_optional_setting(spec, name, value):
    """Apply a single optional setting to a spec builder if the value is not None."""
    attr = {
        'curve_name': 'curveName_',
        'target_collateral': 'targetCollateral_',
        'target_tenor': 'targetTenor_',
        'calibrate_discount': 'calibrateDiscountCurve_',
        'libor_basis': 'liborBasis_',
        'smoothing_weight': 'smoothingWeight_',
        'tolerance': 'tolerance_',
        'fit_tolerance': 'fitTolerance_',
        'max_evaluations': 'maxEvaluations_',
        'max_restarts': 'maxRestarts_',
        'initial_guess': 'initialGuess_',
        'solve_mode': 'solveMode_',
        'parameterization': 'parameterization_',
        'log_df_scheme': 'logDfScheme_',
    }.get(name)
    if attr is not None:
        setattr(spec, attr, value)


def _build_calibration_spec(today, ccy, instruments, knot_dates, settings):
    """Build a CurveCalibrationSpec_ with sensible defaults and optional overrides."""
    spec = _bindings.CurveCalibrationSpecBuilder_()
    spec.today_ = today
    spec.ccy_ = ccy if isinstance(ccy, _bindings.String_) else _bindings.String_(ccy)

    spec.curveName_ = _bindings.String_("calibrated")
    spec.calibrateDiscountCurve_ = True
    spec.smoothingWeight_ = 1.0
    spec.tolerance_ = 1e-8
    spec.fitTolerance_ = 1e-6
    spec.maxEvaluations_ = 200
    spec.maxRestarts_ = 20
    spec.initialGuess_ = 0.05

    spec.instruments_ = instruments
    spec.knotDates_ = knot_dates

    if settings:
        for key, value in settings.items():
            _apply_optional_setting(spec, key, value)

    return spec


def calibrate_curve(today, ccy, instruments, knot_dates, settings=None, jacobian_mode=None):
    """High-level single-curve calibration with sensible defaults.

    Args:
        today: Date_ for the calibration date
        ccy: Currency string (e.g. "USD")
        instruments: List of YCInstrument_ handles
        knot_dates: List of Date_ knot points
        settings: Optional dict of override settings. Supported keys:
            curve_name, target_collateral, target_tenor, calibrate_discount,
            libor_basis, smoothing_weight, tolerance, fit_tolerance,
            max_evaluations, max_restarts, initial_guess, solve_mode,
            parameterization, log_df_scheme
        jacobian_mode: CurveJacobianMode enum (None = default without Jacobian)

    Returns:
        CalibrationResult_ with curve_ and diagnostics_
    """
    spec = _build_calibration_spec(today, ccy, instruments, knot_dates, settings)
    if jacobian_mode is not None:
        return _bindings.CalibrateSingleCurve(spec.Build(), jacobian_mode)
    else:
        return _bindings.CalibrateSingleCurve(spec.Build())
