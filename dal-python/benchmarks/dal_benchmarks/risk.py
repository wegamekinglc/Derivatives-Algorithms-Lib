"""Steady-state rate-risk fixtures and validation outside the timed region."""

from dataclasses import dataclass
import math

import dal

from .calibration import calibration_options, index, leg
from .harness import Workload, require


@dataclass
class RiskFixture:
    market: object
    trades: list
    build_provenance: object
    quotes: int


def rate_market(today, components, xccy=None, hour=9, minute=0):
    return dal.RatePricingMarket_(
        valuation_time=dal.DateTime_(today, hour, minute),
        result_currency="USD",
        curve_components=components,
        xccy_market=xccy,
        fixings=dal.MarketFixingSnapshot_New({}),
    )


def deposit(today, maturity, name, key="discount"):
    terms = dal.DepositTradeTerms_(
        notional=1_000_000,
        contract_rate=0.02,
        lend=True,
        index=index(months=6),
        discount_component_key=key,
    )
    return dal.RateTradeDefinition_(
        instrument_id=name,
        instrument_type=dal.RateInstrumentType.DEPOSIT,
        trade_date=today,
        start_date=today,
        maturity_date=maturity,
        currency="USD",
        terms=terms,
    )


def irs_terms(ordinal, discount, forward, generic):
    identity = dal.FixingIdentity_()
    identity.index_name = "USD-LIBOR" if generic else "USD-SOFR"
    identity.fixing_hour = 11
    identity.fixing_minute = 0
    # The public trade validator requires positive notionals. Represent the native
    # fixture's short positions by reversing payer direction with the same magnitude.
    notional = (250_000 if ordinal % 3 == 0 else 1_000_000) if generic else 1_000_000
    pay_fixed = ordinal % 3 != 0 if generic else ordinal % 2 == 0
    return dal.FixedFloatTradeTerms_(
        notional=notional,
        contract_rate=0.03,
        pay_fixed=pay_fixed,
        fixed_leg=leg(6 if generic else 12, unadjusted=generic),
        float_leg=leg(3 if generic else 12, unadjusted=generic),
        float_index=index(projected=generic, unadjusted=generic),
        fixing_identity=identity,
        forecast_component_key=forward,
        discount_component_key=discount,
    )


def irs(
    today,
    start,
    maturity,
    ordinal,
    *,
    discount="discount",
    forward="forecast",
    ois=False,
    generic=False,
):
    terms = irs_terms(ordinal, discount, forward, generic)
    return dal.RateTradeDefinition_(
        instrument_id=f"irs-{ordinal}",
        instrument_type=dal.RateInstrumentType.OIS
        if ois
        else dal.RateInstrumentType.IRS,
        trade_date=today,
        start_date=start,
        maturity_date=maturity,
        currency="USD",
        terms=dal.OisTradeTerms_(value=terms)
        if ois
        else dal.IrsTradeTerms_(value=terms),
    )


def single_fixture(width, count, mode):
    today = dal.Date_(2025, 1, 2)
    knots = [
        dal.Date_(2025 + (i + 1) // 2, 7 if i % 2 == 0 else 1, 2) for i in range(width)
    ]
    # Generate deposit quotes from the native fixture's piecewise constant forwards.
    forwards = [0.02 + 0.0005 * i for i in range(width)]
    integral, previous, quotes = 0.0, today, []
    for i, maturity in enumerate(knots):
        integral += forwards[max(0, i - 1)] * (maturity - previous) / 365
        quotes.append(math.expm1(integral) / ((maturity - today) / 365))
        previous = maturity
    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_, builder.ccy_, builder.curveName_ = (
        today,
        dal.String_("USD"),
        dal.String_("single_quote_bench"),
    )
    builder.parameterization_ = dal.CurveParameterization.PIECEWISE_CONSTANT_FWD
    builder.knot_policy = dal.CurveKnotPolicy.INPUT
    builder.tolerance_, builder.initialGuess_ = 1e-10, 0.02
    builder.knotDates_ = knots
    builder.instruments_ = [
        dal.Deposit_New(today, today, maturity, quote, index(months=6))
        for maturity, quote in zip(knots, quotes)
    ]
    spec = builder.Build()
    options = calibration_options(mode)
    options.compute_forward_jacobian = False
    result = dal.CalibrateSingleCurve(spec, options)
    market = rate_market(today, {"discount": result.curve_})
    config = dal.RateQuoteRiskProvenanceConfig_(
        calibration_id="single-quote-bench",
        component_key_by_parameter_block={"single_quote_bench": "discount"},
    )

    def build():
        return dal.BuildSingleCurveQuoteRiskProvenance(
            spec=spec,
            result=result,
            options=options,
            bound_market=market,
            config=config,
        )

    return RiskFixture(
        market,
        [deposit(today, knots[-1], f"deposit-{i}") for i in range(count)],
        build,
        width,
    )


def flat_swap_quote(today, years, months, forward):
    """Independent par quote for the native generic fixture's flat 2% discount curve."""

    def coupons(frequency):
        previous = today
        for month in range(frequency, 12 * years + 1, frequency):
            end = dal.Date_(2025 + month // 12, 1 + month % 12, 2)
            yield (end - previous) / 365, math.exp(-0.02 * (end - today) / 365)
            previous = end

    floating = sum(
        math.expm1(forward * accrual) * df for accrual, df in coupons(months)
    )
    annuity = sum(accrual * df for accrual, df in coupons(6))
    return floating / annuity


def generic_instrument(today, block, months, ordinal):
    maturity = dal.Date_(2026 + ordinal, 1, 2)
    if block == 0:
        years = (maturity - today) / 365
        quote = math.expm1(0.02 * years) / years
        return dal.Deposit_New(today, today, maturity, quote, index())
    quote = flat_swap_quote(today, ordinal + 1, months, 0.034 + 0.004 * (block - 1))
    return dal.Swap_New(
        today, today, maturity, quote, leg(6), index(True, months), leg(months)
    )


def generic_declaration(today, width, block):
    size = width // 3 + (block < width % 3)
    declaration = dal.JointCurveDeclaration_()
    declaration.curve_name = "repeated_name"
    declaration.parameterization = dal.CurveParameterization.PIECEWISE_CONSTANT_FWD
    declaration.calibrate_discount_curve = block == 0
    declaration.target_collateral = dal.CollateralType_OIS()
    months = 6 if block == 2 else 3
    if block:
        declaration.target_tenor = dal.PeriodLength_New(f"{months}M")
    declaration.base_layered_over_discount = block != 0
    declaration.knot_dates = [dal.Date_(2025 + i, 7, 2) for i in range(size)]
    declaration.instruments = [
        generic_instrument(today, block, months, i) for i in range(size)
    ]
    return declaration


def generic_fixture(width, count, mode="ANALYTIC"):
    """Three layered flat PWC curves and quotes from jointquoteriskfixtures.hpp."""
    today = dal.Date_(2025, 1, 2)
    spec = dal.JointMultiCurveCalibrationSpec_()
    spec.today, spec.ccy = today, "USD"
    spec.tolerance, spec.fit_tolerance, spec.initial_guess = 1e-11, 1e-9, 0.025
    spec.max_evaluations, spec.max_restarts = 1000, 100
    spec.curves = [generic_declaration(today, width, block) for block in range(3)]
    options = dal.JointMultiCurveCalibrationOptions_()
    options.compute_eff_jacobian_inverse = True
    options.jacobian_mode = getattr(dal.CurveJacobianMode, mode)
    result = dal.CalibrateJointMultiCurveBundle(spec, options)
    require(result.converged, "generic joint calibration did not converge")
    forwards = {str(key): value for key, value in result.forward_curves.items()}
    market = rate_market(
        today,
        {
            "curve:0": next(iter(result.discount_curves.values())),
            "curve:1": forwards[str(dal.PeriodLength_New("3M"))],
            "curve:2": forwards[str(dal.PeriodLength_New("6M"))],
        },
        hour=0,
    )
    config = dal.RateQuoteRiskProvenanceConfig_(
        calibration_id="generic-joint",
        component_key_by_parameter_block={f"curve:{i}": f"curve:{i}" for i in range(3)},
    )

    def build():
        return dal.BuildJointMultiCurveQuoteRiskProvenance(
            spec=spec,
            result=result,
            options=options,
            bound_market=market,
            config=config,
        )

    maturity = dal.Date_(2026 + (width // 3 + (1 < width % 3)) - 1, 1, 2)
    trades = [
        irs(
            today,
            today,
            maturity,
            i,
            discount="curve:0",
            forward="curve:1",
            generic=True,
        )
        for i in range(count)
    ]
    return RiskFixture(market, trades, build, width)


def check_provenance(value, width):
    require(value.available, f"unavailable provenance: {value.reason}")
    require(len(value.axis.quotes) == width, "provenance quote count changed")
    require(
        bool(value.axis.fingerprint) and bool(value.state.fingerprint),
        "missing provenance fingerprint",
    )
    require(value.effective_inverse.Cols() == width, "effective inverse width changed")


def check_quote_risk(value, fixture, expected_pvs):
    require(not value.provenance_failures, "quote risk provenance failed")
    require(len(value.buckets) == fixture.quotes, "quote bucket count changed")
    require(
        len(value.meta) == len(fixture.trades)
        and all(row.eligible for row in value.meta),
        "ineligible quote-risk trade",
    )
    for row, pv in zip(value.meta, expected_pvs):
        require(
            math.isclose(row.pv, pv, rel_tol=1e-10, abs_tol=1e-8),
            "quote-risk PV differs from passive pricing",
        )
    for bucket in value.buckets:
        require(
            math.isfinite(bucket.d_pv_d_decimal_quote) and math.isfinite(bucket.dv01),
            "nonfinite quote risk",
        )
        require(bucket.dv01 == bucket.d_pv_d_decimal_quote * 1e-4, "DV01 units changed")


def prepare_quote(fixture_factory, provenance_only=False):
    fixture = fixture_factory()
    if provenance_only:
        return Workload(
            fixture.build_provenance,
            lambda result: check_provenance(result, fixture.quotes),
        )
    provenance = fixture.build_provenance()
    check_provenance(provenance, fixture.quotes)
    priced = dal.PriceRateTrades(trades=fixture.trades, market=fixture.market)
    require(all(row.succeeded for row in priced), "passive pricing failed")
    pvs = [row.pv for row in priced]
    # Calibration and provenance construction remain entirely outside the timer.
    return Workload(
        lambda: dal.AggregateRatePortfolioQuoteRisk(
            trades=fixture.trades, market=fixture.market, provenances=[provenance]
        ),
        lambda value: check_quote_risk(value, fixture, pvs),
    )


def node_fixture(count, operation):
    today, start = dal.Date_(2026, 1, 15), dal.Date_(2026, 4, 15)
    knots = [dal.Date_(2026, 7, 15)] + [
        dal.Date_(year, 1, 15) for year in range(2027, 2034)
    ]
    components = {
        key: dal.DiscountPWC_New(key, "USD", knots, [rate] * 8)
        for key, rate in (("discount", 0.03), ("forecast", 0.035))
    }
    market = rate_market(today, components, hour=10, minute=30)
    ois = operation == "ois"
    maturity = dal.Date_(2031 if ois else 2036, 4, 15)
    trades = [
        irs(today, start, maturity, i, ois=ois) for i in range(1 if ois else count)
    ]
    keys = ["forecast"] if ois else ["forecast", "discount"]
    return market, trades, keys


def prepare_nodes(count, operation):
    market, trades, keys = node_fixture(count, operation)
    expected = [
        dal.RateTradeNodeSensitivities(trade=trade, market=market, component_key=key)
        for trade in trades
        for key in keys
    ]
    require(all(row.eligible for row in expected), "node-risk reference ineligible")

    def run():
        if operation in ("single", "ois"):
            return [
                dal.RateTradeNodeSensitivities(
                    trade=trade, market=market, component_key=key
                )
                for trade in trades
                for key in keys
            ]
        return dal.RateTradeNodeSensitivitiesBatch(
            trades=trades, market=market, component_keys=keys
        )

    def validate(value):
        rows = [cell.result for cell in value] if operation == "batch" else value
        require(len(rows) == len(expected), "node-risk cell count changed")
        for row, reference in zip(rows, expected):
            require(
                row.eligible
                and math.isfinite(row.pv)
                and all(math.isfinite(x) for x in row.gradient),
                "node-risk cell failed",
            )
            require(
                row.gradient == reference.gradient and row.pv == reference.pv,
                "batch/single node risk differs",
            )

    return Workload(run, validate)
