"""Fresh public DAL single, multi-curve and XCCY calibration solves."""

import math

import dal

from dal_benchmarks.calibration import calibration_options, index, leg
from dal_benchmarks.harness import require
from . import calibration_scenarios as inputs


def date(value):
    return dal.Date_(value.year, value.month, value.day)


def instruments(key, data):
    today = data["dates"][0]
    convention = index(key.endswith("forward"), months=12)
    return [
        dal.Swap_New(today, today, maturity, quote, leg(), convention, leg())
        for maturity, quote in zip(data["dates"][1:], data["quotes"][key])
    ]


def single_curve(key, data, discount=None):
    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = data["dates"][0]
    builder.ccy_, builder.curveName_ = dal.String_(key[:3].upper()), dal.String_(key)
    builder.targetCollateral_ = dal.CollateralType_OIS()
    builder.targetTenor_ = dal.PeriodLength_New("12M")
    builder.calibrateDiscountCurve_ = key.endswith("discount")
    builder.liborBasis_ = dal.DayBasis_New("ACT_365F")
    builder.parameterization_ = dal.CurveParameterization.LOG_DISCOUNT
    builder.logDfScheme_ = dal.LogDfScheme.LOG_LINEAR
    builder.knot_policy = dal.CurveKnotPolicy.INPUT
    builder.knotDates_ = data["dates"]
    builder.instruments_ = instruments(key, data)
    builder.tolerance_, builder.fitTolerance_, builder.initialGuess_ = (
        1e-12,
        1e-10,
        0.025,
    )
    if discount is not None:
        builder.discountCurves_ = {dal.CollateralType_OIS(): discount}
    result = dal.CalibrateSingleCurve(
        builder.Build(), calibration_options("ANALYTIC", False)
    )
    require(
        result.diagnostics_.maxAbsResidual_ < 1e-9,
        "DAL single calibration did not converge",
    )
    return result.curve_


def declaration(key, data):
    result = dal.JointCurveDeclaration_()
    result.curve_name = key
    result.calibrate_discount_curve = key.endswith("discount")
    result.target_collateral = dal.CollateralType_OIS()
    result.target_tenor = dal.PeriodLength_New("12M")
    result.parameterization = dal.CurveParameterization.LOG_DISCOUNT
    result.log_df_scheme = dal.LogDfScheme.LOG_LINEAR
    result.knot_dates, result.instruments = data["dates"][1:], instruments(key, data)
    return result


def solve_rates(kind, data):
    if kind == "multi_joint":
        spec = dal.JointMultiCurveCalibrationSpec_()
        spec.today, spec.ccy = data["dates"][0], "USD"
        spec.curves = [declaration(key, data) for key in inputs.KEYS[:2]]
        spec.tolerance, spec.fit_tolerance, spec.initial_guess = 1e-12, 1e-10, 0.025
        options = dal.JointMultiCurveCalibrationOptions_()
        options.compute_jacobian_at_solution = options.compute_eff_jacobian_inverse = (
            False
        )
        result = dal.CalibrateJointMultiCurveBundle(spec, options)
        require(result.converged, "DAL multi-curve calibration did not converge")
        return dict(
            zip(
                inputs.KEYS,
                (
                    next(iter(result.discount_curves.values())),
                    next(iter(result.forward_curves.values())),
                ),
            )
        )
    discount = single_curve(inputs.KEYS[0], data)
    curves = {inputs.KEYS[0]: discount}
    if kind == "multi_staged":
        curves[inputs.KEYS[1]] = single_curve(inputs.KEYS[1], data, discount)
    return curves


def xccy_config():
    convention = dal.CrossCurrencyConvention_()
    convention.initial_notional_exchange = convention.final_notional_exchange = True
    convention.spread_on_foreign_leg = True
    convention.domestic_index, convention.foreign_index = (
        index(True, months=12),
        index(True, months=12),
    )
    convention.domestic_leg, convention.foreign_leg = leg(), leg()
    config = dal.CrossCurrencySwapConfig_()
    config.pair = dal.CurrencyPair_New("USD", "EUR")
    config.domestic_notional, config.foreign_notional = 1.10, 1.0
    config.convention = convention
    config.notional_mode = dal.XccyNotionalMode.FIXED
    return config


def fixed_curves(data):
    return {
        key: dal.DiscountLogDF_New(key, key[:3].upper(), data["dates"], values)
        for key, values in data["log_dfs"].items()
    }


def block(currency, curves):
    return dal.CurveBlock_New(
        currency,
        currency,
        {dal.CollateralType_OIS(): curves[f"{currency.lower()}_discount"]},
        {dal.PeriodLength_New("12M"): curves[f"{currency.lower()}_forward"]},
        dal.DayBasis_New("ACT_365F"),
    )


def staged_xccy(data, config, swaps):
    curves = fixed_curves(data)
    builder = dal.CrossCurrencyCalibrationSpecBuilder_()
    builder.today = data["dates"][0]
    builder.valuation_time = dal.DateTime_(builder.today, 0)
    builder.collateral_currency, builder.basis_pair = dal.Ccy_("USD"), config.pair
    builder.domestic_curve_block, builder.foreign_curve_block = (
        block("USD", curves),
        block("EUR", curves),
    )
    builder.fx_spot, builder.initial_guess, builder.tolerance = 1.10, -0.003, 1e-12
    builder.fixings = dal.MarketFixingSnapshot_New({})
    # PWC knots begin right-hand intervals; the first rate also extends to today.
    builder.knot_dates = [builder.today.AddDays(1)] + data["dates"][1:-1]
    builder.instruments = swaps
    result = dal.CalibrateXccyMarket(
        builder.Build(), calibration_options("ANALYTIC", False, "staged")
    )
    require(
        result.diagnostics.max_abs_residual < 1e-9,
        "DAL XCCY calibration did not converge",
    )
    return curves, result.basis_curve


def currency_spec(currency, data):
    result = dal.JointCurrencyCurveSpec_()
    result.ccy, result.libor_basis = dal.Ccy_(currency), dal.DayBasis_New("ACT_365F")
    result.curves = [
        declaration(f"{currency.lower()}_{suffix}", data)
        for suffix in ("discount", "forward")
    ]
    return result


def joint_xccy(data, config, swaps):
    basis = dal.XccyBasisCurveDeclaration_()
    basis.curve_name, basis.knot_dates, basis.instruments = (
        "basis",
        data["dates"][1:],
        swaps,
    )
    basis.parameterization, basis.log_df_scheme = (
        dal.CurveParameterization.LOG_DISCOUNT,
        dal.LogDfScheme.LOG_LINEAR,
    )
    builder = dal.JointXccyCalibrationSpecBuilder_()
    builder.valuation_time = dal.DateTime_(data["dates"][0], 0)
    builder.pair, builder.collateral_currency, builder.fx_spot = (
        config.pair,
        dal.Ccy_("USD"),
        1.10,
    )
    builder.domestic, builder.foreign = (
        currency_spec("USD", data),
        currency_spec("EUR", data),
    )
    builder.basis, builder.fixings = basis, dal.MarketFixingSnapshot_New({})
    builder.solver_options.tolerance, builder.solver_options.fit_tolerance = (
        1e-12,
        1e-10,
    )
    builder.solver_options.initial_guess, builder.solver_options.max_evaluations = (
        0.02,
        1000,
    )
    result = dal.CalibrateJointXccyMarket(
        builder.Build(), calibration_options("ANALYTIC", False, "joint")
    )
    require(result.converged, "DAL joint XCCY calibration did not converge")
    curves = {}
    for currency, value in (
        ("usd", result.domestic_curve_block),
        ("eur", result.foreign_curve_block),
    ):
        curves[f"{currency}_discount"] = next(iter(value.discount_curves.values()))
        curves[f"{currency}_forward"] = next(iter(value.forward_curves.values()))
    return curves, result.basis_curve


def runner(case):
    nodes = inputs.dates(case["size"])
    data = {
        "dates": [date(value) for value in nodes],
        "quotes": inputs.quotes(case["size"]),
        "log_dfs": {
            key: [math.log(inputs.discount(key, d, case["size"])) for d in nodes]
            for key in inputs.KEYS[:4]
        },
    }
    queries = [date(value) for value in inputs.queries(case["size"])]
    today = data["dates"][0]

    def run():
        basis = None
        if case["kind"].startswith("xccy_"):
            config = xccy_config()
            swaps = [
                dal.CrossCurrencySwap_New(today, today, maturity, quote, config)
                for maturity, quote in zip(
                    data["dates"][1:], data["quotes"][inputs.KEYS[4]]
                )
            ]
            solve = staged_xccy if case["kind"] == "xccy_staged" else joint_xccy
            curves, basis = solve(data, config, swaps)
        else:
            curves = solve_rates(case["kind"], data)
        values = []
        for key in inputs.curve_keys(case["kind"]):
            if key == inputs.KEYS[4]:
                values.extend(
                    curves[inputs.KEYS[2]](today, d) / basis(today, d) for d in queries
                )
            else:
                values.extend(curves[key](today, d) for d in queries)
        return values

    return run
