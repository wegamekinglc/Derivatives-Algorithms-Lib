#!/usr/bin/env python3
"""Calibrate USD, EUR, and XCCY basis curves in one joint solve."""

import math

import dal


TODAY = dal.Date_(2025, 6, 20)
START = dal.Date_(2025, 3, 20)
XCCY_MATURITY = dal.Date_(2026, 3, 20)
CURVE_MATURITY = dal.Date_(2026, 6, 20)
VALUATION_TIME = dal.DateTime_(TODAY, 12, 0)
TOLERANCE = 1.0e-9


def require(condition: bool, message: str) -> None:
    """Raise an optimization-proof validation error."""
    if not condition:
        raise RuntimeError(message)


def make_snapshot():
    """Create the authoritative observations for the started MTM trade."""
    start_fixing = dal.DateTime_(START, 11, 0)
    today_fixing = dal.DateTime_(TODAY, 11, 0)
    return dal.MarketFixingSnapshot_New(
        {
            "USD-JOINT-3M": {start_fixing: 0.040, today_fixing: 0.041},
            "EUR-JOINT-3M": {start_fixing: 0.030, today_fixing: 0.031},
            "FX[EUR/USD]": {dal.DateTime_(TODAY, 10, 30): 1.20},
        }
    )


def make_currency_spec(curve_name: str, ccy: str, market_rate: float):
    """Build one OIS discount-curve declaration."""
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"),
        dal.DayBasis_New("ACT_365F"),
        dal.CollateralType_OIS(),
    )

    declaration = dal.JointCurveDeclaration_()
    declaration.curve_name = curve_name
    declaration.instruments = [
        dal.Deposit_New(TODAY, TODAY, CURVE_MATURITY, market_rate, index)
    ]
    declaration.knot_dates = [CURVE_MATURITY]
    declaration.target_collateral = dal.CollateralType_OIS()
    declaration.calibrate_discount_curve = True
    declaration.parameterization = dal.CurveParameterization.PIECEWISE_CONSTANT_FWD
    declaration.log_df_scheme = dal.LogDfScheme.LOG_LINEAR
    declaration.initial_guess_per_node = [market_rate]

    result = dal.JointCurrencyCurveSpec_()
    result.ccy = dal.Ccy_(ccy)
    result.libor_basis = dal.DayBasis_New("ACT_365F")
    result.curves = [declaration]
    return result


def make_mtm_instrument():
    """Build the started mark-to-market cross-currency swap."""
    domestic_leg = dal.RateLegConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_365F")
    )
    foreign_leg = dal.RateLegConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_365F")
    )
    domestic_index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"),
        dal.DayBasis_New("ACT_365F"),
        dal.CollateralType_OIS(),
    )
    foreign_index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"),
        dal.DayBasis_New("ACT_365F"),
        dal.CollateralType_OIS(),
    )

    convention = dal.CrossCurrencyConvention_()
    convention.initial_notional_exchange = True
    convention.final_notional_exchange = True
    convention.spread_on_foreign_leg = True
    convention.domestic_leg = domestic_leg
    convention.domestic_index = domestic_index
    convention.foreign_leg = foreign_leg
    convention.foreign_index = foreign_index

    domestic_fixing = dal.FixingIdentity_()
    domestic_fixing.index_name = "USD-JOINT-3M"
    domestic_fixing.fixing_hour = 11
    domestic_fixing.fixing_minute = 0

    foreign_fixing = dal.FixingIdentity_()
    foreign_fixing.index_name = "EUR-JOINT-3M"
    foreign_fixing.fixing_hour = 11
    foreign_fixing.fixing_minute = 0

    config = dal.CrossCurrencySwapConfigBuilder_()
    config.pair = dal.CurrencyPair_New("USD", "EUR")
    config.domestic_notional = 110.0
    config.foreign_notional = 100.0
    config.convention = convention
    config.notional_mode = dal.XccyNotionalMode.MARK_TO_MARKET
    config.fx_reset = dal.FxResetConvention_New(
        0, dal.Holidays_(""), dal.BizDayConvention_.FOLLOWING, 10, 30
    )
    config.domestic_rate_fixing = domestic_fixing
    config.foreign_rate_fixing = foreign_fixing

    return dal.CrossCurrencySwap_New(
        TODAY, START, XCCY_MATURITY, 0.001, config.build()
    )


def make_spec():
    """Assemble the simultaneous domestic, foreign, and basis solve."""
    basis = dal.XccyBasisCurveDeclaration_()
    basis.curve_name = "usd_eur_basis"
    basis.instruments = [make_mtm_instrument()]
    basis.knot_dates = [XCCY_MATURITY]
    basis.parameterization = dal.CurveParameterization.PIECEWISE_CONSTANT_FWD
    basis.smoothing_weight = 1.0
    basis.initial_guess_per_node = [0.001]

    builder = dal.JointXccyCalibrationSpecBuilder_()
    builder.valuation_time = VALUATION_TIME
    builder.pair = dal.CurrencyPair_New("USD", "EUR")
    builder.collateral_currency = dal.Ccy_("USD")
    builder.fx_spot = 1.10
    builder.domestic = make_currency_spec("usd_ois", "USD", 0.040)
    builder.foreign = make_currency_spec("eur_ois", "EUR", 0.030)
    builder.basis = basis
    builder.fixings = make_snapshot()
    builder.solver_options.initial_guess = 0.01
    builder.solver_options.tolerance = TOLERANCE
    builder.solver_options.max_evaluations = 400
    return builder.build()


def validate_ranges(ranges, total: int, label: str) -> None:
    """Require named blocks to be non-empty, contiguous, and exhaustive."""
    expected_offset = 0
    for block in ranges:
        require(
            block.offset == expected_offset,
            f"{label} range {block.name} is not contiguous",
        )
        require(block.size > 0, f"{label} range {block.name} is empty")
        expected_offset += block.size
    require(
        expected_offset == total,
        f"{label} ranges cover {expected_offset}, expected {total}",
    )


def validate_result(result) -> None:
    """Validate the joint calibration result under normal and optimized Python."""
    require(result.converged, "joint XCCY calibration did not converge")
    require(
        len(result.residuals) == result.jacobian_at_solution.rows(),
        "residual/Jacobian row mismatch",
    )
    validate_ranges(result.residual_ranges, len(result.residuals), "residual")
    validate_ranges(
        result.parameter_ranges, result.jacobian_at_solution.cols(), "parameter"
    )
    forwards = result.fx_forward_curve.forwards
    require(
        len(forwards) == len(result.fx_forward_curve.dates) and len(forwards) > 0,
        "invalid FX forward layout",
    )
    require(all(math.isfinite(value) for value in forwards), "non-finite FX forward")
    require(
        math.isfinite(result.joint_max_abs_residual),
        "non-finite maximum residual",
    )
    require(
        result.joint_max_abs_residual <= TOLERANCE,
        "maximum residual exceeds tolerance",
    )


def print_ranges(label: str, ranges) -> None:
    """Print named half-open calibration blocks."""
    print(f"{label} ranges:")
    for block in ranges:
        print(f"  {block.name}: [{block.offset}, {block.offset + block.size})")


def main() -> int:
    """Run and validate the installed-surface joint calibration example."""
    result = dal.CalibrateJointXccyMarket(make_spec())
    validate_result(result)

    jacobian = result.jacobian_at_solution
    print(f"Converged: {result.converged}")
    print(f"Maximum absolute residual: {result.joint_max_abs_residual:.12g}")
    print(f"Jacobian dimensions: {jacobian.rows()}x{jacobian.cols()}")
    print_ranges("Parameter", result.parameter_ranges)
    print_ranges("Residual", result.residual_ranges)
    print("FX forwards:")
    for date, forward in zip(
        result.fx_forward_curve.dates, result.fx_forward_curve.forwards
    ):
        print(f"  {date}: {forward:.12g}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
