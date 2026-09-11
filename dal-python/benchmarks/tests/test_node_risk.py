"""Check risk units, interpolation, algorithm dispatch and fresh differentiation."""

from datetime import datetime
from pathlib import Path
import sys
from types import SimpleNamespace

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from dal_comparisons.scenarios import (
    BUMP,
    LOG_DFS,
    NODES,
    TIMES,
    node_dv01,
    pv,
    trades,
    validate,
)
from dal_comparisons.suite import prepare


def off_node_portfolio():
    portfolio = trades(2)
    for trade in portfolio:
        trade["start"] = datetime(2026, 4, 15)
        trade["end"] = datetime(2045, 4, 15)
    return portfolio


@pytest.mark.parametrize("trade", off_node_portfolio())
def test_analytic_buckets_match_independent_node_bumps(trade):
    buckets = node_dv01(trade)
    step = 1e-6
    for i in range(1, len(NODES)):
        up, down = list(LOG_DFS), list(LOG_DFS)
        up[i] -= TIMES[i] * step
        down[i] += TIMES[i] * step
        finite_difference = (
            (pv(trade, log_dfs=up) - pv(trade, log_dfs=down)) * BUMP / (2 * step)
        )
        assert buckets[i - 1] == pytest.approx(finite_difference, abs=2e-6, rel=1e-10)
    # All bucket exposures sum to parallel first-order zero-curve risk.
    parallel = (pv(trade, step) - pv(trade, -step)) * BUMP / (2 * step)
    assert sum(buckets) == pytest.approx(parallel, abs=2e-6, rel=1e-10)


@pytest.mark.parametrize("backend", ["dal", "quantlib", "rateslib"])
def test_off_node_risk_matches_cashflow_derivatives(backend):
    from importlib import import_module

    portfolio = off_node_portfolio()
    reference = [value for trade in portfolio for value in node_dv01(trade)]
    adapter = import_module(f"dal_comparisons.{backend}_adapter")
    run = adapter.node_risk_runner(portfolio)
    validate(run(), reference, abs_tol=2e-6)
    validate(run(), reference, abs_tol=2e-6)


def test_dal_calls_native_aad_each_time_without_passive_repricing(monkeypatch):
    from dal_comparisons import dal_adapter as adapter

    calls = []
    native = adapter.dal.RateTradeNodeSensitivitiesBatch

    def aad(**kwargs):
        calls.append(kwargs)
        return native(**kwargs)

    def passive(**_kwargs):
        pytest.fail("AAD risk must not fall back to passive repricing")

    monkeypatch.setattr(adapter.dal, "RateTradeNodeSensitivitiesBatch", aad)
    monkeypatch.setattr(adapter.dal, "PriceRateTrades", passive)
    work = prepare("dal", {"operation": "node_dv01", "size": 2})
    for _ in range(3):
        work.validate(work.run())
    assert len(calls) == 3
    assert all(call["component_keys"] == ["curve"] for call in calls)


@pytest.mark.parametrize(
    "fault", ["ineligible", "missing", "short", "reordered", "nonfinite"]
)
def test_invalid_aad_cells_fail_instead_of_producing_timings(monkeypatch, fault):
    from dal_comparisons import dal_adapter as adapter

    native = adapter.dal.RateTradeNodeSensitivitiesBatch

    def broken(**kwargs):
        cells = native(**kwargs)
        row = cells[0].result
        result = SimpleNamespace(
            eligible=row.eligible, reason="test failure", gradient=row.gradient
        )
        if fault == "ineligible":
            result.eligible = False
        elif fault == "short":
            result.gradient.pop()
        elif fault == "nonfinite":
            result.gradient[0] = float("nan")
        cells[0] = SimpleNamespace(
            instrument_id=cells[0].instrument_id, component_key="curve", result=result
        )
        if fault == "missing":
            cells.pop()
        if fault == "reordered":
            cells.reverse()
        return cells

    monkeypatch.setattr(adapter.dal, "RateTradeNodeSensitivitiesBatch", broken)
    work = prepare("dal", {"operation": "node_dv01", "size": 2})
    with pytest.raises(ValueError):
        work.validate(work.run())


def test_rateslib_requires_an_active_dual_result(monkeypatch):
    from dal_comparisons import rateslib_adapter as adapter

    monkeypatch.setattr(
        adapter, "swap", lambda _value: SimpleNamespace(npv=lambda **_kwargs: 1.0)
    )
    work = prepare("rateslib", {"operation": "node_dv01", "size": 2})
    with pytest.raises(ValueError, match="lost automatic differentiation"):
        work.run()


@pytest.mark.parametrize("extra", [False, True])
def test_rateslib_rejects_incorrect_gradient_width_before_conversion(
    monkeypatch, extra
):
    from dal_comparisons import rateslib_adapter as adapter

    original = adapter.gradient

    def incorrect(value, variables):
        result = list(original(value, variables))
        return result + [0.0] if extra else result[:-1]

    monkeypatch.setattr(adapter, "gradient", incorrect)
    work = prepare("rateslib", {"operation": "node_dv01", "size": 2})
    with pytest.raises(ValueError, match="gradient width mismatch"):
        work.run()


def test_quantlib_reprices_all_node_shifts_each_time(monkeypatch):
    from dal_comparisons import quantlib_adapter as adapter

    prices = adapter.prices
    calls = []

    def fresh(swaps):
        calls.append(len(swaps))
        return prices(swaps)

    monkeypatch.setattr(adapter, "prices", fresh)
    work = prepare("quantlib", {"operation": "node_dv01", "size": 2})
    work.validate(work.run())
    work.validate(work.run())
    assert calls == [2] * (2 * 21 * 2)


def test_unknown_risk_operation_is_rejected():
    with pytest.raises(ValueError, match="unknown comparison operation"):
        prepare("dal", {"operation": "typo", "size": 2})
