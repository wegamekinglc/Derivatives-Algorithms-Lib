"""Closed financial preparation and passive segmented native MC projection."""

import copy
from concurrent.futures import ThreadPoolExecutor
from enum import IntEnum
import gc
import math

import dal
import pytest

from test_quote_risk import _run_with_quote_risk_gil_heartbeat


POINT = [100.0, 0.2, 0.03, 0.01, 2.0]
ROWS = [[1.0, 0.3, -0.1, 0.2, 0.5], [-2.0, -0.6, 0.2, -0.4, -1.0], [0.0, 1.0, 0.0, 0.0, 0.0]]


class IntegerEnum(IntEnum):
    ONE = 1


def bumps(rows=None, steps=None, **budgets):
    rows = ROWS if rows is None else rows
    directions = dal.DoubleMatrix_(rows) if rows else dal.DoubleMatrix_(0, 5)
    return dal.BumpOverAADRequest_(directions=directions,
                                  steps=([2e-4, 1e-4, 2e-4] if steps is None else steps), **budgets)


def polynomial_plan():
    product = dal.Product_New(
        ["SCALE", dal.Date_(2027, 1, 2)],
        ["2", "pay PAYS SCALE * FIX(EQ[SEGMENTED_PYTHON]) ^ 2"],
    )
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 1, 2))
    return dal.BlackScholesMonteCarloPlan_New(product, valuation=valuation)


def gaussians(paths=35, offset=7, precision="Default"):
    precise = precision == "Precise"
    random = dal.SobolRSG_New(offset, ndim=1, precise=precise, polish=precise)
    return [row[0] for row in dal.SobolRSG_Get_Normal(random, paths).to_rows()]


def polynomial_oracle(point, normals):
    spot, vol, rate, div, scale = point
    value = 0.0
    gradient = [0.0] * 5
    hessian = [[0.0] * 5 for _ in range(5)]
    for normal in normals:
        unit = spot ** 2 * math.exp(rate - 2 * div - vol ** 2 + 2 * vol * normal)
        payoff = scale * unit
        value += payoff / len(normals)
        log_gradient = [2 / spot, 2 * (normal - vol), 1.0, -2.0, 1 / scale]
        log_diagonal = [-2 / spot ** 2, -2.0, 0.0, 0.0, -1 / scale ** 2]
        for i in range(5):
            gradient[i] += payoff * log_gradient[i] / len(normals)
            for j in range(5):
                hessian[i][j] += payoff * (log_gradient[i] * log_gradient[j]
                                           + (log_diagonal[i] if i == j else 0)) / len(normals)
    return value, gradient, hessian


def secant(point, row, step, normals):
    plus = polynomial_oracle([x + step * d for x, d in zip(point, row)], normals)[1]
    minus = polynomial_oracle([x - step * d for x, d in zip(point, row)], normals)[1]
    return [(p - m) / (2 * step) for p, m in zip(plus, minus)]


def linear_plan():
    product = dal.Product_New(
        ["SCALE", dal.Date_(2027, 1, 2)],
        ["2", "pay PAYS SCALE * FIX(EQ[SEGMENTED_PYTHON])"],
    )
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 1, 2))
    return dal.BlackScholesMonteCarloPlan_New(product, valuation=valuation)


def test_closed_preparation_and_explicit_point():
    plan = linear_plan()
    point = [100.0, 0.0, 0.03, 0.01, *plan.script_constants]
    result = dal.BlackScholesMonteCarlo_Get_Risk(plan, point, 1)
    assert result.value == pytest.approx(200.0 * math.exp(-0.01), abs=1e-10)
    assert result.point == point
    assert result.parameter_labels == ["spot", "vol", "rate", "div", "SCALE"]
    assert result.plan.valuation.evaluation_date == dal.Date_(2026, 1, 2)


@pytest.mark.parametrize("precision, bridge", [("Default", False), ("Fast", True), ("Precise", True)])
def test_finite_path_value_every_gradient_and_actual_step_hvp(precision, bridge):
    plan = polynomial_plan()
    settings = dal.SegmentedMonteCarloSettings_(first_path=7, use_bb=bridge, normal_precision=precision, segment_steps=1)
    result = dal.BlackScholesMonteCarlo_Get_Curvature(plan, POINT, 35, bumps(), settings=settings)
    expected, gradient, _ = polynomial_oracle(POINT, gaussians(precision=precision))
    assert result.base.value == pytest.approx(expected, rel=1e-12, abs=1e-9)
    assert result.base.gradient == pytest.approx(gradient, rel=1e-12, abs=1e-8)
    for row, step, product in zip(result.directions.to_rows(), result.steps, result.hessian_products.to_rows()):
        assert product == pytest.approx(secant(POINT, row, step, gaussians(precision=precision)), rel=2e-8, abs=1e-5)
    assert result.hessian_products.to_rows()[1] == pytest.approx(
        [-2 * x for x in result.hessian_products.to_rows()[0]], rel=1e-9, abs=1e-6)
    execution = result.base.execution
    assert (execution.path_count, execution.first_path, execution.batch_size, execution.batches) == (35, 7, 32, 2)
    assert 1 <= execution.lanes <= 2
    assert execution.segment_steps == 1
    assert execution.rsg == "sobol"
    assert execution.use_bb is bridge
    assert execution.normal_precision == precision
    assert execution.max_path_tape_bytes > 0
    assert execution.max_path_checkpoint_bytes > 0
    assert result.execution.method == "BumpOverSegmentedNativeAAD"
    assert result.execution.gradient_evaluations == 7
    assert result.execution.numeric_payload_bytes == 352
    assert result.execution.max_path_tape_bytes >= execution.max_path_tape_bytes


def test_smooth_hessian_convergence_is_separate_from_step_secant_oracle():
    plan = polynomial_plan()
    normals = gaussians()
    row = ROWS[0]
    hessian = polynomial_oracle(POINT, normals)[2]
    exact = [sum(hessian[i][j] * row[j] for j in range(5)) for i in range(5)]
    errors = []
    for step in [0.01, 0.005]:
        result = dal.BlackScholesMonteCarlo_Get_Curvature(
            plan, POINT, 35, bumps([row], [step]), settings=dal.SegmentedMonteCarloSettings_(first_path=7))
        products = result.hessian_products.to_rows()[0]
        assert products == pytest.approx(secant(POINT, row, step, normals), rel=2e-8, abs=1e-6)
        errors.append(max(abs(x - y) for x, y in zip(products, exact)))
    assert 0.20 < errors[1] / errors[0] < 0.30


@pytest.mark.parametrize("configuration", [
    {"rsg": "mrg32", "first_path": 7},
    {"rsg": "irn", "first_path": 3, "use_bb": True, "normal_precision": "Precise"},
    {"rsg": "sobol", "first_path": 7, "scramble_key": 2 ** 64 - 1},
])
def test_sampling_choices_replay_identical_first_gradients(configuration):
    plan = polynomial_plan()
    settings = dal.SegmentedMonteCarloSettings_(**configuration)
    request = bumps([ROWS[0]], [2e-4])
    result = dal.BlackScholesMonteCarlo_Get_Curvature(plan, POINT, 33, request, settings=settings)
    base = dal.BlackScholesMonteCarlo_Get_Risk(plan, POINT, 33, settings=settings)
    assert result.base.value == base.value
    assert result.base.gradient == base.gradient
    plus = dal.BlackScholesMonteCarlo_Get_Risk(plan, [x + 2e-4 * d for x, d in zip(POINT, ROWS[0])], 33, settings=settings)
    minus = dal.BlackScholesMonteCarlo_Get_Risk(plan, [x - 2e-4 * d for x, d in zip(POINT, ROWS[0])], 33, settings=settings)
    assert result.hessian_products.to_rows()[0] == pytest.approx(
        [(p - m) / 4e-4 for p, m in zip(plus.gradient, minus.gradient)], rel=2e-8, abs=1e-5)
    for field, value in configuration.items():
        assert getattr(result.settings, field) == value
        assert getattr(result.base.execution, field) == value


def test_empty_directions_and_exact_numeric_budget():
    plan = polynomial_plan()
    for rows, steps, byte_count in [([], [], 88), ([ROWS[0]], [2e-4], 176)]:
        result = dal.BlackScholesMonteCarlo_Get_Curvature(
            plan, POINT, 1, bumps(rows, steps, numeric_payload_budget_bytes=byte_count))
        assert result.execution.numeric_payload_bytes == byte_count
        assert result.execution.gradient_evaluations == 1 + 2 * len(rows)
        assert len(result.base.gradient) == 5
        assert (result.hessian_products.Rows(), result.hessian_products.Cols()) == (len(rows), 5)
        for cap in [0, byte_count - 1]:
            with pytest.raises(RuntimeError, match="numeric payload budget"):
                dal.BlackScholesMonteCarlo_Get_Curvature(plan, POINT, 1, bumps(rows, steps, numeric_payload_budget_bytes=cap))
    assert dal.BlackScholesMonteCarlo_Get_Risk(plan, POINT, 1).value == result.base.value


def test_path_capacity_limits_and_effective_recording_cap():
    plan = polynomial_plan()
    settings = dal.SegmentedMonteCarloSettings_(checkpoint_capacity_budget_bytes=2 ** 30,
                                               recording_capacity_budget_bytes=2 ** 30)
    result = dal.BlackScholesMonteCarlo_Get_Curvature(
        plan, POINT, 1, bumps([], [], recording_capacity_budget_bytes=2 ** 29), settings=settings)
    assert result.settings.recording_capacity_budget_bytes == 2 ** 30
    assert result.execution.recording_capacity_budget_bytes == 2 ** 29
    checkpoint = result.execution.max_path_checkpoint_bytes
    assert dal.BlackScholesMonteCarlo_Get_Risk(
        plan, POINT, 1, settings=dal.SegmentedMonteCarloSettings_(checkpoint_capacity_budget_bytes=checkpoint)).value == result.base.value
    for cap in [0, checkpoint - 1]:
        with pytest.raises(RuntimeError, match="checkpoint"):
            dal.BlackScholesMonteCarlo_Get_Risk(
                plan, POINT, 1, settings=dal.SegmentedMonteCarloSettings_(checkpoint_capacity_budget_bytes=cap))
    with pytest.raises(RuntimeError, match="capacity|budget"):
        dal.BlackScholesMonteCarlo_Get_Curvature(plan, POINT, 1, bumps([], [], recording_capacity_budget_bytes=0))
    assert dal.BlackScholesMonteCarlo_Get_Risk(plan, POINT, 1).value == result.base.value


def test_plan_results_and_nested_getters_own_all_inputs():
    plan = polynomial_plan()
    point = POINT.copy()
    request = bumps()
    settings = dal.SegmentedMonteCarloSettings_(first_path=7)
    result = dal.BlackScholesMonteCarlo_Get_Curvature(plan, point, 35, request, settings=settings)
    saved = result.hessian_products.to_rows()
    point[:] = [1.0]
    plan.script_constants[0] = -99
    plan.contract_events[0] = "invalid"
    plan.contract_dates.clear()
    plan.valuation.evaluation_date = dal.Date_(2030, 1, 2)
    result.point[0] = 1.0
    result.steps[0] = 10
    result.directions[0, 0] = 0
    result.hessian_products[0, 0] = 0
    result.base.gradient[0] = 0
    with pytest.raises(AttributeError):
        settings.first_path = 99
    with pytest.raises(AttributeError):
        result.execution.gradient_evaluations = 0
    del plan, point, settings, request
    gc.collect()
    dal.EvaluationDate_Set(dal.Date_(2040, 1, 2))
    for copied in [result, copy.copy(result), copy.deepcopy(result)]:
        assert copied.point == POINT
        assert copied.hessian_products.to_rows() == saved
        assert copied.plan.script_constants == [2.0]
        assert copied.plan.contract_events[0] == "2"
        assert copied.plan.valuation.evaluation_date == dal.Date_(2026, 1, 2)
        assert len(copied.plan.contract_dates) == 2
    for copied in [copy.copy(result.plan), copy.deepcopy(result.plan)]:
        replay = dal.BlackScholesMonteCarlo_Get_Curvature(copied, POINT, 35, bumps(), settings=result.settings)
        assert replay.hessian_products.to_rows() == saved
    mean = dal.BlackScholesMonteCarlo_Get_Risk(result.plan, POINT, 35, settings=result.settings)
    for copied in [copy.copy(mean), copy.deepcopy(mean)]:
        copied.gradient[0] = -1
        assert copied.gradient == result.base.gradient
        assert copied.point == POINT
    for settings_copy in [copy.copy(result.settings), copy.deepcopy(result.settings)]:
        assert settings_copy.first_path == 7


def test_explicit_history_is_sealed_and_constants_remain_differentiable():
    index = "EQ[SEGMENTED_SEALED_HISTORY]"
    history = dal.Date_(2025, 1, 2)
    product = dal.Product_New(["SCALE", history, dal.Date_(2027, 1, 2)],
                              ["2", f"x = SCALE * FIX({index})", f"pay PAYS x * FIX({index})"])
    snapshot = dal.MarketFixingSnapshot_New({index: {dal.DateTime_(history, 0): 80.0}})
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 1, 2), fixings=snapshot)
    plan = dal.BlackScholesMonteCarloPlan_New(product, valuation=valuation)
    valuation.fixings = dal.MarketFixingSnapshot_New({index: {dal.DateTime_(history, 0): 90.0}})
    valuation.evaluation_date = dal.Date_(2028, 1, 2)
    dal.EvaluationDate_Set(dal.Date_(2040, 1, 2))
    result = dal.BlackScholesMonteCarlo_Get_Risk(plan, [100, 0, 0.03, 0.01, 3.0], 1)
    assert result.value == pytest.approx(24000 * math.exp(-0.01), abs=1e-8)
    assert result.gradient[4] == pytest.approx(8000 * math.exp(-0.01), abs=1e-8)
    known = [observation for observation in result.plan.observations if observation.historical]
    assert len(known) == 1
    assert known[0].value == 80
    result.plan.observations.clear()
    assert len(result.plan.observations) == 2


def test_time_zero_and_historical_only_have_no_gaussian_dimension_limit():
    today = dal.Date_(2026, 1, 2)
    now = dal.BlackScholesMonteCarloPlan_New(
        dal.Product_New([today], ["pay PAYS FIX(EQ[SEGMENTED_NOW])"]),
        valuation=dal.ScriptValuationSettings_(evaluation_date=today))
    settings = dal.SegmentedMonteCarloSettings_(first_path=2 ** 32)
    result = dal.BlackScholesMonteCarlo_Get_Risk(now, POINT[:4], 1, settings=settings)
    assert result.value == 100
    assert result.gradient == [1, 0, 0, 0]
    history = dal.Date_(2025, 1, 2)
    index = "EQ[SEGMENTED_PAST]"
    past = dal.BlackScholesMonteCarloPlan_New(
        dal.Product_New(["SCALE", history], ["2", f"APPEND(v, SCALE * FIX({index})) pay PAYS 0 pay = SUM(v)"]),
        valuation=dal.ScriptValuationSettings_(evaluation_date=today,
            fixings=dal.MarketFixingSnapshot_New({index: {dal.DateTime_(history, 0): 80.0}})))
    result = dal.BlackScholesMonteCarlo_Get_Risk(past, [*POINT[:4], -3.0], 1, settings=settings)
    assert result.value == -240
    assert result.gradient == [0, 0, 0, 0, 80]
    assert result.plan.script_constants == [2]


@pytest.mark.parametrize("configuration", [
    {"rsg": "invalid"}, {"normal_precision": "invalid"}, {"rsg": "irn", "scramble_key": 1},
    {"first_path": 2 ** 32 - 1},
])
def test_invalid_native_sampling_rejects_and_recovers(configuration):
    plan = polynomial_plan()
    with pytest.raises(RuntimeError):
        dal.BlackScholesMonteCarlo_Get_Risk(plan, POINT, 1, settings=dal.SegmentedMonteCarloSettings_(**configuration))
    assert math.isfinite(dal.BlackScholesMonteCarlo_Get_Risk(plan, POINT, 1).value)


@pytest.mark.parametrize("configuration", [
    {"rsg": 1}, {"rsg": b"sobol"}, {"rsg": "sobol\x00"}, {"use_bb": 1},
    {"normal_precision": False}, {"first_path": True}, {"first_path": 1.2},
    {"first_path": IntegerEnum.ONE}, {"segment_steps": False}, {"segment_steps": 0},
    {"checkpoint_capacity_budget_bytes": -1}, {"recording_capacity_budget_bytes": True},
    {"scramble_key": -1}, {"scramble_key": True}, {"scramble_key": IntegerEnum.ONE},
    {"scramble_key": 2 ** 64}, {"first_path": 2 ** 128},
])
def test_strict_settings_conversion(configuration):
    with pytest.raises((TypeError, RuntimeError)):
        dal.SegmentedMonteCarloSettings_(**configuration)


@pytest.mark.parametrize("point", [None, "numbers", iter(POINT), [True] * 5, [IntegerEnum.ONE] * 5,
                                     ["1"] * 5, [float("nan")] * 5, [float("inf")] * 5, [2 ** 10000] * 5,
                                     [], [100, -0.1, 0.03, 0.01, 2]])
def test_strict_point_conversion_and_native_domains(point):
    plan = polynomial_plan()
    with pytest.raises((TypeError, RuntimeError)):
        dal.BlackScholesMonteCarlo_Get_Risk(plan, point, 1)
    assert math.isfinite(dal.BlackScholesMonteCarlo_Get_Risk(plan, POINT, 1).value)


@pytest.mark.parametrize("paths", [0, -1, True, 1.2, IntegerEnum.ONE, 2 ** 64])
def test_strict_positive_path_count(paths):
    with pytest.raises((TypeError, RuntimeError), match="num_path"):
        dal.BlackScholesMonteCarlo_Get_Risk(polynomial_plan(), POINT, paths)


def test_closed_types_malformed_geometry_and_domain_crossing_bumps():
    plan = polynomial_plan()
    with pytest.raises(TypeError):
        dal.BlackScholesMonteCarloPlan_()
    with pytest.raises(TypeError):
        dal.SegmentedMonteCarloSettings_("sobol")
    with pytest.raises(TypeError, match="plan"):
        dal.BlackScholesMonteCarlo_Get_Risk(None, POINT, 1)
    with pytest.raises(TypeError, match="settings"):
        dal.BlackScholesMonteCarlo_Get_Risk(plan, POINT, 1, settings={})
    with pytest.raises(TypeError, match="bumps"):
        dal.BlackScholesMonteCarlo_Get_Curvature(plan, POINT, 1, None)
    with pytest.raises(RuntimeError, match="column"):
        dal.BlackScholesMonteCarlo_Get_Curvature(plan, POINT, 1,
            dal.BumpOverAADRequest_(directions=dal.DoubleMatrix_([[1.0]]), steps=[0.01]))
    with pytest.raises(RuntimeError, match="nonzero"):
        dal.BlackScholesMonteCarlo_Get_Curvature(plan, POINT, 1, bumps([[0.0] * 5], [0.01]))
    with pytest.raises(RuntimeError, match="direction=0.*minus"):
        dal.BlackScholesMonteCarlo_Get_Curvature(plan, POINT, 1, bumps([[0, 1, 0, 0, 0]], [0.3]))
    assert math.isfinite(dal.BlackScholesMonteCarlo_Get_Curvature(plan, POINT, 1, bumps()).base.value)


def test_factory_rejects_exercise_and_invalid_smoothing():
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 1, 2))
    exercise = dal.Product_New([dal.Date_(2027, 1, 2)], ["EXERCISE MAX(100 - FIX(EQ[SEGMENTED_EXERCISE]), 0)"])
    with pytest.raises(RuntimeError, match="EXERCISE"):
        dal.BlackScholesMonteCarloPlan_New(exercise, valuation=valuation)
    for width in [True, float("nan"), float("inf"), -0.1]:
        with pytest.raises((RuntimeError, TypeError)):
            dal.BlackScholesMonteCarloPlan_New(exercise, valuation=valuation, smoothing=width)
    with pytest.raises(TypeError, match="product"):
        dal.BlackScholesMonteCarloPlan_New(None)


def test_gil_release_and_independent_concurrent_callers():
    plan = polynomial_plan()
    settings = dal.SegmentedMonteCarloSettings_(first_path=7)
    calculate = lambda: dal.BlackScholesMonteCarlo_Get_Curvature(plan, POINT, 1025, bumps(), settings=settings)
    result = _run_with_quote_risk_gil_heartbeat(calculate, barrier=lambda _: None)
    with ThreadPoolExecutor(max_workers=2) as pool:
        copies = list(pool.map(lambda _: calculate(), range(2)))
    for copied in copies:
        assert copied.base.value == result.base.value
        assert copied.base.gradient == result.base.gradient
        assert copied.hessian_products.to_rows() == result.hessian_products.to_rows()
