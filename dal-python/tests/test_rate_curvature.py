"""Passive native rate-trade quote-curvature contracts and independent oracles."""

import copy
from concurrent.futures import ThreadPoolExecutor
from enum import IntEnum
import gc
import math

import dal
import pytest

from test_joint_quote_risk import joint_inputs
from test_quote_risk import _run_with_quote_risk_gil_heartbeat
from test_xccy_calibration import _make_xccy_spec
from test_xccy_joint import _basis_instrument, _joint_curve, _joint_spec


def single_inputs(quotes=(0.025, 0.03), *, currency="USD", component="rate_curvature"):
    today = dal.Date_(2025, 1, 2)
    index = dal.RateIndexConvention_()
    index.day_basis = dal.DayBasis_New("ACT_365F")
    index.business_day_convention = dal.BizDayConvention_.UNADJUSTED
    index.accrual_holidays = dal.Holidays_("")
    maturities = [dal.Date_(2026, 1, 2), dal.Date_(2027, 1, 2)]
    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = today
    builder.ccy_ = dal.String_("USD")
    builder.curveName_ = dal.String_("rate_curvature")
    builder.parameterization_ = dal.CurveParameterization.LOG_DISCOUNT
    builder.knotPolicy_ = dal.CurveKnotPolicy.INPUT
    builder.initialGuess_ = 0.025
    builder.tolerance_ = 1e-14
    builder.instruments_ = [dal.Deposit_New(today, today, date, quote, index) for date, quote in zip(maturities, quotes)]
    builder.knotDates_ = [today, *maturities]
    terms = dal.DepositTradeTerms_(notional=1.0, contract_rate=0.028, lend=True, index=index,
                                  discount_component_key=component)
    trade = dal.RateTradeDefinition_(instrument_id="off-knot", instrument_type=dal.RateInstrumentType.DEPOSIT,
                                    trade_date=today, start_date=today, maturity_date=dal.Date_(2026, 7, 2),
                                    currency=currency, terms=terms)
    return builder, trade


def bump_request(rows=None, steps=None, **budgets):
    rows = [[1.0, 0.3], [-2.0, -0.6], [0.0, 1.0]] if rows is None else rows
    matrix = dal.DoubleMatrix_(rows) if rows else dal.DoubleMatrix_(0, 2)
    return dal.BumpOverAADRequest_(directions=matrix, steps=[2e-4, 1e-4, 2e-4] if steps is None else steps, **budgets)


def evaluate(*, rows=None, steps=None, **budgets):
    builder, trade = single_inputs()
    return dal.RateTradeQuoteCurvature([trade], dal.RateCalibration_New(builder.Build()),
                                      bump_request(rows, steps, **budgets))


def deposit_oracle(quotes):
    time = 546 / 365
    a, b = 2 - time, time - 1
    first, second = 1 + quotes[0], 1 + 2 * quotes[1]
    discounted_payment = (1 + 0.028 * time) * first ** -a * second ** -b
    gradient = [-discounted_payment * a / first, -discounted_payment * b * 2 / second]
    hessian = [[discounted_payment * a * (a + 1) / first ** 2,
                discounted_payment * a * b * 2 / (first * second)],
               [discounted_payment * a * b * 2 / (first * second),
                discounted_payment * b * (b + 1) * 4 / second ** 2]]
    return discounted_payment - 1, gradient, hessian


def analytic_secant(point, direction, step):
    plus = deposit_oracle([q + step * d for q, d in zip(point, direction)])[1]
    minus = deposit_oracle([q - step * d for q, d in zip(point, direction)])[1]
    return [(p - m) / (2 * step) for p, m in zip(plus, minus)]


def test_public_calibration_factory_returns_passive_snapshot():
    factory = dal.RateCalibration_New
    builder, _ = single_inputs()
    snapshot = factory(builder.Build())
    assert snapshot.point == [0.025, 0.03]
    assert len(snapshot.parameters) == 2
    assert snapshot.provenance.available


def test_deposit_matches_independent_value_gradient_and_finite_step_products():
    result = evaluate()
    curvature = result.curvature
    value, gradient, _ = deposit_oracle([0.025, 0.03])
    assert result.currency == "USD"
    assert curvature.value == pytest.approx(value, abs=1e-12)
    assert curvature.gradient == pytest.approx(gradient, abs=1e-10)
    for direction, step, product in zip(curvature.directions.to_rows(), curvature.steps,
                                        curvature.hessian_products.to_rows()):
        assert product == pytest.approx(analytic_secant(curvature.point, direction, step), abs=2e-9)
    assert curvature.hessian_products.to_rows()[1] == pytest.approx(
        [-2 * value for value in curvature.hessian_products.to_rows()[0]], abs=1e-10)
    execution = curvature.execution
    assert execution.method == "BumpOverRecalibratedNativeRateAAD"
    assert execution.quote_gradient_evaluations == execution.calibrations == execution.objective_reverse_sweeps == 7
    assert execution.numeric_payload_bytes == 160
    assert execution.peak_tape_bytes > 0
    assert execution.cleanup_reserve_bytes > 0


def test_finite_step_products_converge_toward_analytic_hessian():
    direction = [1.0, 0.3]
    hessian = deposit_oracle([0.025, 0.03])[2]
    exact = [sum(a * b for a, b in zip(row, direction)) for row in hessian]
    errors = []
    for step in [0.002, 0.0002]:
        actual = evaluate(rows=[direction], steps=[step]).curvature.hessian_products.to_rows()[0]
        assert actual == pytest.approx(analytic_secant([0.025, 0.03], direction, step), abs=2e-9)
        errors.append(max(abs(a - b) for a, b in zip(actual, exact)))
    assert errors[1] < errors[0] / 20
    assert errors[1] < 2e-6


def test_snapshot_replay_freezes_source_and_detaches_all_results():
    builder, trade = single_inputs()
    source = dal.RateCalibration_New(builder.Build())
    axis = source.provenance.axis.fingerprint
    quotes = [0.027, 0.032]
    replay = dal.RateCalibration_Recalibrate(source, quotes)
    quotes[0] = 99
    builder.instruments_ = []
    result = dal.RateTradeQuoteCurvature([trade], replay, bump_request())
    expected = result.curvature.hessian_products.to_rows()
    curvature = result.curvature
    source.point[0] = 99
    source.parameters[0] = 99
    replay.point[0] = 99
    curvature.point[0] = 99
    curvature.gradient[0] = 99
    curvature.steps[0] = 99
    curvature.directions[0, 0] = 99
    curvature.hessian_products[0, 0] = 99
    curvature.base_calibration.point[0] = 99
    assert source.point == [0.025, 0.03]
    assert replay.point == [0.027, 0.032]
    assert replay.provenance.axis.fingerprint == axis
    assert result.curvature.hessian_products.to_rows() == expected
    assert result.curvature.value == pytest.approx(deposit_oracle(replay.point)[0], abs=1e-12)
    values = [source, replay, result, curvature, curvature.execution,
              dal.RateTradeQuoteCurvatureSettings_(weights=[-0.5])]
    for item in values:
        assert type(copy.copy(item)) is type(item)
        assert type(copy.deepcopy(item)) is type(item)
    with pytest.raises(AttributeError):
        curvature.execution.calibrations = 0
    with pytest.raises(AttributeError):
        source.point = []
    del source, replay, builder, trade, values
    gc.collect()
    assert result.curvature.hessian_products.to_rows() == expected


@pytest.mark.parametrize("family", ["single", "joint", "staged_xccy", "joint_xccy"])
def test_all_calibration_families_preserve_axes_in_base_only_replay(family):
    if family == "single":
        spec = single_inputs()[0].Build()
    elif family == "joint":
        spec = joint_inputs(strict=True)[0]
    elif family == "staged_xccy":
        spec = _make_xccy_spec(dal.CurveSolveMode.EXACT, years=(2,))[0]
    else:
        spec = _joint_spec()
    snapshot = dal.RateCalibration_New(spec)
    replay = dal.RateCalibration_Recalibrate(snapshot, tuple(snapshot.point))
    assert replay.parameters == pytest.approx(snapshot.parameters, abs=1e-10)
    assert replay.point == snapshot.point
    assert replay.provenance.axis.fingerprint == snapshot.provenance.axis.fingerprint
    assert len(replay.provenance.axis.quotes) == len(snapshot.point)
    changed_quotes = [quote + 1e-5 * (ordinal + 1) for ordinal, quote in enumerate(snapshot.point)]
    changed = dal.RateCalibration_Recalibrate(snapshot, changed_quotes)
    assert changed.point == changed_quotes
    assert changed.provenance.axis.fingerprint == snapshot.provenance.axis.fingerprint
    assert changed.provenance.state.fingerprint != snapshot.provenance.state.fingerprint
    assert dal.RateCalibration_Recalibrate(changed, snapshot.point).parameters == pytest.approx(snapshot.parameters, abs=1e-9)
    keys = set(snapshot.provenance.component_key_by_parameter_block)
    if family == "joint":
        assert keys == {"curve:0", "curve:1"}
    if family == "staged_xccy":
        assert len(snapshot.point) == 1
        assert len(snapshot.parameters) == 1
    if family == "joint_xccy":
        assert keys == {"domestic:0:usd_ois", "foreign:0:eur_ois", "basis:usd_eur_basis"}


def test_weights_duplicate_ids_and_zero_weights_preserve_linearity():
    builder, trade = single_inputs()
    snapshot = dal.RateCalibration_New(builder.Build())
    weights = [1.0, -0.5, 0.0]
    settings = dal.RateTradeQuoteCurvatureSettings_(weights=weights)
    weights[0] = 99
    settings.weights[0] = 99
    assert settings.weights == [1.0, -0.5, 0.0]
    reference = dal.RateTradeQuoteCurvature([trade], snapshot, bump_request()).curvature
    weighted = dal.RateTradeQuoteCurvature((trade, trade, trade), snapshot, bump_request(), settings=settings).curvature
    assert weighted.value == pytest.approx(0.5 * reference.value, abs=1e-12)
    assert weighted.gradient == pytest.approx([0.5 * value for value in reference.gradient], abs=1e-10)
    for actual, row in zip(weighted.hessian_products.to_rows(), reference.hessian_products.to_rows()):
        assert actual == pytest.approx([0.5 * value for value in row], abs=2e-9)
    zero = dal.RateTradeQuoteCurvature([trade], snapshot, bump_request(),
                                     settings=dal.RateTradeQuoteCurvatureSettings_(weights=(0,))).curvature
    assert zero.value == 0
    assert zero.gradient == [0, 0]
    assert zero.hessian_products.to_rows() == [[0, 0]] * 3


@pytest.mark.parametrize("directions,steps,payload", [([], [], 40), ([[1.0, 0.3]], [2e-4], 80)])
def test_exact_numeric_cap_and_empty_directions(directions, steps, payload):
    result = evaluate(rows=directions, steps=steps, numeric_payload_budget_bytes=payload).curvature
    assert result.hessian_products.Rows() == len(directions)
    assert result.hessian_products.Cols() == 2
    assert result.gradient == pytest.approx(deposit_oracle([0.025, 0.03])[1], abs=1e-10)
    assert result.execution.numeric_payload_bytes == payload
    assert result.execution.calibrations == 1 + 2 * len(directions)
    with pytest.raises(RuntimeError, match="budget"):
        evaluate(rows=directions, steps=steps, numeric_payload_budget_bytes=payload - 1)


@pytest.mark.parametrize("budget", ["numeric_payload_budget_bytes", "recording_capacity_budget_bytes"])
def test_zero_caps_reject_and_valid_request_recovers(budget):
    with pytest.raises(RuntimeError, match="budget"):
        evaluate(**{budget: 0})
    result = evaluate(**{budget: 64 * 1024 * 1024})
    assert math.isfinite(result.curvature.value)


@pytest.mark.parametrize("failure", ["empty", "weights", "currency", "component", "zero_weight"])
def test_native_admission_and_recovery(failure):
    builder, trade = single_inputs()
    source = dal.RateCalibration_New(builder.Build())
    settings = None
    trades = [trade]
    if failure == "empty":
        trades = []
    elif failure == "weights":
        settings = dal.RateTradeQuoteCurvatureSettings_(weights=[1.0, 2.0])
    elif failure == "currency":
        trades = [single_inputs(currency="EUR")[1]]
    else:
        trades = [single_inputs(component="missing")[1]]
        if failure == "zero_weight":
            settings = dal.RateTradeQuoteCurvatureSettings_(weights=[0.0])
    with pytest.raises(RuntimeError):
        dal.RateTradeQuoteCurvature(trades, source, bump_request(), settings=settings)
    assert math.isfinite(dal.RateTradeQuoteCurvature([trade], source, bump_request()).curvature.value)


class NumericEnum(IntEnum):
    VALUE = 1


@pytest.mark.parametrize("field", ["quotes", "weights"])
@pytest.mark.parametrize("invalid", [True, NumericEnum.VALUE, "0.03", object(), float("nan"), float("inf"), 10 ** 400])
def test_strict_finite_numbers_identify_field(field, invalid):
    builder, _ = single_inputs()
    with pytest.raises((TypeError, RuntimeError, OverflowError), match=field):
        if field == "quotes":
            dal.RateCalibration_Recalibrate(dal.RateCalibration_New(builder.Build()), [invalid, 0.03])
        else:
            dal.RateTradeQuoteCurvatureSettings_(weights=[invalid])


@pytest.mark.parametrize("invalid", [None, "0.03", {0.025, 0.03}, iter([0.025, 0.03])])
def test_replay_requires_copied_list_or_tuple(invalid):
    builder, _ = single_inputs()
    with pytest.raises(TypeError, match="quotes"):
        dal.RateCalibration_Recalibrate(dal.RateCalibration_New(builder.Build()), invalid)


@pytest.mark.parametrize("quotes", [[], [0.025], [0.025, 0.03, 0.04]])
def test_replay_rejects_quote_count_and_keeps_source(quotes):
    source = dal.RateCalibration_New(single_inputs()[0].Build())
    with pytest.raises(RuntimeError):
        dal.RateCalibration_Recalibrate(source, quotes)
    assert dal.RateCalibration_Recalibrate(source, source.point).point == source.point


@pytest.mark.parametrize("invalid", ["1", {1}, iter([1])])
def test_weights_require_owned_list_or_tuple(invalid):
    with pytest.raises(TypeError, match="weights"):
        dal.RateTradeQuoteCurvatureSettings_(weights=invalid)


@pytest.mark.parametrize("rows", [[[0.0, 0.0]], [[1.0]], [[1.0, 0.0, 0.0]]])
def test_native_bump_geometry_rejects_and_recovers(rows):
    with pytest.raises(RuntimeError):
        evaluate(rows=rows, steps=[2e-4])
    assert evaluate(rows=[], steps=[]).curvature.gradient == pytest.approx(
        deposit_oracle([0.025, 0.03])[1], abs=1e-10)


@pytest.mark.parametrize("field,invalid", [("trades", None), ("trades", iter([])), ("trades", [None]),
                                          ("calibration", object()), ("bumps", object()), ("settings", object())])
def test_typed_financial_arguments_identify_field(field, invalid):
    builder, trade = single_inputs()
    arguments = dict(trades=[trade], calibration=dal.RateCalibration_New(builder.Build()), bumps=bump_request())
    arguments[field] = invalid
    with pytest.raises(TypeError, match=field):
        dal.RateTradeQuoteCurvature(**arguments)


def test_settings_and_factory_types_are_explicit_and_keyword_only():
    with pytest.raises(TypeError, match="fixings"):
        dal.RateTradeQuoteCurvatureSettings_(fixings={})
    with pytest.raises(TypeError):
        dal.RateTradeQuoteCurvatureSettings_([1.0])
    with pytest.raises(TypeError, match="spec"):
        dal.RateCalibration_New(object())
    with pytest.raises(TypeError, match="calibration"):
        dal.RateCalibration_Recalibrate(object(), [0.025, 0.03])
    for cls in [dal.RateCalibrationSnapshot_, dal.RateQuoteCurvatureResult_,
                dal.RateTradeQuoteCurvatureResult_, dal.RateQuoteCurvatureExecution_]:
        with pytest.raises(TypeError):
            cls()


def test_explicit_historical_fixings_survive_owner_collection():
    builder, _ = single_inputs()
    today = dal.Date_(2025, 1, 2)
    index = dal.RateIndexConvention_()
    index.day_basis = dal.DayBasis_New("ACT_365F")
    index.business_day_convention = dal.BizDayConvention_.UNADJUSTED
    index.accrual_holidays = dal.Holidays_("")
    index.forecast_tenor = dal.PeriodLength_New("12M")
    identity = dal.FixingIdentity_()
    identity.index_name = "USD-PYTHON-CURVATURE"
    identity.fixing_hour = 10
    identity.fixing_minute = 0
    terms = dal.FraTradeTerms_(notional=1.0, contract_rate=0.028, receive_floating=True,
                              settle_at_start=False, index=index, fixing_identity=identity,
                              forecast_component_key="rate_curvature", discount_component_key="rate_curvature")
    trade = dal.RateTradeDefinition_(instrument_id="history", instrument_type=dal.RateInstrumentType.FRA,
                                    trade_date=dal.Date_(2024, 7, 2), start_date=dal.Date_(2024, 7, 2),
                                    maturity_date=dal.Date_(2025, 7, 2), currency="USD", terms=terms)
    required = dal._dal._RequiredHistoricalRateTradeFixings([trade], dal.DateTime_(today, 0))
    assert len(required) == 1
    _, name, time = required[0]
    fixings = dal.MarketFixingSnapshot_New({name: {time: 0.031}})
    settings = dal.RateTradeQuoteCurvatureSettings_(fixings=fixings)
    assert settings.fixings is fixings
    source = dal.RateCalibration_New(builder.Build())
    first = dal.RateTradeQuoteCurvature([trade], source, bump_request(), settings=settings).curvature
    del fixings
    gc.collect()
    second = dal.RateTradeQuoteCurvature([trade], source, bump_request(), settings=settings).curvature
    assert second.value == first.value
    assert second.hessian_products.to_rows() == first.hessian_products.to_rows()
    expected = (0.031 - 0.028) * (1.025 ** -(181 / 365))
    assert second.value == pytest.approx(expected, abs=1e-12)


def test_saved_calibration_fixing_conflicts_reject_before_valid_recovery():
    today = dal.Date_(2025, 1, 16)
    time = dal.DateTime_(dal.Date_(2024, 12, 16), 10)
    history = dal.MarketFixingSnapshot_New({"SAVED-PYTHON-CURVATURE": {time: 0.03}})
    basis = dal.XccyBasisCurveDeclaration_()
    basis.curve_name = "usd_eur_basis"
    basis.instruments_ = [_basis_instrument()]
    basis.knot_dates = [today.AddDays(365)]
    basis.parameterization = dal.CurveParameterization.PIECEWISE_CONSTANT_FWD
    basis.initial_guess_per_node = [0.001]
    builder = dal.JointXccyCalibrationSpecBuilder_()
    builder.valuation_time = dal.DateTime_(today, 0)
    builder.pair = dal.CurrencyPair_New("USD", "EUR")
    builder.collateral_currency = dal.Ccy_("USD")
    builder.fx_spot = 1.10
    builder.domestic = _joint_curve("usd_ois", "USD", 0.04)
    builder.foreign = _joint_curve("eur_ois", "EUR", 0.03)
    builder.basis = basis
    builder.fixings = history
    builder.solver_options.initial_guess = 0.01
    builder.solver_options.tolerance = 1e-9
    builder.solver_options.max_evaluations = 400
    source = dal.RateCalibration_New(builder.Build())
    trade = single_inputs(component="domestic:0:usd_ois")[1]
    bumps = dal.BumpOverAADRequest_(directions=dal.DoubleMatrix_(0, 3), steps=[])
    reference = dal.RateTradeQuoteCurvature([trade], source, bumps).curvature
    conflict = dal.MarketFixingSnapshot_New({"SAVED-PYTHON-CURVATURE": {time: 0.04}})
    settings = dal.RateTradeQuoteCurvatureSettings_(fixings=conflict)
    with pytest.raises(RuntimeError, match="[Cc]onflict"):
        dal.RateTradeQuoteCurvature([trade], source, bumps, settings=settings)
    recovered = dal.RateTradeQuoteCurvature([trade], source, bumps).curvature
    assert recovered.gradient == reference.gradient


def test_native_financial_work_releases_gil_and_independent_threads_agree():
    builder, trade = single_inputs()
    source = dal.RateCalibration_New(builder.Build())
    request = bump_request([[1.0, 0.3]] * 16, [2e-4] * 16)

    def calculate():
        return dal.RateTradeQuoteCurvature([trade], source, request)

    result = _run_with_quote_risk_gil_heartbeat(calculate, barrier=lambda _: None)
    with ThreadPoolExecutor(max_workers=2) as executor:
        futures = [executor.submit(calculate) for _ in range(2)]
        for future in futures:
            concurrent = future.result(timeout=30)
            assert concurrent.curvature.gradient == result.curvature.gradient
            assert concurrent.curvature.hessian_products.to_rows() == result.curvature.hessian_products.to_rows()
