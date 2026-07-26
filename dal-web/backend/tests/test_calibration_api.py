from __future__ import annotations

import time


def _index() -> dict:
    return {
        "spot_lag": 0,
        "fixing_lag": 0,
        "use_projection_curve": False,
        "forecast_tenor": "P12M",
        "day_basis": "ACT_365F",
        "business_day_convention": "Following",
        "fixing_holidays": "",
        "accrual_holidays": "",
        "end_of_month": False,
        "collateral": "OIS",
    }


def single_request() -> dict:
    return {
        "schema_version": 1,
        "name": "usd_ois_2026_01_02",
        "today": "2026-01-02",
        "currency": "USD",
        "declaration": {
            "curve_name": "usd_ois",
            "target_collateral": "OIS",
            "target_tenor": None,
            "calibrate_discount_curve": True,
            "libor_basis": "ACT_365F",
            "parameterization": "PIECEWISE_CONSTANT_FWD",
            "log_df_scheme": None,
            "knot_policy": "INPUT",
            "knot_dates": ["2027-01-02"],
            "base_curve_id": None,
            "discount_curve_ids": {},
            "forward_curve_ids": {},
            "initial_guess_per_node": [0.04],
        },
        "instruments": [
            {
                "kind": "DEPOSIT",
                "label": "USD DEP 1Y",
                "trade_date": "2026-01-02",
                "start": "2026-01-02",
                "maturity": "2027-01-02",
                "market_rate": 0.04,
                "index": _index(),
            }
        ],
        "solver": {
            "solve_mode": "EXACT",
            "smoothing_weight": 1.0,
            "tolerance": 1e-8,
            "fit_tolerance": 1e-6,
            "initial_guess": 0.05,
            "max_evaluations": 200,
            "max_restarts": 20,
        },
        "options": {
            "jacobian_mode": "ANALYTIC",
            "include_jacobian": False,
            "include_effective_inverse": False,
        },
    }


def test_calibration_routes_and_202_location(client) -> None:
    response = client.post("/api/calibrations/single", json=single_request())

    assert response.status_code == 202
    body = response.json()
    assert response.headers["location"] == f"/api/calibrations/{body['id']}"
    assert body["status"] == "running"
    assert body["phase"] == "queued"
    assert body["resolved_knot_plan"]["requested_policy"] == "INPUT"
    assert body["expected_execution_identity"]["execution_policy"] == "INPUT"
    assert body["actual_execution_identity"] is None
    bump_while_running = client.get(
        response.headers["location"],
        params={"quote_bump_index": 0, "quote_bump_size": 0.0001},
    )
    assert bump_while_running.status_code == 409
    assert bump_while_running.json()["error"]["code"] == "RUN_NOT_COMPLETED"

    paths = client.app.openapi()["paths"]
    assert {
        "/api/calibrations/single",
        "/api/calibrations/xccy/staged",
        "/api/calibrations/xccy/joint",
        "/api/calibrations/{calibration_id}",
        "/api/curves/{curve_id}",
    } <= paths.keys()


def test_single_admission_rejects_knots_that_do_not_cover_final_maturity(
    client,
) -> None:
    request = single_request()
    request["declaration"]["knot_dates"] = ["2026-06-02"]

    response = client.post("/api/calibrations/single", json=request)

    assert response.status_code == 422
    assert response.json()["error"]["code"] == "VALIDATION_ERROR"
    assert response.json()["error"]["location"] == [
        "body",
        "declaration",
        "knot_dates",
        0,
    ]


def test_single_admission_maps_unknown_day_basis_to_stable_convention_error(
    client,
) -> None:
    request = single_request()
    request["instruments"][0]["index"]["day_basis"] = "NOT_A_DAY_BASIS"

    response = client.post("/api/calibrations/single", json=request)

    assert response.status_code == 422
    assert response.json()["error"] == {
        "code": "UNSUPPORTED_CONVENTION",
        "message": (
            "instruments[0].index.day_basis value 'NOT_A_DAY_BASIS' "
            "is not a supported day_basis"
        ),
        "location": ["body", "instruments", 0, "index", "day_basis"],
        "context": {
            "convention_kind": "day_basis",
            "value": "NOT_A_DAY_BASIS",
        },
    }


def test_instrument_knot_policy_rejects_an_all_historical_candidate_set(
    client,
) -> None:
    request = single_request()
    request["declaration"]["knot_policy"] = "INSTRUMENTS"
    request["declaration"]["knot_dates"] = []
    request["instruments"][0]["trade_date"] = "2025-01-02"
    request["instruments"][0]["start"] = "2025-01-02"
    request["instruments"][0]["maturity"] = "2025-06-02"

    response = client.post("/api/calibrations/single", json=request)

    assert response.status_code == 422
    assert response.json()["error"]["code"] == "VALIDATION_ERROR"
    assert response.json()["error"]["location"] == [
        "body",
        "instruments",
        0,
        "maturity",
    ]


def test_openapi_pins_joint_capacity_and_failed_integrity_examples(client) -> None:
    document = client.app.openapi()
    schemas = document["components"]["schemas"]
    assert (
        schemas["JointXccyCalibrationRequest"][
            "x-dal-max-total-free-parameters"
        ]
        == 200
    )
    joint_422 = document["paths"]["/api/calibrations/xccy/joint"]["post"][
        "responses"
    ]["422"]["content"]["application/json"]["examples"]
    capacity = joint_422["joint_free_parameter_limit_exceeded"]["value"]["error"]
    assert capacity == {
        "code": "JOINT_FREE_PARAMETER_LIMIT_EXCEEDED",
        "message": "joint calibration has 201 free parameters; maximum is 200",
        "location": ["body", "basis", "parameterization"],
        "context": {
            "total_free_parameters": 201,
            "max_total_free_parameters": 200,
            "cumulative_before": 199,
            "cumulative_after": 201,
            "offending_group": "basis",
            "offending_declaration_index": None,
            "offending_parameterization": "PIECEWISE_CONSTANT_FWD",
            "offending_storage_nodes": 2,
        },
    }
    run_examples = document["paths"]["/api/calibrations/{calibration_id}"][
        "get"
    ]["responses"]["200"]["content"]["application/json"]["examples"]
    failed = run_examples[
        "persisted_expected_execution_identity_hash_mismatch"
    ]["value"]
    assert failed["status"] == "failed"
    assert failed["actual_jacobian_mode"] is None
    assert failed["actual_execution_identity"] is None
    assert failed["actual_execution_identity_hash"] is None
    assert failed["timings"] == {
        "native_solve_ms": None,
        "serialization_ms": None,
    }
    assert (
        failed["error"]["code"]
        == "PERSISTED_EXPECTED_EXECUTION_IDENTITY_HASH_MISMATCH"
    )


def test_new_router_has_stable_validation_and_not_found_envelopes(client) -> None:
    invalid = single_request()
    invalid["declaration"]["knot_policy"] = "INSTRUMENTS"

    response = client.post("/api/calibrations/single", json=invalid)

    assert response.status_code == 422
    assert response.json()["error"]["code"] == "KNOT_POLICY_INPUT_NOT_ALLOWED"
    missing = client.get(f"/api/calibrations/{'f' * 32}")
    assert missing.status_code == 404
    assert missing.json()["error"]["code"] == "CALIBRATION_NOT_FOUND"


def test_new_router_sanitizes_unknown_exceptions(client, monkeypatch) -> None:
    from app.routers import calibrations as calibration_router

    def explode(*_args, **_kwargs):
        raise RuntimeError("secret /tmp/database.sqlite")

    monkeypatch.setattr(calibration_router, "get_calibration_response", explode)
    response = client.get(f"/api/calibrations/{'e' * 32}")

    assert response.status_code == 500
    assert response.json()["error"]["code"] == "INTERNAL_SERVER_ERROR"
    assert "secret" not in response.text


def test_single_completion_persists_curve_and_exact_get_bytes(client) -> None:
    submitted = client.post("/api/calibrations/single", json=single_request())
    run_id = submitted.json()["id"]

    for _ in range(20):
        response = client.get(f"/api/calibrations/{run_id}")
        if response.json()["status"] != "running":
            break
        time.sleep(0.01)

    body = response.json()
    assert body["status"] == "completed"
    assert int(response.headers["x-dal-response-bytes"]) == len(response.content)
    assert response.headers["x-dal-response-limit"] == "1048576"
    assert body["actual_execution_identity"] == body["expected_execution_identity"]
    curve_id = body["curves"][0]["id"]
    curve = client.get(f"/api/curves/{curve_id}")
    assert curve.status_code == 200
    assert curve.json()["source_run_id"] == run_id
    assert int(curve.headers["x-dal-response-bytes"]) == len(curve.content)

    bump = client.get(
        f"/api/calibrations/{run_id}",
        params={"quote_bump_index": 0, "quote_bump_size": 0.0001},
    )
    assert bump.status_code == 409
    assert bump.json()["error"]["code"] == "MATRIX_NOT_AVAILABLE"


def test_single_exact_analytic_materializes_matrices_and_backend_bump(client) -> None:
    request = single_request()
    request["options"] = {
        "jacobian_mode": "ANALYTIC",
        "include_jacobian": True,
        "include_effective_inverse": True,
    }
    submitted = client.post("/api/calibrations/single", json=request)
    run_id = submitted.json()["id"]

    for _ in range(20):
        response = client.get(f"/api/calibrations/{run_id}")
        if response.json()["status"] != "running":
            break
        time.sleep(0.01)

    body = response.json()
    assert body["status"] == "completed"
    assert body["jacobian"]["availability"] == "available"
    assert body["jacobian"]["shape"] == [1, 1]
    assert body["effective_inverse"]["availability"] == "available"
    assert body["effective_inverse"]["shape"] == [1, 1]
    assert body["jacobian"]["row_axis"] == body["effective_inverse"]["column_axis"]
    assert body["jacobian"]["column_axis"] == body["effective_inverse"]["row_axis"]
    assert body["jacobian"]["row_axis"] == [
        f"residual:{body['instrument_diagnostics'][0]['instrument_id']}"
    ]

    bumped = client.get(
        f"/api/calibrations/{run_id}",
        params={"quote_bump_index": 0, "quote_bump_size": 0.0001},
    )
    assert bumped.status_code == 200
    preview = bumped.json()["quote_bump_preview"]
    expected = (
        body["effective_inverse"]["values"][0][0]
        * 0.0001
        / body["effective_inverse"]["residual_tolerance"]
    )
    assert preview["delta_parameters"][0]["value"] == expected
