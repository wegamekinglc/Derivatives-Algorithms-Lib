"""Joint and staged XCCY calibration, provenance and aggregation workloads."""

import math

import dal

from .calibration import calibration_options, index, leg
from .harness import Workload, require
from .risk import RiskFixture, rate_market


def config():
    convention = dal.CrossCurrencyConvention_()
    convention.initial_notional_exchange = True
    convention.final_notional_exchange = True
    convention.spread_on_foreign_leg = True
    convention.domestic_index = index(True)
    convention.foreign_index = index(True)
    convention.domestic_leg = leg(3)
    convention.foreign_leg = leg(3)
    value = dal.CrossCurrencySwapConfig_()
    value.pair = dal.CurrencyPair_New("USD", "EUR")
    value.domestic_notional, value.foreign_notional = 110.0, 100.0
    value.convention = convention
    value.notional_mode = dal.XccyNotionalMode.FIXED
    return value


def trade(today, maturity, ordinal, trade_config):
    terms = dal.XccyTradeTerms_(
        position_count=10_000.0,
        contract_spread=0.0015,
        spread_on_foreign_leg=True,
        receive_non_spread_pay_spread=True,
        config=trade_config,
    )
    return dal.RateTradeDefinition_(
        instrument_id=f"xccy-{ordinal}",
        instrument_type=dal.RateInstrumentType.XCCY,
        trade_date=today,
        start_date=today,
        maturity_date=maturity,
        currency="USD",
        terms=terms,
    )


def currency_spec(today, ccy, width):
    declarations = []
    for projected in (False, True):
        declaration = dal.JointCurveDeclaration_()
        declaration.curve_name = f"{ccy}_{'3m' if projected else 'ois'}"
        declaration.parameterization = dal.CurveParameterization.PIECEWISE_CONSTANT_FWD
        declaration.calibrate_discount_curve = not projected
        declaration.target_collateral = dal.CollateralType_OIS()
        declaration.target_tenor = dal.PeriodLength_New("3M")
        declaration.knot_dates = [dal.Date_(2025 + i, 7, 16) for i in range(width)]
        rate = (0.015 if ccy == "USD" else 0.01) + (0.009 if projected else 0)
        declaration.instruments = [
            dal.Deposit_New(
                today,
                today,
                dal.Date_(2026 + i, 1, 16),
                math.expm1(rate * ((dal.Date_(2026 + i, 1, 16) - today) / 365))
                / ((dal.Date_(2026 + i, 1, 16) - today) / 365),
                index(projected),
            )
            for i in range(width)
        ]
        declarations.append(declaration)
    value = dal.JointCurrencyCurveSpec_()
    value.ccy = dal.Ccy_(ccy)
    value.libor_basis = dal.DayBasis_New("ACT_365F")
    value.curves = declarations
    return value


def fixed_block(ccy, width):
    knots = [dal.Date_(2026 + i, 1, 16) for i in range(width)]
    discount = dal.DiscountPWC_New(
        f"{ccy}_ois", ccy, knots, [0.015 if ccy == "USD" else 0.01] * width
    )
    forward = dal.DiscountPWC_New(
        f"{ccy}_3m", ccy, knots, [0.024 if ccy == "USD" else 0.019] * width
    )
    return dal.CurveBlock_New(
        ccy,
        ccy,
        {dal.CollateralType_OIS(): discount},
        {dal.PeriodLength_New("3M"): forward},
        dal.DayBasis_New("ACT_365F"),
    )


def spec_for(kind, width):
    today = dal.Date_(2025, 1, 16)
    trade_config = config()
    knots = [dal.Date_(2026 + i, 1, 16) for i in range(width)]
    instruments = [
        dal.CrossCurrencySwap_New(
            today, today, dal.Date_(2026 + i, 7, 16), 0.001 + 0.0001 * i, trade_config
        )
        for i in range(width)
    ]
    if kind == "joint":
        basis = dal.XccyBasisCurveDeclaration_()
        basis.curve_name, basis.knot_dates, basis.instruments = (
            "usd_eur_basis",
            knots,
            instruments,
        )
        basis.parameterization = dal.CurveParameterization.PIECEWISE_CONSTANT_FWD
        builder = dal.JointXccyCalibrationSpecBuilder_()
        builder.valuation_time = dal.DateTime_(today, 9, 0)
        builder.pair = trade_config.pair
        builder.collateral_currency = dal.Ccy_("USD")
        builder.fx_spot = 1.10
        builder.domestic = currency_spec(today, "USD", width)
        builder.foreign = currency_spec(today, "EUR", width)
        builder.basis = basis
        builder.fixings = dal.MarketFixingSnapshot_New({})
        builder.solver_options.initial_guess = 0.005
        builder.solver_options.tolerance = 1e-10
        builder.solver_options.max_evaluations = 1000
    else:
        builder = dal.CrossCurrencyCalibrationSpecBuilder_()
        builder.today = today
        builder.valuation_time = dal.DateTime_(today, 9, 0)
        builder.basis_pair = trade_config.pair
        builder.collateral_currency = dal.Ccy_("USD")
        builder.domestic_curve_block, builder.foreign_curve_block = (
            fixed_block("USD", width),
            fixed_block("EUR", width),
        )
        builder.fx_spot, builder.initial_guess, builder.tolerance = 1.10, 0.001, 1e-10
        builder.fixings = dal.MarketFixingSnapshot_New({})
        builder.knot_dates, builder.instruments = knots, instruments
    return builder.Build()


def calibrator(kind):
    return dal.CalibrateJointXccyMarket if kind == "joint" else dal.CalibrateXccyMarket


def check_calibration(value, kind, width, diagnostics, mode):
    if kind == "joint":
        require(value.converged, "joint XCCY calibration did not converge")
        residuals, matrix = value.residuals, value.jacobian_at_solution
        require(len(residuals) == 5 * width, "joint XCCY residual count changed")
    else:
        residuals, matrix = value.diagnostics.residuals, value.diagnostics.jacobian
        require(len(residuals) == width, "staged XCCY residual count changed")
    require(
        all(math.isfinite(r) and abs(r) < 1e-7 for r in residuals),
        "XCCY residual too large",
    )
    require(
        matrix.rows() == (len(residuals) if diagnostics and mode == "ANALYTIC" else 0),
        "XCCY Jacobian retention changed",
    )


def prepare_calibration(kind, mode, diagnostics, width=5):
    spec = spec_for(kind, width)
    options = calibration_options(mode, diagnostics, kind)
    return Workload(
        lambda: calibrator(kind)(spec, options),
        lambda value: check_calibration(value, kind, width, diagnostics, mode),
    )


def fixture(kind, width, count, mode):
    today = dal.Date_(2025, 1, 16)
    spec = spec_for(kind, width)
    options = calibration_options(mode, kind=kind)
    options.compute_forward_jacobian = False
    result = calibrator(kind)(spec, options)
    if kind == "joint":
        blocks = (result.domestic_curve_block, result.foreign_curve_block)
        curves = []
        for block in blocks:
            curves.extend(
                (
                    next(iter(block.discount_curves.values())),
                    next(iter(block.forward_curves.values())),
                )
            )
        curves.append(result.basis_curve)
        bindings = {
            entry.name: f"component-{i}"
            for i, entry in enumerate(result.parameter_ranges)
        }
        require(len(bindings) == len(curves), "joint XCCY parameter blocks changed")
        components = {f"component-{i}": curve for i, curve in enumerate(curves)}
        xccy_market = dal.CrossCurrencyMarket_New(
            domestic_block=blocks[0],
            foreign_block=blocks[1],
            fx_spot=1.10,
            valuation_time=dal.DateTime_(today, 9, 0),
            collateral_currency="USD",
            fixings=result.fixings,
            basis_curve=result.basis_curve,
        )
        build_function = dal.BuildJointXccyQuoteRiskProvenance
    else:
        components = {"basis": result.basis_curve}
        bindings = {"basis:xccy_basis_USD": "basis"}
        xccy_market = result.market
        build_function = dal.BuildStagedXccyBasisQuoteRiskProvenance
    market = rate_market(today, components, xccy_market)
    provenance_config = dal.RateQuoteRiskProvenanceConfig_(
        calibration_id=f"{kind}-quote-bench", component_key_by_parameter_block=bindings
    )

    def build():
        return build_function(
            spec=spec,
            result=result,
            options=options,
            bound_market=market,
            config=provenance_config,
        )

    maturity = dal.Date_(2025 + width, 1 if kind == "joint" else 7, 16)
    trades = [trade(today, maturity, i, config()) for i in range(count)]
    return RiskFixture(market, trades, build, width * (5 if kind == "joint" else 1))


def prepare_nodes(count):
    data = fixture("joint", 5, count, "ANALYTIC")
    keys = [f"component-{i}" for i in range(5)]
    expected = [
        dal.RateTradeNodeSensitivities(trade=t, market=data.market, component_key=k)
        for t in data.trades
        for k in keys
    ]

    def validate(value):
        require(len(value) == 5 * count, "XCCY node cell count changed")
        for cell, reference in zip(value, expected):
            require(
                cell.result.eligible and reference.eligible, "XCCY node cell ineligible"
            )
            require(
                cell.result.gradient == reference.gradient
                and math.isfinite(cell.result.pv),
                "XCCY batch/single disagreement",
            )

    return Workload(
        lambda: dal.RateTradeNodeSensitivitiesBatch(
            trades=data.trades, market=data.market, component_keys=keys
        ),
        validate,
    )
