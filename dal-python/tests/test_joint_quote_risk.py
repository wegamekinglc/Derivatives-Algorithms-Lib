"""Reachable generic joint calibration, ownership and quote-risk contracts."""

import gc
import math
import csv
from pathlib import Path

import dal
import pytest

from test_quote_risk import _run_with_quote_risk_gil_heartbeat


def joint_inputs(*, layered=False, mode=None, inverse=True, bump=None, strict=False):
    today = dal.Date_(2025, 1, 2)
    basis = dal.DayBasis_New("ACT_365F")
    collateral = dal.CollateralType_OIS()
    fixed = dal.RateLegConvention_New(dal.PeriodLength_New("6M"), basis)
    floating = dal.RateLegConvention_New(dal.PeriodLength_New("3M"), basis)
    index = dal.RateIndexConvention_New(dal.PeriodLength_New("3M"), basis, collateral, True)
    discount_index = dal.RateIndexConvention_New(dal.PeriodLength_New("3M"), basis, collateral)
    spec = dal.JointMultiCurveCalibrationSpec_()
    spec.today = today
    spec.ccy = "USD"
    spec.tolerance = 1e-11
    if strict:
        spec.tolerance = 1e-13
        spec.fit_tolerance = 1e-12
    spec.initial_guess = 0.025
    spec.max_evaluations = 1000
    spec.max_restarts = 100
    declarations = []
    for block, count in enumerate((3, 2)):
        declaration = dal.JointCurveDeclaration_()
        declaration.curve_name = "repeated_name"
        declaration.parameterization = dal.CurveParameterization.PIECEWISE_CONSTANT_FWD
        declaration.calibrate_discount_curve = block == 0
        declaration.target_collateral = collateral
        if block:
            declaration.target_tenor = dal.PeriodLength_New("3M")
        declaration.base_layered_over_discount = layered and block == 1
        instruments, knots = [], []
        for ordinal in range(count):
            maturity = dal.Date_(2026 + ordinal, 1, 2)
            knots.append(dal.Date_(2025 + ordinal, 7, 2))
            quote = 0.02 + 0.001 * ordinal if block == 0 else 0.035 + 0.001 * ordinal
            if bump and bump[:2] == (block, ordinal):
                quote += bump[2]
            instrument = (
                dal.Deposit_New(today, today, maturity, quote, discount_index)
                if block == 0 else dal.Swap_New(today, today, maturity, quote, fixed, index, floating)
            )
            instruments.append(instrument)
        declaration.instruments = instruments
        declaration.knot_dates = knots
        declarations.append(declaration)
    spec.curves = declarations
    options = dal.JointMultiCurveCalibrationOptions_()
    options.compute_eff_jacobian_inverse = inverse
    if mode is not None:
        options.jacobian_mode = mode
    result = dal.CalibrateJointMultiCurveBundle(spec, options)
    market = dal.RatePricingMarket_(
        valuation_time=dal.DateTime_(today, 0), result_currency="USD",
        curve_components={"discount": next(iter(result.discount_curves.values())),
                          "forward": next(iter(result.forward_curves.values()))},
        fixings=dal.MarketFixingSnapshot_New({}),
    )
    config = dal.RateQuoteRiskProvenanceConfig_(
        calibration_id="python-generic-joint",
        component_key_by_parameter_block={"curve:0": "discount", "curve:1": "forward"},
    )
    identity = dal.FixingIdentity_()
    identity.index_name = "USD-3M"
    identity.fixing_hour = 11
    identity.fixing_minute = 0
    terms = dal.FixedFloatTradeTerms_(
        notional=1_000_000.0, contract_rate=0.03, pay_fixed=True, fixed_leg=fixed, float_leg=floating,
        float_index=index, fixing_identity=identity,
        forecast_component_key="forward", discount_component_key="discount",
    )
    trade = dal.RateTradeDefinition_(
        instrument_id="joint-irs", instrument_type=dal.RateInstrumentType.IRS, trade_date=today,
        start_date=today, maturity_date=dal.Date_(2027, 1, 2), currency="USD", terms=dal.IrsTradeTerms_(value=terms),
    )
    return spec, options, result, market, config, trade


def provenance_from(inputs):
    spec, options, result, market, config, _ = inputs
    return dal.BuildJointMultiCurveQuoteRiskProvenance(
        spec=spec, result=result, options=options, bound_market=market, config=config,
    )


def unregistered_xccy_inputs(*, bump=None, register=False, opaque_leaf=None):
    spec, options, result, market, config, _ = joint_inputs(bump=bump, strict=True)
    today = spec.today
    basis = dal.DayBasis_New("ACT_365F")
    collateral = dal.CollateralType_OIS()
    components = {"discount": next(iter(result.discount_curves.values())), "forward": next(iter(result.forward_curves.values()))}
    valuation_time = dal.DateTime_(today, 0, 0)
    fixings = dal.MarketFixingSnapshot_New({})
    knots = [dal.Date_(2025, 7, 2), dal.Date_(2026, 7, 2), dal.Date_(2027, 7, 2)]

    def flat(name, currency, rate, base=None):
        return dal.DiscountPWC_New(name, currency, knots, [rate] * len(knots), base)

    euro_discount = flat("eur-ois", "EUR", 0.017)
    euro_forward = flat("eur-3m", "EUR", 0.019, opaque_leaf)
    extra = flat("usd-6m", "USD", 0.015, components["discount"])
    basis_curve = flat("basis", "EUR", 0.001)
    domestic = dal.CurveBlock_New(
        "EUR", "EUR", {collateral: euro_discount}, {dal.PeriodLength_New("3M"): euro_forward}, basis,
    )
    forwards = dict(result.forward_curves)
    forwards[dal.PeriodLength_New("6M")] = extra
    foreign = dal.CurveBlock_New("USD", "USD", result.discount_curves, forwards, basis)
    xccy = dal.CrossCurrencyMarket_New(
        domestic_block=domestic, foreign_block=foreign, fx_spot=0.9,
        valuation_time=valuation_time, collateral_currency="EUR", fixings=fixings, basis_curve=basis_curve,
    )
    if register:
        components["extra"] = extra
    market = dal.RatePricingMarket_(
        valuation_time=valuation_time, result_currency="USD", curve_components=components,
        fixings=fixings, xccy_market=xccy,
    )
    convention = dal.CrossCurrencyConvention_()
    for side, tenor in (("domestic", "3M"), ("foreign", "6M")):
        period = dal.PeriodLength_New(tenor)
        setattr(convention, f"{side}_index", dal.RateIndexConvention_New(period, basis, collateral, True))
        setattr(convention, f"{side}_leg", dal.RateLegConvention_New(period, basis))
    xccy_config = dal.CrossCurrencySwapConfig_()
    xccy_config.pair = dal.CurrencyPair_New("EUR", "USD")
    xccy_config.domestic_notional = 900_000.0
    xccy_config.foreign_notional = 1_000_000.0
    xccy_config.convention = convention
    for side, name in (("domestic", "EUR-3M"), ("foreign", "USD-6M")):
        identity = dal.FixingIdentity_()
        identity.index_name = name
        identity.fixing_hour = 11
        identity.fixing_minute = 0
        setattr(xccy_config, f"{side}_rate_fixing", identity)
    terms = dal.XccyTradeTerms_(
        position_count=1.0, contract_spread=0.0015, spread_on_foreign_leg=True,
        receive_non_spread_pay_spread=True, config=xccy_config,
    )
    trade = dal.RateTradeDefinition_(
        instrument_id="unregistered-xccy", instrument_type=dal.RateInstrumentType.XCCY,
        trade_date=today, start_date=today, maturity_date=dal.Date_(2027, 1, 2), currency="EUR", terms=terms,
    )
    return spec, options, result, market, config, trade


def test_unregistered_xccy_base_matches_recalibration_and_registration_control():
    inputs = unregistered_xccy_inputs()
    risk = dal.AggregateRatePortfolioQuoteRisk(trades=[inputs[-1]], market=inputs[3], provenances=[provenance_from(inputs)])
    assert risk.meta[0].eligible and not risk.meta[0].structural_zero  # nosec B101
    assert len(risk.buckets) == 5  # nosec B101
    prices = []
    for bump in (1e-6, -1e-6, 1e-4, -1e-4):
        shifted = unregistered_xccy_inputs(bump=(0, 1, bump))
        price = dal.PriceRateTrades(trades=[shifted[-1]], market=shifted[3])[0]
        assert price.succeeded  # nosec B101
        prices.append(price.pv)
    bucket = risk.buckets[1]
    assert bucket.d_pv_d_decimal_quote == pytest.approx((prices[0] - prices[1]) / 2e-6, abs=1e-3)  # nosec B101
    assert bucket.dv01 == pytest.approx((prices[2] - prices[3]) / 2.0, abs=1e-5)  # nosec B101
    assert bucket.actual_pv_ccy == "EUR"  # nosec B101
    control = unregistered_xccy_inputs(register=True)
    registered = dal.AggregateRatePortfolioQuoteRisk(trades=[control[-1]], market=control[3], provenances=[provenance_from(control)])
    assert [b.d_pv_d_decimal_quote for b in registered.buckets] == [b.d_pv_d_decimal_quote for b in risk.buckets]  # nosec B101


def test_native_opaque_joint_fixture_projects_existing_failure_metadata():
    native_fixture = pytest.importorskip("_dal_quote_risk_test")
    inputs = unregistered_xccy_inputs(opaque_leaf=native_fixture.opaque_curve(None, "EUR"))
    provenance = provenance_from(inputs)
    assert provenance.available  # nosec B101
    risk = dal.AggregateRatePortfolioQuoteRisk(trades=[inputs[-1]], market=inputs[3], provenances=[provenance])
    assert not risk.buckets  # nosec B101
    assert len(risk.meta) == 1  # nosec B101
    meta = risk.meta[0]
    assert not meta.eligible and not meta.structural_zero  # nosec B101
    assert meta.reason == "QUOTE_RISK_TRADE_PROVENANCE_INCOMPLETE"  # nosec B101
    assert meta.original_node_risk_reason == "AAD_EVALUATION_FAILED"  # nosec B101
    assert meta.failing_component_key == "discount"  # nosec B101
    assert meta.actual_pv_ccy == "EUR" and math.isfinite(meta.pv)  # nosec B101


@pytest.mark.parametrize("layered", [False, True])
@pytest.mark.parametrize("mode", [dal.CurveJacobianMode.ANALYTIC, dal.CurveJacobianMode.BUMPED])
def test_joint_quote_risk_recalibration_units_and_gc(layered, mode):
    inputs = joint_inputs(layered=layered, mode=mode)
    provenance = provenance_from(inputs)
    _, _, result, market, _, trade = inputs
    assert result.converged  # nosec B101
    assert result.eff_jacobian_inverse_availability == "available"  # nosec B101
    assert [(r.curve_index, r.offset, r.size) for r in result.residual_ranges] == [(0, 0, 3), (1, 3, 2)]  # nosec B101
    assert result.residual_instrument_ordinals == (0, 1, 2, 0, 1)  # nosec B101
    with pytest.raises(AttributeError):
        result.converged = False
    with pytest.raises(AttributeError):
        result.parameter_ranges[0].size = 99
    assert provenance.kind == "JOINT_MULTI_CURVE"  # nosec B101
    assert provenance.axis.scheme == "dal.quote-risk-axis/2+jcs+sha256"  # nosec B101
    del inputs, result
    gc.collect()
    risk = dal.AggregateRatePortfolioQuoteRisk(trades=[trade], market=market, provenances=[provenance])
    assert len(risk.buckets) == 5 and risk.meta[0].eligible  # nosec B101
    assert not risk.provenance_failures  # nosec B101
    for bucket, (block, ordinal) in zip(risk.buckets, [(0, 0), (0, 1), (0, 2), (1, 0), (1, 1)]):
        prices = []
        for shift in (1e-6, -1e-6, 1e-4, -1e-4):
            shifted_market = joint_inputs(layered=layered, mode=mode, bump=(block, ordinal, shift))[3]
            priced = dal.PriceRateTrades(trades=[trade], market=shifted_market)[0]
            assert priced.succeeded and math.isfinite(priced.pv)  # nosec B101
            prices.append(priced.pv)
        derivative, dv01 = (prices[0] - prices[1]) / 2e-6, (prices[2] - prices[3]) / 2
        scale = max(1.0, abs(risk.meta[0].pv), *map(abs, prices))
        assert math.isclose(bucket.d_pv_d_decimal_quote, derivative, rel_tol=5e-6, abs_tol=5e-6 * scale)  # nosec B101
        assert math.isclose(bucket.dv01, dv01, rel_tol=5e-6, abs_tol=5e-10 * scale)  # nosec B101
        assert bucket.dv01 == bucket.d_pv_d_decimal_quote * 1e-4  # nosec B101
        assert bucket.actual_pv_ccy == "USD"  # nosec B101


def test_joint_factory_releases_gil_and_reports_default_unavailability():
    inputs = joint_inputs()
    calibrated = _run_with_quote_risk_gil_heartbeat(
        lambda: dal.CalibrateJointMultiCurveBundle(inputs[0], inputs[1]),
        barrier=dal._dal._CurveCalibrationGilBarrier_EnableForTesting,
    )
    assert calibrated.converged  # nosec B101
    provenance = _run_with_quote_risk_gil_heartbeat(lambda: provenance_from(inputs))
    market, trade = inputs[3], inputs[5]
    risk = _run_with_quote_risk_gil_heartbeat(
        lambda: dal.AggregateRatePortfolioQuoteRisk(trades=[trade], market=market, provenances=[provenance])
    )
    assert risk.meta[0].eligible  # nosec B101
    unavailable = provenance_from(joint_inputs(inverse=False))
    assert not unavailable.available  # nosec B101
    assert unavailable.reason == "QUOTE_RISK_INVERSE_NOT_REQUESTED"  # nosec B101
    with pytest.raises(TypeError):
        dal.BuildJointMultiCurveQuoteRiskProvenance(inputs[0], inputs[2], inputs[1], market, inputs[4])


def test_joint_cpp_python_excel_reference_parity():
    inputs = joint_inputs()
    risk = dal.AggregateRatePortfolioQuoteRisk(trades=[inputs[5]], market=inputs[3], provenances=[provenance_from(inputs)])
    path = Path(__file__).resolve().parents[2] / "dal-public/tests/data/generic_joint_quote_risk_v2.csv"
    with path.open(encoding="utf-8", newline="") as source:
        rows = list(csv.DictReader(source))
    assert len(risk.buckets) == len(rows) == 5  # nosec B101
    for bucket, row in zip(risk.buckets, rows):
        assert bucket.axis_fingerprint == row["axis_fingerprint"]  # nosec B101
        assert bucket.quote_key == row["quote_key"]  # nosec B101
        assert bucket.actual_pv_ccy == row["currency"]  # nosec B101
        assert abs(bucket.d_pv_d_decimal_quote - float(row["derivative"])) <= 1e-5  # nosec B101
        assert abs(bucket.dv01 - float(row["dv01"])) <= 1e-9  # nosec B101
