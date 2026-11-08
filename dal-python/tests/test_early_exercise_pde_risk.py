import copy
from concurrent.futures import ThreadPoolExecutor

import pytest
import dal


PREFIX = "InvalidEarlyExercisePdeRiskRequest"


@pytest.fixture
def native():
    import importlib
    import os
    if os.environ.get("DAL_PYTHON_BUILD_PACKAGE"):
        return importlib.import_module("_dal_pde_risk_test")
    return pytest.importorskip("_dal_pde_risk_test")


def valuation(*, spot=100., volatility=.2, **settings):
    today, maturity = dal.Date_(2026, 10, 18), dal.Date_(2027, 10, 18)
    contract = dal.EarlyExercisePdeContract_(today, maturity, 100., "PUT", "BERMUDAN",
                                           exercise_dates=[maturity])
    market = dal.EarlyExercisePdeMarket_(spot, volatility,
        [dal.EarlyExercisePdeCarrySegment_(maturity, .05, .03)])
    config = dict(grid_points=41, upper=400., ordinary_steps=2, rannacher_intervals=0)
    config.update(settings)
    return dal.EarlyExercisePdeRequest_(contract, market, settings=dal.EarlyExercisePdeSettings_(**config))


def test_owning_risk_projection():
    request = dal.EarlyExercisePdeRiskRequest_(valuation())
    result = dal.EarlyExercisePdeRiskResult_New(request)
    assert result.price_available
    assert len(result.scenarios) == 17
    assert [r.risk_id for r in result.risks] == ["Delta", "Gamma", "Vega", "Rate[0]", "DividendYield[0]"]
    assert result.method == "BlackScholesEarlyExerciseFullRepriceFixedPlan"
    assert result.risk_basis == "NATIVE_MARKET_PARAMETER_PARTIAL"
    assert result.aad_used is False
    assert result.schema_version == result.plan.schema_version == 1
    assert result.plan.plan_id.startswith("p3v1:sha256:")
    assert result.request.risk.spot_bump is None
    assert result.plan.resolved_risk.spot_bump == 20.
    assert result.execution.work_units == 1326


@pytest.mark.parametrize("key", ["delta", "gamma", "vega", "carry_risk"])
@pytest.mark.parametrize("bad", [0, 1, "True", None, dal.OptionType_.PUT])
def test_flags_are_actual_bool(key, bad):
    with pytest.raises(TypeError, match=PREFIX + ".*" + key):
        dal.EarlyExercisePdeRiskSettings_(**{key: bad})


@pytest.mark.parametrize("key", ["spot_bump", "volatility_bump",
    "delta_stability_tolerance", "gamma_stability_tolerance", "vega_stability_tolerance",
    "rate_stability_tolerance", "dividend_yield_stability_tolerance"])
@pytest.mark.parametrize("bad", [True, "0.1", dal.OptionType_.PUT])
def test_reals_do_not_coerce(key, bad):
    with pytest.raises(TypeError, match=PREFIX + ".*" + key):
        dal.EarlyExercisePdeRiskSettings_(**{key: bad})


@pytest.mark.parametrize("key", ["volatility_bump", "delta_stability_tolerance",
    "gamma_stability_tolerance", "vega_stability_tolerance",
    "rate_stability_tolerance", "dividend_yield_stability_tolerance"])
@pytest.mark.parametrize("bad", [0., -1., float("nan"), float("inf"), 10**1000])
def test_real_bounds(key, bad):
    with pytest.raises(ValueError, match=PREFIX + ".*" + key):
        dal.EarlyExercisePdeRiskSettings_(**{key: bad})


@pytest.mark.parametrize("key", ["max_scenarios", "max_work_units",
    "numeric_payload_budget_bytes", "workspace_budget_bytes"])
@pytest.mark.parametrize("bad", [True, 1., "129", None, dal.OptionType_.PUT])
def test_counts_do_not_coerce(key, bad):
    with pytest.raises(TypeError, match=PREFIX + ".*" + key):
        dal.EarlyExercisePdeRiskSettings_(**{key: bad})


@pytest.mark.parametrize("key", ["max_scenarios", "max_work_units",
    "numeric_payload_budget_bytes", "workspace_budget_bytes"])
@pytest.mark.parametrize("bad", [-1, 2**100])
def test_count_range(key, bad):
    with pytest.raises(ValueError, match=PREFIX + ".*" + key):
        dal.EarlyExercisePdeRiskSettings_(**{key: bad})


def test_sequences_paths_keywords_and_typed_arguments():
    for field in ["rate_bumps", "dividend_yield_bumps"]:
        for bad in [None, "1e-4", b"1e-4", 1e-4, [1e-4, True], [[1e-4]]]:
            with pytest.raises(TypeError, match=PREFIX + ".*" + field):
                dal.EarlyExercisePdeRiskSettings_(**{field: bad})
        for bad in [[1e-4, 0.], [1e-4, float("nan")]]:
            with pytest.raises(ValueError, match=PREFIX + r".*" + field + r"\[1\]"):
                dal.EarlyExercisePdeRiskSettings_(**{field: bad})
    with pytest.raises(TypeError, match=PREFIX + ".*settings"):
        dal.EarlyExercisePdeRiskSettings_(True)
    with pytest.raises(TypeError, match=PREFIX + ".*Delta"):
        dal.EarlyExercisePdeRiskSettings_(Delta=True)
    for bad in [None, {}, valuation().contract, "request"]:
        with pytest.raises(TypeError, match=PREFIX + ".*valuation"):
            dal.EarlyExercisePdeRiskRequest_(bad)
    with pytest.raises(TypeError, match=PREFIX + ".*risk"):
        dal.EarlyExercisePdeRiskRequest_(valuation(), risk={})
    with pytest.raises(TypeError, match=PREFIX):
        dal.EarlyExercisePdeRiskRequest_(valuation(), dal.EarlyExercisePdeRiskSettings_())
    with pytest.raises(TypeError, match=PREFIX + ".*request"):
        dal.EarlyExercisePdeRiskResult_New(valuation())
    with pytest.raises(TypeError, match=PREFIX + ".*other"):
        dal.EarlyExercisePdeRiskResult_New(other=valuation())
    with pytest.raises(ValueError, match=PREFIX + ".*switches"):
        dal.EarlyExercisePdeRiskSettings_(delta=False, gamma=False, vega=False, carry_risk=False)
    with pytest.raises(ValueError, match=PREFIX + ".*rate_bumps.length"):
        dal.EarlyExercisePdeRiskRequest_(valuation(),
            risk=dal.EarlyExercisePdeRiskSettings_(rate_bumps=[1e-4, 1e-4]))
    with pytest.raises(ValueError, match=PREFIX + ".*spot_bump"):
        dal.EarlyExercisePdeRiskRequest_(valuation(), risk=dal.EarlyExercisePdeRiskSettings_(spot_bump=.1))


def test_scalar_getter_keyword_errors_keep_identifier():
    result = dal.EarlyExercisePdeRiskResult_New(dal.EarlyExercisePdeRiskRequest_(valuation()))
    for args, kwargs in [((), {}), ((), {"other": "Delta"}), (("Delta",), {"risk_id": "Delta"})]:
        with pytest.raises(TypeError, match=PREFIX):
            result.risk_value(*args, **kwargs)


def test_readonly_detached_copies_and_no_recomputation():
    bumps = [1e-4]
    settings = dal.EarlyExercisePdeRiskSettings_(rate_bumps=bumps)
    request = dal.EarlyExercisePdeRiskRequest_(valuation(), risk=settings)
    result = dal.EarlyExercisePdeRiskResult_New(request)
    bumps[0] = .9
    original = (result.price, result.execution.work_units, result.execution.total_solves,
                result.execution.numeric_payload_bytes, result.plan.plan_id)
    for copier in [copy.copy, copy.deepcopy]:
        cloned = copier(result)
        for _ in range(3):
            cloned.risks.clear()
            cloned.scenarios.pop()
            cloned.risks[1].fine_stencil.clear()
            cloned.plan.grid[0] = -1
            cloned.plan.steps.clear()
            cloned.plan.rate_bump_origins.clear()
            cloned.request.risk.rate_bumps[0] = 1.
            cloned.base_result.values[0] = -100.
        assert len(cloned.risks) == 5 and len(cloned.scenarios) == 17
        assert len(cloned.risks[1].fine_stencil) == 3
        assert cloned.plan.grid[0] == 0
        assert cloned.request.risk.rate_bumps == [1e-4]
        assert original == (cloned.price, cloned.execution.work_units, cloned.execution.total_solves,
                            cloned.execution.numeric_payload_bytes, cloned.plan.plan_id)
    del request, settings, result
    assert cloned.request.valuation.market.spot == 100.
    for obj, field, value in [(cloned, "price", 0), (cloned.plan, "grid", []),
        (cloned.risks[0], "value", 0), (cloned.execution, "work_units", 0),
        (cloned.request.risk, "delta", False), (cloned.scenarios[0], "price", 0)]:
        with pytest.raises(AttributeError):
            setattr(obj, field, value)
    for type_name in ["Record", "Scenario", "Plan", "PlanStep", "StencilEntry", "Execution", "Result"]:
        with pytest.raises(TypeError):
            getattr(dal, "EarlyExercisePdeRisk" + type_name + "_")()


@pytest.mark.parametrize("cap,status,work", [(1326, "PARTIAL", 1326),
    (1325, "WORK_LIMIT", 1287), (390, "WORK_LIMIT", 390),
    (78, "WORK_LIMIT", 78), (77, "BASE_FAILED", 39)])
def test_exact_cumulative_work_and_state_projection(cap, status, work):
    request = dal.EarlyExercisePdeRiskRequest_(valuation(),
        risk=dal.EarlyExercisePdeRiskSettings_(max_work_units=cap))
    result = dal.EarlyExercisePdeRiskResult_New(request)
    assert result.status == status
    assert result.execution.work_units == work
    assert sum(s.work_units for s in result.scenarios) == work
    assert result.execution.attempted_scenarios == result.execution.successful_scenarios + result.execution.failed_scenarios
    assert result.execution.planned_scenarios == result.execution.attempted_scenarios + result.execution.not_run_scenarios
    assert all(s.status != "PLANNED" for s in result.scenarios)
    for scenario in result.scenarios:
        if scenario.status.startswith("NOT_RUN"):
            assert scenario.work_units == scenario.total_solves == scenario.actual_steps == 0
            assert scenario.price is scenario.failure is scenario.solver_status is None
    if cap == 77:
        assert result.price_available is False
        with pytest.raises(RuntimeError, match="EarlyExercisePdeRiskPriceUnavailable"):
            _ = result.price
        assert result.base_result.failure.status == "WORK_LIMIT"
    else:
        assert result.price_available
    for row in result.risks:
        if row.value is None:
            with pytest.raises(RuntimeError, match="EarlyExercisePdeRiskValueUnavailable.*" + row.risk_id.replace("[", r"\[").replace("]", r"\]")):
                result.risk_value(row.risk_id)
        else:
            assert result.risk_value(row.risk_id) == row.value
    with pytest.raises(RuntimeError, match="EarlyExercisePdeRiskUnknownRiskId"):
        result.risk_value("delta")


def test_complete_default_profile_and_parallel_isolation():
    requests = [dal.EarlyExercisePdeRiskRequest_(valuation(spot=s, grid_points=401, ordinary_steps=500))
                for s in [100., 105.]]
    serial = [dal.EarlyExercisePdeRiskResult_New(r) for r in requests]
    assert all(r.is_complete and r.status == "COMPLETE" for r in serial)
    with ThreadPoolExecutor(max_workers=2) as pool:
        parallel = list(pool.map(dal.EarlyExercisePdeRiskResult_New, requests))
    for a, b in zip(serial, parallel):
        assert (a.price, [r.value for r in a.risks], a.plan.plan_id) == (b.price, [r.value for r in b.risks], b.plan.plan_id)


def test_preflight_boundaries_failure_recovery():
    base = valuation()
    result = dal.EarlyExercisePdeRiskResult_New(dal.EarlyExercisePdeRiskRequest_(base))
    for field, exact in [("max_scenarios", 17),
        ("numeric_payload_budget_bytes", result.execution.planned_numeric_payload_bytes),
        ("workspace_budget_bytes", result.execution.planned_workspace_bytes)]:
        good = dal.EarlyExercisePdeRiskRequest_(base, risk=dal.EarlyExercisePdeRiskSettings_(**{field: exact}))
        assert dal.EarlyExercisePdeRiskResult_New(good).price == result.price
        for _ in range(2):
            bad = dal.EarlyExercisePdeRiskRequest_(base, risk=dal.EarlyExercisePdeRiskSettings_(**{field: exact - 1}))
            with pytest.raises(RuntimeError, match="EarlyExercisePdeRiskBudgetExceeded"):
                dal.EarlyExercisePdeRiskResult_New(bad)
            assert dal.EarlyExercisePdeRiskResult_New(good).price == result.price
    assert result.execution.numeric_payload_bytes <= result.execution.planned_numeric_payload_bytes
    assert result.execution.peak_workspace_bytes <= result.execution.planned_workspace_bytes
    assert result.execution.peak_native_bytes <= result.execution.planned_peak_native_bytes


def test_barrier_coordinated_heartbeat_during_native_call():
    import sys
    import threading

    request = dal.EarlyExercisePdeRiskRequest_(valuation(grid_points=801, ordinary_steps=2000))
    barrier = threading.Barrier(2)
    stop = threading.Event()
    ticks = [0]
    def heartbeat():
        barrier.wait()
        while not stop.is_set():
            ticks[0] += 1
            stop.wait(.001)
    previous = sys.getswitchinterval()
    thread = threading.Thread(target=heartbeat)
    try:
        sys.setswitchinterval(60.)
        thread.start()
        barrier.wait()
        before = ticks[0]
        result = dal.EarlyExercisePdeRiskResult_New(request)
        after = ticks[0]
        assert result.price_available
        assert after > before
    finally:
        stop.set()
        thread.join()
        sys.setswitchinterval(previous)


FIELDS = {
    "EarlyExercisePdeRiskStencilEntry_": "scenario_index weight".split(),
    "EarlyExercisePdeRiskRecord_": "risk_index risk_id parameter_id derivative_order segment_index start_date end_date parameter_value unit value status reason method interpretation fine_bump coarse_bump bump_origin fine_estimate coarse_estimate step_difference normalized_step_difference stability_tolerance fine_stencil coarse_stencil".split(),
    "EarlyExercisePdeRiskPlanStep_": "source_time_index time_index dt theta carry_index damped projection".split(),
    "EarlyExercisePdeRiskPlan_": "schema_version plan_id evaluation_date maturity_date upper grid_points grid_spacing grid time_grid steps epsilon theta effective_rannacher_intervals penalty_factor max_iterations_per_step linear_backward_error_limit interpolation_policy resolved_risk spot_bump_origin volatility_bump_origin rate_bump_origins dividend_yield_bump_origins planned_scenarios base_planned_steps planned_steps planned_numeric_payload_bytes planned_workspace_bytes planned_peak_native_bytes".split(),
    "EarlyExercisePdeRiskScenario_": "scenario_index scenario_id plan_id parameter_id segment_index parameter_value offset status solver_status work_limit_source price_available price epsilon planned_steps actual_steps completed_steps total_solves work_units numeric_payload_bytes peak_workspace_bytes max_iteration_change max_linear_backward_error max_obstacle_violation max_dual_violation max_complementarity_residual failure evaluation_membership evaluation_boundary_intersects_spot_stencil future_contact_changed upwind_changed".split(),
    "EarlyExercisePdeRiskExecution_": "planned_scenarios attempted_scenarios successful_scenarios failed_scenarios not_run_scenarios total_solves work_units planned_numeric_payload_bytes planned_workspace_bytes planned_peak_native_bytes numeric_payload_bytes peak_workspace_bytes peak_native_bytes work_limit_scenario_index work_limit_source".split(),
    "EarlyExercisePdeRiskSettings_": "delta gamma vega carry_risk spot_bump volatility_bump rate_bumps dividend_yield_bumps delta_stability_tolerance gamma_stability_tolerance vega_stability_tolerance rate_stability_tolerance dividend_yield_stability_tolerance max_scenarios max_work_units numeric_payload_budget_bytes workspace_budget_bytes".split(),
    "EarlyExercisePdeTimeNode_": "time_years calendar_date is_exercise_date is_carry_knot is_maturity".split(),
    "EarlyExercisePdeFailure_": "status time_index iteration calendar_date node residual threshold work_units reason".split(),
}


def snapshot(value):
    fields = FIELDS.get(type(value).__name__)
    if fields is not None:
        return {field: snapshot(getattr(value, field)) for field in fields}
    if isinstance(value, list):
        return [snapshot(item) for item in value]
    return value


@pytest.mark.parametrize("cap,config", [(1326, {}), (1325, {}), (78, {}), (77, {}),
    (100000000, {"grid_points": 401, "ordinary_steps": 500})])
def test_every_python_field_matches_direct_core_on_same_library(cap, config, native):
    request = dal.EarlyExercisePdeRiskRequest_(valuation(**config),
        risk=dal.EarlyExercisePdeRiskSettings_(max_work_units=cap))
    result = dal.EarlyExercisePdeRiskResult_New(request)
    expected = native._test_early_exercise_risk_core(request)
    actual = {field: snapshot(getattr(result, field)) for field in
              ["status", "plan", "risks", "scenarios", "execution"]}
    actual["price"] = result.price if result.price_available else None
    assert actual == expected


def test_python_binding_preserves_recording_and_finished_native_tape(native):
    good = dal.EarlyExercisePdeRiskRequest_(valuation())
    budget = dal.EarlyExercisePdeRiskRequest_(valuation(),
        risk=dal.EarlyExercisePdeRiskSettings_(max_scenarios=0))
    work = dal.EarlyExercisePdeRiskRequest_(valuation(),
        risk=dal.EarlyExercisePdeRiskSettings_(max_work_units=77))
    def evaluate():
        with pytest.raises(ValueError, match=PREFIX):
            dal.EarlyExercisePdeRiskSettings_(volatility_bump=0)
        with pytest.raises(RuntimeError, match="EarlyExercisePdeRiskBudgetExceeded"):
            dal.EarlyExercisePdeRiskResult_New(budget)
        assert dal.EarlyExercisePdeRiskResult_New(work).status == "BASE_FAILED"
        assert dal.EarlyExercisePdeRiskResult_New(good).price_available
    native._test_early_exercise_risk_tape(evaluate)


def test_two_segment_and_shared_spot_scenario_caps():
    base = valuation()
    maturity = base.contract.maturity_date
    market = dal.EarlyExercisePdeMarket_(100., .2, [
        dal.EarlyExercisePdeCarrySegment_(dal.Date_(2027, 3, 4), .05, .03),
        dal.EarlyExercisePdeCarrySegment_(maturity, .06, .02)])
    two = dal.EarlyExercisePdeRequest_(base.contract, market, settings=base.settings)
    for price, flags, exact in [(two, {}, 25),
        (base, {"vega": False, "carry_risk": False}, 5)]:
        settings = dal.EarlyExercisePdeRiskSettings_(**flags, max_scenarios=exact)
        result = dal.EarlyExercisePdeRiskResult_New(dal.EarlyExercisePdeRiskRequest_(price, risk=settings))
        assert len(result.scenarios) == exact
        if exact == 25:
            assert [r.risk_id for r in result.risks][-2:] == ["Rate[1]", "DividendYield[1]"]
            assert result.risks[5].start_date == dal.Date_(2027, 3, 4)
        settings = dal.EarlyExercisePdeRiskSettings_(**flags, max_scenarios=exact - 1)
        with pytest.raises(RuntimeError, match="EarlyExercisePdeRiskBudgetExceeded"):
            dal.EarlyExercisePdeRiskResult_New(dal.EarlyExercisePdeRiskRequest_(price, risk=settings))


def test_zero_solve_and_real_numerical_failure_recovery():
    base = valuation()
    maturity, today = base.contract.maturity_date, base.contract.evaluation_date
    contract = dal.EarlyExercisePdeContract_(today, maturity, 100., "PUT", "AMERICAN")
    market = dal.EarlyExercisePdeMarket_(80., 0.,
        [dal.EarlyExercisePdeCarrySegment_(maturity, 0., 0.)])
    zero = dal.EarlyExercisePdeRequest_(contract, market,
        settings=dal.EarlyExercisePdeSettings_(grid_points=41, upper=400.,
            ordinary_steps=2, rannacher_intervals=0, max_work_units=0))
    result = dal.EarlyExercisePdeRiskResult_New(dal.EarlyExercisePdeRiskRequest_(zero,
        risk=dal.EarlyExercisePdeRiskSettings_(gamma=False, vega=False, carry_risk=False,
                                             max_work_units=0, max_scenarios=1)))
    assert result.status == "PARTIAL" and result.price == 20.
    assert result.execution.work_units == result.execution.total_solves == 0
    assert result.risks[0].status == "UNAVAILABLE_DEGENERATE_DIFFUSION"
    market = dal.EarlyExercisePdeMarket_(80., 0., [
        dal.EarlyExercisePdeCarrySegment_(dal.Date_(2027, 3, 4), .1, .1),
        dal.EarlyExercisePdeCarrySegment_(maturity, -.01, -.01)])
    for penalty, expected in [(1., "BASE_FAILED"), (1e8, "PARTIAL")]:
        price = dal.EarlyExercisePdeRequest_(contract, market,
            settings=dal.EarlyExercisePdeSettings_(grid_points=41, upper=400.,
                ordinary_steps=500, penalty_factor=penalty))
        result = dal.EarlyExercisePdeRiskResult_New(dal.EarlyExercisePdeRiskRequest_(price))
        assert result.status == expected
        if penalty == 1.:
            assert result.base_result.failure.status == "LCP_RESIDUAL_LIMIT"
            assert result.scenarios[0].failure.status == "LCP_RESIDUAL_LIMIT"
            assert all(row.status == "UNAVAILABLE_BASE_FAILED" for row in result.risks)
