"""Curve Lab persisted axes and exact quote-risk acceptance contract."""

from __future__ import annotations

import hashlib
import json
import threading
import time
from dataclasses import FrozenInstanceError, fields, is_dataclass
from decimal import Decimal

import pytest


def _wait_for_job(
    client,
    collection: str,
    job_id: str,
    terminal_states: set[str],
) -> dict[str, object]:
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        record = client.get(f"/api/curve-lab/{collection}/{job_id}").json()
        if record["state"] in terminal_states:
            return record
        time.sleep(0.01)
    pytest.fail(f"{collection}/{job_id} did not reach a terminal state")


def _completed_risk(client, response) -> dict[str, object]:
    assert response.status_code == 202, response.text
    admitted = response.json()
    assert admitted["state"] == "QUEUED"
    completed = _wait_for_job(
        client,
        "risk-runs",
        admitted["id"],
        {"SUCCEEDED", "FAILED", "TIMED_OUT"},
    )
    assert completed["state"] == "SUCCEEDED", completed
    return completed


def _completed_import(client, response) -> dict[str, object]:
    assert response.status_code == 202, response.text
    admitted = response.json()
    assert admitted["state"] == "QUEUED"
    completed = _wait_for_job(
        client,
        "import-jobs",
        admitted["id"],
        {"SUCCEEDED", "FAILED", "TIMED_OUT"},
    )
    assert completed["state"] == "SUCCEEDED", completed
    return completed


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
    snapshot = client.post(
        "/api/curve-lab/fixing-snapshots",
        json={
            "id": "fixings-2026-01-15",
            "observations": [],
        },
    )
    assert snapshot.status_code in {201, 409}, snapshot.text
    draft_response = client.post("/api/curve-lab/drafts", json=_document())
    assert draft_response.status_code == 201, draft_response.text
    draft = draft_response.json()
    run_response = client.post(f"/api/curve-lab/drafts/{draft['id']}/build-runs")
    assert run_response.status_code == 202, run_response.text
    assert run_response.json()["state"] == "QUEUED"
    run = _wait_for_job(
        client,
        "build-runs",
        run_response.json()["id"],
        {"SUCCEEDED", "FAILED", "TIMED_OUT"},
    )
    assert run["state"] == "SUCCEEDED", run
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


def test_risk_admission_requires_resolved_immutable_fixing_snapshot(client) -> None:
    _, version = _publish_version(client)
    request = _request(version["id"])
    request["fixing_snapshot_id"] = "missing-snapshot"

    response = client.post("/api/curve-lab/risk-runs", json=request)

    assert response.status_code == 404
    assert response.json()["detail"]["code"] == "FIXING_SNAPSHOT_NOT_FOUND"


def test_risk_admission_rejects_fixing_kind_and_unit_before_queueing(client) -> None:
    _, version = _publish_version(client)
    snapshot = client.post(
        "/api/curve-lab/fixing-snapshots",
        json={
            "id": "incompatible-fixings",
            "observations": [
                {
                    "index_name": "USD-SOFR",
                    "fixing_time": "2026-01-14T11:00:00Z",
                    "kind": "FX",
                    "units": "DOMESTIC_PER_FOREIGN",
                    "value": "1.1",
                }
            ],
        },
    )
    assert snapshot.status_code == 201, snapshot.text
    request = _request(version["id"])
    request["fixing_snapshot_id"] = "incompatible-fixings"
    request["measures"] = ["PV"]
    request["target"]["trades"] = [_historical_fra_trade()]

    response = client.post("/api/curve-lab/risk-runs", json=request)

    assert response.status_code == 422
    assert response.json()["detail"] == {
        "code": "FIXING_SNAPSHOT_INCOMPATIBLE",
        "message": "A supplied historical fixing has incompatible semantics.",
        "field": "fixing_snapshot.observations[0]",
        "value": {
            "index_name": "USD-SOFR",
            "fixing_time": "2026-01-14T11:00:00Z",
            "kind": "FX",
            "units": "DOMESTIC_PER_FOREIGN",
        },
        "resource_id": "incompatible-fixings",
        "details": {
            "expected_kind": "RATE",
            "expected_units": "DECIMAL_RATE",
        },
    }


def _historical_fra_trade() -> dict[str, object]:
    return {
        **_trade(),
        "instrument_type": "FRA",
        "trade_date": "2026-01-13",
        "start_date": "2026-01-14",
        "maturity_date": "2026-04-14",
        "terms": {
            "notional": "100",
            "contract_rate": "0.05",
            "side": "RECEIVE_FLOATING",
            "settlement_style": "AT_END",
            "forecast_tenor": "3M",
            "day_basis": "ACT_365F",
            "collateral": "OIS",
            "index_name": "USD-SOFR",
            "fixing_hour": 11,
            "fixing_minute": 0,
            "discount_component_key": "clab/v1/local/discount/USD/OIS",
            "forecast_component_key": "clab/v1/local/discount/USD/OIS",
        },
    }


def _compiled_required_fixing(*_args) -> list[dict[str, object]]:
    return [
        {
            "trade_index": 0,
            "index_name": "USD-SOFR",
            "fixing_time": "2026-01-14T11:00:00",
            "kind": "RATE",
            "units": "DECIMAL_RATE",
        }
    ]


@pytest.mark.parametrize(
    "fixing_time",
    [
        "2026-01-14T11:00:00Z",
        "2026-01-14T12:00:00+01:00",
        "2026-01-14T11:00:00",
    ],
)
def test_risk_admission_matches_compiled_fixing_times_by_canonical_utc(
    client,
    monkeypatch,
    fixing_time,
) -> None:
    import app.services.dal_gateway as gateway_module

    _, version = _publish_version(client)
    gateway = gateway_module.get_gateway()
    monkeypatch.setattr(
        gateway,
        "curve_lab_required_historical_fixings",
        _compiled_required_fixing,
    )
    snapshot = client.post(
        "/api/curve-lab/fixing-snapshots",
        json={
            "id": "canonical-utc-fixings",
            "observations": [
                {
                    "index_name": "USD-SOFR",
                    "fixing_time": fixing_time,
                    "kind": "RATE",
                    "units": "DECIMAL_RATE",
                    "value": "0.04",
                }
            ],
        },
    )
    assert snapshot.status_code == 201, snapshot.text
    request = _request(version["id"])
    request["fixing_snapshot_id"] = "canonical-utc-fixings"
    request["measures"] = ["PV"]
    request["target"]["trades"] = [_historical_fra_trade()]

    response = client.post("/api/curve-lab/risk-runs", json=request)

    assert response.status_code == 202, response.text
    assert response.json()["state"] == "QUEUED"


def test_risk_admission_preserves_missing_fixing_error_after_utc_normalization(
    client,
    monkeypatch,
) -> None:
    import app.services.dal_gateway as gateway_module

    _, version = _publish_version(client)
    gateway = gateway_module.get_gateway()
    monkeypatch.setattr(
        gateway,
        "curve_lab_required_historical_fixings",
        _compiled_required_fixing,
    )
    snapshot = client.post(
        "/api/curve-lab/fixing-snapshots",
        json={
            "id": "wrong-time-fixings",
            "observations": [
                {
                    "index_name": "USD-SOFR",
                    "fixing_time": "2026-01-14T11:01:00Z",
                    "kind": "RATE",
                    "units": "DECIMAL_RATE",
                    "value": "0.04",
                }
            ],
        },
    )
    assert snapshot.status_code == 201, snapshot.text
    request = _request(version["id"])
    request["fixing_snapshot_id"] = "wrong-time-fixings"
    request["measures"] = ["PV"]
    request["target"]["trades"] = [_historical_fra_trade()]

    response = client.post("/api/curve-lab/risk-runs", json=request)

    assert response.status_code == 422
    assert response.json()["detail"] == {
        "code": "MISSING_HISTORICAL_FIXING",
        "message": "A required historical fixing is absent from the immutable snapshot.",
        "field": "target.trades[0]",
        "value": {
            "index_name": "USD-SOFR",
            "fixing_time": "2026-01-14T11:00:00",
        },
        "resource_id": "wrong-time-fixings",
        "details": {
            "constraint": ("fixing_time before evaluation_time requires an exact snapshot value")
        },
    }


def test_risk_trade_contract_rejects_open_terms_before_side_effects(client) -> None:
    request = _request("f" * 32)
    request["target"]["trades"][0]["terms"]["mystery_knob"] = True

    response = client.post("/api/curve-lab/risk-runs", json=request)

    assert response.status_code == 422
    assert response.json()["detail"]["code"] == "REQUEST_VALIDATION_FAILED"


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


def test_log_discount_parameter_axis_comes_from_native_free_layout(client) -> None:
    document = _document()
    document["declarations"][0]["parameterization"] = "LOG_DISCOUNT"
    document["solver"]["parameterization"] = "LOG_DISCOUNT"
    draft_response = client.post("/api/curve-lab/drafts", json=document)
    assert draft_response.status_code == 201, draft_response.text

    run_response = client.post(f"/api/curve-lab/drafts/{draft_response.json()['id']}/build-runs")

    assert run_response.status_code == 202, run_response.text
    run = _wait_for_job(
        client,
        "build-runs",
        run_response.json()["id"],
        {"SUCCEEDED", "FAILED", "TIMED_OUT"},
    )
    assert run["state"] == "SUCCEEDED", run
    assert len(run["parameter_axis"]) == 1
    assert run["parameter_axis"][0]["coordinate_kind"] == "LOG_DISCOUNT"


def test_base_pricing_reconstructs_selected_version_without_recalibration(
    client,
    monkeypatch,
) -> None:
    from app.services.dal_gateway import get_gateway
    from app.services.store import get_store

    _, version = _publish_version(client)
    stored = get_store().get_curve_lab_version(version["id"])
    gateway = get_gateway()
    curves = gateway._curve_lab_archive_curves(
        stored["native_payload"],
        stored["root_kind"],
        stored["verification"]["document"],
        stored["native_payload_hash"],
    )
    assert set(curves) == {"clab/v1/local/discount/USD/OIS"}

    seen: list[str] = []

    def record_version(
        _document,
        trades,
        _evaluation_time,
        _base_currency,
        *,
        curve_version,
        **_kwargs,
    ):
        seen.append(curve_version["native_payload_hash"])
        return [
            {
                "trade_id": trade["trade_id"],
                "instrument_type": trade["instrument_type"],
                "succeeded": True,
                "pv": "1",
                "currency": "USD",
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": ["clab/v1/local/discount/USD/OIS"],
                "error": "",
            }
            for trade in trades
        ]

    monkeypatch.setattr(gateway, "price_curve_lab_trades", record_version)

    request = _request(version["id"])
    request["measures"] = ["PV"]
    response = client.post("/api/curve-lab/risk-runs", json=request)

    completed = _completed_risk(client, response)
    assert completed["curve_version_id"] == version["id"]
    assert seen == [version["native_payload_hash"]]


def test_imported_runtime_manifest_enables_pv_and_node_risk(
    client,
    monkeypatch,
) -> None:
    from app.services.dal_gateway import get_gateway

    _, source = _publish_version(client)
    payload = client.get(f"/api/curve-lab/versions/{source['id']}/native-json").content
    manifest = client.get(f"/api/curve-lab/versions/{source['id']}/runtime-manifest").json()
    imported = client.post(
        "/api/curve-lab/import-jobs",
        content=payload,
        headers={
            "Content-Type": "application/json",
            "X-Curve-Lab-Runtime-Manifest": json.dumps(manifest),
        },
    )
    completed_import = _completed_import(client, imported)
    imported_version_id = completed_import["resulting_version_id"]

    gateway = get_gateway()

    def price(document, trades, _evaluation_time, base_currency, **kwargs):
        assert document["declarations"][0]["component_key"] == ("clab/v1/local/discount/USD/OIS")
        assert kwargs["curve_version"]["source_kind"] == "IMPORT"
        parameter_bumps = kwargs.get("parameter_bumps") or []
        pv = Decimal("12") + sum(
            (Decimal(str(bump)) * 4 for _, bump in parameter_bumps),
            Decimal(0),
        )
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
                "aad_node_gradient": ["4"],
            }
        ]

    monkeypatch.setattr(gateway, "price_curve_lab_trades", price)
    request = _request(imported_version_id)
    request["measures"] = ["PV"]
    request["sensitivity_layers"] = ["TRADE_TO_NODE"]
    request["options"] = {"aad_fallback": "FORBID"}

    risk = client.post("/api/curve-lab/risk-runs", json=request)

    completed_risk = _completed_risk(client, risk)
    matrix = client.get(
        f"/api/curve-lab/risk-runs/{completed_risk['id']}/matrices/trade-to-node"
    ).json()
    assert matrix["method"] == "NATIVE_AAD_PARITY_VERIFIED"
    assert matrix["values"] == [["4"]]


def test_risk_create_acknowledges_queued_before_native_pricing(
    client,
    monkeypatch,
) -> None:
    from app.services.dal_gateway import get_gateway

    _, version = _publish_version(client)
    entered = threading.Event()
    release = threading.Event()

    def slow_price(
        _document,
        trades,
        _evaluation_time,
        base_currency,
        **_kwargs,
    ):
        entered.set()
        assert release.wait(timeout=5)
        return [
            {
                "trade_id": trade["trade_id"],
                "instrument_type": trade["instrument_type"],
                "succeeded": True,
                "pv": "1",
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": [],
                "error": "",
            }
            for trade in trades
        ]

    monkeypatch.setattr(get_gateway(), "price_curve_lab_trades", slow_price)
    request = _request(version["id"])
    request["measures"] = ["PV"]

    started = time.monotonic()
    response = client.post("/api/curve-lab/risk-runs", json=request)
    elapsed = time.monotonic() - started

    assert response.status_code == 202
    assert response.json()["state"] == "QUEUED"
    assert elapsed < 0.3
    assert entered.wait(timeout=1)
    release.set()
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        completed = client.get(f"/api/curve-lab/risk-runs/{response.json()['id']}").json()
        if completed["state"] in {"SUCCEEDED", "FAILED", "TIMED_OUT"}:
            break
        time.sleep(0.01)
    assert completed["state"] == "SUCCEEDED"


@pytest.mark.parametrize("expire_after_running", [False, True])
def test_risk_deadline_terminalizes_without_partial_matrices(
    client,
    monkeypatch,
    expire_after_running: bool,
) -> None:
    import app.services.curve_risk as curve_risk

    _, version = _publish_version(client)

    class CapturingReservation:
        submitted: tuple[object, ...] | None = None

        def submit(self, function, /, *args):
            self.submitted = (function, *args)

        def cancel(self) -> None:
            pass

    reservation = CapturingReservation()
    monkeypatch.setattr(curve_risk, "_reserve_job", lambda: reservation)
    request = _request(version["id"])
    request["measures"] = ["PV"]
    request["sensitivity_layers"] = []
    response = client.post("/api/curve-lab/risk-runs", json=request)
    assert response.status_code == 202, response.text
    assert reservation.submitted is not None
    function, worker_store, gateway, snapshot = reservation.submitted

    deadline_checks = 0

    def expired(_deadline):
        nonlocal deadline_checks
        deadline_checks += 1
        return deadline_checks > int(expire_after_running)

    monkeypatch.setattr(curve_risk, "deadline_expired", expired)
    native_calls = 0

    def price(_document, trades, _evaluation_time, base_currency, **_kwargs):
        nonlocal native_calls
        native_calls += 1
        return [
            {
                "trade_id": trades[0]["trade_id"],
                "instrument_type": "DEPOSIT",
                "succeeded": True,
                "pv": "100",
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": [],
                "error": "",
            }
        ]

    monkeypatch.setattr(gateway, "price_curve_lab_trades", price)
    states: list[tuple[str, list[dict]]] = []
    original_publish = worker_store.publish_curve_lab_risk_run

    def publish(record, matrices):
        states.append((record["state"], matrices))
        return original_publish(record, matrices)

    monkeypatch.setattr(worker_store, "publish_curve_lab_risk_run", publish)

    function(worker_store, gateway, snapshot)

    completed = client.get(f"/api/curve-lab/risk-runs/{response.json()['id']}").json()
    assert completed["state"] == "TIMED_OUT"
    assert completed["error"] == {
        "code": "SOFT_DEADLINE_EXCEEDED",
        "message": "Curve Lab work exceeded its persisted soft deadline.",
        "field": "deadline_at",
        "value": completed["deadline_at"],
        "resource_id": completed["id"],
        "details": {},
    }
    assert completed["created_at"] <= completed["finished_at"]
    assert states == (
        [("RUNNING", []), ("TIMED_OUT", [])] if expire_after_running else [("TIMED_OUT", [])]
    )
    assert native_calls == int(expire_after_running)
    matrix = client.get(f"/api/curve-lab/risk-runs/{completed['id']}/matrices/key-rate-dv01")
    assert matrix.status_code == 404


def test_risk_worker_receives_one_deeply_immutable_admission_snapshot(
    client,
    monkeypatch,
) -> None:
    import app.services.curve_risk as curve_risk

    _, version = _publish_version(client)

    class CapturingReservation:
        submitted: tuple[object, ...] | None = None

        def submit(self, function, /, *args):
            self.submitted = (function, *args)

        def cancel(self) -> None:
            pass

    reservation = CapturingReservation()
    monkeypatch.setattr(curve_risk, "_reserve_job", lambda: reservation)
    original_snapshot = curve_risk._AdmittedRiskSnapshot
    snapshot_constructions = 0

    def construct_snapshot(*args, **kwargs):
        nonlocal snapshot_constructions
        snapshot_constructions += 1
        return original_snapshot(*args, **kwargs)

    monkeypatch.setattr(curve_risk, "_AdmittedRiskSnapshot", construct_snapshot)
    original_estimate = curve_risk.estimate_work
    estimate_calls = 0

    def counted_estimate(*args, **kwargs):
        nonlocal estimate_calls
        estimate_calls += 1
        return original_estimate(*args, **kwargs)

    monkeypatch.setattr(curve_risk, "estimate_work", counted_estimate)
    original_admit = curve_risk._admit_risk_run
    admitted_models: list[object] = []

    def admit(store, gateway, request, **kwargs):
        admitted_models.append(request)
        return original_admit(store, gateway, request, **kwargs)

    monkeypatch.setattr(curve_risk, "_admit_risk_run", admit)
    request = _request(version["id"])
    request["measures"] = ["PV"]
    response = client.post("/api/curve-lab/risk-runs", json=request)

    assert response.status_code == 202, response.text
    assert reservation.submitted is not None
    function, store, gateway, snapshot = reservation.submitted
    assert function is curve_risk._execute_risk_run_guarded
    assert snapshot.run_id == response.json()["id"]
    assert snapshot_constructions == 1
    assert estimate_calls == 1

    def assert_deeply_immutable(value: object) -> None:
        if is_dataclass(value):
            for field in fields(value):
                assert_deeply_immutable(getattr(value, field.name))
            return
        if isinstance(value, tuple):
            for item in value:
                assert_deeply_immutable(item)
            return
        assert value is None or isinstance(value, (bool, int, float, str, bytes))

    assert_deeply_immutable(snapshot)
    with pytest.raises(FrozenInstanceError):
        snapshot.run_id = "mutated"

    admitted_request = json.loads(snapshot.request_bytes)
    admitted_document = curve_risk._thaw_json_object(snapshot.document)
    request["target"]["trades"][0]["terms"]["notional"] = "999"
    object.__setattr__(
        admitted_models[0].target.trades[0].terms,
        "notional",
        Decimal("999"),
    )
    reconstructed = curve_risk.RiskRunRequestV2.model_validate_json(snapshot.request_bytes)
    object.__setattr__(
        reconstructed.target.trades[0].terms,
        "notional",
        Decimal("999"),
    )
    visible_version = store.get_curve_lab_version(version["id"])
    visible_version["verification"]["document"]["declarations"].clear()
    visible_build = store.get_curve_lab_build_run(visible_version["build_run_id"])
    visible_build["request"]["instruments"].clear()
    visible_fixing = store.get_curve_lab_fixing_snapshot("fixings-2026-01-15")
    visible_fixing["observations"].append({"mutated": True})

    assert json.loads(snapshot.request_bytes) == admitted_request
    assert curve_risk._thaw_json_object(snapshot.request_projection) == admitted_request
    assert curve_risk._thaw_json_object(snapshot.document) == admitted_document

    def forbidden(*_args, **_kwargs):
        raise AssertionError("worker reread mutable admitted source state")

    for method in (
        "get_curve_lab_version",
        "get_curve_lab_build_run",
        "get_curve_lab_draft",
        "get_curve_lab_import_job",
        "get_curve_lab_fixing_snapshot",
        "resolve_curve_lab_versions",
    ):
        monkeypatch.setattr(store, method, forbidden)

    def price(_document, trades, _evaluation_time, base_currency, **_kwargs):
        return [
            {
                "trade_id": trade["trade_id"],
                "instrument_type": trade["instrument_type"],
                "succeeded": True,
                "pv": "100",
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": [],
                "error": "",
            }
            for trade in trades
        ]

    monkeypatch.setattr(gateway, "price_curve_lab_trades", price)
    original_execute = curve_risk._execute_risk_run
    received_identical_snapshot: list[bool] = []

    def execute(worker_store, current_gateway, received_snapshot):
        received_identical_snapshot.append(received_snapshot is snapshot)
        return original_execute(worker_store, current_gateway, received_snapshot)

    monkeypatch.setattr(curve_risk, "_execute_risk_run", execute)

    function(store, gateway, snapshot)

    completed = client.get(f"/api/curve-lab/risk-runs/{snapshot.run_id}").json()
    assert completed["state"] == "SUCCEEDED", completed
    assert completed["result"]["pricing"][0]["pv"] == "100"
    assert received_identical_snapshot == [True]
    assert snapshot_constructions == 1
    assert estimate_calls == 1


def test_risk_queue_rejects_after_two_running_and_one_hundred_queued(
    client,
    monkeypatch,
) -> None:
    from app.services.dal_gateway import get_gateway

    _, version = _publish_version(client)
    entered = threading.Barrier(3)
    release = threading.Event()

    def blocked_price(
        _document,
        trades,
        _evaluation_time,
        base_currency,
        **_kwargs,
    ):
        if not release.is_set():
            entered.wait(timeout=5)
            assert release.wait(timeout=10)
        return [
            {
                "trade_id": trade["trade_id"],
                "instrument_type": trade["instrument_type"],
                "succeeded": True,
                "pv": "1",
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": [],
                "error": "",
            }
            for trade in trades
        ]

    monkeypatch.setattr(get_gateway(), "price_curve_lab_trades", blocked_price)
    request = _request(version["id"])
    request["measures"] = ["PV"]
    accepted: list[str] = []
    try:
        for _ in range(2):
            response = client.post("/api/curve-lab/risk-runs", json=request)
            assert response.status_code == 202, response.text
            accepted.append(response.json()["id"])
        entered.wait(timeout=5)

        for _ in range(100):
            response = client.post("/api/curve-lab/risk-runs", json=request)
            assert response.status_code == 202, response.text
            accepted.append(response.json()["id"])

        rejected = client.post("/api/curve-lab/risk-runs", json=request)
        assert rejected.status_code == 429
        assert rejected.headers["retry-after"] == "1"
        assert rejected.json()["detail"]["code"] == "CURVE_LAB_QUEUE_FULL"
    finally:
        release.set()

    for run_id in accepted:
        completed = _wait_for_job(
            client,
            "risk-runs",
            run_id,
            {"SUCCEEDED", "FAILED", "TIMED_OUT"},
        )
        assert completed["state"] == "SUCCEEDED", completed


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
    dependent_run = _wait_for_job(
        client,
        "build-runs",
        dependent_run_response.json()["id"],
        {"SUCCEEDED", "FAILED", "TIMED_OUT"},
    )
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
        curve_version=None,
        dependencies=(),
        parameter_bumps=None,
        **_kwargs,
    ):
        assert parameter_bumps is None
        observed.extend(
            (record["native_payload_hash"], record["native_payload"]) for record in dependencies
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
                "dependency_component_keys": ["clab/v1/local/discount/USD/OIS"],
                "error": "",
            }
        ]

    monkeypatch.setattr(gateway_module.get_gateway(), "price_curve_lab_trades", price)
    request = _request(dependent["id"])
    request["measures"] = ["PV", "DV01"]
    request["sensitivity_layers"] = []

    created = client.post("/api/curve-lab/risk-runs", json=request)

    _completed_risk(client, created)
    assert observed
    assert {content_hash for content_hash, _ in observed} == {source["native_payload_hash"]}
    assert all(payload for _, payload in observed)


def test_admitted_risk_snapshot_survives_source_and_dependency_archive_without_rereads(
    client,
    monkeypatch,
) -> None:
    import app.services.curve_risk as curve_risk

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
    dependent_run = _wait_for_job(
        client,
        "build-runs",
        dependent_run_response.json()["id"],
        {"SUCCEEDED", "FAILED", "TIMED_OUT"},
    )
    assert dependent_run["state"] == "SUCCEEDED", dependent_run
    dependent_response = client.post(
        "/api/curve-lab/versions",
        json={
            "draft_id": dependent_draft["id"],
            "draft_revision": dependent_draft["revision"],
            "draft_fingerprint": dependent_draft["fingerprint"],
            "build_run_id": dependent_run["id"],
            "name": "dependent snapshot source",
            "idempotency_key": "dependent-snapshot-source",
        },
    )
    assert dependent_response.status_code == 201, dependent_response.text
    dependent = dependent_response.json()

    class CapturingReservation:
        submitted: tuple[object, ...] | None = None

        def submit(self, function, /, *args):
            self.submitted = (function, *args)

        def cancel(self) -> None:
            pass

    reservation = CapturingReservation()
    monkeypatch.setattr(curve_risk, "_reserve_job", lambda: reservation)
    original_snapshot = curve_risk._AdmittedRiskSnapshot
    snapshot_constructions = 0

    def construct_snapshot(*args, **kwargs):
        nonlocal snapshot_constructions
        snapshot_constructions += 1
        return original_snapshot(*args, **kwargs)

    monkeypatch.setattr(curve_risk, "_AdmittedRiskSnapshot", construct_snapshot)
    original_estimate = curve_risk.estimate_work
    estimate_calls = 0

    def counted_estimate(*args, **kwargs):
        nonlocal estimate_calls
        estimate_calls += 1
        return original_estimate(*args, **kwargs)

    monkeypatch.setattr(curve_risk, "estimate_work", counted_estimate)
    request = _request(dependent["id"])
    request["measures"] = ["PV"]
    request["sensitivity_layers"] = []

    admitted_response = client.post("/api/curve-lab/risk-runs", json=request)

    assert admitted_response.status_code == 202, admitted_response.text
    assert reservation.submitted is not None
    function, worker_store, gateway, snapshot = reservation.submitted
    assert snapshot_constructions == 1
    assert estimate_calls == 1
    assert client.post(f"/api/curve-lab/versions/{source['id']}/archive").status_code == 200
    assert client.post(f"/api/curve-lab/versions/{dependent['id']}/archive").status_code == 200
    archived = client.get(
        "/api/curve-lab/versions",
        params={"include_archived": True},
    ).json()
    archived_by_id = {version["id"]: version for version in archived}
    assert archived_by_id[source["id"]]["visibility_state"] == "ARCHIVED"
    assert archived_by_id[dependent["id"]]["visibility_state"] == "ARCHIVED"

    def forbidden(*_args, **_kwargs):
        raise AssertionError("worker reread mutable admitted source state")

    for method in (
        "get_curve_lab_version",
        "get_curve_lab_build_run",
        "get_curve_lab_draft",
        "get_curve_lab_import_job",
        "get_curve_lab_fixing_snapshot",
        "resolve_curve_lab_versions",
    ):
        monkeypatch.setattr(worker_store, method, forbidden)

    observed_dependencies: list[str] = []

    def price(
        _document,
        trades,
        _evaluation_time,
        base_currency,
        *,
        curve_version,
        dependencies,
        **_kwargs,
    ):
        assert curve_version["id"] == dependent["id"]
        assert isinstance(curve_version["native_payload"], bytes)
        observed_dependencies.extend(item["id"] for item in dependencies)
        assert all(isinstance(item["native_payload"], bytes) for item in dependencies)
        return [
            {
                "trade_id": trades[0]["trade_id"],
                "instrument_type": "DEPOSIT",
                "succeeded": True,
                "pv": "100",
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": ["clab/v1/local/discount/USD/OIS"],
                "error": "",
            }
        ]

    monkeypatch.setattr(gateway, "price_curve_lab_trades", price)
    original_execute = curve_risk._execute_risk_run
    received_identical_snapshot: list[bool] = []

    def execute(store, current_gateway, received_snapshot):
        received_identical_snapshot.append(received_snapshot is snapshot)
        return original_execute(store, current_gateway, received_snapshot)

    monkeypatch.setattr(curve_risk, "_execute_risk_run", execute)

    function(worker_store, gateway, snapshot)

    completed = client.get(f"/api/curve-lab/risk-runs/{admitted_response.json()['id']}").json()
    assert completed["state"] == "SUCCEEDED", completed
    assert completed["result"]["pricing"][0]["pv"] == "100"
    assert observed_dependencies == [source["id"]]
    assert received_identical_snapshot == [True]
    assert snapshot_constructions == 1
    assert estimate_calls == 1


def test_staged_xccy_snapshot_preserves_dependency_axis_and_aad_order_after_archive(
    client,
    monkeypatch,
) -> None:
    import app.services.curve_risk as curve_risk
    from app.services.canonical_json import canonical_json_bytes
    from app.services.curve_lab_plan import resolved_declaration_order
    from app.services.store import get_store

    store = get_store()
    fixing_response = client.post(
        "/api/curve-lab/fixing-snapshots",
        json={"id": "staged-xccy-fixings", "observations": []},
    )
    assert fixing_response.status_code == 201, fixing_response.text

    domestic_key = "clab/v1/local/discount/USD/OIS"
    foreign_key = "clab/v1/local/discount/EUR/OIS"
    basis_key = "clab/v1/xccy/basis/USD-EUR/3M"
    domestic_id = "d" * 32
    foreign_id = "e" * 32
    staged_id = "f" * 32
    build_id = "b" * 32
    draft_id = "a" * 32
    created_at = "2026-01-15T10:00:00Z"

    def add_dependency(
        version_id: str,
        component_key: str,
        currency: str,
        payload: bytes,
    ) -> dict:
        content_hash = hashlib.sha256(payload).hexdigest()
        document = {
            "mode": "SINGLE",
            "declarations": [
                {
                    "component_key": component_key,
                    "role": "DISCOUNT",
                    "currency": currency,
                    "parameterization": "PIECEWISE_CONSTANT_FWD",
                }
            ],
            "dependency_version_ids": [],
        }
        record = {
            "id": version_id,
            "idempotency_key": f"staged-{currency.lower()}-dependency",
            "source_kind": "IMPORT",
            "build_run_id": None,
            "import_job_id": None,
            "native_payload": payload,
            "native_payload_length": len(payload),
            "native_payload_hash": content_hash,
            "archive_numeric_format": "JSON_MAX_DIGITS10_V1",
            "root_kind": "DISCOUNT_CURVE",
            "build_validation_state": "VERIFIED",
            "visibility_state": "VISIBLE",
            "name": f"{currency} dependency",
            "version_note": None,
            "tags": [],
            "verification": {
                "document": document,
                "dependency_manifest": [],
                "resolved_plan": {},
                "quote_axis": [],
                "parameter_axis": [],
            },
            "created_at": created_at,
        }
        stored, was_created = store.add_curve_lab_version(record)
        assert was_created is True
        return stored

    domestic = add_dependency(domestic_id, domestic_key, "USD", b"domestic-archive")
    foreign = add_dependency(foreign_id, foreign_key, "EUR", b"foreign-archive")
    dependency_manifest = [
        {
            "version_id": domestic_id,
            "content_hash": domestic["native_payload_hash"],
            "root_kind": domestic["root_kind"],
        },
        {
            "version_id": foreign_id,
            "content_hash": foreign["native_payload_hash"],
            "root_kind": foreign["root_kind"],
        },
    ]
    staged_document = {
        "schema_version": 2,
        "mode": "STAGED_XCCY",
        "as_of_date": "2026-01-15",
        "market_snapshot_id": "staged-xccy-market",
        "declarations": [
            {
                "component_key": basis_key,
                "role": "BASIS",
                "currency": "USD",
                "parameterization": "PIECEWISE_CONSTANT_FWD",
            },
            {
                "component_key": foreign_key,
                "role": "DISCOUNT",
                "currency": "EUR",
                "parameterization": "PIECEWISE_CONSTANT_FWD",
            },
            {
                "component_key": domestic_key,
                "role": "DISCOUNT",
                "currency": "USD",
                "parameterization": "PIECEWISE_CONSTANT_FWD",
            },
        ],
        "instruments": [
            {
                "instrument_id": "9" * 32,
                "instrument_type": "XCCY",
                "trade_date": "2026-01-15",
                "start_date": "2026-01-16",
                "maturity_date": "2028-01-15",
                "currency_or_pair": "USD-EUR",
                "raw_quote": "0.001",
                "normalized_quote": "0.001",
                "included": True,
                "terms": {
                    "component_key": basis_key,
                    "fx_spot": 1.1,
                },
            }
        ],
        "dependency_version_ids": [domestic_id, foreign_id],
        "solver": {
            "solve_mode": "EXACT",
            "parameterization": "PIECEWISE_CONSTANT_FWD",
        },
    }
    parameter_axis = [
        {
            "global_parameter_index": 0,
            "parameter_id": f"{domestic_key}:PIECEWISE_CONSTANT_FWD:2028-01-15:RIGHT",
            "component_key": domestic_key,
            "stage_id": "stage-0",
            "stage_local_parameter_index": 0,
            "component_local_parameter_index": 0,
            "coordinate_kind": "PIECEWISE_CONSTANT_FWD",
            "node_date": "2028-01-15",
            "side": "RIGHT",
            "native_parameter_unit": "DECIMAL_RATE",
            "display_label": "USD late",
        },
        {
            "global_parameter_index": 1,
            "parameter_id": f"{domestic_key}:PIECEWISE_CONSTANT_FWD:2027-01-15:RIGHT",
            "component_key": domestic_key,
            "stage_id": "stage-0",
            "stage_local_parameter_index": 1,
            "component_local_parameter_index": 1,
            "coordinate_kind": "PIECEWISE_CONSTANT_FWD",
            "node_date": "2027-01-15",
            "side": "RIGHT",
            "native_parameter_unit": "DECIMAL_RATE",
            "display_label": "USD early",
        },
        {
            "global_parameter_index": 2,
            "parameter_id": f"{foreign_key}:PIECEWISE_CONSTANT_FWD:2028-01-15:RIGHT",
            "component_key": foreign_key,
            "stage_id": "stage-1",
            "stage_local_parameter_index": 0,
            "component_local_parameter_index": 0,
            "coordinate_kind": "PIECEWISE_CONSTANT_FWD",
            "node_date": "2028-01-15",
            "side": "RIGHT",
            "native_parameter_unit": "DECIMAL_RATE",
            "display_label": "EUR duplicate date",
        },
        {
            "global_parameter_index": 3,
            "parameter_id": f"{basis_key}:PIECEWISE_CONSTANT_FWD:2028-01-15:RIGHT",
            "component_key": basis_key,
            "stage_id": "stage-2",
            "stage_local_parameter_index": 0,
            "component_local_parameter_index": 0,
            "coordinate_kind": "PIECEWISE_CONSTANT_FWD",
            "node_date": "2028-01-15",
            "side": "RIGHT",
            "native_parameter_unit": "DECIMAL_RATE",
            "display_label": "basis duplicate date",
        },
        {
            "global_parameter_index": 4,
            "parameter_id": f"{basis_key}:PIECEWISE_CONSTANT_FWD:2029-01-15:RIGHT",
            "component_key": basis_key,
            "stage_id": "stage-2",
            "stage_local_parameter_index": 1,
            "component_local_parameter_index": 1,
            "coordinate_kind": "PIECEWISE_CONSTANT_FWD",
            "node_date": "2029-01-15",
            "side": "RIGHT",
            "native_parameter_unit": "DECIMAL_RATE",
            "display_label": "basis later knot",
        },
    ]
    quote_axis = [
        {
            "global_quote_index": 0,
            "quote_id": "9" * 32,
            "instrument_id": "9" * 32,
            "component_key": basis_key,
            "stage_id": "stage-2",
            "group_id": basis_key,
            "stage_local_quote_index": 0,
            "quote_coordinate_kind": "SPREAD",
            "canonical_raw_unit": "DECIMAL",
            "raw_quote": "0.001",
            "normalized_quote": "0.001",
            "normalized_unit": "DECIMAL_RATE",
            "exact_risk_raw_bump": "0.0001",
            "normalized_risk_bump": "0.0001",
            "display_label": "XCCY 2028-01-15",
        }
    ]
    store.add_curve_lab_draft(
        {
            "id": draft_id,
            "schema_version": 2,
            "revision": 1,
            "fingerprint": "1" * 64,
            "document": staged_document,
            "state": "ACTIVE",
            "created_at": created_at,
            "updated_at": created_at,
        }
    )
    staged_payload = b"staged-xccy-archive"
    staged_hash = hashlib.sha256(staged_payload).hexdigest()
    resolved_plan = {
        "mode": "STAGED_XCCY",
        "runtime_manifest": {
            "components": [
                {"component_key": domestic_key},
                {"component_key": foreign_key},
                {"component_key": basis_key},
            ]
        },
    }
    store.add_curve_lab_build_run(
        {
            "id": build_id,
            "draft_id": draft_id,
            "draft_revision": 1,
            "draft_fingerprint": "1" * 64,
            "state": "SUCCEEDED",
            "request": staged_document,
            "resolved_plan": resolved_plan,
            "quote_axis": quote_axis,
            "parameter_axis": parameter_axis,
            "dependency_manifest": dependency_manifest,
            "native_payload": staged_payload,
            "native_payload_hash": staged_hash,
            "diagnostics": {},
            "error": None,
            "created_at": created_at,
            "deadline_at": "2099-01-01T00:00:00Z",
            "finished_at": created_at,
        }
    )
    staged_version = {
        "id": staged_id,
        "idempotency_key": "staged-xccy-source",
        "source_kind": "BUILD",
        "build_run_id": build_id,
        "import_job_id": None,
        "native_payload": staged_payload,
        "native_payload_length": len(staged_payload),
        "native_payload_hash": staged_hash,
        "archive_numeric_format": "JSON_MAX_DIGITS10_V1",
        "root_kind": "CURVE_SET",
        "build_validation_state": "VERIFIED",
        "visibility_state": "VISIBLE",
        "name": "staged XCCY",
        "version_note": None,
        "tags": [],
        "verification": {
            "draft_id": draft_id,
            "draft_revision": 1,
            "draft_fingerprint": "1" * 64,
            "document": staged_document,
            "dependency_manifest": dependency_manifest,
            "resolved_plan": resolved_plan,
            "quote_axis": quote_axis,
            "parameter_axis": parameter_axis,
        },
        "created_at": created_at,
    }
    _, was_created = store.add_curve_lab_version(staged_version)
    assert was_created is True

    assert [
        declaration["component_key"] for declaration in resolved_declaration_order(staged_document)
    ] == [domestic_key, foreign_key, basis_key]
    assert [entry["version_id"] for entry in dependency_manifest] == [
        domestic_id,
        foreign_id,
    ]
    assert [axis["parameter_id"] for axis in parameter_axis] == [
        f"{domestic_key}:PIECEWISE_CONSTANT_FWD:2028-01-15:RIGHT",
        f"{domestic_key}:PIECEWISE_CONSTANT_FWD:2027-01-15:RIGHT",
        f"{foreign_key}:PIECEWISE_CONSTANT_FWD:2028-01-15:RIGHT",
        f"{basis_key}:PIECEWISE_CONSTANT_FWD:2028-01-15:RIGHT",
        f"{basis_key}:PIECEWISE_CONSTANT_FWD:2029-01-15:RIGHT",
    ]
    assert [axis["node_date"] for axis in parameter_axis if axis["component_key"] == basis_key] == [
        "2028-01-15",
        "2029-01-15",
    ]

    class CapturingReservation:
        submitted: tuple[object, ...] | None = None

        def submit(self, function, /, *args):
            self.submitted = (function, *args)

        def cancel(self) -> None:
            pass

    reservations: list[CapturingReservation] = []

    def reserve():
        reservation = CapturingReservation()
        reservations.append(reservation)
        return reservation

    monkeypatch.setattr(curve_risk, "_reserve_job", reserve)
    original_snapshot = curve_risk._AdmittedRiskSnapshot
    snapshot_constructions = 0

    def construct_snapshot(*args, **kwargs):
        nonlocal snapshot_constructions
        snapshot_constructions += 1
        return original_snapshot(*args, **kwargs)

    monkeypatch.setattr(curve_risk, "_AdmittedRiskSnapshot", construct_snapshot)
    original_estimate = curve_risk.estimate_work
    estimate_calls = 0

    def estimate(*args, **kwargs):
        nonlocal estimate_calls
        estimate_calls += 1
        return original_estimate(*args, **kwargs)

    monkeypatch.setattr(curve_risk, "estimate_work", estimate)
    from app.services.dal_gateway import get_gateway

    risk_gateway = get_gateway()
    monkeypatch.setattr(
        risk_gateway,
        "curve_lab_required_historical_fixings",
        lambda *_args, **_kwargs: [],
    )
    dependency_orders: list[list[str]] = []
    native_gradient = ["1.25", "2.5", "3.75", "5", "6.25"]
    central_gradient = {
        _trade(0)["trade_id"]: [Decimal(value) for value in native_gradient],
        "2" * 32: [
            Decimal("10"),
            Decimal("20"),
            Decimal("30"),
            Decimal("40"),
            Decimal("50"),
        ],
    }

    def price(
        _document,
        trades,
        _evaluation_time,
        base_currency,
        *,
        dependencies,
        **_kwargs,
    ):
        dependency_orders.append([dependency["id"] for dependency in dependencies])
        return [
            {
                "trade_id": trade["trade_id"],
                "instrument_type": trade["instrument_type"],
                "succeeded": True,
                "pv": "100" if trade["instrument_type"] == "DEPOSIT" else "200",
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": [
                    domestic_key,
                    foreign_key,
                    basis_key,
                ],
                "error": "",
                "aad_node_gradient": (
                    native_gradient if trade["instrument_type"] == "DEPOSIT" else None
                ),
            }
            for trade in trades
        ]

    def parameter_bump(
        _document,
        trades,
        _evaluation_time,
        base_currency,
        axis,
        bump,
        *,
        dependencies,
        **_kwargs,
    ):
        dependency_orders.append([dependency["id"] for dependency in dependencies])
        parameter_index = next(
            index
            for index, item in enumerate(parameter_axis)
            if item["parameter_id"] == axis["parameter_id"]
        )
        return [
            {
                "trade_id": trade["trade_id"],
                "instrument_type": trade["instrument_type"],
                "succeeded": True,
                "pv": str(
                    (Decimal("100") if trade["instrument_type"] == "DEPOSIT" else Decimal("200"))
                    + Decimal(str(bump)) * central_gradient[trade["trade_id"]][parameter_index]
                ),
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": [],
                "error": "",
            }
            for trade in trades
        ]

    monkeypatch.setattr(risk_gateway, "price_curve_lab_trades", price)
    monkeypatch.setattr(
        risk_gateway,
        "price_curve_lab_parameter_bump",
        parameter_bump,
    )
    audit_actions: list[tuple[str, str]] = []
    original_audit = store.add_curve_lab_audit_event

    def add_audit(record):
        audit_actions.append((record["action"], record["target_id"]))
        original_audit(record)

    monkeypatch.setattr(store, "add_curve_lab_audit_event", add_audit)
    publications: list[tuple[str, str, list[str]]] = []
    original_publish = store.publish_curve_lab_risk_run

    def publish(record, matrices):
        publications.append(
            (
                record["id"],
                record["state"],
                [matrix["matrix_id"] for matrix in matrices],
            )
        )
        return original_publish(record, matrices)

    monkeypatch.setattr(store, "publish_curve_lab_risk_run", publish)

    deposit = _trade(0)
    deposit["terms"]["discount_component_key"] = domestic_key
    xccy = {
        "trade_id": "2" * 32,
        "instrument_type": "XCCY",
        "trade_date": "2026-01-15",
        "start_date": "2026-01-16",
        "maturity_date": "2028-01-15",
        "currency_or_pair": "USD-EUR",
        "terms": {
            "position_count": "1",
            "contract_spread": "0.001",
            "side": "RECEIVE_NON_SPREAD_PAY_SPREAD",
            "domestic_notional": "100",
            "foreign_notional": "90",
            "fx_spot": "1.1",
        },
    }

    def request(trades: list[dict], fallback: str) -> dict:
        return {
            "curve_version_id": staged_id,
            "target": {"trades": trades},
            "measures": ["PV"],
            "sensitivity_layers": ["TRADE_TO_NODE"],
            "fixing_snapshot_id": "staged-xccy-fixings",
            "evaluation_time": "2026-01-15T10:30:00Z",
            "base_currency": "USD",
            "options": {"aad_fallback": fallback},
        }

    full_response = client.post(
        "/api/curve-lab/risk-runs",
        json=request([deposit], "FORBID"),
    )
    assert full_response.status_code == 202, full_response.text
    assert reservations[0].submitted is not None
    full_function, full_store, full_gateway, full_snapshot = reservations[0].submitted
    assert full_function is curve_risk._execute_risk_run_guarded
    curve_risk._execute_risk_run(full_store, full_gateway, full_snapshot)
    full_run = client.get(f"/api/curve-lab/risk-runs/{full_response.json()['id']}").json()
    assert full_run["state"] == "SUCCEEDED", full_run
    full_matrix = client.get(
        f"/api/curve-lab/risk-runs/{full_run['id']}/matrices/trade-to-node"
    ).json()
    assert full_matrix["values"] == [native_gradient]
    assert full_matrix["trade_methods"] == ["NATIVE_AAD"]

    rejected = client.post(
        "/api/curve-lab/risk-runs",
        json=request([deposit, xccy], "FORBID"),
    )
    assert rejected.status_code == 422, rejected.text
    assert rejected.json()["detail"] == {
        "code": "AAD_METHOD_UNAVAILABLE",
        "message": "At least one admitted trade has no complete native AAD pricing plan.",
        "field": "options.aad_fallback",
        "value": "FORBID",
        "resource_id": None,
        "details": {"constraint": "statically ineligible trades require aad_fallback=ALLOW"},
    }
    assert len(reservations) == 1

    partial_request = request([deposit, xccy], "ALLOW")
    control_response = client.post("/api/curve-lab/risk-runs", json=partial_request)
    assert control_response.status_code == 202, control_response.text
    assert reservations[1].submitted is not None
    control_function, control_store, control_gateway, control_snapshot = reservations[1].submitted
    assert control_function is curve_risk._execute_risk_run_guarded
    curve_risk._execute_risk_run(control_store, control_gateway, control_snapshot)
    control_run = client.get(f"/api/curve-lab/risk-runs/{control_response.json()['id']}").json()
    control_matrix = client.get(
        f"/api/curve-lab/risk-runs/{control_run['id']}/matrices/trade-to-node"
    ).json()
    assert control_run["state"] == "SUCCEEDED", control_run
    assert control_matrix["values"] == [
        native_gradient,
        ["10", "20", "30", "40", "50"],
    ]
    assert control_matrix["trade_methods"] == [
        "NATIVE_AAD",
        "CENTRAL_NATIVE_PARAMETER_BUMP",
    ]
    assert set(control_run["result"]) == {"pricing", "sensitivity_matrices"}

    archived_response = client.post("/api/curve-lab/risk-runs", json=partial_request)
    assert archived_response.status_code == 202, archived_response.text
    assert reservations[2].submitted is not None
    archived_function, archived_store, archived_gateway, archived_snapshot = reservations[
        2
    ].submitted
    assert [
        curve_risk._thaw_json_object(item)["id"] for item in archived_snapshot.dependencies
    ] == [domestic_id, foreign_id]
    assert [
        curve_risk._thaw_json_object(item)["parameter_id"]
        for item in archived_snapshot.parameter_axis
    ] == [axis["parameter_id"] for axis in parameter_axis]

    for version_id in (domestic_id, foreign_id, staged_id):
        archived = client.post(f"/api/curve-lab/versions/{version_id}/archive")
        assert archived.status_code == 200, archived.text
        assert archived.json()["visibility_state"] == "ARCHIVED"

    def forbidden(*_args, **_kwargs):
        raise AssertionError("staged worker reread archived mutable state")

    for method in (
        "get_curve_lab_version",
        "get_curve_lab_build_run",
        "get_curve_lab_draft",
        "get_curve_lab_import_job",
        "get_curve_lab_fixing_snapshot",
        "resolve_curve_lab_versions",
    ):
        monkeypatch.setattr(archived_store, method, forbidden)
    original_execute = curve_risk._execute_risk_run
    worker_identity: list[bool] = []

    def execute(worker_store, gateway_instance, received_snapshot):
        worker_identity.append(received_snapshot is archived_snapshot)
        return original_execute(worker_store, gateway_instance, received_snapshot)

    monkeypatch.setattr(curve_risk, "_execute_risk_run", execute)
    archived_function(archived_store, archived_gateway, archived_snapshot)

    archived_run = client.get(f"/api/curve-lab/risk-runs/{archived_response.json()['id']}").json()
    archived_matrix = client.get(
        f"/api/curve-lab/risk-runs/{archived_run['id']}/matrices/trade-to-node"
    ).json()

    stable_run_fields = (
        "curve_version_id",
        "calibration_run_id",
        "import_job_id",
        "source_kind",
        "request",
        "fixing_snapshot_hash",
        "target_fingerprint",
        "quote_axis",
        "parameter_axis",
        "estimated_work",
        "state",
        "result",
        "error",
    )
    assert canonical_json_bytes(
        {field: control_run[field] for field in stable_run_fields}
    ) == canonical_json_bytes({field: archived_run[field] for field in stable_run_fields})
    stable_matrix_fields = tuple(field for field in control_matrix if field not in {"risk_run_id"})
    assert canonical_json_bytes(
        {field: control_matrix[field] for field in stable_matrix_fields}
    ) == canonical_json_bytes({field: archived_matrix[field] for field in stable_matrix_fields})
    assert archived_run["created_at"] <= archived_run["finished_at"]
    assert worker_identity == [True]
    assert snapshot_constructions == 3
    assert estimate_calls == 3
    assert all(order == [domestic_id, foreign_id] for order in dependency_orders)
    assert audit_actions[-4:] == [
        ("VERSION_ARCHIVED", domestic_id),
        ("VERSION_ARCHIVED", foreign_id),
        ("VERSION_ARCHIVED", staged_id),
        ("RISK_RUN_SUCCEEDED", archived_run["id"]),
    ]
    for run_id in (full_run["id"], control_run["id"], archived_run["id"]):
        assert [
            (state, matrix_ids)
            for published_id, state, matrix_ids in publications
            if published_id == run_id
        ] == [
            ("QUEUED", []),
            ("RUNNING", []),
            ("SUCCEEDED", ["trade-to-node"]),
        ]


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
        curve_version=None,
        dependencies=(),
        **_kwargs,
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

    run = _completed_risk(client, created)
    assert calls == ["0.04", "0.0401", "0.0401"]
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


def test_risk_quote_replay_delegates_to_the_exact_decimal_bump(monkeypatch) -> None:
    import app.services.curve_risk as curve_risk

    calls: list[tuple[str, str]] = []

    def apply(value: str, bump: str) -> str:
        calls.append((value, bump))
        return "0.0401"

    monkeypatch.setattr(curve_risk, "apply_exact_decimal_bump", apply)
    document = _document()
    document["instruments"][0]["instrument_id"] = "a" * 32
    axis = [
        {
            "instrument_id": "a" * 32,
            "raw_quote": "0.04",
            "exact_risk_raw_bump": "0.0001",
            "canonical_raw_unit": "DECIMAL",
        }
    ]

    bumped = curve_risk._bumped_document(document, axis, None)

    assert calls == [("0.04", "0.0001")]
    assert bumped["instruments"][0]["raw_quote"] == "0.0401"


def test_import_job_is_readable_and_import_quote_risk_requires_lineage(client) -> None:
    _, version = _publish_version(client)
    payload = client.get(f"/api/curve-lab/versions/{version['id']}/native-json").content
    imported = client.post(
        "/api/curve-lab/import-jobs",
        content=payload,
        headers={"Content-Type": "application/json"},
    )
    job = _completed_import(client, imported)
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
        curve_version=None,
        dependencies=(),
        **_kwargs,
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

    run = _completed_risk(client, response)
    rows = run["result"]["pricing"]
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
    assert "/home/builder" not in json.dumps(run)
    assert "curve.cpp" not in json.dumps(run)
    assert "CalibrateCurve" not in json.dumps(run)


def test_work_estimator_charges_full_two_parameter_bumps_per_trade() -> None:
    from inspect import signature

    from app.services.curve_risk import estimate_work

    assert "allow_aad_fallback" not in signature(estimate_work).parameters
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
        curve_version=None,
        dependencies=(),
        **_kwargs,
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
        curve_version=None,
        dependencies=(),
        **_kwargs,
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

    run = _completed_risk(client, response)
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


def test_forbid_fallback_publishes_native_aad_only_after_central_parity(
    client,
    monkeypatch,
) -> None:
    import app.services.dal_gateway as gateway_module

    _, version = _publish_version(client)
    gateway = gateway_module.get_gateway()
    bumps: list[float] = []

    def price(_document, trades, _evaluation_time, base_currency, **_kwargs):
        return [
            {
                "trade_id": trades[0]["trade_id"],
                "instrument_type": "DEPOSIT",
                "succeeded": True,
                "pv": "10",
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": ["clab/v1/local/discount/USD/OIS"],
                "error": "",
                "aad_node_gradient": ["3"],
            }
        ]

    def parameter_bump(
        _document,
        trades,
        _evaluation_time,
        base_currency,
        _axis,
        bump,
        **_kwargs,
    ):
        bumps.append(bump)
        return [
            {
                "trade_id": trades[0]["trade_id"],
                "instrument_type": "DEPOSIT",
                "succeeded": True,
                "pv": str(Decimal("10") + Decimal(str(bump)) * 3),
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": [],
                "error": "",
            }
        ]

    monkeypatch.setattr(gateway, "price_curve_lab_trades", price)
    monkeypatch.setattr(gateway, "price_curve_lab_parameter_bump", parameter_bump)
    request = _request(version["id"])
    request["measures"] = ["PV"]
    request["sensitivity_layers"] = ["TRADE_TO_NODE"]
    request["options"] = {"aad_fallback": "FORBID"}

    response = client.post("/api/curve-lab/risk-runs", json=request)

    run = _completed_risk(client, response)
    matrix = client.get(f"/api/curve-lab/risk-runs/{run['id']}/matrices/trade-to-node").json()
    assert run["estimated_work"]["N_param"] == 2
    assert run["estimated_work"]["parameter_bump_price_evaluations"] == 2
    assert matrix["method"] == "NATIVE_AAD_PARITY_VERIFIED"
    assert matrix["trade_methods"] == ["NATIVE_AAD"]
    assert matrix["values"] == [["3"]]
    assert matrix["aad_parity"][0]["status"] == "PASSED"
    assert matrix["aad_parity"][0]["central_values"] == ["3"]
    assert matrix["aad_parity"][0]["aad_values"] == ["3"]
    assert bumps == [1e-06, -1e-06]


def test_aad_parity_failure_reuses_central_row_when_fallback_is_allowed(
    client,
    monkeypatch,
) -> None:
    from app.services.dal_gateway import get_gateway

    _, version = _publish_version(client)
    gateway = get_gateway()
    bumps: list[float] = []

    def price(_document, trades, _evaluation_time, base_currency, **_kwargs):
        return [
            {
                "trade_id": trades[0]["trade_id"],
                "instrument_type": "DEPOSIT",
                "succeeded": True,
                "pv": "10",
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": [],
                "error": "",
                "aad_node_gradient": ["3"],
            }
        ]

    def parameter_bump(
        _document,
        trades,
        _evaluation_time,
        base_currency,
        _axis,
        bump,
        **_kwargs,
    ):
        bumps.append(bump)
        return [
            {
                "trade_id": trades[0]["trade_id"],
                "instrument_type": "DEPOSIT",
                "succeeded": True,
                "pv": str(Decimal("10") + Decimal(str(bump)) * 2),
                "currency": base_currency,
                "required_historical_fixings": [],
                "missing_historical_fixings": [],
                "dependency_component_keys": [],
                "error": "",
            }
        ]

    monkeypatch.setattr(gateway, "price_curve_lab_trades", price)
    monkeypatch.setattr(gateway, "price_curve_lab_parameter_bump", parameter_bump)
    request = _request(version["id"])
    request["measures"] = ["PV"]
    request["sensitivity_layers"] = ["TRADE_TO_NODE"]
    request["options"] = {"aad_fallback": "ALLOW"}

    run = _completed_risk(client, client.post("/api/curve-lab/risk-runs", json=request))

    matrix = client.get(f"/api/curve-lab/risk-runs/{run['id']}/matrices/trade-to-node").json()
    assert matrix["method"] == "CENTRAL_PARAMETER_BUMP_AFTER_AAD_PARITY_FAILURE"
    assert matrix["trade_methods"] == ["CENTRAL_PARAMETER_BUMP_AFTER_AAD_PARITY_FAILURE"]
    assert matrix["values"] == [["2"]]
    assert matrix["aad_parity"][0]["status"] == "FAILED"
    assert bumps == [1e-06, -1e-06]


def test_forbidden_jacobian_replay_rejects_before_native_dispatch(client, monkeypatch) -> None:
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
