"""Restart-recovery tests for the SQLAlchemy-backed store.

A "restart" is simulated by resetting the process-wide store/gateway
singletons and building a fresh app (new engine, new sessions) over the same
on-disk SQLite file.  The pinned semantics:

* entities created through the API in one process are fully visible in the
  next;
* a valuation left at ``status="running"`` by a killed process is reconciled
  at startup to ``status="failed"`` with ``error_message="Server restarted
  while pricing"`` (``main._reconcile_orphaned_valuations``, H6), while
  completed and already-failed rows are left untouched;
* under ``DAL_WEB_STORE=memory`` a restart simply loses everything -- the
  documented in-memory trade-off.
"""

from __future__ import annotations

import contextlib
from collections.abc import Iterator
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

from app.schemas import ValuationConfig, ValuationResult

_CREATED_AT = "2026-07-19T00:00:00+00:00"


def _reset_singletons() -> None:
    # Mirrors tests/conftest.py: the next get_store()/get_gateway() call then
    # rebuilds from the current environment, simulating a fresh process.
    import app.services.dal_gateway as gw
    import app.services.store as st

    gw._gateway_box[0] = None
    st._store_box[0] = None


@contextlib.contextmanager
def _fresh_app(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path, *, memory: bool = False
) -> Iterator[TestClient]:
    """Yield a TestClient for a fresh "process" over tmp_path's SQLite file."""
    _reset_singletons()
    if memory:
        monkeypatch.setenv("DAL_WEB_STORE", "memory")
    else:
        monkeypatch.delenv("DAL_WEB_STORE", raising=False)
        monkeypatch.setenv("DAL_WEB_DB_URL", f"sqlite:///{tmp_path / 'restart.db'}")
    from app.main import create_app

    with TestClient(create_app()) as client:
        yield client

    import app.services.store as st

    store = st._store_box[0]
    if store is not None and hasattr(store, "close"):
        store.close()


def _create_product_model_trade(client) -> tuple[str, str, str]:
    product = client.post(
        "/api/products",
        json={
            "name": "Restart Call",
            "description": "",
            "rows": [
                {"date_kind": "label", "label": "STRIKE", "event": "100.0"},
                {
                    "date_kind": "date",
                    "date": "2023-09-15",
                    "event": "call pays MAX(spot() - STRIKE, 0.0)",
                },
            ],
        },
    )
    assert product.status_code == 201
    model = client.post(
        "/api/models",
        json={
            "name": "Restart BS",
            "kind": "BSModelData_",
            "bs": {"spot": 100.0, "vol": 0.2, "rate": 0.0, "div": 0.0},
        },
    )
    assert model.status_code == 201
    trade = client.post(
        "/api/trades",
        json={
            "name": "restart trade",
            "book": "EQ",
            "notional": 2.0,
            "quantity": 3.0,
            "product_id": product.json()["id"],
            "model_id": model.json()["id"],
        },
    )
    assert trade.status_code == 201
    return product.json()["id"], model.json()["id"], trade.json()["id"]


def _valuation(target_id: str, status: str, **overrides) -> ValuationResult:
    base: dict = {
        "target_kind": "trade",
        "target_id": target_id,
        "backend": "dal",
        "is_native": True,
        "config": ValuationConfig(num_paths=64),
        "total_pv": 0.0,
        "trades": [],
        "created_at": _CREATED_AT,
        "status": status,
    }
    base.update(overrides)
    return ValuationResult(**base)


def test_entities_created_via_api_survive_restart(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    with _fresh_app(monkeypatch, tmp_path) as client:
        product_id, model_id, trade_id = _create_product_model_trade(client)
        portfolio = client.post("/api/portfolios", json={"name": "Restart PF"})
        assert portfolio.status_code == 201
        portfolio_id = portfolio.json()["id"]
        add = client.post(f"/api/portfolios/{portfolio_id}/trades/{trade_id}")
        assert add.status_code == 200

    with _fresh_app(monkeypatch, tmp_path) as client:
        product = client.get(f"/api/products/{product_id}")
        assert product.status_code == 200
        assert product.json()["name"] == "Restart Call"
        assert len(product.json()["rows"]) == 2

        model = client.get(f"/api/models/{model_id}")
        assert model.status_code == 200
        assert model.json()["bs"]["spot"] == 100.0

        trade = client.get(f"/api/trades/{trade_id}")
        assert trade.status_code == 200
        assert trade.json()["book"] == "EQ"
        assert trade.json()["notional"] == 2.0
        assert trade.json()["quantity"] == 3.0

        portfolio = client.get(f"/api/portfolios/{portfolio_id}")
        assert portfolio.status_code == 200
        assert portfolio.json()["trade_ids"] == [trade_id]

        members = client.get(f"/api/portfolios/{portfolio_id}/trades")
        assert members.status_code == 200
        assert [t["id"] for t in members.json()] == [trade_id]


def test_running_valuation_reconciles_to_failed_on_restart(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    from app.services.store import get_store

    with _fresh_app(monkeypatch, tmp_path) as client:
        _product_id, _model_id, trade_id = _create_product_model_trade(client)
        store = get_store()
        running = store.add_valuation(_valuation(trade_id, "running"))
        completed = store.add_valuation(_valuation(trade_id, "completed", total_pv=5.0))
        failed = store.add_valuation(_valuation(trade_id, "failed", error_message="original boom"))

    with _fresh_app(monkeypatch, tmp_path) as client:
        orphaned = client.get(f"/api/valuations/{running.id}")
        assert orphaned.status_code == 200
        assert orphaned.json()["status"] == "failed"
        assert orphaned.json()["error_message"] == "Server restarted while pricing"

        untouched_completed = client.get(f"/api/valuations/{completed.id}")
        assert untouched_completed.json()["status"] == "completed"
        assert untouched_completed.json()["total_pv"] == 5.0
        assert untouched_completed.json()["error_message"] is None

        untouched_failed = client.get(f"/api/valuations/{failed.id}")
        assert untouched_failed.json()["status"] == "failed"
        assert untouched_failed.json()["error_message"] == "original boom"


def test_memory_store_loses_valuations_on_restart(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    from app.services.store import get_store

    with _fresh_app(monkeypatch, tmp_path, memory=True) as client:
        get_store().add_valuation(_valuation("any-trade", "running"))
        assert len(client.get("/api/valuations").json()) == 1

    with _fresh_app(monkeypatch, tmp_path, memory=True) as client:
        assert client.get("/api/valuations").json() == []
