"""Complete fixed-grid financial risk through an owning Python boundary."""

import copy
from concurrent.futures import ThreadPoolExecutor
from enum import IntEnum
import gc
import math
import threading
import time

import dal
import pytest


POINT = [0.05, 0.20, 110.0]


def dense_solve(matrix, rhs):
    """Independent partial-pivot elimination, without DAL matrix operations."""
    size = len(matrix)
    for column in range(size):
        pivot = max(range(column, size), key=lambda row: abs(matrix[row][column]))
        matrix[column], matrix[pivot] = matrix[pivot], matrix[column]
        rhs[column], rhs[pivot] = rhs[pivot], rhs[column]
        for row in range(column + 1, size):
            factor = matrix[row][column] / matrix[column][column]
            for next_column in range(column + 1, size):
                matrix[row][next_column] -= factor * matrix[column][next_column]
            for layer in range(2):
                rhs[row][layer] -= factor * rhs[column][layer]
    solution = [[0.0, 0.0] for _ in range(size)]
    for row in reversed(range(size)):
        for layer in range(2):
            solution[row][layer] = (rhs[row][layer] - sum(
                matrix[row][column] * solution[column][layer]
                for column in range(row + 1, size))) / matrix[row][row]
    return solution


def independent_prices(point, settings, *, boundary_point=None, terminal_strike=None):
    rate, sigma, strike = point
    boundary_rate, _, boundary_strike = point if boundary_point is None else boundary_point
    terminal_strike = strike if terminal_strike is None else terminal_strike
    nodes, intervals = settings.grid_points, settings.ordinary_steps
    spacing = settings.upper / (nodes - 1)
    grid = [spacing * row for row in range(nodes)]
    state = [[max(spot - terminal_strike, 0.0), max(terminal_strike - spot, 0.0)] for spot in grid]
    for step in range(intervals + 2):
        damping = step < 4
        dt = settings.expiry / intervals * (0.5 if damping else 1.0)
        theta = 1.0 if damping else 0.5
        tau = settings.expiry / intervals * (0.5 * (step + 1) if damping else step - 1)
        matrix = [[float(row == column) for column in range(nodes)] for row in range(nodes)]
        rhs = [row.copy() for row in state]
        for row in range(1, nodes - 1):
            diffusion = sigma * sigma * grid[row] ** 2 / spacing ** 2
            drift = (rate - settings.dividend_yield) * grid[row] / spacing
            generator = [0.5 * (diffusion - drift), -diffusion - rate, 0.5 * (diffusion + drift)]
            for local, coefficient in enumerate(generator):
                column = row + local - 1
                matrix[row][column] -= dt * theta * coefficient
                for layer in range(2):
                    rhs[row][layer] += dt * (1 - theta) * coefficient * state[column][layer]
        discounted_strike = boundary_strike * math.exp(-boundary_rate * tau)
        rhs[0] = [0.0, discounted_strike]
        rhs[-1] = [settings.upper * math.exp(-settings.dividend_yield * tau) - discounted_strike, 0.0]
        state = dense_solve(matrix, rhs)
    index = settings.spot_index
    return state[(nodes - 1) // 4 if index is None else index]


def small_settings(**kwargs):
    return dal.EuropeanPdeSettings_(grid_points=9, ordinary_steps=8, **kwargs)


def test_owning_small_grid_matches_accepted_financial_prices_and_every_risk():
    result = dal.EuropeanPdeRiskResult_New(*POINT, settings=small_settings())
    assert result.prices == pytest.approx([4.153690693968586, 10.770781220697858], abs=1e-10)
    expected = [[40.40684028445804, 27.8766807705311, -0.0907395605282994],
                [-64.1496803210784, 27.87666960963085, 0.8605082862555484]]
    for actual, risk in zip(result.jacobian.to_rows(), expected):
        assert actual == pytest.approx(risk, abs=1e-9)
    assert result.point == POINT
    assert result.payoff_labels == ["Call", "Put"]
    assert result.parameter_labels == ["Rate", "Volatility", "Strike"]
    assert result.parameter_units == ["price per decimal rate", "price per decimal volatility", "price per strike price unit"]
    assert result.method == "NativeAADFixedGridEuropeanTheta"
    assert result.settings.spot_index == 2
    assert result.spot == 100.0
    assert result.grid == [50.0 * row for row in range(9)]
    assert result.execution.actual_steps == 10
    assert result.execution.numeric_payload_bytes == 8 * (17 + 9 + 6 * 10)
    assert result.execution.peak_tape_bytes > 0
    assert result.execution.cleanup_reserve_bytes > 0
    assert result.execution.reverse_scratch_peak_bytes > 0
    assert result.transpose_error_labels == ["CallLayer/CallSeed", "CallLayer/PutSeed", "PutLayer/CallSeed", "PutLayer/PutSeed"]
    assert len(result.forward_backward_errors.to_rows()) == 10
    assert len(result.transpose_backward_errors.to_rows()) == 10
    for row in result.forward_backward_errors.to_rows():
        assert len(row) == 2
        assert all(0 <= error <= 1e-12 for error in row)
    for row in result.transpose_backward_errors.to_rows():
        assert len(row) == 4
        assert all(0 <= error <= 1e-12 for error in row)


@pytest.mark.parametrize("point,configuration", [
    (POINT, dict(grid_points=9, ordinary_steps=8)),
    ([-0.02, 0.31, 83.0], dict(grid_points=10, ordinary_steps=2, upper=270.0, spot_index=3, expiry=0.7, dividend_yield=-0.01)),
    ([0.08, 0.17, 140.0], dict(grid_points=9, ordinary_steps=8, upper=360.0, spot_index=5, expiry=1.3, dividend_yield=0.04)),
])
def test_independent_complete_dense_program_and_three_bump_sizes(point, configuration):
    settings = dal.EuropeanPdeSettings_(**configuration)
    result = dal.EuropeanPdeRiskResult_New(*point, settings=settings)
    assert result.prices == pytest.approx(independent_prices(point, settings), abs=1e-10)
    risks = result.jacobian.to_rows()
    for coordinate, initial in enumerate([1e-3, 1e-3, 0.1]):
        for factor in [1.0, 0.5, 0.25]:
            step = initial * factor
            plus, minus = point.copy(), point.copy()
            plus[coordinate] += step
            minus[coordinate] -= step
            high, low = independent_prices(plus, settings), independent_prices(minus, settings)
            ceiling = [300.0, 1000.0, 0.0][coordinate] * step ** 2 + 1e-7
            for layer in range(2):
                assert risks[layer][coordinate] == pytest.approx((high[layer] - low[layer]) / (2 * step), abs=ceiling)


def test_boundary_and_terminal_contributions_are_in_the_published_risks():
    settings = small_settings(spot_index=7)
    result = dal.EuropeanPdeRiskResult_New(*POINT, settings=settings)
    for coordinate, step in [(0, 1e-5), (2, 1e-3)]:
        plus, minus = POINT.copy(), POINT.copy()
        plus[coordinate] += step
        minus[coordinate] -= step
        high = independent_prices(plus, settings, boundary_point=POINT)
        low = independent_prices(minus, settings, boundary_point=POINT)
        frozen = [(p - m) / (2 * step) for p, m in zip(high, low)]
        assert abs(result.jacobian.to_rows()[0][coordinate] - frozen[0]) > 0.1
    settings = small_settings()
    result = dal.EuropeanPdeRiskResult_New(*POINT, settings=settings)
    high = independent_prices([POINT[0], POINT[1], POINT[2] + 1e-3], settings, terminal_strike=POINT[2])
    low = independent_prices([POINT[0], POINT[1], POINT[2] - 1e-3], settings, terminal_strike=POINT[2])
    assert abs(result.jacobian.to_rows()[1][2] - (high[1] - low[1]) / 0.002) > 0.1


class IntegerEnum(IntEnum):
    ONE = 1


@pytest.mark.parametrize("bad", [True, "0.2", IntegerEnum.ONE, None, object()])
def test_strict_passive_real_parameters(bad):
    with pytest.raises(TypeError, match="volatility"):
        dal.EuropeanPdeRiskResult_New(0.05, bad, 110.0)


@pytest.mark.parametrize("coordinate,bad,field", [
    (0, math.nan, "rate"), (0, math.inf, "rate"), (1, 0.0, "volatility"),
    (1, -0.2, "volatility"), (1, math.inf, "volatility"),
    (2, 0.0, "strike"), (2, 400.0, "strike"), (2, 100.0, "grid nodes"),
])
def test_invalid_parameter_values_reject_and_recover(coordinate, bad, field):
    point = POINT.copy()
    point[coordinate] = bad
    with pytest.raises(RuntimeError, match=field):
        dal.EuropeanPdeRiskResult_New(*point, settings=small_settings())
    assert dal.EuropeanPdeRiskResult_New(*POINT, settings=small_settings()).prices[0] > 0


@pytest.mark.parametrize("configuration,field", [
    (dict(grid_points=4), "grid_points"), (dict(ordinary_steps=1), "ordinary_steps"),
    (dict(grid_points=10), "quarter-grid"), (dict(spot_index=0), "spot_index"),
    (dict(spot_index=61), "spot_index"), (dict(upper=0.0), "upper"),
    (dict(expiry=0.0), "expiry"), (dict(dividend_yield=math.nan), "dividend"),
    (dict(forward_backward_error_limit=-1.0), "forward"),
    (dict(transpose_backward_error_limit=math.inf), "transpose"),
    (dict(forward_backward_error_limit=2.0), "forward"),
    (dict(transpose_backward_error_limit=2.0), "transpose"),
])
def test_settings_admission(configuration, field):
    with pytest.raises(RuntimeError, match=field):
        dal.EuropeanPdeSettings_(**configuration)


def test_exact_result_budget_zero_short_and_wrong_types():
    settings = small_settings()
    size = 8 * (17 + 9 + 6 * 10)
    for limit in [0, size - 1]:
        with pytest.raises(RuntimeError, match="payload"):
            dal.EuropeanPdeRiskResult_New(*POINT, settings=settings, numeric_payload_budget_bytes=limit)
    result = dal.EuropeanPdeRiskResult_New(*POINT, settings=settings, numeric_payload_budget_bytes=size)
    assert result.numeric_payload_budget_bytes == size
    assert result.recording_capacity_budget_bytes is None
    with pytest.raises(RuntimeError, match="capacity"):
        dal.EuropeanPdeRiskResult_New(*POINT, settings=settings, recording_capacity_budget_bytes=0)
    for bad in [True, 1.5, "100", IntegerEnum.ONE]:
        with pytest.raises(TypeError):
            dal.EuropeanPdeRiskResult_New(*POINT, numeric_payload_budget_bytes=bad)
    with pytest.raises(TypeError):
        dal.EuropeanPdeRiskResult_New(*POINT, settings={})
    with pytest.raises(TypeError):
        dal.EuropeanPdeRiskResult_New(*POINT, settings)
    with pytest.raises(TypeError):
        dal.EuropeanPdeSettings_(9)
    with pytest.raises(TypeError):
        dal.EuropeanPdeSettings_(grid_points=True)


def test_representability_and_checked_integer_edges_recover():
    for configuration in [
        dict(grid_points=2**100), dict(ordinary_steps=2**31 - 1),
        dict(spot_index=2**100), dict(expiry=math.ulp(0.0)),
        dict(upper=math.ulp(0.0)), dict(dividend_yield=math.inf),
    ]:
        with pytest.raises(RuntimeError):
            dal.EuropeanPdeSettings_(**configuration)
    for budget in [-1, 2**100]:
        with pytest.raises(RuntimeError):
            dal.EuropeanPdeRiskResult_New(*POINT, numeric_payload_budget_bytes=budget)
    with pytest.raises(RuntimeError, match="rate"):
        dal.EuropeanPdeRiskResult_New(10**400, 0.2, 110.0)
    with pytest.raises(RuntimeError, match="discounted strike"):
        dal.EuropeanPdeRiskResult_New(-1000.0, 0.2, 110.0, settings=small_settings())
    with pytest.raises(RuntimeError, match="discounted upper"):
        dal.EuropeanPdeRiskResult_New(*POINT, settings=small_settings(dividend_yield=-1000.0))
    with pytest.raises(RuntimeError):
        dal.EuropeanPdeRiskResult_New(0.05, 1e300, 110.0, settings=small_settings())
    assert dal.EuropeanPdeRiskResult_New(*POINT, settings=small_settings()).prices[0] > 0


def test_detached_settings_arrays_reports_and_result_survive_gc():
    settings = small_settings()
    result = dal.EuropeanPdeRiskResult_New(*POINT, settings=settings)
    expected = result.jacobian.to_rows()
    result.prices[0] = -999.0
    result.point[0] = -999.0
    result.grid[0] = -999.0
    result.jacobian[0, 0] = -999.0
    result.forward_backward_errors[0, 0] = -999.0
    result.transpose_backward_errors[0, 0] = -999.0
    with pytest.raises(AttributeError):
        result.settings.expiry = 2.0
    del settings
    gc.collect()
    dal.EuropeanPdeRiskResult_New(0.07, 0.3, 120.0, settings=small_settings())
    for actual in [result, copy.copy(result), copy.deepcopy(result)]:
        assert actual.jacobian.to_rows() == expected
        assert actual.point == POINT
        assert actual.grid[0] == 0.0
        assert actual.forward_backward_errors.to_rows()[0][0] >= 0.0
        assert actual.transpose_backward_errors.to_rows()[0][0] >= 0.0


def test_gil_release_with_synchronized_native_wait_and_independent_threads():
    settings = small_settings()
    operation = lambda: dal.EuropeanPdeRiskResult_New(*POINT, settings=settings)
    released = threading.Event()
    errors = []
    def release():
        try:
            deadline = time.monotonic() + 5.0
            while not dal._dal._EuropeanPdeRiskGilBarrier_ReleaseForTesting():
                if time.monotonic() >= deadline:
                    raise AssertionError("native PDE wrapper did not allow Python barrier release")
                time.sleep(0)
            released.set()
        except BaseException as error:
            errors.append(error)
    dal._dal._EuropeanPdeRiskGilBarrier_ArmForTesting()
    worker = threading.Thread(target=release)
    worker.start()
    try:
        result = operation()
    finally:
        worker.join(timeout=6.0)
    assert not worker.is_alive()
    assert not errors
    assert released.is_set()
    with ThreadPoolExecutor(max_workers=2) as pool:
        concurrent = list(pool.map(lambda _: operation(), range(2)))
    for other in concurrent:
        assert other.prices == result.prices
        assert other.jacobian.to_rows() == result.jacobian.to_rows()
