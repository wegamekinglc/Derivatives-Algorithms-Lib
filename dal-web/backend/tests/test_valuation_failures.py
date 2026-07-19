"""Failure-path tests for the valuation API.

Two failure layers are pinned here:

* per-trade pricing failures (``gateway.value`` raising) are caught inside
  ``_price_trade`` and surface as ``TradeValuation.error`` -- the valuation
  itself still reaches ``status="completed"`` so one bad trade cannot abort
  a whole portfolio;
* task-level failures (the store raising inside the background pricing task)
  are caught by the pricing coroutine and surface as ``status="failed"`` with
  ``error_message`` set, zeroed PV and empty Greeks.

Also pinned: the ``running -> completed | failed`` polling lifecycle for each
layer, and the unchanged ``NotFoundError -> 404`` mapping on the valuation
endpoints.
"""

from __future__ import annotations

import time

import dal  # the fake installed by conftest


def _create_european_product(client, strike: str = "100.0") -> str:
    payload = {
        "name": "Call",
        "description": "",
        "rows": [
            {"date_kind": "label", "label": "STRIKE", "event": strike},
            {
                "date_kind": "date",
                "date": "2023-09-15",
                "event": "call pays MAX(spot() - STRIKE, 0.0)",
            },
        ],
    }
    resp = client.post("/api/products", json=payload)
    assert resp.status_code == 201
    return resp.json()["id"]


def _create_bs_model(client) -> str:
    resp = client.post(
        "/api/models",
        json={
            "name": "BS",
            "kind": "BSModelData_",
            "bs": {"spot": 100.0, "vol": 0.2, "rate": 0.0, "div": 0.0},
        },
    )
    assert resp.status_code == 201
    return resp.json()["id"]


def _create_trade(client, product_id: str, model_id: str, name: str = "t") -> str:
    resp = client.post(
        "/api/trades",
        json={"name": name, "product_id": product_id, "model_id": model_id, "notional": 1.0},
    )
    assert resp.status_code == 201
    return resp.json()["id"]


def _create_portfolio(client, trade_ids: list[str]) -> str:
    resp = client.post("/api/portfolios", json={"name": "PF"})
    assert resp.status_code == 201
    portfolio_id = resp.json()["id"]
    for trade_id in trade_ids:
        add = client.post(f"/api/portfolios/{portfolio_id}/trades/{trade_id}")
        assert add.status_code == 200
    return portfolio_id


def _wait_for_valuation(client, valuation_id: str, max_polls: int = 20) -> dict:
    for _ in range(max_polls):
        resp = client.get(f"/api/valuations/{valuation_id}")
        assert resp.status_code == 200
        body = resp.json()
        if body["status"] != "running":
            return body
        time.sleep(0.1)
    raise AssertionError(f"Valuation {valuation_id} did not settle within {max_polls} polls")


def test_trade_pricing_error_surfaces_as_trade_error(client, monkeypatch) -> None:
    """A gateway failure inside one trade's pricing yields an errored trade,
    not a failed valuation: status stays 'completed' with the error carried
    on the trade row."""
    model_id = _create_bs_model(client)
    trade_id = _create_trade(client, _create_european_product(client), model_id)

    def _boom(*args, **kwargs):
        raise RuntimeError("boom")

    monkeypatch.setattr(dal, "MonteCarlo_Value", _boom)

    resp = client.post(
        f"/api/trades/{trade_id}/value",
        json={"num_paths": 64, "enable_aad": False, "evaluation_date": "2022-09-15"},
    )
    assert resp.status_code == 200
    pending = resp.json()
    assert pending["status"] == "running"

    body = _wait_for_valuation(client, pending["id"])
    assert body["status"] == "completed"
    assert body["error_message"] is None
    assert body["total_pv"] == 0.0
    assert body["total_greeks"] == {}
    assert len(body["trades"]) == 1
    trade_row = body["trades"][0]
    assert trade_row["trade_id"] == trade_id
    assert "boom" in trade_row["error"]
    assert trade_row["pv"] == 0.0
    assert trade_row["scaled_pv"] == 0.0


def test_portfolio_valuation_isolates_single_failing_trade(client, monkeypatch) -> None:
    """One failing trade must not abort the portfolio: the healthy trade's PV
    is still priced and aggregated, and only the failing row carries an error."""
    model_id = _create_bs_model(client)
    good_trade = _create_trade(client, _create_european_product(client, "100.0"), model_id, "good")
    bad_product = _create_european_product(client, "BOMB")
    bad_trade = _create_trade(client, bad_product, model_id, "bad")
    portfolio_id = _create_portfolio(client, [good_trade, bad_trade])

    real_monte_carlo = dal.MonteCarlo_Value

    def _selective_boom(product, *args, **kwargs):
        _dates, events = product
        if any("BOMB" in str(event) for event in events):
            raise RuntimeError("pricing exploded")
        return real_monte_carlo(product, *args, **kwargs)

    monkeypatch.setattr(dal, "MonteCarlo_Value", _selective_boom)

    resp = client.post(
        f"/api/portfolios/{portfolio_id}/value",
        json={"num_paths": 64, "enable_aad": False, "evaluation_date": "2022-09-15"},
    )
    assert resp.status_code == 200

    body = _wait_for_valuation(client, resp.json()["id"])
    assert body["status"] == "completed"
    assert len(body["trades"]) == 2
    by_name = {row["trade_name"]: row for row in body["trades"]}
    assert by_name["good"]["error"] is None
    assert by_name["good"]["scaled_pv"] == 8.0  # canned fake PV
    assert "pricing exploded" in by_name["bad"]["error"]
    assert by_name["bad"]["scaled_pv"] == 0.0
    # Only the healthy trade contributes to the aggregate.
    assert body["total_pv"] == 8.0


def test_portfolio_task_failure_marks_valuation_failed(client, monkeypatch) -> None:
    """A store failure inside the pricing task (after the endpoint's existence
    pre-check) flips the valuation to 'failed' with the documented shape."""
    from app.services.store import get_store

    model_id = _create_bs_model(client)
    trade_id = _create_trade(client, _create_european_product(client), model_id)
    portfolio_id = _create_portfolio(client, [trade_id])

    def _unavailable(portfolio_id: str):
        raise RuntimeError("store unavailable")

    # The endpoint pre-check uses get_portfolio, so patching portfolio_trades
    # fails only the background task, never the request itself.
    monkeypatch.setattr(get_store(), "portfolio_trades", _unavailable)

    resp = client.post(
        f"/api/portfolios/{portfolio_id}/value",
        json={"num_paths": 64, "enable_aad": False},
    )
    assert resp.status_code == 200
    pending = resp.json()
    assert pending["status"] == "running"

    body = _wait_for_valuation(client, pending["id"])
    assert body["status"] == "failed"
    assert body["error_message"] == "store unavailable"
    assert body["total_pv"] == 0.0
    assert body["total_greeks"] == {}
    assert body["trades"] == []


def test_trade_task_failure_marks_valuation_failed(client, monkeypatch) -> None:
    """Same task-level failure on the single-trade path: the endpoint's
    pre-check succeeds (first get_trade call), the task's lookup raises."""
    from app.services.store import get_store

    model_id = _create_bs_model(client)
    trade_id = _create_trade(client, _create_european_product(client), model_id)

    store = get_store()
    real_get_trade = store.get_trade
    calls = {"n": 0}

    def _flaky_get_trade(trade_id: str):
        calls["n"] += 1
        if calls["n"] == 1:
            return real_get_trade(trade_id)
        raise RuntimeError("store blew up")

    monkeypatch.setattr(store, "get_trade", _flaky_get_trade)

    resp = client.post(
        f"/api/trades/{trade_id}/value",
        json={"num_paths": 64, "enable_aad": False},
    )
    assert resp.status_code == 200
    pending = resp.json()
    assert pending["status"] == "running"

    body = _wait_for_valuation(client, pending["id"])
    assert body["status"] == "failed"
    assert body["error_message"] == "store blew up"
    assert body["total_pv"] == 0.0
    assert body["total_greeks"] == {}
    assert body["trades"] == []


def test_get_unknown_valuation_returns_404(client) -> None:
    resp = client.get("/api/valuations/does-not-exist")
    assert resp.status_code == 404
    assert "does-not-exist" in resp.json()["detail"]


def test_value_unknown_trade_returns_404(client) -> None:
    resp = client.post("/api/trades/does-not-exist/value", json={"num_paths": 64})
    assert resp.status_code == 404
    assert "does-not-exist" in resp.json()["detail"]


def test_value_unknown_portfolio_returns_404(client) -> None:
    resp = client.post("/api/portfolios/does-not-exist/value", json={"num_paths": 64})
    assert resp.status_code == 404
    assert "does-not-exist" in resp.json()["detail"]
