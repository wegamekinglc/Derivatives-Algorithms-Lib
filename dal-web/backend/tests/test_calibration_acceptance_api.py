from __future__ import annotations

import copy
import time
from types import SimpleNamespace
from unittest import mock

import pytest
from app.routers import calibrations as calibration_router
from app.schemas.calibrations import SingleCalibrationRequest
from app.services import calibrations as calibration_service
from app.services.dal_gateway import SingleGatewayAdmissionRequest, get_gateway
from app.services.store import get_store
from fastapi.testclient import TestClient

from tests.calibration_contract_fixtures import (
    first_offender_request,
    future_knots,
    joint_capacity_request,
    joint_request,
    matrix_metadata_request,
    policy_resolution_request,
    single_request,
    staged_request,
    submit_and_wait,
    wait_for_terminal,
)


def test_fix_b6_b7_api_02_staged_and_joint_complete_through_production_api(
    client,
) -> None:
    """FIX-B6/FIX-B7 — API-02 observes all three production POST surfaces."""
    usd = submit_and_wait(
        client,
        "/api/calibrations/single",
        single_request("USD", 0.04),
    )
    eur = submit_and_wait(
        client,
        "/api/calibrations/single",
        single_request("EUR", 0.03),
    )
    assert usd["status"] == eur["status"] == "completed"

    staged = submit_and_wait(
        client,
        "/api/calibrations/xccy/staged",
        staged_request(usd["curves"][0]["id"], eur["curves"][0]["id"]),
    )
    joint = submit_and_wait(
        client,
        "/api/calibrations/xccy/joint",
        joint_request(),
    )

    assert staged["status"] == joint["status"] == "completed"
    assert staged["kind"] == "xccy_staged"
    assert joint["kind"] == "xccy_joint"
    assert staged["actual_jacobian_mode"] == "ANALYTIC"
    assert joint["actual_jacobian_mode"] == "ANALYTIC"
    assert staged["jacobian"]["shape"] == [1, 1]
    assert joint["jacobian"]["shape"] == [3, 3]


@pytest.mark.parametrize("policy", ("INPUT", "INSTRUMENTS", "AUGMENTED"))
def test_fix_cb1_policy_resolution_matches_direct_planner_and_persistence(
    client,
    policy: str,
) -> None:
    """FIX-CB1-POLICY-RESOLUTION — API-10 authoritative full plan."""
    payload = policy_resolution_request(policy)
    request = SingleCalibrationRequest.model_validate(payload)
    gateway = get_gateway()
    direct = gateway._plan_single(
        SingleGatewayAdmissionRequest(request, {})
    ).to_bounded_dto()
    expected = direct.model_dump(mode="json")

    with mock.patch.object(
        gateway,
        "_plan_single",
        wraps=gateway._plan_single,
    ) as planner:
        submitted = client.post("/api/calibrations/single", json=payload)

    assert submitted.status_code == 202
    body = submitted.json()
    run_id = body["id"]
    terminal = wait_for_terminal(
        client,
        {"location": submitted.headers["location"]},
    )
    persisted = get_store().get_calibration_run(run_id)
    assert planner.call_count == 1
    assert body["resolved_knot_plan"] == expected
    assert terminal["resolved_knot_plan"] == expected
    assert persisted.resolved_knot_plan == expected
    assert (
        body["resolved_knot_plan_hash"]
        == terminal["resolved_knot_plan_hash"]
        == persisted.resolved_knot_plan_hash
        == calibration_service.canonical_model_hash(direct)
    )
    assert body["expected_execution_identity"]["execution_policy"] == "INPUT"
    assert terminal["actual_execution_identity"]["execution_policy"] == "INPUT"

    expected_trace = {
        "INPUT": [
            ("INPUT", "ADDED"),
            ("INPUT", "ADDED"),
        ],
        "INSTRUMENTS": [
            ("INSTRUMENT_START", "FILTERED_NOT_AFTER_TODAY"),
            ("INSTRUMENT_END", "ADDED"),
            ("INSTRUMENT_START", "ADDED"),
            ("INSTRUMENT_END", "ADDED"),
        ],
        "AUGMENTED": [
            ("INPUT", "ADDED"),
            ("INPUT", "ADDED"),
            ("INSTRUMENT_START", "FILTERED_NOT_AFTER_TODAY"),
            ("INSTRUMENT_END", "DUPLICATE"),
            ("INSTRUMENT_START", "ADDED"),
            ("INSTRUMENT_END", "DUPLICATE"),
        ],
    }[policy]
    assert [
        (item["origin"]["kind"], item["disposition"])
        for item in expected["candidate_trace"]
    ] == expected_trace


@pytest.mark.parametrize("source", ("input", "start", "maturity", "augmented"))
def test_fix_cb1_first_offender_preserves_traversal_source_and_origins(
    client,
    source: str,
) -> None:
    """FIX-CB1-FIRST-OFFENDER — API-11 retains traversal source/order."""
    payload, expected = first_offender_request(source)
    gateway = get_gateway()
    store = get_store()
    with (
        mock.patch.object(
            gateway,
            "_plan_single",
            wraps=gateway._plan_single,
        ) as planner,
        mock.patch.object(
            store,
            "add_calibration_admission",
            wraps=store.add_calibration_admission,
        ) as insert,
        mock.patch.object(
            gateway,
            "calibrate_single",
            wraps=gateway.calibrate_single,
        ) as solve,
    ):
        response = client.post("/api/calibrations/single", json=payload)

    assert response.status_code == 422
    error = response.json()["error"]
    assert error["code"] == "CURVE_STORAGE_NODE_LIMIT_EXCEEDED"
    assert error["location"] == expected["location"]
    assert error["context"]["candidate_ordinal"] == expected["candidate_ordinal"]
    assert error["context"]["candidate_date"] == expected["candidate_date"]
    for key, value in expected["origin"].items():
        assert error["context"]["candidate_origin"][key] == value
    assert error["context"]["origins"]
    assert error["context"]["origins"][-1] == error["context"]["candidate_origin"]
    assert planner.call_count == 1
    assert insert.call_count == solve.call_count == 0


def test_fix_b2_input_zero_planner_precedence_has_exact_api_12_counts(
    client,
) -> None:
    """FIX-INPUT-ZERO-PLANNER-PRECEDENCE — API-12 observes 1/1/0/0."""
    request = single_request("USD", 0.02)
    request["declaration"].update(
        {
            "parameterization": "ZERO_RATE",
            "log_df_scheme": "LOG_LINEAR",
            "knot_dates": future_knots(100),
            "initial_guess_per_node": [0.02],
        }
    )
    request["solver"]["solve_mode"] = "EXACT"
    request["options"] = {
        "jacobian_mode": "ANALYTIC",
        "include_jacobian": True,
        "include_effective_inverse": True,
    }
    gateway = get_gateway()
    store = get_store()
    with (
        mock.patch.object(
            gateway,
            "plan_single_admission",
            wraps=gateway.plan_single_admission,
        ) as admission,
        mock.patch.object(
            gateway,
            "_plan_single",
            wraps=gateway._plan_single,
        ) as planner,
        mock.patch.object(
            store,
            "add_calibration_admission",
            wraps=store.add_calibration_admission,
        ) as insert,
        mock.patch.object(
            gateway,
            "calibrate_single",
            wraps=gateway.calibrate_single,
        ) as solve,
    ):
        response = client.post("/api/calibrations/single", json=request)

    assert response.status_code == 422
    error = response.json()["error"]
    assert error["code"] == "CURVE_STORAGE_NODE_LIMIT_EXCEEDED"
    assert error["location"] == [
        "body",
        "declaration",
        "knot_dates",
        99,
    ]
    assert admission.call_count == planner.call_count == 1
    assert insert.call_count == solve.call_count == 0


def test_fix_pwlf_matrix_metadata_200_covers_all_mat_07_flag_permutations(
    client,
) -> None:
    """FIX-PWLF-MATRIX-METADATA-200 — MAT-07 uses production adapters."""
    completed = submit_and_wait(
        client,
        "/api/calibrations/single",
        matrix_metadata_request(
            include_jacobian=False,
            include_effective_inverse=False,
        ),
    )
    assert completed["status"] == "completed"
    assert completed["jacobian"]["shape"] == [100, 200]
    assert completed["effective_inverse"]["shape"] == [200, 100]
    assert len(completed["jacobian"]["column_axis"]) == 200
    assert len(completed["effective_inverse"]["row_axis"]) == 200
    assert completed["jacobian"]["values"] is None
    assert completed["effective_inverse"]["values"] is None

    cases = (
        (True, False, "include_jacobian"),
        (False, True, "include_effective_inverse"),
        (True, True, "include_jacobian"),
    )
    for include_jacobian, include_inverse, first_field in cases:
        response = client.post(
            "/api/calibrations/single",
            json=matrix_metadata_request(
                include_jacobian=include_jacobian,
                include_effective_inverse=include_inverse,
            ),
        )
        assert response.status_code == 422
        error = response.json()["error"]
        assert error["code"] == "MATRIX_DIMENSION_EXCEEDED"
        assert error["location"] == ["body", "options", first_field]


def test_fix_joint_free_parameter_limit_200_covers_mat_08_boundaries(
    client,
) -> None:
    """FIX-JOINT-FREE-PARAMETER-LIMIT-200 — MAT-08 covers 200/201/202."""
    completed = submit_and_wait(
        client,
        "/api/calibrations/xccy/joint",
        joint_capacity_request(200),
    )
    assert completed["status"] == "completed"
    assert completed["jacobian"]["shape"] == [3, 200]
    assert completed["effective_inverse"]["shape"] == [200, 3]
    assert len(completed["named_ranges"]["parameters"]) == 3

    expected = {
        201: (["body", "basis", "parameterization"], 199, 201, "basis"),
        202: (
            ["body", "foreign", "declarations", 0, "parameterization"],
            200,
            201,
            "foreign",
        ),
    }
    for total, (location, before, after, group) in expected.items():
        response = client.post(
            "/api/calibrations/xccy/joint",
            json=joint_capacity_request(total),
        )
        assert response.status_code == 422
        error = response.json()["error"]
        assert error["code"] == "JOINT_FREE_PARAMETER_LIMIT_EXCEEDED"
        assert error["location"] == location
        assert error["context"]["total_free_parameters"] == total
        assert error["context"]["cumulative_before"] == before
        assert error["context"]["cumulative_after"] == after
        assert error["context"]["offending_group"] == group


class _CountingRunResponseAdapter:
    def __init__(self, delegate) -> None:
        self.delegate = delegate
        self.calls: list[bytes] = []

    def dump_json(self, value) -> bytes:
        encoded = self.delegate.dump_json(value)
        self.calls.append(encoded)
        return encoded


def test_fix_b3_serialization_passes_are_two_for_completion_and_one_for_get(
    client,
    monkeypatch,
) -> None:
    """FIX-B3-SERIALIZATION-PASSES — SER-01/PERF-02 exact encode counts."""
    adapter = _CountingRunResponseAdapter(
        calibration_service.RUN_RESPONSE_ADAPTER
    )
    monkeypatch.setattr(calibration_service, "RUN_RESPONSE_ADAPTER", adapter)
    monkeypatch.setattr(calibration_router, "RUN_RESPONSE_ADAPTER", adapter)

    submitted = client.post(
        "/api/calibrations/single",
        json=single_request("USD", 0.04),
    )
    assert submitted.status_code == 202
    run_id = submitted.json()["id"]
    store = get_store()
    for _ in range(200):
        record = store.get_calibration_run(run_id)
        if record.status != "running":
            break
        time.sleep(0.005)
    assert record.status == "completed"
    assert len(adapter.calls) == 2
    assert adapter.calls[0] != adapter.calls[1]
    persisted_timing = record.serialization_ms
    assert persisted_timing is not None

    adapter.calls.clear()
    response = client.get(f"/api/calibrations/{run_id}")

    assert response.status_code == 200
    assert len(adapter.calls) == 1
    assert adapter.calls[0] == response.content
    assert int(response.headers["content-length"]) == len(response.content)
    assert int(response.headers["x-dal-response-bytes"]) == len(
        response.content
    )
    assert (
        store.get_calibration_run(run_id).serialization_ms
        == persisted_timing
    )


def test_fix_b4_preview_reserve_and_defensive_guard_are_atomic(
    client,
    monkeypatch,
) -> None:
    """FIX-B4-PREVIEW-RESERVE — MAT-04/MAT-05 preserve completed rows."""
    with monkeypatch.context() as admission_patch:
        admission_patch.setattr(
            calibration_service,
            "_estimate_success_response_bytes",
            lambda *_args: (1 << 20) + 1,
        )
        rejected = client.post(
            "/api/calibrations/single",
            json={
                **single_request("USD", 0.04),
                "options": {
                    "jacobian_mode": "ANALYTIC",
                    "include_jacobian": True,
                    "include_effective_inverse": True,
                },
            },
        )
    assert rejected.status_code == 422
    assert rejected.json()["error"]["code"] == "RESPONSE_LIMIT_EXCEEDED"
    assert rejected.json()["error"]["context"]["preview_reserved"] is True

    completed = submit_and_wait(
        client,
        "/api/calibrations/single",
        {
            **single_request("USD", 0.04),
            "options": {
                "jacobian_mode": "ANALYTIC",
                "include_jacobian": True,
                "include_effective_inverse": True,
            },
        },
    )
    run_id = completed["id"]
    store = get_store()
    before = copy.deepcopy(store.get_calibration_run(run_id))
    ordinary = client.get(f"/api/calibrations/{run_id}")
    monkeypatch.setattr(
        calibration_router,
        "MAX_RESPONSE_BYTES",
        len(ordinary.content) + 1,
    )

    guarded = client.get(
        f"/api/calibrations/{run_id}",
        params={"quote_bump_index": 0, "quote_bump_size": 0.0001},
    )

    assert guarded.status_code == 500
    assert guarded.json()["error"]["code"] == "RESPONSE_LIMIT_GUARD_BREACH"
    after = store.get_calibration_run(run_id)
    assert after.status == "completed"
    assert after.result_payload == before.result_payload
    assert after.actual_execution_identity == before.actual_execution_identity
    assert after.finished_at == before.finished_at


def test_fix_b8_submitted_index_beats_canonical_issue_order_and_control(
    client,
) -> None:
    """FIX-B8-SUBMITTED-INDEX — HTTP issue order differs from diagnostics."""
    payload = single_request("USD", 0.02)
    payload["declaration"]["knot_dates"] = [
        "2027-01-02",
        "2028-01-02",
        "2029-01-02",
    ]
    payload["declaration"]["initial_guess_per_node"] = [0.02] * 3
    payload["instruments"] = [
        {
            **payload["instruments"][0],
            "label": "submitted-3y-invalid",
            "trade_date": "2025-12-31",
            "maturity": "2029-01-02",
            "market_rate": 0.03,
        },
        {
            **payload["instruments"][0],
            "label": "submitted-1y-invalid",
            "trade_date": "2025-12-30",
            "maturity": "2027-01-02",
            "market_rate": 0.01,
        },
        {
            **payload["instruments"][0],
            "label": "submitted-2y-valid",
            "maturity": "2028-01-02",
            "market_rate": 0.02,
        },
    ]
    gateway = get_gateway()
    original = gateway.plan_single_admission

    def plan(*args, **kwargs):
        admitted = original(*args, **kwargs)
        report = SimpleNamespace(
            eligible=False,
            issues=(
                SimpleNamespace(
                    reason=SimpleNamespace(name="TRADE_DATE_MISMATCH"),
                    instrument_index=0,
                    reset_index=-1,
                    native_message="canonical 1Y issue",
                ),
                SimpleNamespace(
                    reason=SimpleNamespace(name="TRADE_DATE_MISMATCH"),
                    instrument_index=2,
                    reset_index=-1,
                    native_message="canonical 3Y issue",
                ),
            ),
        )
        return admitted._replace(analytic_eligibility=report)

    store = get_store()
    with (
        mock.patch.object(gateway, "plan_single_admission", side_effect=plan),
        mock.patch.object(
            store,
            "add_calibration_admission",
            wraps=store.add_calibration_admission,
        ) as insert,
        mock.patch.object(
            gateway,
            "calibrate_single",
            wraps=gateway.calibrate_single,
        ) as native,
    ):
        rejected = client.post("/api/calibrations/single", json=payload)
    assert rejected.status_code == 422
    error = rejected.json()["error"]
    assert error["code"] == "ANALYTIC_INELIGIBLE"
    assert error["location"] == ["body", "instruments", 0, "trade_date"]
    assert error["context"]["input_index"] == 0
    assert error["context"]["calibration_index"] == 2
    assert insert.call_count == native.call_count == 0

    for instrument in payload["instruments"]:
        instrument["trade_date"] = "2026-01-02"
    completed = submit_and_wait(
        client,
        "/api/calibrations/single",
        payload,
    )
    assert [
        diagnostic["market_rate"]
        for diagnostic in completed["instrument_diagnostics"]
    ] == [0.01, 0.02, 0.03]


def test_fix_b9_router_500_covers_retrieval_adapter_and_legacy_boundary(
    client,
    monkeypatch,
) -> None:
    """FIX-B9-ROUTER-500 — only calibration routes use the new envelope."""
    secret = "secret /tmp/calibration.db"

    def explode(*_args, **_kwargs):
        raise RuntimeError(secret)

    with monkeypatch.context() as retrieval:
        retrieval.setattr(
            calibration_router,
            "get_calibration_response",
            explode,
        )
        response = client.get(f"/api/calibrations/{'e' * 32}")
    assert response.status_code == 500
    assert response.json()["error"]["code"] == "INTERNAL_SERVER_ERROR"
    assert secret not in response.text
    assert len(response.json()["error"]["context"]["incident_id"]) == 32

    completed = submit_and_wait(
        client,
        "/api/calibrations/single",
        single_request("USD", 0.02),
    )
    with monkeypatch.context() as adapter:
        adapter.setattr(
            calibration_router.RUN_RESPONSE_ADAPTER,
            "dump_json",
            explode,
        )
        response = client.get(f"/api/calibrations/{completed['id']}")
    assert response.status_code == 500
    assert response.json()["error"]["code"] == "INTERNAL_SERVER_ERROR"
    assert secret not in response.text

    from app.routers import products

    with monkeypatch.context() as legacy:
        legacy.setattr(products, "product_templates", explode)
        with TestClient(client.app, raise_server_exceptions=False) as legacy_client:
            response = legacy_client.get("/api/products/templates")
    assert response.status_code == 500
    assert response.text == "Internal Server Error"
    assert "INTERNAL_SERVER_ERROR" not in response.text
