import copy
import math
from concurrent.futures import ThreadPoolExecutor

import pytest

import dal


def request(style="BERMUDAN", **settings):
    today = dal.Date_(2026, 10, 18)
    mid = dal.Date_(2027, 3, 4)
    maturity = dal.Date_(2027, 10, 18)
    contract = dal.EarlyExercisePdeContract_(today, maturity, 100.0, "PUT", style,
        exercise_dates=[mid, maturity] if style == "BERMUDAN" else [])
    market = dal.EarlyExercisePdeMarket_(80.0, 0.0,
        [dal.EarlyExercisePdeCarrySegment_(maturity, 0.1, 0.1)])
    return dal.EarlyExercisePdeRequest_(contract, market,
        settings=dal.EarlyExercisePdeSettings_(grid_points=41, ordinary_steps=6000, **settings))


def test_exact_date_independent_price_and_detached_owning_results():
    r = request()
    result = dal.EarlyExercisePdeResult_New(r)
    assert result.is_success
    assert result.status == "SUCCESS"
    assert abs(result.price - 20 * math.exp(-0.1 * 137 / 365)) < 1e-4
    assert result.method == "BlackScholesBermudanThetaProjection"
    assert len(result.exercise_levels) == 2
    level = result.exercise_levels[0]
    assert result.time_grid[level.time_index].time_years == 137 / 365
    assert level.kind == "BERMUDAN_PROJECTION"
    assert result.execution.max_complementarity_residual is None
    grid = result.grid
    grid[0] = -100
    assert result.grid[0] == 0
    dates = result.request.contract.exercise_dates
    dates.clear()
    del r
    assert len(copy.deepcopy(result).request.contract.exercise_dates) == 2
    assert not hasattr(result, "jacobian")
    with pytest.raises(AttributeError):
        result.request.market.spot = 1.0


def test_failure_has_diagnostics_without_price_or_values():
    def failure_request(**settings):
        base = request("AMERICAN", **settings)
        mid, maturity = dal.Date_(2027, 3, 4), base.contract.maturity_date
        market = dal.EarlyExercisePdeMarket_(80.0, 0.0, [
            dal.EarlyExercisePdeCarrySegment_(mid, 0.1, 0.1),
            dal.EarlyExercisePdeCarrySegment_(maturity, -0.01, -0.01)])
        return dal.EarlyExercisePdeRequest_(base.contract, market, settings=base.settings)

    result = dal.EarlyExercisePdeResult_New(failure_request(penalty_factor=1.0))
    assert not result.is_success
    assert result.status == "LCP_RESIDUAL_LIMIT"
    assert result.failure.status == result.status
    assert result.failure.residual > result.failure.threshold
    with pytest.raises(RuntimeError, match="LCP_RESIDUAL_LIMIT"):
        _ = result.price
    with pytest.raises(RuntimeError, match="LCP_RESIDUAL_LIMIT"):
        _ = result.values
    work = dal.EarlyExercisePdeResult_New(failure_request(max_work_units=0))
    assert work.status == "WORK_LIMIT"
    assert work.execution.work_units == 0


@pytest.mark.parametrize("bad", [True, "401", 4.5, 2**70])
def test_strict_integer_input(bad):
    with pytest.raises((TypeError, RuntimeError, OverflowError)):
        dal.EarlyExercisePdeSettings_(grid_points=bad)


@pytest.mark.parametrize("bad", [True, "0.2", float("inf"), float("nan")])
def test_strict_real_input(bad):
    with pytest.raises((TypeError, RuntimeError)):
        dal.EarlyExercisePdeSettings_(theta=bad)


def test_dates_enums_defaults_and_parallel_isolation():
    r = request()
    with pytest.raises(TypeError):
        dal.EarlyExercisePdeCarrySegment_("2027-10-18", 0.1, 0.1)
    with pytest.raises(TypeError):
        dal.EarlyExercisePdeContract_(r.contract.evaluation_date,
            r.contract.maturity_date, 100.0, 1, "AMERICAN")
    settings = dal.EarlyExercisePdeSettings_()
    assert settings.theta == 1.0
    assert settings.grid_points == 401
    assert settings.upper is None
    assert settings.numeric_payload_budget_bytes == 67108864
    enum_contract = dal.EarlyExercisePdeContract_(r.contract.evaluation_date,
        r.contract.maturity_date, 100.0, dal.OptionType_.PUT,
        dal.EarlyExerciseStyle_.AMERICAN)
    assert enum_contract.exercise_style == "AMERICAN"
    with ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(dal.EarlyExercisePdeResult_New, [r, request("AMERICAN")]))
    assert abs(results[0].price - 20 * math.exp(-0.1 * 137 / 365)) < 1e-4
    assert abs(results[1].price - 20) < 1e-6
