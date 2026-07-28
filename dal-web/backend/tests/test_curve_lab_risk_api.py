"""Curve Lab persisted axes and exact quote-risk acceptance contract."""

from __future__ import annotations

from decimal import Decimal


def _document() -> dict[str, object]:
    return {
        "schema_version": 2,
        "mode": "SINGLE",
        "as_of_date": "2026-01-15",
        "market_snapshot_id": "market-2026-01-15",
        "declarations": [
            {
                "component_key": "clab/v1/local/discount/USD/OIS",
                "role": "DISCOUNT",
                "currency": "USD",
                "parameterization": "PIECEWISE_CONSTANT_FWD",
            }
        ],
        "instruments": [
            {
                "instrument_type": "DEPOSIT",
                "trade_date": "2026-01-15",
                "start_date": "2026-01-15",
                "maturity_date": "2027-01-15",
                "currency_or_pair": "USD",
                "raw_quote": "0.04",
                "source": "TEST",
                "observed_at": "2026-01-15T00:00:00Z",
                "included": True,
                "terms": {
                    "component_key": "clab/v1/local/discount/USD/OIS",
                    "index_name": "USD-SOFR",
                },
            }
        ],
        "dependency_version_ids": [],
        "solver": {
            "solve_mode": "EXACT",
            "parameterization": "PIECEWISE_CONSTANT_FWD",
        },
    }


def _publish_version(client) -> tuple[dict, dict]:
    draft_response = client.post("/api/curve-lab/drafts", json=_document())
    assert draft_response.status_code == 201, draft_response.text
    draft = draft_response.json()
    run_response = client.post(f"/api/curve-lab/drafts/{draft['id']}/build-runs")
    assert run_response.status_code == 202, run_response.text
    run = run_response.json()
    version_response = client.post(
        "/api/curve-lab/versions",
        json={
            "draft_id": draft["id"],
            "draft_revision": draft["revision"],
            "draft_fingerprint": draft["fingerprint"],
            "build_run_id": run["id"],
            "name": "USD OIS",
            "idempotency_key": "risk-version",
        },
    )
    assert version_response.status_code == 201, version_response.text
    return run, version_response.json()


def _trade(index: int = 0) -> dict[str, object]:
    return {
        "trade_id": f"{index + 1:032x}",
        "instrument_type": "DEPOSIT",
        "trade_date": "2026-01-15",
        "start_date": "2026-01-15",
        "maturity_date": "2027-01-15",
        "currency_or_pair": "USD",
        "terms": {
            "notional": "100",
            "contract_rate": "0.05",
            "side": "LEND",
            "forecast_tenor": "3M",
            "day_basis": "ACT_365F",
            "collateral": "OIS",
            "discount_component_key": "clab/v1/local/discount/USD/OIS",
        },
    }


def _request(version_id: str) -> dict[str, object]:
    return {
        "curve_version_id": version_id,
        "target": {"trades": [_trade()]},
        "measures": ["PV", "DV01", "KEY_RATE_DV01"],
        "sensitivity_layers": [],
        "fixing_snapshot_id": "fixings-2026-01-15",
        "evaluation_time": "2026-01-15T10:30:00Z",
        "base_currency": "USD",
        "options": {},
    }


def test_build_persists_exact_quote_and_parameter_axes(client) -> None:
    run, _ = _publish_version(client)

    assert run["quote_axis"] == [
        {
            "global_quote_index": 0,
            "quote_id": run["request"]["instruments"][0]["instrument_id"],
            "instrument_id": run["request"]["instruments"][0]["instrument_id"],
            "component_key": "clab/v1/local/discount/USD/OIS",
            "stage_id": "stage-0",
            "group_id": "clab/v1/local/discount/USD/OIS",
            "stage_local_quote_index": 0,
            "quote_coordinate_kind": "RATE",
            "canonical_raw_unit": "DECIMAL",
            "raw_quote": "0.04",
            "normalized_quote": "0.04",
            "normalized_unit": "DECIMAL_RATE",
            "exact_risk_raw_bump": "0.0001",
            "normalized_risk_bump": "0.0001",
            "display_label": "DEPOSIT 2027-01-15",
        }
    ]
    assert run["parameter_axis"] == [
        {
            "global_parameter_index": 0,
            "parameter_id": (
                "clab/v1/local/discount/USD/OIS:PIECEWISE_CONSTANT_FWD:2027-01-15:RIGHT"
            ),
            "component_key": "clab/v1/local/discount/USD/OIS",
            "stage_id": "stage-0",
            "stage_local_parameter_index": 0,
            "component_local_parameter_index": 0,
            "coordinate_kind": "PIECEWISE_CONSTANT_FWD",
            "node_date": "2027-01-15",
            "side": "RIGHT",
            "native_parameter_unit": "DECIMAL_RATE",
            "display_label": "USD OIS 2027-01-15 RIGHT",
        }
    ]


def test_risk_reuses_every_pinned_dependency_archive_after_publication_and_archive(
    client,
    monkeypatch,
) -> None:
    import app.services.dal_gateway as gateway_module

    _, source = _publish_version(client)
    dependent_document = _document()
    dependent_document["dependency_version_ids"] = [source["id"]]
    dependent_draft_response = client.post(
        "/api/curve-lab/drafts",
        json=dependent_document,
    )
    assert dependent_draft_response.status_code == 201, dependent_draft_response.text
    dependent_draft = dependent_draft_response.json()
    dependent_run_response = client.post(
        f"/api/curve-lab/drafts/{dependent_draft['id']}/build-runs"
    )
    assert dependent_run_response.status_code == 202, dependent_run_response.text
    dependent_run = dependent_run_response.json()
    assert dependent_run["state"] == "SUCCEEDED", dependent_run
    dependent_version_response = client.post(
        "/api/curve-lab/versions",
        json={
            "draft_id": dependent_draft["id"],
            "draft_revision": dependent_draft["revision"],
            "draft_fingerprint": dependent_draft["fingerprint"],
            "build_run_id": dependent_run["id"],
            "name": "dependent",
            "idempotency_key": "dependent-risk-version",
        },
    )
    assert dependent_version_response.status_code == 201, dependent_version_response.text
    dependent = dependent_version_response.json()
    assert client.post(f"/api/curve-lab/versions/{source['id']}/archive").status_code == 200

    observed: list[tuple[str, bytes]] = []

    def price(
        _document,
        trades,
        _evaluation_time,
        base_currency,
        *,
        dependencies=(),
        parameter_bumps=None,
    ):
        assert parameter_bumps is None
        observed.extend(
            (record["native_payload_hash"], record["native_payload"])
            for record in dependencies
        )
        return [
            {
                "trade_id": trades[0]["trade_id"],
                "instrument_type": "DEPOSIT",
                "succeeded": True,
                "pv": "100",
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": [
                    "clab/v1/local/discount/USD/OIS"
                ],
                "error": "",
            }
        ]

    monkeypatch.setattr(gateway_module.get_gateway(), "price_curve_lab_trades", price)
    request = _request(dependent["id"])
    request["measures"] = ["PV", "DV01"]
    request["sensitivity_layers"] = []

    created = client.post("/api/curve-lab/risk-runs", json=request)

    assert created.status_code == 202, created.text
    assert created.json()["state"] == "SUCCEEDED"
    assert observed
    assert {content_hash for content_hash, _ in observed} == {
        source["native_payload_hash"]
    }
    assert all(payload for _, payload in observed)


def test_risk_run_recalibrates_parallel_and_each_key_rate_and_persists_matrix(
    client, monkeypatch
) -> None:
    import app.services.dal_gateway as gateway_module

    _, version = _publish_version(client)
    gateway = gateway_module.get_gateway()
    calls: list[str] = []

    def price(
        document,
        trades,
        evaluation_time,
        base_currency,
        *,
        dependencies=(),
    ):
        assert dependencies == []
        quote = str(document["instruments"][0]["raw_quote"])
        calls.append(quote)
        pv = Decimal("100") + Decimal(quote)
        return [
            {
                "trade_id": trades[0]["trade_id"],
                "instrument_type": "DEPOSIT",
                "succeeded": True,
                "pv": str(pv),
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": ["clab/v1/local/discount/USD/OIS"],
                "error": "",
            }
        ]

    monkeypatch.setattr(gateway, "price_curve_lab_trades", price)

    created = client.post("/api/curve-lab/risk-runs", json=_request(version["id"]))

    assert created.status_code == 202, created.text
    run = created.json()
    assert calls == ["0.04", "0.0401", "0.0401"]
    assert run["state"] == "SUCCEEDED"
    assert run["estimated_work"]["T"] == 1
    assert run["estimated_work"]["P"] == 1
    assert run["estimated_work"]["Q"] == 1
    assert run["estimated_work"]["price_evaluations"] == 3
    assert run["estimated_work"]["calibration_solves"] == 2
    pricing = run["result"]["pricing"]
    assert set(pricing[0]) == {
        "trade_id",
        "instrument_type",
        "status",
        "pv",
        "currency",
        "normalized_plan_hash",
        "required_historical_fixing_keys",
        "dependency_component_keys",
    }
    assert run["result"]["dv01"] == [{"trade_id": _trade()["trade_id"], "value": "0.0001"}]
    assert run["result"]["key_rate_sum"] == [{"trade_id": _trade()["trade_id"], "value": "0.0001"}]
    assert run["result"]["nonlinear_reconciliation"] == [
        {"trade_id": _trade()["trade_id"], "value": "0"}
    ]

    restarted = client.get(f"/api/curve-lab/risk-runs/{run['id']}")
    assert restarted.status_code == 200
    assert restarted.json() == run
    matrix = client.get(f"/api/curve-lab/risk-runs/{run['id']}/matrices/key-rate-dv01")
    assert matrix.status_code == 200, matrix.text
    assert matrix.json()["rows"] == 1
    assert matrix.json()["columns"] == 1
    assert matrix.json()["orientation"] == "TRADE_X_QUOTE"
    assert matrix.json()["availability"] == "AVAILABLE"
    assert matrix.json()["values"] == [["0.0001"]]


def test_import_job_is_readable_and_import_quote_risk_requires_lineage(client) -> None:
    _, version = _publish_version(client)
    payload = client.get(f"/api/curve-lab/versions/{version['id']}/native-json").content
    imported = client.post(
        "/api/curve-lab/import-jobs",
        content=payload,
        headers={"Content-Type": "application/json"},
    )
    assert imported.status_code == 202, imported.text
    job = imported.json()
    assert client.get(f"/api/curve-lab/import-jobs/{job['id']}").json() == job

    request = _request(job["resulting_version_id"])
    request["measures"] = ["DV01"]
    rejected = client.post("/api/curve-lab/risk-runs", json=request)
    assert rejected.status_code == 409
    assert rejected.json()["detail"]["code"] == "CALIBRATION_LINEAGE_REQUIRED"


def test_work_limit_rejection_has_zero_native_or_run_side_effects(client, monkeypatch) -> None:
    import app.services.dal_gateway as gateway_module

    _, version = _publish_version(client)
    gateway = gateway_module.get_gateway()
    called = False

    def forbidden(*_args, **_kwargs):
        nonlocal called
        called = True
        raise AssertionError("native pricing started before work admission")

    monkeypatch.setattr(gateway, "price_curve_lab_trades", forbidden)
    request = _request(version["id"])
    request["target"] = {"trades": [_trade(index) for index in range(1001)]}

    rejected = client.post("/api/curve-lab/risk-runs", json=request)

    assert rejected.status_code == 422
    assert rejected.json()["detail"]["code"] == "RISK_WORK_LIMIT_EXCEEDED"
    assert rejected.json()["detail"]["field"] == "estimated_work.T"
    assert called is False


def test_base_pricing_partial_failure_uses_exact_discriminated_key_sets(
    client, monkeypatch
) -> None:
    import app.services.dal_gateway as gateway_module

    _, version = _publish_version(client)
    gateway = gateway_module.get_gateway()
    trades = [_trade(0), _trade(1)]

    def price(
        _document,
        native_trades,
        _evaluation_time,
        base_currency,
        *,
        dependencies=(),
    ):
        assert dependencies == []
        return [
            {
                "trade_id": native_trades[0]["trade_id"],
                "instrument_type": "DEPOSIT",
                "succeeded": True,
                "pv": "1.25",
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": ["clab/v1/local/discount/USD/OIS"],
                "error": "",
            },
            {
                "trade_id": native_trades[1]["trade_id"],
                "instrument_type": "DEPOSIT",
                "succeeded": False,
                "pv": "0",
                "currency": base_currency,
                "required_historical_fixings": [("USD-SOFR", "2026-01-14T11:00:00")],
                "missing_historical_fixings": [("USD-SOFR", "2026-01-14T11:00:00")],
                "dependency_component_keys": ["clab/v1/local/discount/USD/OIS"],
                "error": (
                    "/home/builder/Derivatives-Algorithms-Lib/dal-cpp/"
                    "curve.cpp:412 CalibrateCurve(): Missing historical fixing USD-SOFR"
                ),
            },
        ]

    monkeypatch.setattr(gateway, "price_curve_lab_trades", price)
    request = _request(version["id"])
    request["target"] = {"trades": trades}
    request["measures"] = ["PV"]

    response = client.post("/api/curve-lab/risk-runs", json=request)

    assert response.status_code == 202, response.text
    rows = response.json()["result"]["pricing"]
    assert set(rows[0]) == {
        "trade_id",
        "instrument_type",
        "status",
        "pv",
        "currency",
        "normalized_plan_hash",
        "required_historical_fixing_keys",
        "dependency_component_keys",
    }
    assert set(rows[1]) == {
        "trade_id",
        "instrument_type",
        "status",
        "error",
        "required_historical_fixing_keys",
        "missing_historical_fixing_keys",
        "dependency_component_keys",
    }
    assert rows[1]["error"]["code"] == "MISSING_HISTORICAL_FIXING"
    assert rows[1]["error"]["message"] == "Native trade pricing failed."
    assert "/home/builder" not in response.text
    assert "curve.cpp" not in response.text
    assert "CalibrateCurve" not in response.text


def test_work_estimator_charges_full_two_parameter_bumps_per_trade() -> None:
    from app.services.curve_risk import estimate_work

    estimate = estimate_work(
        trades=1_000,
        aad_eligible_trades=1_000,
        parameters=500,
        quotes=0,
        measures=("PV",),
        sensitivity_layers=("TRADE_TO_NODE",),
    )

    assert estimate["parameter_bump_price_evaluations"] == 1_000_000
    assert estimate["aad_price_evaluations"] == 1_000
    assert estimate["price_evaluations"] == 1_002_000
    assert estimate["overflow"] is False


def test_requested_sensitivity_layers_are_persisted_with_explicit_axes_and_methods(
    client, monkeypatch
) -> None:
    import app.services.dal_gateway as gateway_module

    _, version = _publish_version(client)
    gateway = gateway_module.get_gateway()

    def price(
        document,
        trades,
        _evaluation_time,
        base_currency,
        *,
        dependencies=(),
    ):
        assert dependencies == []
        quote = Decimal(str(document["instruments"][0]["raw_quote"]))
        return [
            {
                "trade_id": trades[0]["trade_id"],
                "instrument_type": "DEPOSIT",
                "succeeded": True,
                "pv": str(Decimal("100") + quote),
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": ["clab/v1/local/discount/USD/OIS"],
                "error": "",
            }
        ]

    def parameters(document, _axis, *, dependencies=()):
        assert dependencies == []
        return [str(document["instruments"][0]["raw_quote"])]

    def parameter_bump(
        _document,
        trades,
        _evaluation_time,
        base_currency,
        _axis,
        bump,
        *,
        dependencies=(),
    ):
        assert dependencies == []
        return [
            {
                "trade_id": trades[0]["trade_id"],
                "instrument_type": "DEPOSIT",
                "succeeded": True,
                "pv": str(Decimal("100.04") + Decimal(str(bump)) * 2),
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": ["clab/v1/local/discount/USD/OIS"],
                "error": "",
            }
        ]

    monkeypatch.setattr(gateway, "price_curve_lab_trades", price)
    monkeypatch.setattr(gateway, "curve_lab_parameter_values", parameters)
    monkeypatch.setattr(
        gateway,
        "price_curve_lab_parameter_bump",
        parameter_bump,
    )
    request = _request(version["id"])
    request["measures"] = ["PV"]
    request["sensitivity_layers"] = [
        "TRADE_TO_NODE",
        "CALIBRATION_JACOBIAN",
        "COMPOSED_QUOTE_DIAGNOSTIC",
    ]

    response = client.post("/api/curve-lab/risk-runs", json=request)

    assert response.status_code == 202, response.text
    run = response.json()
    assert run["estimated_work"]["N_param"] == 2
    assert run["estimated_work"]["N_jac"] == 2
    expected = {
        "trade-to-node": ("TRADE_X_PARAMETER", [["2"]]),
        "calibration-jacobian": ("PARAMETER_X_QUOTE", [["1"]]),
        "composed-quote-diagnostic": ("TRADE_X_QUOTE", [["2"]]),
    }
    for matrix_id, (orientation, values) in expected.items():
        matrix_response = client.get(f"/api/curve-lab/risk-runs/{run['id']}/matrices/{matrix_id}")
        assert matrix_response.status_code == 200, matrix_response.text
        matrix = matrix_response.json()
        assert matrix["availability"] == "AVAILABLE"
        assert matrix["orientation"] == orientation
        assert matrix["values"] == values


def test_forbidden_node_fallback_rejects_before_native_dispatch(
    client, monkeypatch
) -> None:
    import app.services.dal_gateway as gateway_module

    _, version = _publish_version(client)
    gateway = gateway_module.get_gateway()
    called = False

    def forbidden(*_args, **_kwargs):
        nonlocal called
        called = True
        raise AssertionError("native work started before fallback admission")

    monkeypatch.setattr(gateway, "price_curve_lab_trades", forbidden)
    request = _request(version["id"])
    request["measures"] = ["PV"]
    request["sensitivity_layers"] = ["TRADE_TO_NODE"]
    request["options"] = {"aad_fallback": "FORBID"}

    rejected = client.post("/api/curve-lab/risk-runs", json=request)

    assert rejected.status_code == 422
    assert rejected.json()["detail"]["code"] == "AAD_METHOD_UNAVAILABLE"
    assert called is False


def test_forbidden_jacobian_replay_rejects_before_native_dispatch(
    client, monkeypatch
) -> None:
    import app.services.dal_gateway as gateway_module

    _, version = _publish_version(client)
    gateway = gateway_module.get_gateway()
    called = False

    def forbidden(*_args, **_kwargs):
        nonlocal called
        called = True
        raise AssertionError("native work started before fallback admission")

    monkeypatch.setattr(gateway, "price_curve_lab_trades", forbidden)
    request = _request(version["id"])
    request["measures"] = ["PV"]
    request["sensitivity_layers"] = ["CALIBRATION_JACOBIAN"]
    request["options"] = {"jacobian_replay_fallback": "FORBID"}

    rejected = client.post("/api/curve-lab/risk-runs", json=request)

    assert rejected.status_code == 422
    assert rejected.json()["detail"]["code"] == "JACOBIAN_METHOD_UNAVAILABLE"
    assert called is False


def test_openapi_closes_pricing_result_success_and_failure_variants(client) -> None:
    schemas = client.app.openapi()["components"]["schemas"]

    success = schemas["PricingTradeSuccessV1"]
    failure = schemas["PricingTradeFailureV1"]
    assert success["additionalProperties"] is False
    assert failure["additionalProperties"] is False
    assert set(success["properties"]) == {
        "trade_id",
        "instrument_type",
        "status",
        "pv",
        "currency",
        "normalized_plan_hash",
        "required_historical_fixing_keys",
        "dependency_component_keys",
    }
    assert set(failure["properties"]) == {
        "trade_id",
        "instrument_type",
        "status",
        "error",
        "required_historical_fixing_keys",
        "missing_historical_fixing_keys",
        "dependency_component_keys",
    }
