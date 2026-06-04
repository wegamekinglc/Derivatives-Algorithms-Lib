"""End-to-end API tests covering the portfolio-management workflow."""

from __future__ import annotations


def test_health_reports_stub_backend(client):
    resp = client.get("/api/health")
    assert resp.status_code == 200
    body = resp.json()
    assert body["status"] == "ok"
    assert body["backend"] == "dal_stub"
    assert body["is_native"] is False


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
    body = val_resp.json()
    assert body["target_kind"] == "portfolio"
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
    body = val_resp.json()
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
