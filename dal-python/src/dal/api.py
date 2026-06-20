from . import dal as _bindings


def Product_New(events_dates: list, events: list[str]):
    wrapped = [d if isinstance(d, _bindings.Cell_) else _bindings.Cell_(d) for d in events_dates]
    return _bindings.Product_New(wrapped, events)


def calibrate_curve(today, ccy, instruments, knot_dates, *,
                    curve_name="calibrated",
                    target_collateral=None,
                    target_tenor=None,
                    calibrate_discount=True,
                    libor_basis=None,
                    smoothing_weight=1.0,
                    tolerance=1e-8,
                    fit_tolerance=1e-6,
                    max_evaluations=200,
                    max_restarts=20,
                    initial_guess=0.05,
                    solve_mode=None,
                    parameterization=None,
                    log_df_scheme=None,
                    jacobian_mode=None):
    """High-level single-curve calibration with sensible defaults.

    Args:
        today: Date_ for the calibration date
        ccy: Currency string (e.g. "USD")
        instruments: List of YCInstrument_ handles
        knot_dates: List of Date_ knot points
        curve_name: Name for the calibrated curve
        jacobian_mode: CurveJacobianMode enum (None = default EXACT without Jacobian)

    Returns:
        CalibrationResult_ with curve_ and diagnostics_
    """
    spec = _bindings.CurveCalibrationSpecBuilder_()
    spec.today_ = today
    spec.ccy_ = ccy if isinstance(ccy, _bindings.String_) else _bindings.String_(ccy)
    spec.curveName_ = curve_name if isinstance(curve_name, _bindings.String_) else _bindings.String_(curve_name)
    spec.instruments_ = instruments
    spec.knotDates_ = knot_dates
    spec.calibrateDiscountCurve_ = calibrate_discount
    spec.smoothingWeight_ = smoothing_weight
    spec.tolerance_ = tolerance
    spec.fitTolerance_ = fit_tolerance
    spec.maxEvaluations_ = max_evaluations
    spec.maxRestarts_ = max_restarts
    spec.initialGuess_ = initial_guess

    if target_collateral is not None:
        spec.targetCollateral_ = target_collateral
    if target_tenor is not None:
        spec.targetTenor_ = target_tenor
    if libor_basis is not None:
        spec.liborBasis_ = libor_basis
    if solve_mode is not None:
        spec.solveMode_ = solve_mode
    if parameterization is not None:
        spec.parameterization_ = parameterization
    if log_df_scheme is not None:
        spec.logDfScheme_ = log_df_scheme

    if jacobian_mode is not None:
        return _bindings.CalibrateSingleCurve(spec.Build(), jacobian_mode)
    else:
        return _bindings.CalibrateSingleCurve(spec.Build())
