from __future__ import annotations

import copy
import json
from types import SimpleNamespace
from unittest import mock

import pytest
from pydantic import ValidationError

from app.routers import calibrations as calibration_router
from app.services import calibrations as calibration_service
from app.services import dal_gateway as gateway_module
from app.services.dal_gateway import get_gateway
from app.services.db.models import CalibrationRunRow
from app.services.store import get_store
from tests.calibration_contract_fixtures import (
    deposit,
    escaped_matrix_request,
    future_knots,
    joint_capacity_request,
    joint_request,
    single_request,
    staged_request,
    submit_and_wait,
    wait_for_terminal,
    xccy_swap,
)


def _dated_deposits(
    currency: str,
    knot_dates: list[str],
    count: int | None = None,
) -> list[dict[str, object]]:
    maturities = list(knot_dates)
    if count is not None and count > len(maturities):
        maturities.extend(future_knots(count)[len(maturities) :])
    return [
        {
            **deposit(currency, 0.02 + index * 0.0001),
            "label": f"{currency} DEP {index + 1}",
            "maturity": maturity,
        }
        for index, maturity in enumerate(maturities[:count])
    ]


def _single_representation_request(
    parameterization: str,
    *,
    scalar: float,
    submitted_seed: list[float] | None = None,
) -> dict[str, object]:
    request = single_request("USD", 0.02)
    future = ["2027-01-02", "2028-01-02"]
    knots = (
        ["2026-01-02", *future]
        if parameterization == "LOG_DISCOUNT"
        else future
    )
    request["declaration"].update(
        {
            "parameterization": parameterization,
            "log_df_scheme": (
                "LOG_LINEAR"
                if parameterization in {"ZERO_RATE", "LOG_DISCOUNT"}
                else None
            ),
            "knot_dates": knots,
            "initial_guess_per_node": (
                [] if submitted_seed is None else submitted_seed
            ),
        }
    )
    request["instruments"] = _dated_deposits("USD", future)
    request["solver"]["initial_guess"] = scalar
    request["solver"]["solve_mode"] = "APPROXIMATE"
    return request


def _persisted_request(completed: dict[str, object]) -> dict[str, object]:
    payload = get_store().get_calibration_run(str(completed["id"])).request_payload
    assert isinstance(payload, dict)
    return payload


@pytest.mark.parametrize(
    ("parameterization", "expected"),
    (
        ("PIECEWISE_CONSTANT_FWD", [0.03, 0.03]),
        ("PIECEWISE_LINEAR_FWD", [0.03, 0.03, 0.03, 0.03]),
        ("ZERO_RATE", [0.03, 0.03]),
        ("LOG_DISCOUNT", [-0.03, -0.06]),
    ),
)
def test_fix_b6_initial_seeds_reach_single_native_inspection_order(
    client,
    parameterization: str,
    expected: list[float],
) -> None:
    """FIX-B6-INITIAL-SEEDS — each representation reaches the gateway raw order."""
    gateway = get_gateway()
    observed: list[list[float]] = []
    original = gateway.calibrate_single

    def calibrate(pre_lock_request, *args):
        observed.append(
            list(pre_lock_request.request.declaration.initial_guess_per_node)
        )
        return original(pre_lock_request, *args)

    with mock.patch.object(gateway, "calibrate_single", side_effect=calibrate):
        completed = submit_and_wait(
            client,
            "/api/calibrations/single",
            _single_representation_request(
                parameterization,
                scalar=0.03,
            ),
        )

    assert completed["status"] == "completed"
    assert observed == [expected]
    assert _persisted_request(completed)["declaration"][
        "resolved_initial_guess_per_node"
    ] == expected


@pytest.mark.parametrize(
    ("scalar", "expected"),
    ((-0.01, [0.01, 0.02]), (0.0, [0.0, 0.0])),
)
def test_fix_b6_log_negative_zero_and_explicit_override(
    client,
    scalar: float,
    expected: list[float],
) -> None:
    """FIX-B6-INITIAL-SEEDS — negative/zero scalar and raw override stay distinct."""
    completed = submit_and_wait(
        client,
        "/api/calibrations/single",
        _single_representation_request("LOG_DISCOUNT", scalar=scalar),
    )
    assert _persisted_request(completed)["declaration"][
        "resolved_initial_guess_per_node"
    ] == expected

    overridden = submit_and_wait(
        client,
        "/api/calibrations/single",
        _single_representation_request(
            "LOG_DISCOUNT",
            scalar=scalar,
            submitted_seed=[-0.123, -0.456],
        ),
    )
    assert _persisted_request(overridden)["declaration"][
        "submitted_initial_guess_per_node"
    ] == [-0.123, -0.456]
    assert _persisted_request(overridden)["declaration"][
        "resolved_initial_guess_per_node"
    ] == [-0.123, -0.456]


def test_fix_b6_seed_shape_and_non_finite_values_fail_at_production_api(
    client,
) -> None:
    """FIX-B6-INITIAL-SEEDS — bad length/NaN/Infinity are stable pre-insert 422s."""
    bad_shape = _single_representation_request(
        "PIECEWISE_LINEAR_FWD",
        scalar=0.03,
        submitted_seed=[0.03],
    )
    response = client.post("/api/calibrations/single", json=bad_shape)
    assert response.status_code == 422
    assert response.json()["error"]["code"] == "INITIAL_GUESS_SHAPE_MISMATCH"
    assert response.json()["error"]["location"] == [
        "body",
        "declaration",
        "initial_guess_per_node",
    ]

    for token in ("NaN", "Infinity"):
        invalid = single_request("USD", 0.02)
        raw = json.dumps(invalid).replace('"initial_guess": 0.0', f'"initial_guess": {token}')
        response = client.post(
            "/api/calibrations/single",
            content=raw,
            headers={"content-type": "application/json"},
        )
        assert response.status_code == 422
        assert response.json()["error"]["code"] == "VALIDATION_ERROR"


def test_fix_b6_staged_and_joint_gateway_inspection_see_resolved_vectors(
    client,
) -> None:
    """FIX-B6-INITIAL-SEEDS — staged/joint gateway seams receive resolved seeds."""
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
    gateway = get_gateway()

    staged_payload = staged_request(
        usd["curves"][0]["id"],
        eur["curves"][0]["id"],
    )
    staged_payload["basis"]["knot_dates"] = ["2027-01-02", "2028-01-02"]
    staged_payload["basis"]["instruments"] = [
        {**xccy_swap(), "label": "basis-1", "maturity": "2027-01-02"},
        {**xccy_swap(), "label": "basis-2", "maturity": "2028-01-02"},
    ]
    staged_payload["basis"]["initial_guess_per_node"] = []
    staged_payload["solver"]["initial_guess"] = 0.03
    staged_payload["solver"]["solve_mode"] = "APPROXIMATE"
    observed_staged: list[list[float]] = []
    original_staged = gateway.calibrate_staged_xccy

    def calibrate_staged(request, *args):
        observed_staged.append(
            list(request.request.basis.initial_guess_per_node)
        )
        return original_staged(request, *args)

    with mock.patch.object(
        gateway,
        "calibrate_staged_xccy",
        side_effect=calibrate_staged,
    ):
        staged = submit_and_wait(
            client,
            "/api/calibrations/xccy/staged",
            staged_payload,
        )
    assert staged["status"] == "completed"
    assert observed_staged == [[0.03, 0.03]]

    joint_payload = joint_request()
    joint_payload["solver"]["initial_guess"] = 0.03
    joint_payload["solver"]["solve_mode"] = "APPROXIMATE"
    expected_joint: list[list[float]] = []
    for group in ("domestic", "foreign"):
        declaration = joint_payload[group]["declarations"][0]
        declaration["knot_dates"] = ["2027-01-02", "2028-01-02"]
        declaration["instruments"] = _dated_deposits(
            "USD" if group == "domestic" else "EUR",
            declaration["knot_dates"],
        )
        declaration["initial_guess_per_node"] = []
        expected_joint.append([0.03, 0.03])
    joint_payload["basis"]["knot_dates"] = ["2027-01-02", "2028-01-02"]
    joint_payload["basis"]["instruments"] = [
        {**xccy_swap(), "label": "basis-1", "maturity": "2027-01-02"},
        {**xccy_swap(), "label": "basis-2", "maturity": "2028-01-02"},
    ]
    joint_payload["basis"]["initial_guess_per_node"] = []
    expected_joint.append([0.03, 0.03])
    observed_joint: list[list[list[float]]] = []
    original_joint = gateway.calibrate_joint_xccy

    def calibrate_joint(request, *args):
        normalized = request.request
        observed_joint.append(
            [
                list(normalized.domestic.declarations[0].initial_guess_per_node),
                list(normalized.foreign.declarations[0].initial_guess_per_node),
                list(normalized.basis.initial_guess_per_node),
            ]
        )
        return original_joint(request, *args)

    with mock.patch.object(
        gateway,
        "calibrate_joint_xccy",
        side_effect=calibrate_joint,
    ):
        joint = submit_and_wait(
            client,
            "/api/calibrations/xccy/joint",
            joint_payload,
        )
    assert joint["status"] == "completed"
    assert observed_joint == [expected_joint]


def _single_dimension_request(residual_count: int) -> dict[str, object]:
    request = _single_representation_request(
        "PIECEWISE_CONSTANT_FWD",
        scalar=0.02,
    )
    if residual_count == 3:
        request["instruments"].append(
            {
                **deposit("USD", 0.0202),
                "label": "USD DEP extra",
                "start": "2026-02-02",
                "maturity": "2028-01-02",
            }
        )
    return request


def _staged_dimension_request(
    domestic_curve_id: str,
    foreign_curve_id: str,
    residual_count: int,
) -> dict[str, object]:
    request = staged_request(domestic_curve_id, foreign_curve_id)
    request["basis"]["knot_dates"] = ["2027-01-02", "2028-01-02"]
    request["basis"]["initial_guess_per_node"] = [0.0, 0.0]
    request["basis"]["instruments"] = [
        {
            **xccy_swap(),
            "label": f"basis-{index + 1}",
            "start": (
                "2026-01-02" if index < 2 else "2026-02-02"
            ),
            "maturity": (
                "2027-01-02" if index == 0 else "2028-01-02"
            ),
        }
        for index in range(residual_count)
    ]
    return request


def _joint_dimension_request(overdetermined: bool) -> dict[str, object]:
    request = joint_request()
    basis = request["basis"]
    basis["knot_dates"] = ["2027-01-02", "2028-01-02"]
    basis["initial_guess_per_node"] = [0.0, 0.0]
    basis_count = 3 if overdetermined else 2
    basis["instruments"] = [
        {
            **xccy_swap(),
            "label": f"basis-{index + 1}",
            "start": (
                "2026-01-02" if index < 2 else "2026-02-02"
            ),
            "maturity": (
                "2027-01-02" if index == 0 else "2028-01-02"
            ),
        }
        for index in range(basis_count)
    ]
    return request


def _assert_dimension_case(
    client,
    path: str,
    request: dict[str, object],
    method_name: str,
    *,
    should_reject: bool,
) -> None:
    gateway = get_gateway()
    store = get_store()
    with (
        mock.patch.object(
            store,
            "add_calibration_admission",
            wraps=store.add_calibration_admission,
        ) as insert,
        mock.patch.object(
            gateway,
            method_name,
            wraps=getattr(gateway, method_name),
        ) as native,
    ):
        response = client.post(path, json=request)
        if should_reject:
            assert response.status_code == 422
            error = response.json()["error"]
            assert error["code"] == "EXACT_SYSTEM_OVERDETERMINED"
            assert error["location"] == ["body", "solver", "solve_mode"]
            assert insert.call_count == native.call_count == 0
            return
        assert response.status_code == 202, response.text
        terminal = wait_for_terminal(
            client,
            {"location": response.headers["location"]},
        )
        assert terminal["status"] == "completed", terminal
        assert insert.call_count == native.call_count == 1


def test_fix_b7_exact_dimensions_cover_every_endpoint_mode_and_matrix_flag(
    client,
) -> None:
    """FIX-B7-EXACT-DIMENSIONS — m=n/n+1 × endpoint × mode × four flags."""
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
    endpoints = (
        (
            "/api/calibrations/single",
            "calibrate_single",
            lambda over: _single_dimension_request(3 if over else 2),
        ),
        (
            "/api/calibrations/xccy/staged",
            "calibrate_staged_xccy",
            lambda over: _staged_dimension_request(
                usd["curves"][0]["id"],
                eur["curves"][0]["id"],
                3 if over else 2,
            ),
        ),
        (
            "/api/calibrations/xccy/joint",
            "calibrate_joint_xccy",
            _joint_dimension_request,
        ),
    )
    for path, method_name, factory in endpoints:
        for overdetermined in (False, True):
            for mode in ("EXACT", "APPROXIMATE"):
                for include_jacobian, include_inverse in (
                    (False, False),
                    (True, False),
                    (False, True),
                    (True, True),
                ):
                    request = factory(overdetermined)
                    request["solver"]["solve_mode"] = mode
                    request["options"] = {
                        "jacobian_mode": "ANALYTIC",
                        "include_jacobian": include_jacobian,
                        "include_effective_inverse": include_inverse,
                    }
                    _assert_dimension_case(
                        client,
                        path,
                        request,
                        method_name,
                        should_reject=overdetermined and mode == "EXACT",
                    )


def test_fix_b1_joint_basis_schemes_cross_production_gateway_and_rebuild_dto(
    client,
) -> None:
    """FIX-B1-JOINT-BASIS-SCHEMES — requested/native/result scheme equality."""
    gateway = get_gateway()
    for parameterization in ("ZERO_RATE", "LOG_DISCOUNT"):
        for scheme in ("LOG_LINEAR", "LOG_CUBIC_NATURAL", "MIXED"):
            request = joint_request()
            request["solver"]["solve_mode"] = "APPROXIMATE"
            request["basis"].update(
                {
                    "parameterization": parameterization,
                    "log_df_scheme": scheme,
                    "knot_dates": [
                        "2027-01-02",
                        "2028-01-02",
                        "2029-01-02",
                    ],
                    "initial_guess_per_node": [0.0, 0.0, 0.0],
                    "instruments": [
                        {
                            **xccy_swap(),
                            "label": f"basis-{index + 1}",
                            "maturity": f"{2027 + index}-01-02",
                        }
                        for index in range(3)
                    ],
                }
            )
            observed: list[str] = []
            original = gateway.calibrate_joint_xccy

            def calibrate(
                gateway_request,
                *args,
                _observed=observed,
                _original=original,
            ):
                _observed.append(
                    gateway_request.request.basis.log_df_scheme
                )
                return _original(gateway_request, *args)

            with mock.patch.object(
                gateway,
                "calibrate_joint_xccy",
                side_effect=calibrate,
            ):
                completed = submit_and_wait(
                    client,
                    "/api/calibrations/xccy/joint",
                    request,
                )
            basis_curve = next(
                curve
                for curve in completed["curves"]
                if curve["role"] == "basis"
            )
            assert observed == [scheme]
            assert basis_curve["parameterization"] == parameterization
            assert basis_curve["log_df_scheme"] == scheme

    for parameterization in (
        "PIECEWISE_CONSTANT_FWD",
        "PIECEWISE_LINEAR_FWD",
    ):
        request = joint_request()
        request["basis"]["parameterization"] = parameterization
        request["basis"]["log_df_scheme"] = "LOG_LINEAR"
        response = client.post(
            "/api/calibrations/xccy/joint",
            json=request,
        )
        assert response.status_code == 422
        assert response.json()["error"]["code"] == "VALIDATION_ERROR"
        assert response.json()["error"]["location"] == [
            "body",
            "basis",
        ]


def _single_storage_boundary_request(
    parameterization: str,
    future_count: int,
) -> dict[str, object]:
    request = single_request("USD", 0.02)
    future = future_knots(future_count)
    knots = (
        ["2026-01-02", *future]
        if parameterization == "LOG_DISCOUNT"
        else future
    )
    request["declaration"].update(
        {
            "parameterization": parameterization,
            "log_df_scheme": "LOG_LINEAR",
            "knot_dates": knots,
            "initial_guess_per_node": [],
        }
    )
    request["instruments"] = [
        {
            **deposit("USD", 0.02),
            "maturity": future[-1],
        }
    ]
    request["solver"]["solve_mode"] = "APPROXIMATE"
    return request


def test_fix_b2_storage_boundaries_cover_single_and_joint_anchor_projection(
    client,
) -> None:
    """FIX-B2-STORAGE-BOUNDARY — exact 99/100/101 storage-node boundaries."""
    for parameterization in ("ZERO_RATE", "LOG_DISCOUNT"):
        completed = submit_and_wait(
            client,
            "/api/calibrations/single",
            _single_storage_boundary_request(parameterization, 99),
        )
        assert completed["status"] == "completed"
        curve = completed["curves"][0]
        assert completed["resolved_knot_plan"]["counts"]["storage_nodes"] == 100
        assert curve["anchor_date"] == "2026-01-02"
        assert len(curve["node_dates"]) == (
            99 if parameterization == "ZERO_RATE" else 100
        )

    gateway = get_gateway()
    store = get_store()
    overflow = _single_storage_boundary_request("ZERO_RATE", 100)
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
        ) as native,
    ):
        response = client.post("/api/calibrations/single", json=overflow)
    assert response.status_code == 422
    assert response.json()["error"]["code"] == (
        "CURVE_STORAGE_NODE_LIMIT_EXCEEDED"
    )
    assert response.json()["error"]["location"][-1] == 99
    assert admission.call_count == planner.call_count == 1
    assert insert.call_count == native.call_count == 0

    for target in ("domestic", "basis"):
        request = joint_request()
        declaration = (
            request["domestic"]["declarations"][0]
            if target == "domestic"
            else request["basis"]
        )
        declaration.update(
            {
                "parameterization": "ZERO_RATE",
                "log_df_scheme": "LOG_LINEAR",
                "knot_dates": future_knots(99),
                "initial_guess_per_node": [],
            }
        )
        request["solver"]["solve_mode"] = "APPROXIMATE"
        request["options"] = {
            "jacobian_mode": "ANALYTIC",
            "include_jacobian": False,
            "include_effective_inverse": False,
        }
        completed = submit_and_wait(
            client,
            "/api/calibrations/xccy/joint",
            request,
        )
        expected_name = (
            declaration["curve_name"]
            if target == "domestic"
            else request["basis"]["curve_name"]
        )
        target_curve = next(
            curve
            for curve in completed["curves"]
            if curve["name"] == expected_name
        )
        assert target_curve["anchor_date"] == "2026-01-02"
        assert len(target_curve["node_dates"]) == 99

        declaration["knot_dates"] = future_knots(100)
        store = get_store()
        with (
            mock.patch.object(
                store,
                "add_calibration_admission",
                wraps=store.add_calibration_admission,
            ) as insert,
            mock.patch.object(
                gateway,
                "calibrate_joint_xccy",
                wraps=gateway.calibrate_joint_xccy,
            ) as native,
            mock.patch.object(
                gateway,
                "required_historical_xccy_fixings",
                wraps=gateway.required_historical_xccy_fixings,
            ) as preflight,
        ):
            rejected = client.post(
                "/api/calibrations/xccy/joint",
                json=request,
            )
        assert rejected.status_code == 422
        assert rejected.json()["error"]["code"] == (
            "CURVE_STORAGE_NODE_LIMIT_EXCEEDED"
        )
        assert insert.call_count == native.call_count == preflight.call_count == 0


@pytest.mark.parametrize(
    ("parameterization", "scheme", "reject_future", "accept_future"),
    (
        ("ZERO_RATE", "LOG_LINEAR", None, 1),
        ("ZERO_RATE", "LOG_CUBIC_NATURAL", 1, 2),
        ("ZERO_RATE", "MIXED", 2, 3),
        ("LOG_DISCOUNT", "LOG_LINEAR", None, 1),
        ("LOG_DISCOUNT", "LOG_CUBIC_NATURAL", 1, 2),
        ("LOG_DISCOUNT", "MIXED", 2, 3),
    ),
)
def test_fix_scheme_min_nodes_reject_accept_boundaries_at_api(
    client,
    parameterization: str,
    scheme: str,
    reject_future: int | None,
    accept_future: int,
) -> None:
    """FIX-SCHEME-MIN-NODES — each scheme rejects N-1 and accepts N."""
    def request(count: int) -> dict[str, object]:
        payload = single_request("USD", 0.02)
        future = future_knots(max(count, 1))
        payload["declaration"].update(
            {
                "parameterization": parameterization,
                "log_df_scheme": scheme,
                "knot_dates": (
                    ["2026-01-02", *future[:count]]
                    if parameterization == "LOG_DISCOUNT"
                    else future[:count]
                ),
                "initial_guess_per_node": [],
            }
        )
        payload["instruments"] = [
            {
                **deposit("USD", 0.02),
                "maturity": future[max(count, 1) - 1],
            }
        ]
        payload["solver"]["solve_mode"] = "APPROXIMATE"
        return payload

    if reject_future is not None:
        rejected = client.post(
            "/api/calibrations/single",
            json=request(reject_future),
        )
        assert rejected.status_code == 422
        assert rejected.json()["error"]["code"] == (
            "CURVE_SCHEME_NODE_COUNT_INVALID"
        )
    accepted = submit_and_wait(
        client,
        "/api/calibrations/single",
        request(accept_future),
    )
    assert accepted["status"] == "completed"


def test_fix_joint_free_parameter_limit_exhausts_modes_flags_and_downstream(
    client,
) -> None:
    """FIX-JOINT-FREE-PARAMETER-LIMIT-200 — all 21st-gate permutations."""
    gateway = get_gateway()
    store = get_store()
    for total in (201, 202):
        for mode in ("EXACT", "APPROXIMATE"):
            for include_jacobian, include_inverse in (
                (False, False),
                (True, False),
                (False, True),
                (True, True),
            ):
                payload = joint_capacity_request(total)
                payload["solver"]["solve_mode"] = mode
                payload["options"] = {
                    "jacobian_mode": "ANALYTIC",
                    "include_jacobian": include_jacobian,
                    "include_effective_inverse": include_inverse,
                }
                with (
                    mock.patch.object(
                        calibration_service,
                        "build_joint_admission_count_plan",
                        wraps=calibration_service.build_joint_admission_count_plan,
                    ) as planner,
                    mock.patch.object(
                        calibration_service,
                        "_normalize_joint_xccy_admission",
                        wraps=calibration_service._normalize_joint_xccy_admission,
                    ) as normalize,
                    mock.patch.object(
                        calibration_service,
                        "_check_system_and_matrices",
                        wraps=calibration_service._check_system_and_matrices,
                    ) as dimensions,
                    mock.patch.object(
                        calibration_service,
                        "_estimate_success_response_bytes",
                        wraps=calibration_service._estimate_success_response_bytes,
                    ) as estimator,
                    mock.patch.object(
                        store,
                        "add_calibration_admission",
                        wraps=store.add_calibration_admission,
                    ) as insert,
                    mock.patch.object(
                        gateway,
                        "required_historical_xccy_fixings",
                        wraps=gateway.required_historical_xccy_fixings,
                    ) as preflight,
                    mock.patch.object(
                        gateway,
                        "validate_joint_xccy_admission",
                        wraps=gateway.validate_joint_xccy_admission,
                    ) as eligibility,
                    mock.patch.object(
                        gateway,
                        "calibrate_joint_xccy",
                        wraps=gateway.calibrate_joint_xccy,
                    ) as native,
                ):
                    response = client.post(
                        "/api/calibrations/xccy/joint",
                        json=payload,
                    )
                assert response.status_code == 422
                error = response.json()["error"]
                assert error["code"] == (
                    "JOINT_FREE_PARAMETER_LIMIT_EXCEEDED"
                )
                assert error["context"]["total_free_parameters"] == total
                assert planner.call_count == 1
                assert (
                    normalize.call_count
                    == dimensions.call_count
                    == estimator.call_count
                    == insert.call_count
                    == preflight.call_count
                    == eligibility.call_count
                    == native.call_count
                    == 0
                )

    captured: list[tuple[object, object]] = []
    built: list[object] = []
    original_normalize = calibration_service._normalize_joint_xccy_admission
    original_builder = calibration_service.build_joint_admission_count_plan

    def normalize(request, count_plan):
        captured.append((count_plan, request))
        return original_normalize(request, count_plan)

    def build(**kwargs):
        plan = original_builder(**kwargs)
        built.append(plan)
        return plan

    with (
        mock.patch.object(
            calibration_service,
            "build_joint_admission_count_plan",
            side_effect=build,
        ),
        mock.patch.object(
            calibration_service,
            "_normalize_joint_xccy_admission",
            side_effect=normalize,
        ),
        mock.patch.object(
            calibration_service,
            "_estimate_success_response_bytes",
            wraps=calibration_service._estimate_success_response_bytes,
        ) as estimator,
    ):
        completed = submit_and_wait(
            client,
            "/api/calibrations/xccy/joint",
            joint_capacity_request(200),
        )
    assert completed["status"] == "completed"
    assert len(built) == len(captured) == 1
    assert captured[0][0] is built[0]
    assert captured[0][0].total_free_parameters == 200
    assert estimator.call_count == 1
    assert estimator.call_args.args[2] == built[0].total_free_parameters
    document = client.app.openapi()
    assert document["components"]["schemas"]["JointXccyCalibrationRequest"][
        "x-dal-max-total-free-parameters"
    ] == 200


class _NativeMatrix:
    def __init__(self, rows: list[list[float]]) -> None:
        self.rows = rows

    def to_rows(self) -> list[list[float]]:
        return self.rows


@pytest.mark.parametrize(
    ("rows", "row_axis", "column_axis", "exception"),
    (
        ([], [], [], RuntimeError),
        ([], [], ["c0"], RuntimeError),
        ([[]], ["r0"], [], RuntimeError),
        ([[1.0], [2.0, 3.0]], ["r0", "r1"], ["c0"], RuntimeError),
        ([[float("nan")]], ["r0"], ["c0"], ValidationError),
        ([[float("inf")]], ["r0"], ["c0"], ValidationError),
    ),
)
def test_section_18_native_matrix_boundaries_reject_invalid_carriers(
    rows: list[list[float]],
    row_axis: list[str],
    column_axis: list[str],
    exception: type[Exception],
) -> None:
    """MAT-02/MAT-03 — empty/ragged/non-finite native matrices never escape."""
    with pytest.raises(exception):
        gateway_module._native_matrix_dto(
            _NativeMatrix(rows),
            available=True,
            requested=True,
            row_axis=row_axis,
            column_axis=column_axis,
            scaling="unscaled",
            residual_tolerance=None,
        )


def test_section_18_dates_finiteness_and_quote_bump_boundaries_use_api(
    client,
) -> None:
    """API-01/API-04 — dates, finite values, and bump query boundaries."""
    for knots in (
        ["2027-01-02", "2027-01-02"],
        ["2028-01-02", "2027-01-02"],
    ):
        request = _single_representation_request(
            "PIECEWISE_CONSTANT_FWD",
            scalar=0.02,
        )
        request["declaration"]["knot_dates"] = knots
        response = client.post("/api/calibrations/single", json=request)
        assert response.status_code == 422
        assert response.json()["error"]["code"] == "VALIDATION_ERROR"

    for token in ("NaN", "Infinity"):
        invalid = single_request("USD", 0.02)
        raw = json.dumps(invalid).replace(
            '"market_rate": 0.02',
            f'"market_rate": {token}',
        )
        response = client.post(
            "/api/calibrations/single",
            content=raw,
            headers={"content-type": "application/json"},
        )
        assert response.status_code == 422
        assert response.json()["error"]["code"] == "VALIDATION_ERROR"

    completed = submit_and_wait(
        client,
        "/api/calibrations/single",
        {
            **single_request("USD", 0.02),
            "options": {
                "jacobian_mode": "ANALYTIC",
                "include_jacobian": True,
                "include_effective_inverse": True,
            },
        },
    )
    run_path = f"/api/calibrations/{completed['id']}"
    invalid_queries = (
        {"quote_bump_index": 0},
        {"quote_bump_size": 0.0001},
        {"quote_bump_index": 0, "quote_bump_size": 0.0},
        {"quote_bump_index": 0, "quote_bump_size": 0.0101},
        {"quote_bump_index": 0, "quote_bump_size": "NaN"},
        {"quote_bump_index": 1, "quote_bump_size": 0.0001},
    )
    for query in invalid_queries:
        response = client.get(run_path, params=query)
        assert response.status_code == 422


def _curve_response_payload(
    index: int,
    *,
    currency: str = "USD",
    base: dict[str, object] | None = None,
) -> dict[str, object]:
    return {
        "dto_version": 1,
        "id": f"{index:032x}",
        "name": f"curve-{index}",
        "currency": currency,
        "role": "discount",
        "target": {"collateral": "OIS", "tenor": None},
        "anchor_date": "2026-01-02",
        "base_curve_id": None if base is None else base["id"],
        "base": base,
        "source_run_id": "f" * 32,
        "parameterization": "PIECEWISE_CONSTANT_FWD",
        "day_count": "ACT_365F",
        "log_df_scheme": None,
        "node_dates": ["2027-01-02"],
        "parameters": {"right_forwards": [0.02]},
    }


def test_section_18_base_cycle_depth_nine_and_currency_mismatch_are_rejected() -> None:
    """API-04/DB-01 — recursive curve DTO invariants reject all bad bases."""
    cyclic = _curve_response_payload(1)
    cyclic["base_curve_id"] = cyclic["id"]
    cyclic["base"] = cyclic
    with pytest.raises(ValidationError, match="recursion"):
        calibration_service.CURVE_RESPONSE_ADAPTER.validate_python(cyclic)

    depth_nine: dict[str, object] | None = None
    for index in range(10, 0, -1):
        depth_nine = _curve_response_payload(index, base=depth_nine)
    with pytest.raises(ValidationError, match="depth 8"):
        calibration_service.CURVE_RESPONSE_ADAPTER.validate_python(depth_nine)

    eur_base = _curve_response_payload(20, currency="EUR")
    usd_child = _curve_response_payload(21, currency="USD", base=eur_base)
    with pytest.raises(ValidationError, match="expanded base must match"):
        calibration_service.CURVE_RESPONSE_ADAPTER.validate_python(usd_child)


def test_section_18_reference_boundaries_use_production_admission(
    client,
) -> None:
    """API-04 — missing/currency/role/status reference errors name the field."""
    missing = single_request("USD", 0.02)
    missing["declaration"]["base_curve_id"] = "a" * 32
    response = client.post("/api/calibrations/single", json=missing)
    assert response.status_code == 422
    assert response.json()["error"] == {
        "code": "REFERENCE_MISMATCH",
        "message": f"referenced curve {'a' * 32} does not exist",
        "location": ["body", "declaration", "base_curve_id"],
        "context": {"curve_id": "a" * 32, "constraint": "exists"},
    }

    usd = submit_and_wait(
        client,
        "/api/calibrations/single",
        single_request("USD", 0.02),
    )
    usd_curve_id = str(usd["curves"][0]["id"])
    wrong_role = single_request("USD", 0.02)
    wrong_role["declaration"]["forward_curve_ids"] = {"3M": usd_curve_id}
    response = client.post("/api/calibrations/single", json=wrong_role)
    assert response.status_code == 422
    error = response.json()["error"]
    assert error["code"] == "REFERENCE_MISMATCH"
    assert error["location"] == [
        "body",
        "declaration",
        "forward_curve_ids",
        "3M",
    ]
    assert error["context"] == {
        "curve_id": usd_curve_id,
        "constraint": "role",
        "expected": ["forward"],
        "actual": "discount",
    }

    eur = submit_and_wait(
        client,
        "/api/calibrations/single",
        single_request("EUR", 0.02),
    )
    eur_curve_id = str(eur["curves"][0]["id"])
    wrong_currency = single_request("USD", 0.02)
    wrong_currency["declaration"]["base_curve_id"] = eur_curve_id
    response = client.post("/api/calibrations/single", json=wrong_currency)
    assert response.status_code == 422
    error = response.json()["error"]
    assert error["code"] == "REFERENCE_MISMATCH"
    assert error["location"] == ["body", "declaration", "base_curve_id"]
    assert error["context"] == {
        "curve_id": eur_curve_id,
        "constraint": "currency",
        "expected": "USD",
        "actual": "EUR",
    }

    store = get_store()
    with store._session() as session:
        row = session.get(CalibrationRunRow, str(usd["id"]))
        assert row is not None
        row.status = "running"
        row.phase = "solving"
        row.finished_at = None
        session.commit()
    unfinished = single_request("USD", 0.02)
    unfinished["declaration"]["base_curve_id"] = usd_curve_id
    response = client.post("/api/calibrations/single", json=unfinished)
    assert response.status_code == 422
    error = response.json()["error"]
    assert error["code"] == "REFERENCE_MISMATCH"
    assert error["location"] == ["body", "declaration", "base_curve_id"]
    assert error["context"] == {
        "curve_id": usd_curve_id,
        "constraint": "status",
        "expected": "completed",
        "actual": "running",
    }


@pytest.mark.parametrize("duplicate_kind", ("discount_collateral", "forward_tenor"))
def test_section_18_duplicate_joint_declaration_routes_are_rejected(
    client,
    duplicate_kind: str,
) -> None:
    """API-01/API-04 — duplicate collateral and tenor routes are ambiguous."""
    request = joint_request()
    duplicate = copy.deepcopy(request["domestic"]["declarations"][0])
    duplicate["curve_name"] = f"duplicate-{duplicate_kind}"
    if duplicate_kind == "forward_tenor":
        duplicate.update(
            {
                "calibrate_discount_curve": False,
                "target_tenor": "3M",
            }
        )
        first_forward = copy.deepcopy(duplicate)
        first_forward["curve_name"] = "first-forward"
        request["domestic"]["declarations"].append(first_forward)
    request["domestic"]["declarations"].append(duplicate)

    response = client.post("/api/calibrations/xccy/joint", json=request)

    assert response.status_code == 422
    assert response.json()["error"]["code"] == "VALIDATION_ERROR"


def test_section_18_missing_xccy_leg_route_maps_native_report_to_api(
    client,
    monkeypatch,
) -> None:
    """API-04/API-06 — missing XCCY discount route has the submitted location."""
    request = joint_request()
    request["basis"]["instruments"][0]["config"]["convention"]["domestic_index"][
        "collateral"
    ] = "MISSING"
    issue = SimpleNamespace(
        reason=SimpleNamespace(name="DISCOUNT_ROUTE_MISSING"),
        group="basis",
        declaration_index=-1,
        instrument_index=0,
        reset_index=-1,
        native_message="domestic discount route is missing",
    )
    monkeypatch.setattr(
        get_gateway(),
        "validate_joint_xccy_admission",
        lambda _request: SimpleNamespace(eligible=False, issues=[issue]),
    )

    response = client.post("/api/calibrations/xccy/joint", json=request)

    assert response.status_code == 422
    error = response.json()["error"]
    assert error["code"] == "ANALYTIC_INELIGIBLE"
    assert error["location"] == [
        "body",
        "basis",
        "instruments",
        0,
        "config",
        "convention",
        "domestic_index",
        "collateral",
    ]
    assert error["context"]["reason_code"] == "DISCOUNT_ROUTE_MISSING"


def test_section_18_non_finite_fx_solver_and_native_result_are_rejected(
    client,
    monkeypatch,
) -> None:
    """API-01/API-04/MAT-02 — non-finite input and output never reach JSON."""
    invalid_fx = json.dumps(joint_request()).replace(
        '"fx_spot": 1.1',
        '"fx_spot": NaN',
    )
    response = client.post(
        "/api/calibrations/xccy/joint",
        content=invalid_fx,
        headers={"content-type": "application/json"},
    )
    assert response.status_code == 422
    assert response.json()["error"]["code"] == "VALIDATION_ERROR"

    invalid_solver = json.dumps(single_request("USD", 0.02)).replace(
        '"tolerance": 1e-08',
        '"tolerance": Infinity',
    )
    response = client.post(
        "/api/calibrations/single",
        content=invalid_solver,
        headers={"content-type": "application/json"},
    )
    assert response.status_code == 422
    assert response.json()["error"]["code"] == "VALIDATION_ERROR"

    gateway = get_gateway()
    original = gateway.calibrate_single

    def non_finite_result(*args, **kwargs):
        result = original(*args, **kwargs)
        curves = copy.deepcopy(result.curves)
        curves[0]["parameters"]["right_forwards"][0] = float("nan")
        return result._replace(curves=curves)

    monkeypatch.setattr(gateway, "calibrate_single", non_finite_result)
    failed = submit_and_wait(
        client,
        "/api/calibrations/single",
        single_request("USD", 0.02),
    )
    assert failed["status"] == "failed"
    assert failed["error"]["code"] == "NATIVE_CALIBRATION_FAILED"
    assert b"NaN" not in json.dumps(failed).encode()
    with get_store()._engine.connect() as connection:
        curve_count = connection.exec_driver_sql(
            "SELECT count(*) FROM curve_definition WHERE source_run_id = ?",
            (str(failed["id"]),),
        ).scalar_one()
    assert curve_count == 0


def test_fix_escaped_byte_bound_uses_real_100x100_response_and_preview(
    client,
    monkeypatch,
) -> None:
    """FIX-ESCAPED-BYTE-BOUND — estimator, exact bytes, and preview reserve."""
    estimates: list[int] = []
    original_estimator = calibration_service._estimate_success_response_bytes

    def estimate(*args, **kwargs):
        value = original_estimator(*args, **kwargs)
        estimates.append(value)
        return value

    with mock.patch.object(
        calibration_service,
        "_estimate_success_response_bytes",
        side_effect=estimate,
    ):
        completed = submit_and_wait(
            client,
            "/api/calibrations/single",
            escaped_matrix_request(),
        )

    ordinary = client.get(f"/api/calibrations/{completed['id']}")
    assert ordinary.status_code == 200
    assert completed["jacobian"]["shape"] == [100, 100]
    assert completed["effective_inverse"]["shape"] == [100, 100]
    assert estimates == [estimates[0]]
    assert estimates[0] >= len(ordinary.content)
    assert len(ordinary.content) > 100_000
    assert b"\\u0001" in ordinary.content
    assert "😀".encode() in ordinary.content

    store = get_store()
    before = copy.deepcopy(store.get_calibration_run(str(completed["id"])))
    monkeypatch.setattr(
        calibration_router,
        "MAX_RESPONSE_BYTES",
        len(ordinary.content) + 1,
    )
    preview = client.get(
        f"/api/calibrations/{completed['id']}",
        params={"quote_bump_index": 0, "quote_bump_size": 0.0001},
    )
    assert preview.status_code == 500
    assert preview.json()["error"]["code"] == "RESPONSE_LIMIT_GUARD_BREACH"
    after = store.get_calibration_run(str(completed["id"]))
    assert after.status == "completed"
    assert after.result_payload == before.result_payload


def _three_node_instruments(count: int) -> list[dict[str, object]]:
    nodes = ["2026-02-02", "2026-03-02", "2026-04-02"]
    spans = (
        (nodes[0], nodes[1]),
        (nodes[0], nodes[2]),
        (nodes[1], nodes[2]),
    )
    result: list[dict[str, object]] = []
    kinds = ("DEPOSIT", "FRA")
    for index in range(count):
        start, maturity = spans[index % len(spans)]
        instrument = {
            **deposit("USD", 0.02 + index * 0.0001),
            "kind": kinds[index // len(spans)],
            "label": f"count-{index + 1}",
            "start": start,
            "maturity": maturity,
        }
        result.append(instrument)
    return result


@pytest.mark.parametrize(
    ("policy", "parameterization", "resolved", "storage", "free"),
    (
        ("INPUT", "PIECEWISE_CONSTANT_FWD", 3, 3, 3),
        ("INPUT", "PIECEWISE_LINEAR_FWD", 3, 3, 6),
        ("INPUT", "ZERO_RATE", 3, 4, 3),
        ("INPUT", "LOG_DISCOUNT", 2, 3, 2),
        ("INSTRUMENTS", "PIECEWISE_CONSTANT_FWD", 3, 3, 3),
        ("INSTRUMENTS", "PIECEWISE_LINEAR_FWD", 3, 3, 6),
        ("INSTRUMENTS", "ZERO_RATE", 3, 4, 3),
        ("AUGMENTED", "PIECEWISE_CONSTANT_FWD", 3, 3, 3),
        ("AUGMENTED", "PIECEWISE_LINEAR_FWD", 3, 3, 6),
        ("AUGMENTED", "ZERO_RATE", 3, 4, 3),
        ("AUGMENTED", "LOG_DISCOUNT", 2, 3, 2),
    ),
)
def test_fix_cb1_downstream_counts_share_one_plan_across_every_consumer(
    client,
    policy: str,
    parameterization: str,
    resolved: int,
    storage: int,
    free: int,
) -> None:
    """FIX-CB1-DOWNSTREAM-COUNTS — one admitted plan feeds every count."""
    future = ["2026-02-02", "2026-03-02", "2026-04-02"]
    request = single_request("USD", 0.02)
    log_discount = parameterization == "LOG_DISCOUNT"
    submitted_knots = (
        ["2026-01-02", *future[:2]]
        if log_discount
        else future
    )
    if policy == "INSTRUMENTS":
        submitted_knots = []
    request["declaration"].update(
        {
            "knot_policy": policy,
            "parameterization": parameterization,
            "log_df_scheme": "LOG_LINEAR" if parameterization in {
                "ZERO_RATE",
                "LOG_DISCOUNT",
            } else None,
            "knot_dates": submitted_knots,
            "initial_guess_per_node": [],
        }
    )
    request["instruments"] = _three_node_instruments(free)
    if log_discount:
        for index, instrument in enumerate(request["instruments"]):
            instrument["kind"] = ("DEPOSIT", "FRA")[index]
            instrument["start"] = future[0]
            instrument["maturity"] = future[1]
    request["options"] = {
        "jacobian_mode": "ANALYTIC",
        "include_jacobian": True,
        "include_effective_inverse": True,
    }
    gateway = get_gateway()
    inspected: list[object] = []
    estimated: list[tuple[int, int]] = []
    original_admission = gateway.plan_single_admission
    original_estimator = calibration_service._estimate_success_response_bytes

    def admit(gateway_request, on_plan):
        def inspect_plan(plan):
            inspected.append(plan)
            on_plan(plan)

        return original_admission(gateway_request, inspect_plan)

    def estimate(api_request, residual_count, parameter_count):
        estimated.append((residual_count, parameter_count))
        return original_estimator(api_request, residual_count, parameter_count)

    with (
        mock.patch.object(
            gateway,
            "plan_single_admission",
            side_effect=admit,
        ),
        mock.patch.object(
            calibration_service,
            "_estimate_success_response_bytes",
            side_effect=estimate,
        ),
    ):
        completed = submit_and_wait(
            client,
            "/api/calibrations/single",
            request,
        )

    plan = completed["resolved_knot_plan"]
    assert len(inspected) == 1
    assert plan["counts"] == {
        "submitted_knots": len(submitted_knots),
        "instrument_candidates": 0 if policy == "INPUT" else 2 * free,
        "resolved_declared_nodes": resolved,
        "storage_nodes": storage,
        "free_parameters": free,
    }
    assert completed["jacobian"]["shape"] == [free, free]
    assert completed["effective_inverse"]["shape"] == [free, free]
    assert len(completed["jacobian"]["column_axis"]) == free
    assert len(completed["effective_inverse"]["row_axis"]) == free
    assert estimated == [(free, free)]
    persisted = _persisted_request(completed)
    assert len(
        persisted["declaration"]["resolved_initial_guess_per_node"]
    ) == free
    curve = completed["curves"][0]
    reconstructed_storage = len(curve["node_dates"]) + int(
        parameterization == "ZERO_RATE"
    )
    assert reconstructed_storage == storage
    preview = client.get(
        f"/api/calibrations/{completed['id']}",
        params={"quote_bump_index": 0, "quote_bump_size": 0.0001},
    )
    assert preview.status_code == 200
    assert len(preview.json()["quote_bump_preview"]["delta_parameters"]) == free


def test_fix_cb1_log_policy_rejects_instruments_before_planner_and_augments(
    client,
) -> None:
    """FIX-CB1-LOG-POLICY — anchor ownership is input-only."""
    gateway = get_gateway()
    store = get_store()
    incompatible = single_request("USD", 0.02)
    incompatible["declaration"].update(
        {
            "knot_policy": "INSTRUMENTS",
            "parameterization": "LOG_DISCOUNT",
            "log_df_scheme": "LOG_LINEAR",
            "knot_dates": [],
            "initial_guess_per_node": [],
        }
    )
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
        ) as native,
    ):
        rejected = client.post(
            "/api/calibrations/single",
            json=incompatible,
        )
    assert rejected.status_code == 422
    assert rejected.json()["error"]["code"] == "KNOT_POLICY_INCOMPATIBLE"
    assert (
        admission.call_count
        == planner.call_count
        == insert.call_count
        == native.call_count
        == 0
    )

    augmented = copy.deepcopy(incompatible)
    augmented["declaration"]["knot_policy"] = "AUGMENTED"
    augmented["declaration"]["knot_dates"] = ["2026-01-02"]
    completed = submit_and_wait(
        client,
        "/api/calibrations/single",
        augmented,
    )
    plan = completed["resolved_knot_plan"]
    assert plan["storage_nodes"][0]["date"] == "2026-01-02"
    assert plan["storage_nodes"][0]["origins"] == [
        {
            "kind": "INPUT",
            "input_knot_index": 0,
        }
    ]
    assert completed["curves"][0]["node_dates"] == [
        "2026-01-02",
        "2027-01-02",
    ]


def _boundary_instruments(resolved_count: int) -> list[dict[str, object]]:
    nodes = future_knots(resolved_count)
    spans = [
        (nodes[index], nodes[index + 1])
        for index in range(0, resolved_count - 1, 2)
    ]
    if resolved_count % 2:
        spans.append((nodes[-2], nodes[-1]))
    return [
        {
            **deposit("USD", 0.02 + index * 0.0001),
            "label": f"boundary-{index + 1}",
            "start": start,
            "maturity": maturity,
        }
        for index, (start, maturity) in enumerate(spans)
    ]


def _policy_boundary_request(
    policy: str,
    parameterization: str,
    resolved_count: int,
) -> dict[str, object]:
    log_discount = parameterization == "LOG_DISCOUNT"
    request = single_request("USD", 0.02)
    if policy == "INPUT":
        future = future_knots(resolved_count)
        knots = ["2026-01-02", *future] if log_discount else future
        instruments = [
            {
                **deposit("USD", 0.02),
                "maturity": future[-1],
            }
        ]
    else:
        knots = (
            ["2026-01-02"]
            if log_discount
            else [future_knots(resolved_count)[0]]
            if policy == "AUGMENTED"
            else []
        )
        instruments = _boundary_instruments(resolved_count)
    request["declaration"].update(
        {
            "knot_policy": policy,
            "parameterization": parameterization,
            "log_df_scheme": (
                "LOG_LINEAR"
                if parameterization in {"ZERO_RATE", "LOG_DISCOUNT"}
                else None
            ),
            "knot_dates": knots,
            "initial_guess_per_node": [],
        }
    )
    request["instruments"] = instruments
    request["solver"]["solve_mode"] = "APPROXIMATE"
    return request


@pytest.mark.parametrize(
    ("policy", "parameterization", "accept_count", "reject_count", "schema_reject"),
    (
        ("INPUT", "PIECEWISE_CONSTANT_FWD", 100, 101, True),
        ("INPUT", "PIECEWISE_LINEAR_FWD", 100, 101, True),
        ("INPUT", "ZERO_RATE", 99, 100, False),
        ("INPUT", "LOG_DISCOUNT", 99, 100, True),
        ("INSTRUMENTS", "PIECEWISE_CONSTANT_FWD", 100, 101, False),
        ("INSTRUMENTS", "PIECEWISE_LINEAR_FWD", 100, 101, False),
        ("INSTRUMENTS", "ZERO_RATE", 99, 100, False),
        ("AUGMENTED", "PIECEWISE_CONSTANT_FWD", 100, 101, False),
        ("AUGMENTED", "PIECEWISE_LINEAR_FWD", 100, 101, False),
        ("AUGMENTED", "ZERO_RATE", 99, 100, False),
        ("AUGMENTED", "LOG_DISCOUNT", 99, 100, False),
    ),
)
def test_fix_cb1_policy_boundaries_cover_every_policy_representation_maximum(
    client,
    policy: str,
    parameterization: str,
    accept_count: int,
    reject_count: int,
    schema_reject: bool,
) -> None:
    """FIX-CB1-POLICY-BOUNDARIES — every max accept and first reject."""
    accepted = submit_and_wait(
        client,
        "/api/calibrations/single",
        _policy_boundary_request(
            policy,
            parameterization,
            accept_count,
        ),
    )
    counts = accepted["resolved_knot_plan"]["counts"]
    assert counts["resolved_declared_nodes"] == accept_count
    assert counts["storage_nodes"] == accept_count + int(
        parameterization in {"ZERO_RATE", "LOG_DISCOUNT"}
    )
    assert counts["free_parameters"] == (
        2 * accept_count
        if parameterization == "PIECEWISE_LINEAR_FWD"
        else accept_count
    )

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
        ) as native,
    ):
        rejected = client.post(
            "/api/calibrations/single",
            json=_policy_boundary_request(
                policy,
                parameterization,
                reject_count,
            ),
        )
    assert rejected.status_code == 422
    assert rejected.json()["error"]["code"] == (
        "VALIDATION_ERROR"
        if schema_reject
        else "CURVE_STORAGE_NODE_LIMIT_EXCEEDED"
    )
    assert insert.call_count == native.call_count == 0
    if schema_reject:
        assert admission.call_count == planner.call_count == 0
    else:
        assert admission.call_count == planner.call_count == 1
