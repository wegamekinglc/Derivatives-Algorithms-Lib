"""Owned Python projection of recalibrated native Dupire quote curvature."""

import copy
from concurrent.futures import ThreadPoolExecutor
from enum import IntEnum
import gc
import math

import dal
import pytest

from test_dupire_risk import hybrid_model, inputs
from test_quote_risk import _run_with_quote_risk_gil_heartbeat


class NumericEnum(IntEnum):
    VALUE = 1


def calibration(spreads=None):
    spreads = dal.DoubleMatrix_(3, 2, 0.001) if spreads is None else spreads
    return dal.DupireCalibration_New(
        dal.BSModelData_New(100.0, 0.2, 0.05, 0.02), inputs(spreads), name="curvature",
    )


def product(quote=0.001, *, mixed=False):
    expression = "QUOTE * QUOTE"
    if mixed:
        expression += " + FIX(EQ[LOCAL]) * FIX(EQ[LOCAL]) / 100 + 2 * QUOTE * FIX(EQ[LOCAL])"
    return dal.Product_New(
        ["QUOTE", dal.Date_(2027, 9, 12)], [repr(quote), "pay PAYS " + expression],
    )


def risk_request(*, compiled=False, num_paths=17, quotes=None, direct=None):
    return dal.DupireScriptRiskRequest_(
        num_paths=num_paths, quotes=quotes, direct=direct,
        direct_bindings=[dal.DupireQuoteBinding_(constant_ordinal=0, quote_id="quote:3")],
        valuation=dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 9, 12)),
        simulation=dal.MonteCarloSettings_(enable_aad=True, compiled=compiled),
    )


def directions():
    result = dal.DoubleMatrix_(3, 6, 0.0)
    result[0, 3] = 1.0
    result[1, 3] = -2.0
    result[2, 0] = 1.0
    return result


def bumps(matrix=None, steps=None, **budgets):
    return dal.BumpOverAADRequest_(
        directions=directions() if matrix is None else matrix,
        steps=[2e-4, 1e-4, 2e-4] if steps is None else steps, **budgets,
    )


def plan_for(*, source=None, request=None, mixed=False, **risk_options):
    source = calibration() if source is None else source
    request = dal.DupireScriptCurvatureRequest_(risk=risk_request(**risk_options), bumps=bumps()) if request is None else request
    return dal.DupireScriptCurvaturePlan_New(
        product(mixed=mixed), hybrid_model(source), source, "Z_LOCAL", request,
    )


def test_public_bump_request_owns_required_inputs():
    matrix = directions()
    steps = [2e-4, 1e-4, 2e-4]
    request = dal.BumpOverAADRequest_(directions=matrix, steps=steps)
    matrix[0, 3] = 99
    steps[0] = 99
    request.directions[1, 3] = 99
    request.steps[1] = 99
    assert request.directions[0, 3] == 1.0
    assert request.directions[1, 3] == -2.0
    assert request.steps == [2e-4, 1e-4, 2e-4]
    assert request.numeric_payload_budget_bytes is None
    assert request.recording_capacity_budget_bytes is None
    for duplicate in (copy.copy(request), copy.deepcopy(request)):
        assert duplicate.directions.to_rows() == request.directions.to_rows()
        assert duplicate.steps == request.steps
    with pytest.raises(AttributeError):
        request.steps = []
    with pytest.raises(TypeError):
        dal.BumpOverAADRequest_(matrix, steps)


@pytest.mark.parametrize("compiled", [False, True])
def test_direct_quadratic_has_raw_signed_analytic_gamma_and_exact_work(compiled):
    plan = plan_for(compiled=compiled)
    result = dal.DupireScriptCurvatureResult_New(plan)
    discount = math.exp(-0.05)
    assert result.base.valuation.values == pytest.approx([1e-6 * discount], abs=1e-14)
    assert result.gradient == pytest.approx([0, 0, 0, 0.002 * discount, 0, 0], abs=1e-12)
    assert result.point == [0.001] * 6
    assert [coordinate.id for coordinate in result.input_axis] == [f"quote:{i}" for i in range(6)]
    for row, multiplier in enumerate([1.0, -2.0, 0.0]):
        assert result.hessian_products.to_rows()[row] == pytest.approx(
            [0, 0, 0, multiplier * 2 * discount, 0, 0], abs=1e-10,
        )
    assert result.execution.method == "BumpOverRecalibratedNativeDupireMonteCarloAAD"
    assert result.execution.quote_gradient_evaluations == 7
    assert result.execution.paths_per_evaluation == 17
    assert result.execution.numeric_payload_bytes == plan.numeric_payload_bytes
    assert plan.numeric_payload_bytes == 8 * (2 + 18 + 1 + 5 * 6 + 2 * 3 * 6 + 3)
    assert plan.base_plan.simulation_settings.compiled == compiled
    assert dal.DupireScriptCurvatureResult_New(plan).hessian_products.to_rows() == result.hessian_products.to_rows()


@pytest.mark.parametrize("compiled", [False, True])
def test_mixed_surface_products_match_independently_recalibrated_gradients(compiled):
    vector = [0.3, -0.2, 1.0, 0.4, -0.1, 0.2]
    step = 2e-4
    requested = dal.DupireScriptCurvatureRequest_(
        risk=risk_request(compiled=compiled),
        bumps=bumps(dal.DoubleMatrix_([vector]), [step]),
    )
    result = dal.DupireScriptCurvatureResult_New(plan_for(request=requested, mixed=True))
    gradients = []
    for sign in [1.0, -1.0]:
        values = [0.001 + sign * step * value for value in vector]
        source = calibration(dal.DoubleMatrix_([values[i:i + 2] for i in range(0, 6, 2)]))
        plan = dal.DupireScriptRiskPlan_New(
            product(values[3], mixed=True), hybrid_model(source), source, "Z_LOCAL",
            risk_request(compiled=compiled),
        )
        risk = dal.DupireScriptRiskResult_New(plan)
        gradients.append([value for row in risk.quote_risk.quote_risk.total_adjoints.to_rows() for value in row])
    expected = [(plus - minus) / (2 * step) for plus, minus in zip(*gradients)]
    assert result.hessian_products.to_rows()[0] == pytest.approx(expected, rel=1e-10, abs=1e-8)
    assert max(map(abs, expected)) > 1.0


def test_reported_selected_base_keeps_full_raw_products_and_detached_results():
    full = dal.DupireScriptCurvatureResult_New(plan_for())
    quotes = dal.CalibrationRiskRequest_(inputs=["quote:3", "quote:0"], report_factors=[0.01, 0.5])
    plan = plan_for(quotes=quotes)
    result = dal.DupireScriptCurvatureResult_New(plan)
    assert result.hessian_products.to_rows() == full.hessian_products.to_rows()
    assert len(result.input_axis) == 6
    assert [axis.id for axis in result.base.quote_risk.plan.input_axis] == ["quote:3", "quote:0"]
    assert result.base.quote_risk.reported_jacobian[0, 0] == 0.01 * result.gradient[3]
    retained = result.hessian_products.to_rows()
    result.hessian_products[0, 3] = -999
    result.directions[0, 3] = -999
    result.gradient[3] = -999
    result.point[3] = -999
    result.steps[0] = -999
    result.base.valuation.jacobian[0, 0] = -999
    plan.directions[0, 3] = -999
    plan.point[3] = -999
    plan.steps[0] = -999
    assert result.hessian_products.to_rows() == retained
    assert result.directions[0, 3] == 1.0
    assert result.point[3] == plan.point[3] == 0.001
    assert result.gradient == full.gradient
    for value in (plan, result, result.execution):
        assert type(copy.copy(value)) is type(value)
        assert type(copy.deepcopy(value)) is type(value)
    with pytest.raises(AttributeError):
        result.execution.paths_per_evaluation = 0
    with pytest.raises(TypeError):
        dal.DupireScriptCurvatureResult_()
    del plan, full
    gc.collect()
    assert result.hessian_products.to_rows() == retained


def test_empty_direction_request_and_exact_combined_budget():
    empty = bumps(dal.DoubleMatrix_(0, 6), [])
    request = dal.DupireScriptCurvatureRequest_(risk=risk_request(), bumps=empty)
    plan = plan_for(request=request)
    required = plan.numeric_payload_bytes
    assert required == 8 * (2 + 18 + 1 + 5 * 6)
    for budget in [0, required - 1]:
        rejected = dal.DupireScriptCurvatureRequest_(
            risk=risk_request(),
            bumps=bumps(dal.DoubleMatrix_(0, 6), [], numeric_payload_budget_bytes=budget),
        )
        with pytest.raises(RuntimeError, match="payload budget"):
            plan_for(request=rejected)
    exact = dal.DupireScriptCurvatureRequest_(
        risk=risk_request(), bumps=bumps(dal.DoubleMatrix_(0, 6), [], numeric_payload_budget_bytes=required),
    )
    result = dal.DupireScriptCurvatureResult_New(plan_for(request=exact))
    assert result.hessian_products.Rows() == 0
    assert result.hessian_products.Cols() == 6
    assert result.execution.quote_gradient_evaluations == 1
    assert result.execution.numeric_payload_bytes == required
    assert result.gradient[3] == pytest.approx(0.002 * math.exp(-0.05), abs=1e-12)


@pytest.mark.parametrize("value", [None, True, {}, [[1.0]], iter([])])
def test_directions_require_native_matrix(value):
    with pytest.raises(TypeError, match="BumpOverAADRequest_; directions"):
        dal.BumpOverAADRequest_(directions=value, steps=[])


@pytest.mark.parametrize("value", [None, True, {}, "0.01", iter([0.01]), [True], [NumericEnum.VALUE], ["0.01"], [None]])
def test_steps_require_copied_real_sequences(value):
    with pytest.raises(TypeError, match="BumpOverAADRequest_; steps"):
        dal.BumpOverAADRequest_(directions=dal.DoubleMatrix_(1, 6, 1.0), steps=value)


@pytest.mark.parametrize("value", [0, -0.01, math.nan, math.inf, -math.inf, 10**1000])
def test_steps_reject_nonpositive_nonfinite_or_overflow(value):
    with pytest.raises(RuntimeError, match="steps"):
        dal.BumpOverAADRequest_(directions=dal.DoubleMatrix_(1, 6, 1.0), steps=[value])


@pytest.mark.parametrize("value", [math.nan, math.inf, -math.inf])
def test_direction_values_must_be_finite(value):
    matrix = dal.DoubleMatrix_(1, 6, 1.0)
    matrix[0, 3] = value
    with pytest.raises(RuntimeError, match="directions"):
        dal.BumpOverAADRequest_(directions=matrix, steps=[1e-4])


@pytest.mark.parametrize("field", ["numeric_payload_budget_bytes", "recording_capacity_budget_bytes"])
@pytest.mark.parametrize("value", [True, NumericEnum.VALUE, 1.0, "1", -1, 2**100, dal.CurveJacobianMode.ANALYTIC])
def test_budgets_reject_coercion_negative_and_overflow(field, value):
    with pytest.raises((TypeError, RuntimeError), match=field):
        bumps(**{field: value})


def test_budget_index_protocol_preserves_zero_and_request_copies():
    class Index:
        def __index__(self):
            return 0
    original = bumps(numeric_payload_budget_bytes=Index(), recording_capacity_budget_bytes=0)
    request = dal.DupireScriptCurvatureRequest_(risk=risk_request(), bumps=original)
    assert request.bumps.numeric_payload_budget_bytes == 0
    assert request.bumps.recording_capacity_budget_bytes == 0
    assert copy.deepcopy(request).risk.num_paths == 17
    request.bumps.directions[0, 3] = 99
    assert request.bumps.directions[0, 3] == 1
    with pytest.raises(AttributeError):
        request.risk = None


@pytest.mark.parametrize("field,value", [("risk", None), ("risk", {}), ("risk", True), ("bumps", None), ("bumps", {}), ("bumps", True)])
def test_curvature_request_requires_native_typed_fields(field, value):
    arguments = dict(risk=risk_request(), bumps=bumps())
    arguments[field] = value
    with pytest.raises(TypeError, match="DupireScriptCurvatureRequest_; " + field):
        dal.DupireScriptCurvatureRequest_(**arguments)


@pytest.mark.parametrize("field", ["product", "modelData", "calibration", "component", "request"])
def test_plan_rejects_wrong_native_inputs_with_context(field):
    source = calibration()
    arguments = dict(product=product(), modelData=hybrid_model(source), calibration=source,
                     component="Z_LOCAL", request=dal.DupireScriptCurvatureRequest_(risk=risk_request(), bumps=bumps()))
    arguments[field] = None
    with pytest.raises(TypeError, match="DupireScriptCurvaturePlan_New; " + field):
        dal.DupireScriptCurvaturePlan_New(**arguments)
    with pytest.raises(TypeError, match="DupireScriptCurvatureResult_New; plan"):
        dal.DupireScriptCurvatureResult_New(None)


def test_dimension_zero_direction_and_unsupported_caps_reject_and_recover():
    with pytest.raises(RuntimeError, match="steps"):
        bumps(directions(), [1e-4])
    for matrix, message in [(dal.DoubleMatrix_(1, 5, 1.0), "column"), (dal.DoubleMatrix_(1, 6, 0.0), "nonzero")]:
        requested = dal.DupireScriptCurvatureRequest_(risk=risk_request(), bumps=bumps(matrix, [1e-4]))
        with pytest.raises(RuntimeError, match=message):
            plan_for(request=requested)
    request = dal.DupireScriptCurvatureRequest_(risk=risk_request(), bumps=bumps(recording_capacity_budget_bytes=0))
    with pytest.raises(RuntimeError, match="worker recording capacity"):
        plan_for(request=request)
    source = calibration()
    direct = dal.CalibrationDirectQuoteAdjoints_New(dal.CalibrationPullback_New(source), dal.DoubleMatrix_(3, 2))
    request = dal.DupireScriptCurvatureRequest_(risk=risk_request(direct=direct), bumps=bumps())
    with pytest.raises(RuntimeError, match="external first-order direct seeds"):
        plan_for(source=source, request=request)
    assert dal.DupireScriptCurvatureResult_New(plan_for()).execution.quote_gradient_evaluations == 7


def test_plan_freezes_evaluation_date_and_survives_original_inputs():
    source = calibration()
    request = dal.DupireScriptCurvatureRequest_(risk=risk_request(), bumps=bumps())
    plan = plan_for(source=source, request=request)
    del source, request
    gc.collect()
    dal.EvaluationDate_Set(dal.Date_(2028, 1, 1))
    result = dal.DupireScriptCurvatureResult_New(plan)
    assert result.hessian_products[0, 3] == pytest.approx(2 * math.exp(-0.05), abs=1e-10)


def test_plan_retains_explicit_historical_snapshot():
    source = calibration()
    history_date = dal.Date_(2026, 9, 11)
    history_time = dal.DateTime_(history_date, 0)
    settings = dal.ScriptValuationSettings_(
        evaluation_date=dal.Date_(2026, 9, 12),
        fixings=dal.MarketFixingSnapshot_New({"EQ[CURVATURE_PY_HISTORY]": {history_time: 80.0}}),
    )
    risk = dal.DupireScriptRiskRequest_(
        num_paths=17, direct_bindings=[dal.DupireQuoteBinding_(constant_ordinal=0, quote_id="quote:3")],
        valuation=settings,
    )
    historical = dal.Product_New(
        ["QUOTE", history_date, dal.Date_(2027, 9, 12)],
        ["0.001", "past = FIX(EQ[CURVATURE_PY_HISTORY])", "pay PAYS past + QUOTE * QUOTE"],
    )
    request = dal.DupireScriptCurvatureRequest_(risk=risk, bumps=bumps())
    plan = dal.DupireScriptCurvaturePlan_New(historical, hybrid_model(source), source, "Z_LOCAL", request)
    settings.fixings = dal.MarketFixingSnapshot_New({"EQ[CURVATURE_PY_HISTORY]": {history_time: 90.0}})
    del source, historical, request, risk, settings
    gc.collect()
    result = dal.DupireScriptCurvatureResult_New(plan)
    assert result.base.valuation.values == pytest.approx([(80 + 1e-6) * math.exp(-0.05)], abs=1e-10)
    assert result.hessian_products[0, 3] == pytest.approx(2 * math.exp(-0.05), abs=1e-10)


def test_exercise_and_price_only_requests_reject_before_valid_recovery():
    source = calibration()
    exercise = dal.Product_New(
        ["QUOTE", dal.Date_(2027, 9, 12)],
        ["0.001", "EXERCISE MAX(100 - FIX(EQ[LOCAL]) + QUOTE, 0)"],
        settings=dal.ScriptProductSettings_(default_index="EQ[LOCAL]"),
    )
    request = dal.DupireScriptCurvatureRequest_(risk=risk_request(), bumps=bumps())
    with pytest.raises(RuntimeError, match="exercise-policy"):
        dal.DupireScriptCurvaturePlan_New(exercise, hybrid_model(source), source, "Z_LOCAL", request)
    passive = dal.DupireScriptRiskRequest_(
        num_paths=17, valuation=dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 9, 12)),
        simulation=dal.MonteCarloSettings_(enable_aad=False),
    )
    with pytest.raises(RuntimeError, match="AAD"):
        plan_for(request=dal.DupireScriptCurvatureRequest_(risk=passive, bumps=bumps()))
    assert dal.DupireScriptCurvatureResult_New(plan_for()).execution.quote_gradient_evaluations == 7


def test_execution_releases_gil_and_supports_independent_calling_threads():
    plan = plan_for(num_paths=16385)
    result = _run_with_quote_risk_gil_heartbeat(
        lambda: dal.DupireScriptCurvatureResult_New(plan), barrier=lambda _: None,
    )
    assert result.execution.paths_per_evaluation == 16385
    small = plan_for()
    with ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(lambda _: dal.DupireScriptCurvatureResult_New(small), range(2)))
    assert results[0].hessian_products.to_rows() == results[1].hessian_products.to_rows()
