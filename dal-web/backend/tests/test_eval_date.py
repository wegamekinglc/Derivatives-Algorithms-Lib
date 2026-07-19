"""Evaluation-date restore semantics at the service / API layer.

The gateway-level save/restore contract is pinned by ``tests/test_dal_gateway.py``
(H5 regression tests).  These tests pin the same contract one level up: a
request that carries ``evaluation_date`` must leave the process-global DAL
evaluation date untouched once the request settles -- including when pricing
fails and when concurrent in-flight requests carry different dates.
"""

from __future__ import annotations

import asyncio
import time
from datetime import date

import dal  # the fake installed by conftest

from app.schemas import (
    BSModelParams,
    EventRow,
    ModelDefinition,
    ProductDefinition,
    Trade,
    ValuationConfig,
)

SENTINEL = (2020, 1, 1)


def _set_global_eval_date(year: int, month: int, day: int) -> str:
    dal.EvaluationDate_Set(dal.Date_(year, month, day))
    return repr(dal.EvaluationDate_Get())


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


def _create_trade(client, product_id: str, model_id: str) -> str:
    resp = client.post(
        "/api/trades",
        json={"name": "t", "product_id": product_id, "model_id": model_id, "notional": 1.0},
    )
    assert resp.status_code == 201
    return resp.json()["id"]


def _create_portfolio_with_trade(client, trade_id: str) -> str:
    resp = client.post("/api/portfolios", json={"name": "PF"})
    assert resp.status_code == 201
    portfolio_id = resp.json()["id"]
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


def test_trade_valuation_restores_global_eval_date(client) -> None:
    sentinel = _set_global_eval_date(*SENTINEL)
    trade_id = _create_trade(client, _create_european_product(client), _create_bs_model(client))

    resp = client.post(
        f"/api/trades/{trade_id}/value",
        json={"num_paths": 64, "enable_aad": False, "evaluation_date": "2022-09-15"},
    )
    assert resp.status_code == 200
    body = _wait_for_valuation(client, resp.json()["id"])

    assert body["status"] == "completed"
    assert repr(dal.EvaluationDate_Get()) == sentinel


def test_portfolio_valuation_restores_global_eval_date(client) -> None:
    sentinel = _set_global_eval_date(*SENTINEL)
    trade_id = _create_trade(client, _create_european_product(client), _create_bs_model(client))
    portfolio_id = _create_portfolio_with_trade(client, trade_id)

    resp = client.post(
        f"/api/portfolios/{portfolio_id}/value",
        json={"num_paths": 64, "enable_aad": False, "evaluation_date": "2022-09-15"},
    )
    assert resp.status_code == 200
    body = _wait_for_valuation(client, resp.json()["id"])

    assert body["status"] == "completed"
    assert repr(dal.EvaluationDate_Get()) == sentinel


def test_failed_trade_valuation_restores_global_eval_date(client, monkeypatch) -> None:
    sentinel = _set_global_eval_date(*SENTINEL)
    trade_id = _create_trade(client, _create_european_product(client), _create_bs_model(client))

    def _boom(*args, **kwargs):
        raise RuntimeError("boom")

    monkeypatch.setattr(dal, "MonteCarlo_Value", _boom)

    resp = client.post(
        f"/api/trades/{trade_id}/value",
        json={"num_paths": 64, "enable_aad": False, "evaluation_date": "2022-09-15"},
    )
    assert resp.status_code == 200
    body = _wait_for_valuation(client, resp.json()["id"])

    assert body["trades"][0]["error"] is not None
    assert repr(dal.EvaluationDate_Get()) == sentinel


def test_concurrent_valuations_isolate_evaluation_dates(store, monkeypatch) -> None:
    """Concurrent in-flight pricings must not leak evaluation dates.

    The gateway serializes ``value()`` behind a lock and restores the previous
    date in a ``finally``, so each concurrent request prices under its own
    date and the process-global date survives the batch unchanged.
    """
    import app.services.valuation as valuation_service
    from app.services.dal_gateway import DalGateway

    sentinel = _set_global_eval_date(*SENTINEL)

    seen: list[tuple[str, str]] = []

    def _recording_monte_carlo(
        product,
        model_data,
        num_path,
        method="sobol",
        use_bb=False,
        enable_aad=False,
        smooth=0.01,
    ):
        _dates, events = product
        seen.append((str(events[0]), repr(dal.EvaluationDate_Get())))
        return {"PV": 1.0}

    monkeypatch.setattr(dal, "MonteCarlo_Value", _recording_monte_carlo)

    def _add_strike_trade(strike: str) -> Trade:
        product = store.add_product(
            ProductDefinition(
                name=f"p{strike}",
                rows=[
                    EventRow(date_kind="label", label="STRIKE", event=strike),
                    EventRow(
                        date_kind="date",
                        date=date(2023, 9, 15),
                        event="call pays MAX(spot() - STRIKE, 0.0)",
                    ),
                ],
            )
        )
        model = store.add_model(
            ModelDefinition(
                name=f"m{strike}",
                kind="BSModelData_",
                bs=BSModelParams(spot=100.0, vol=0.2, rate=0.0, div=0.0),
            )
        )
        return store.add_trade(Trade(name=f"t{strike}", product_id=product.id, model_id=model.id))

    trade_a = _add_strike_trade("100.0")
    trade_b = _add_strike_trade("200.0")
    gateway = DalGateway()

    async def _run_both() -> None:
        await asyncio.gather(
            valuation_service.value_single_trade_async(
                store,
                gateway,
                trade_a.id,
                ValuationConfig(num_paths=8, evaluation_date=date(2021, 3, 1)),
            ),
            valuation_service.value_single_trade_async(
                store,
                gateway,
                trade_b.id,
                ValuationConfig(num_paths=8, evaluation_date=date(2023, 6, 15)),
            ),
        )
        # Drain the scheduled pricing tasks deterministically (no sleep-polling);
        # completed tasks have already discarded themselves from the set.
        pending = list(valuation_service._BACKGROUND_TASKS)
        if pending:
            await asyncio.gather(*pending)

    asyncio.run(_run_both())

    assert set(seen) == {("100.0", "2021-03-01"), ("200.0", "2023-06-15")}
    assert repr(dal.EvaluationDate_Get()) == sentinel
    for valuation in store.list_valuations():
        assert valuation.status == "completed"
        assert valuation.trades[0].error is None
