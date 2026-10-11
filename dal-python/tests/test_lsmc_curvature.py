"""Owning native LSMC policy curvature projection."""

import copy
from concurrent.futures import ThreadPoolExecutor
from enum import IntEnum
import gc
import math

import dal
import pytest

from test_quote_risk import _run_with_quote_risk_gil_heartbeat


POINT = [100.0, 0.2, 0.05, 0.0, 50.0]
ROWS = [[1.0, 0.01, 0.002, -0.003, 0.2], [-2.0, -0.02, -0.004, 0.006, -0.4]]
STEPS = [0.1, 0.05]


class IntegerEnum(IntEnum):
    ONE = 1


def product(constant=45.0, history=False):
    dates = ["K", dal.Date_(2027, 4, 10), dal.Date_(2027, 10, 10)]
    events = [str(constant), "EXERCISE 2 * K - spot()", "EXERCISE 2 * K - spot()"]
    if history:
        dates.insert(1, dal.Date_(2026, 10, 9))
        events = [str(constant), "x = 2 * K", "EXERCISE x - spot()", "EXERCISE x - spot()"]
    return dal.Product_New(dates, events)


def plan(compiled=True, mode="Frozen", history=False, **settings):
    simulation = dal.MonteCarloSettings_(enable_aad=True, compiled=compiled, smooth=2.0,
        lsmc_training_paths=128, lsmc_policy_risk_mode=mode, **settings)
    return dal.BlackScholesLsmcPlan_New(product(history=history), simulation=simulation,
        valuation=dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 10, 10)))


def bumps(rows=None, steps=None, **budgets):
    rows = ROWS if rows is None else rows
    matrix = dal.DoubleMatrix_(rows) if rows else dal.DoubleMatrix_(0, 5)
    return dal.BumpOverAADRequest_(directions=matrix, steps=STEPS if steps is None else steps, **budgets)


def normals(result):
    offset = result.execution.training_paths + result.execution.validation_paths
    random = dal.SobolRSG_New(offset, ndim=len(result.plan.time_line))
    return dal.SobolRSG_Get_Normal(random, result.execution.paths_per_replicate).to_rows()


def continuation(policy, spot):
    scalar = not policy.basis_powers
    mean = policy.mean if scalar else policy.normalization_means[0]
    sigma = policy.sigma if scalar else policy.normalization_sigmas[0]
    z = (spot - mean) / sigma
    powers = range(len(policy.coefficients)) if scalar else [row[0] for row in policy.basis_powers]
    return sum(coefficient * z ** power for coefficient, power in zip(policy.coefficients, powers))


def clamp(value):
    return min(1.0, max(0.0, value))


def frozen_price(result, point, gaussians):
    times = result.plan.time_line
    policy = result.base_policy
    eps = result.simulation.smooth
    total = 0.0
    for gaussian in gaussians:
        spot, previous = point[0], 0.0
        spots = []
        for time, normal in zip(times, gaussian):
            dt = time - previous
            spot *= math.exp((point[2] - point[3] - 0.5 * point[1] ** 2) * dt + point[1] * math.sqrt(dt) * normal)
            spots.append(spot)
            previous = time
        value = 0.0
        for date in reversed(range(len(times))):
            if date + 1 < len(times):
                value *= math.exp(-point[2] * (times[date + 1] - times[date]))
            exercise = 2 * point[4] - spots[date]
            weight = clamp((exercise - continuation(policy[date], spots[date])) / eps + 0.5) * clamp(exercise / eps)
            value = weight * exercise + (1 - weight) * value
        total += value * math.exp(-point[2] * times[0])
    return total / len(gaussians)


def frozen_gradient(result, point, gaussians):
    gradient = []
    for coordinate, step in enumerate([1e-3, 1e-6, 1e-6, 1e-6, 1e-3]):
        plus, minus = point.copy(), point.copy()
        plus[coordinate] += step
        minus[coordinate] -= step
        gradient.append((frozen_price(result, plus, gaussians) - frozen_price(result, minus, gaussians)) / (2 * step))
    return gradient


@pytest.mark.parametrize("compiled", [False, True])
@pytest.mark.parametrize("history", [False, True])
def test_frozen_retained_policy_price_every_gradient_and_actual_step_hvp(compiled, history):
    result = dal.BlackScholesLsmc_Get_Curvature(plan(compiled, history=history), POINT, 35, bumps())
    gaussians = normals(result)
    assert result.value == pytest.approx(frozen_price(result, POINT, gaussians), abs=1e-10)
    assert result.gradient == pytest.approx(frozen_gradient(result, POINT, gaussians), abs=2e-6)
    for row, step, actual in zip(ROWS, STEPS, result.hessian_products.to_rows()):
        plus = frozen_gradient(result, [x + step * d for x, d in zip(POINT, row)], gaussians)
        minus = frozen_gradient(result, [x - step * d for x, d in zip(POINT, row)], gaussians)
        assert actual == pytest.approx([(p - m) / (2 * step) for p, m in zip(plus, minus)], abs=2e-5)
    assert result.hessian_products.to_rows()[1] == pytest.approx([-2 * x for x in result.hessian_products.to_rows()[0]], abs=1e-9)
    assert result.execution.method == "BumpOverFrozenNativeLsmcAAD"
    assert result.execution.gradient_evaluations == 5
    assert result.plan.script_constants == [45.0]
    assert result.point == POINT


def existing_gradient(point, simulation, history):
    result = dal.MonteCarlo_ValueWithSettings(product(point[4], history), dal.BSModelData_New(*point[:4]), 35,
        valuation=dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 10, 10)), simulation=simulation)
    return result["PV"], [result["d_" + name] for name in ["spot", "vol", "rate", "div", "K"]]


@pytest.mark.parametrize("compiled", [False, True])
def test_retrained_preserves_the_declared_gradient_estimator_at_outer_points(compiled):
    result = dal.BlackScholesLsmc_Get_Curvature(plan(compiled, "RetrainedBump", history=True), POINT, 35, bumps())
    price, gradient = existing_gradient(POINT, result.simulation, True)
    assert result.value == pytest.approx(price, abs=1e-11)
    assert result.gradient == pytest.approx(gradient, abs=1e-10)
    assert result.value == pytest.approx(frozen_price(result, POINT, normals(result)), abs=1e-10)
    for row, step, actual in zip(ROWS, STEPS, result.hessian_products.to_rows()):
        plus = existing_gradient([x + step * d for x, d in zip(POINT, row)], result.simulation, True)[1]
        minus = existing_gradient([x - step * d for x, d in zip(POINT, row)], result.simulation, True)[1]
        assert actual == pytest.approx([(p - m) / (2 * step) for p, m in zip(plus, minus)], abs=1e-8)
    assert result.execution.method == "BumpOverRetrainedNativeLsmcPolicySecant"
    assert result.simulation.lsmc_policy_bump_relative == 1e-3


@pytest.mark.parametrize("configuration", [
    dict(use_bb=True, normal_precision="Precise", lsmc_validation_paths=64),
    dict(lsmc_rqmc_replicates=2, lsmc_training_seed=7, lsmc_pricing_seed=9),
])
def test_sampling_settings_counts_and_base_estimator(configuration):
    result = dal.BlackScholesLsmc_Get_Curvature(plan(**configuration), POINT, 35, bumps([], []))
    price, gradient = existing_gradient(POINT, result.simulation, False)
    assert result.value == pytest.approx(price, abs=1e-11)
    assert result.gradient == pytest.approx(gradient, abs=1e-10)
    assert result.execution.training_paths == 128
    assert result.execution.validation_paths == configuration.get("lsmc_validation_paths", 0)
    assert result.execution.pricing_replicates == configuration.get("lsmc_rqmc_replicates", 1)
    assert result.execution.paths_per_replicate == 35
    assert result.execution.numeric_payload_bytes == 88
    assert result.execution.max_batch_tape_bytes > 0
    assert result.execution.max_batch_cleanup_reserve_bytes > 0


def test_all_nested_result_and_policy_getters_are_detached_and_survive_gc():
    original = plan()
    result = dal.BlackScholesLsmc_Get_Curvature(original, POINT, 35, bumps())
    saved = result.hessian_products.to_rows()
    coefficients = result.base_policy[0].coefficients
    policy_saved = coefficients.copy()
    coefficients.clear()
    result.base_policy.clear()
    result.base_policy[0].normalization_means.clear()
    result.base_policy[0].normalization_sigmas.clear()
    result.base_policy[0].basis_powers.clear()
    result.simulation.lsmc_policy_risk_mode = "RetrainedBump"
    original.simulation.enable_aad = False
    original.contract_events.clear()
    original.valuation.evaluation_date = dal.Date_(2040, 1, 1)
    result.gradient.clear()
    result.point.clear()
    result.steps.clear()
    result.directions[0, 0] = 0
    result.hessian_products[0, 0] = 0
    with pytest.raises(AttributeError):
        result.base_policy[0].basis_degree = 1
    del original
    gc.collect()
    for value in [result, copy.copy(result), copy.deepcopy(result)]:
        assert value.point == POINT
        assert value.hessian_products.to_rows() == saved
        assert value.base_policy[0].coefficients == policy_saved
        assert value.simulation.lsmc_policy_risk_mode == "Frozen"
        assert value.plan.valuation.evaluation_date == dal.Date_(2026, 10, 10)
        assert value.plan.simulation.enable_aad
    for policy in [copy.copy(result.base_policy[0]), copy.deepcopy(result.base_policy[0])]:
        assert policy.coefficients == policy_saved
        assert policy.sigma > 0
        assert policy.effective_rank >= 0
        assert isinstance(policy.solver, str)
        assert isinstance(policy.fallback_reason, str)
        assert isinstance(policy.degenerate_reason, str)
        assert isinstance(policy.condition_path_count, int)


@pytest.mark.parametrize("point", [None, "point", iter(POINT), [True] * 5, [IntegerEnum.ONE] * 5,
    ["1"] * 5, [float("nan")] * 5, [float("inf")] * 5, [2 ** 10000] * 5, [], [100, -0.1, 0.05, 0, 50]])
def test_strict_point_conversion_and_native_preflight(point):
    with pytest.raises((TypeError, RuntimeError)):
        dal.BlackScholesLsmc_Get_Curvature(plan(), point, 1, bumps([], []))


@pytest.mark.parametrize("paths", [0, -1, True, 1.2, IntegerEnum.ONE, 2 ** 64])
def test_strict_path_count(paths):
    with pytest.raises((TypeError, RuntimeError), match="num_path"):
        dal.BlackScholesLsmc_Get_Curvature(plan(), POINT, paths, bumps([], []))


@pytest.mark.parametrize("field,value", [("plan", None), ("plan", object()), ("bumps", None), ("bumps", {})])
def test_typed_owning_request_inputs(field, value):
    args = dict(plan=plan(), point=POINT, num_path=1, bumps=bumps([], []))
    args[field] = value
    with pytest.raises(TypeError, match=field):
        dal.BlackScholesLsmc_Get_Curvature(**args)


def test_exact_short_zero_budgets_domain_crossing_and_recovery():
    prepared = plan()
    for rows, steps, exact in [([], [], 88), ([ROWS[0]], [0.1], 176), (ROWS, STEPS, 264)]:
        result = dal.BlackScholesLsmc_Get_Curvature(prepared, POINT, 35, bumps(rows, steps, numeric_payload_budget_bytes=exact))
        assert result.execution.numeric_payload_bytes == exact
        for cap in [0, exact - 1]:
            with pytest.raises(RuntimeError):
                dal.BlackScholesLsmc_Get_Curvature(prepared, POINT, 35, bumps(rows, steps, numeric_payload_budget_bytes=cap))
    with pytest.raises(RuntimeError, match="capacity|budget"):
        dal.BlackScholesLsmc_Get_Curvature(prepared, POINT, 35, bumps([], [], recording_capacity_budget_bytes=0))
    with pytest.raises(RuntimeError, match="direction=0; minus"):
        dal.BlackScholesLsmc_Get_Curvature(prepared, POINT, 35, bumps([[0, 1, 0, 0, 0]], [0.3]))
    assert math.isfinite(dal.BlackScholesLsmc_Get_Curvature(prepared, POINT, 35, bumps([], [])).value)


def test_closed_factory_rejects_wrong_types_disabled_aad_and_no_live_exercise():
    with pytest.raises(TypeError):
        dal.BlackScholesLsmcPlan_New(None)
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 10, 10))
    with pytest.raises(RuntimeError, match="AAD"):
        dal.BlackScholesLsmcPlan_New(product(), valuation=valuation, simulation=dal.MonteCarloSettings_())
    ordinary = dal.Product_New([dal.Date_(2027, 10, 10)], ["pay PAYS spot()"])
    with pytest.raises(RuntimeError, match="EXERCISE"):
        dal.BlackScholesLsmcPlan_New(ordinary, valuation=valuation)
    with pytest.raises(TypeError):
        dal.BlackScholesLsmcPlan_New(product(), valuation)
    with pytest.raises(TypeError):
        dal.BlackScholesLsmcPlan_()


def test_frozen_observations_and_global_date_cannot_change_current_constant_history():
    history = dal.Date_(2026, 10, 9)
    snapshot = dal.MarketFixingSnapshot_New({"EQ[LSMC_PYTHON_HISTORY]": {dal.DateTime_(history, 0): 80.0}})
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 10, 10), fixings=snapshot)
    contract = dal.Product_New(["K", history, dal.Date_(2027, 4, 10), dal.Date_(2027, 10, 10)],
        ["45", "x = K * FIX(EQ[LSMC_PYTHON_HISTORY]) / 40", "EXERCISE x - spot()", "EXERCISE x - spot()"],
        settings=dal.ScriptProductSettings_(default_index="EQ[LSMC_PYTHON_HISTORY]"))
    simulation = plan().simulation
    prepared = dal.BlackScholesLsmcPlan_New(contract, valuation=valuation, simulation=simulation)
    valuation.fixings = dal.MarketFixingSnapshot_New({"EQ[LSMC_PYTHON_HISTORY]": {dal.DateTime_(history, 0): 90.0}})
    dal.EvaluationDate_Set(dal.Date_(2040, 1, 1))
    result = dal.BlackScholesLsmc_Get_Curvature(prepared, POINT, 35, bumps())
    assert result.value == pytest.approx(frozen_price(result, POINT, normals(result)), abs=1e-10)
    assert result.plan.observations[0].value == 80.0
    assert result.plan.script_constants == [45.0]
    assert result.plan.valuation.evaluation_date == dal.Date_(2026, 10, 10)
    assert result.plan.contract_settings.default_index == "EQ[LSMC_PYTHON_HISTORY]"


def test_gil_release_and_independent_calling_threads():
    prepared = plan()
    request = bumps()
    operation = lambda: dal.BlackScholesLsmc_Get_Curvature(prepared, POINT, 35, request)
    _run_with_quote_risk_gil_heartbeat(operation, barrier=lambda _: None)
    expected = operation().hessian_products.to_rows()
    with ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(lambda _: operation(), range(2)))
    for result in results:
        assert result.hessian_products.to_rows() == expected


def test_closed_owning_lsmc_plan_and_empty_directions():
    product = dal.Product_New([dal.Date_(2027, 4, 10), dal.Date_(2027, 10, 10)],
                              ["EXERCISE 100 - spot()", "EXERCISE 100 - spot()"])
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 10, 10))
    simulation = dal.MonteCarloSettings_(enable_aad=True, compiled=True, smooth=2.0, lsmc_training_paths=64)
    plan = dal.BlackScholesLsmcPlan_New(product, valuation=valuation, simulation=simulation)
    bumps = dal.BumpOverAADRequest_(directions=dal.DoubleMatrix_(0, 4), steps=[])
    result = dal.BlackScholesLsmc_Get_Curvature(plan, [100.0, 0.2, 0.05, 0.0], 35, bumps)
    assert result.parameter_labels == ["spot", "vol", "rate", "div"]
    assert len(result.gradient) == 4
    assert len(result.base_policy) == 2
    assert result.hessian_products.Rows() == 0
    assert result.hessian_products.Cols() == 4
    assert result.execution.gradient_evaluations == 1
