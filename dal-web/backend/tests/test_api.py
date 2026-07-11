"""End-to-end API tests covering the portfolio-management workflow."""

from __future__ import annotations

import time


def test_health_reports_dal_backend(client):
    resp = client.get("/api/health")
    assert resp.status_code == 200
    body = resp.json()
    assert body["status"] == "ok"
    assert body["backend"] == "dal"
    assert body["is_native"] is True


def test_product_templates_available(client):
    resp = client.get("/api/products/templates")
    assert resp.status_code == 200
    keys = {t["key"] for t in resp.json()}
    assert {"european_call", "up_and_out_call", "snowball"} <= keys


def _create_european_product(client) -> str:
    payload = {
        "name": "Test European Call",
        "description": "",
        "rows": [
            {"date_kind": "label", "label": "STRIKE", "event": "100.0"},
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
    payload = {
        "name": "BS ATM",
        "kind": "BSModelData_",
        "bs": {"spot": 100.0, "vol": 0.2, "rate": 0.0, "div": 0.0},
    }
    resp = client.post("/api/models", json=payload)
    assert resp.status_code == 201
    return resp.json()["id"]


def _dupire_payload() -> dict:
    return {
        "name": "Skewed Dupire",
        "kind": "DupireModelData_",
        "dupire": {
            "spot": 100.0,
            "rate": 0.03,
            "repo": 0.01,
            "spots": [90.0, 100.0, 110.0],
            "times": [0.5, 1.0],
            "vols": [[0.24, 0.23], [0.21, 0.20], [0.19, 0.18]],
        },
    }


def test_create_non_flat_dupire_model(client):
    resp = client.post("/api/models", json=_dupire_payload())

    assert resp.status_code == 201
    assert resp.json()["dupire"]["vols"][0] == [0.24, 0.23]


def test_create_dupire_model_rejects_wrong_surface_shape(client):
    payload = _dupire_payload()
    payload["dupire"]["vols"] = [[0.24, 0.23], [0.21, 0.20]]

    resp = client.post("/api/models", json=payload)

    assert resp.status_code == 422


def test_update_dupire_model_rejects_ragged_surface(client):
    created = client.post("/api/models", json=_dupire_payload())
    model_id = created.json()["id"]
    invalid = _dupire_payload()["dupire"]
    invalid["vols"] = [[0.24, 0.23], [0.21], [0.19, 0.18]]

    resp = client.put(f"/api/models/{model_id}", json={"dupire": invalid})

    assert resp.status_code == 422


def _wait_for_valuation(client, valuation_id: str, max_polls: int = 20) -> dict:
    """Poll a valuation until it is no longer 'running' (background task completed)."""
    for _ in range(max_polls):
        resp = client.get(f"/api/valuations/{valuation_id}")
        assert resp.status_code == 200
        body = resp.json()
        if body["status"] != "running":
            return body
        time.sleep(0.1)
    raise AssertionError(f"Valuation {valuation_id} did not complete within {max_polls} polls")


def test_full_workflow_value_portfolio(client):
    product_id = _create_european_product(client)
    model_id = _create_bs_model(client)

    trade_resp = client.post(
        "/api/trades",
        json={
            "name": "Call 100",
            "book": "EQ",
            "notional": 1_000_000.0,
            "quantity": 1.0,
            "product_id": product_id,
            "model_id": model_id,
        },
    )
    assert trade_resp.status_code == 201
    trade_id = trade_resp.json()["id"]

    pf_resp = client.post("/api/portfolios", json={"name": "Book A"})
    assert pf_resp.status_code == 201
    pf_id = pf_resp.json()["id"]

    add_resp = client.post(f"/api/portfolios/{pf_id}/trades/{trade_id}")
    assert add_resp.status_code == 200
    assert trade_id in add_resp.json()["trade_ids"]

    val_resp = client.post(
        f"/api/portfolios/{pf_id}/value",
        json={"num_paths": 1024, "enable_aad": True, "evaluation_date": "2022-09-15"},
    )
    assert val_resp.status_code == 200
    pending = val_resp.json()
    assert pending["status"] == "running"
    assert pending["target_kind"] == "portfolio"

    # Poll until the background pricing completes.
    body = _wait_for_valuation(client, pending["id"])
    assert body["status"] == "completed"
    assert body["total_pv"] > 0.0
    assert "d_spot" in body["total_greeks"]
    # PV is scaled by notional
    assert body["total_pv"] > 1_000_000.0
    assert len(body["trades"]) == 1


def test_trade_value_endpoint(client):
    product_id = _create_european_product(client)
    model_id = _create_bs_model(client)
    trade_resp = client.post(
        "/api/trades",
        json={
            "name": "Call 100",
            "product_id": product_id,
            "model_id": model_id,
            "notional": 1.0,
        },
    )
    trade_id = trade_resp.json()["id"]
    val_resp = client.post(
        f"/api/trades/{trade_id}/value",
        json={"num_paths": 512, "enable_aad": True, "evaluation_date": "2022-09-15"},
    )
    assert val_resp.status_code == 200
    pending = val_resp.json()
    assert pending["status"] == "running"

    body = _wait_for_valuation(client, pending["id"])
    assert body["status"] == "completed"
    assert 7.0 < body["total_pv"] < 9.0


def test_product_debug_endpoint(client):
    resp = client.post(
        "/api/products/debug",
        json={
            "rows": [
                {"date_kind": "label", "label": "STRIKE", "event": "100.0"},
                {
                    "date_kind": "date",
                    "date": "2023-09-15",
                    "event": "call pays MAX(spot() - STRIKE, 0.0)",
                },
            ]
        },
    )
    assert resp.status_code == 200
    assert "STRIKE" in resp.json()["debug"]


def test_create_trade_with_unknown_product_fails(client):
    model_id = _create_bs_model(client)
    resp = client.post(
        "/api/trades",
        json={"name": "bad", "product_id": "missing", "model_id": model_id},
    )
    assert resp.status_code == 422


def test_delete_product_referenced_by_trade_fails(client):
    product_id = _create_european_product(client)
    model_id = _create_bs_model(client)
    client.post(
        "/api/trades",
        json={"name": "t", "product_id": product_id, "model_id": model_id},
    )
    resp = client.delete(f"/api/products/{product_id}")
    assert resp.status_code == 409
    assert "referenced by trade" in resp.json()["detail"]


def test_delete_model_referenced_by_trade_fails(client):
    product_id = _create_european_product(client)
    model_id = _create_bs_model(client)
    client.post(
        "/api/trades",
        json={"name": "t", "product_id": product_id, "model_id": model_id},
    )
    resp = client.delete(f"/api/models/{model_id}")
    assert resp.status_code == 409
    assert "referenced by trade" in resp.json()["detail"]


def test_delete_product_without_references_succeeds(client):
    product_id = _create_european_product(client)
    resp = client.delete(f"/api/products/{product_id}")
    assert resp.status_code == 204


def test_update_product(client):
    product_id = _create_european_product(client)
    resp = client.put(
        f"/api/products/{product_id}",
        json={"name": "Renamed Call"},
    )
    assert resp.status_code == 200
    assert resp.json()["name"] == "Renamed Call"


def test_update_model(client):
    model_id = _create_bs_model(client)
    resp = client.put(
        f"/api/models/{model_id}",
        json={"bs": {"spot": 110.0, "vol": 0.25, "rate": 0.01, "div": 0.0}},
    )
    assert resp.status_code == 200
    assert resp.json()["bs"]["spot"] == 110.0


def test_update_trade(client):
    product_id = _create_european_product(client)
    model_id = _create_bs_model(client)
    trade_resp = client.post(
        "/api/trades",
        json={"name": "orig", "product_id": product_id, "model_id": model_id},
    )
    trade_id = trade_resp.json()["id"]
    resp = client.put(
        f"/api/trades/{trade_id}",
        json={"name": "renamed", "notional": 2_000_000.0},
    )
    assert resp.status_code == 200
    assert resp.json()["name"] == "renamed"
    assert resp.json()["notional"] == 2_000_000.0


def test_update_trade_with_missing_product_fails(client):
    product_id = _create_european_product(client)
    model_id = _create_bs_model(client)
    trade_resp = client.post(
        "/api/trades",
        json={"name": "t", "product_id": product_id, "model_id": model_id},
    )
    trade_id = trade_resp.json()["id"]
    resp = client.put(
        f"/api/trades/{trade_id}",
        json={"product_id": "nonexistent"},
    )
    assert resp.status_code == 404


def test_valuation_returns_running_then_completed(client):
    product_id = _create_european_product(client)
    model_id = _create_bs_model(client)
    trade_resp = client.post(
        "/api/trades",
        json={"name": "t", "product_id": product_id, "model_id": model_id, "notional": 1.0},
    )
    trade_id = trade_resp.json()["id"]
    val_resp = client.post(
        f"/api/trades/{trade_id}/value",
        json={"num_paths": 256, "enable_aad": False, "evaluation_date": "2022-09-15"},
    )
    pending = val_resp.json()
    assert pending["status"] == "running"
    assert pending["total_pv"] == 0.0

    body = _wait_for_valuation(client, pending["id"])
    assert body["status"] == "completed"
    assert body["total_pv"] > 0.0


def test_delete_unused_product_and_model(client):
    """End-to-end: after deleting a trade, its product/model can be deleted."""
    product_id = _create_european_product(client)
    model_id = _create_bs_model(client)
    trade_resp = client.post(
        "/api/trades",
        json={"name": "t", "product_id": product_id, "model_id": model_id},
    )
    trade_id = trade_resp.json()["id"]
    # Can't delete while trade exists.
    assert client.delete(f"/api/products/{product_id}").status_code == 409
    # Delete the trade (cascades out of portfolios too).
    assert client.delete(f"/api/trades/{trade_id}").status_code == 204
    # Now product and model can be deleted.
    assert client.delete(f"/api/products/{product_id}").status_code == 204
    assert client.delete(f"/api/models/{model_id}").status_code == 204


def test_valuation_rejects_path_count_over_cap(client):
    """The num_paths upper bound is enforced at the API boundary."""
    product_id = _create_european_product(client)
    model_id = _create_bs_model(client)
    trade_resp = client.post(
        "/api/trades",
        json={"name": "t", "product_id": product_id, "model_id": model_id},
    )
    trade_id = trade_resp.json()["id"]
    over_cap = (1 << 24) + 1
    val_resp = client.post(
        f"/api/trades/{trade_id}/value",
        json={"num_paths": over_cap, "enable_aad": True},
    )
    assert val_resp.status_code == 422
