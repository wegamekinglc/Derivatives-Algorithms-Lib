"""Process-isolated production HTTP writer/reader for curve reconstruction."""

from __future__ import annotations

import argparse
import gc
import json
import os
from contextlib import ExitStack
from pathlib import Path
from unittest import mock


def _post_and_wait(client, path: str, payload: dict[str, object]) -> dict[str, object]:
    submitted = client.post(path, json=payload)
    submitted.raise_for_status()
    location = submitted.headers["location"]
    for _ in range(2_000):
        response = client.get(location)
        response.raise_for_status()
        body = response.json()
        if body["status"] != "running":
            if body["status"] != "completed":
                raise RuntimeError(f"{path} failed: {body}")
            return body
    raise RuntimeError(f"{path} did not terminalize")


def _single_payload(
    name: str,
    parameterization: str,
    *,
    currency: str = "USD",
    base_curve_id: str | None = None,
) -> dict[str, object]:
    from tests.calibration_contract_fixtures import deposit, single_request

    future = ["2027-01-02", "2028-01-02", "2029-01-02"]
    payload = single_request(currency, 0.02)
    payload["name"] = f"api_05_{name}"
    payload["declaration"].update(
        {
            "curve_name": name,
            "parameterization": parameterization,
            "log_df_scheme": (
                "MIXED" if parameterization in {"ZERO_RATE", "LOG_DISCOUNT"} else None
            ),
            "knot_dates": (
                ["2026-01-02", *future] if parameterization == "LOG_DISCOUNT" else future
            ),
            "base_curve_id": base_curve_id,
            "initial_guess_per_node": [],
        }
    )
    payload["instruments"] = [
        {
            **deposit(currency, 0.02 + index * 0.0001),
            "label": f"{currency} API-05 DEP {index + 1}",
            "maturity": maturity,
        }
        for index, maturity in enumerate(future)
    ]
    payload["solver"]["solve_mode"] = "APPROXIMATE"
    payload["solver"]["initial_guess"] = 0.02
    payload["options"] = {
        "jacobian_mode": "ANALYTIC",
        "include_jacobian": False,
        "include_effective_inverse": False,
    }
    return payload


def _joint_payload() -> dict[str, object]:
    from tests.calibration_contract_fixtures import joint_request, xccy_swap

    payload = joint_request()
    payload["name"] = "api_05_joint"
    payload["basis"].update(
        {
            "parameterization": "ZERO_RATE",
            "log_df_scheme": "MIXED",
            "knot_dates": [
                "2027-01-02",
                "2028-01-02",
                "2029-01-02",
            ],
            "initial_guess_per_node": [],
            "instruments": [
                {
                    **xccy_swap(),
                    "label": f"API-05 XCCY {index + 1}",
                    "maturity": f"{2027 + index}-01-02",
                }
                for index in range(3)
            ],
        }
    )
    payload["solver"]["solve_mode"] = "APPROXIMATE"
    payload["options"] = {
        "jacobian_mode": "ANALYTIC",
        "include_jacobian": False,
        "include_effective_inverse": False,
    }
    return payload


def _create_http_state(client) -> dict[str, dict[str, str]]:
    from tests.calibration_contract_fixtures import staged_request

    run_ids: dict[str, str] = {}
    curve_ids: dict[str, str] = {}
    previous: str | None = None
    for name, parameterization in (
        ("base", "PIECEWISE_CONSTANT_FWD"),
        ("pwc", "PIECEWISE_CONSTANT_FWD"),
        ("pwlf", "PIECEWISE_LINEAR_FWD"),
        ("zero", "ZERO_RATE"),
        ("log", "LOG_DISCOUNT"),
    ):
        terminal = _post_and_wait(
            client,
            "/api/calibrations/single",
            _single_payload(
                name,
                parameterization,
                base_curve_id=previous,
            ),
        )
        run_ids[name] = str(terminal["id"])
        curve_ids[name] = str(terminal["curves"][0]["id"])
        previous = curve_ids[name]

    eur = _post_and_wait(
        client,
        "/api/calibrations/single",
        _single_payload("eur_base", "PIECEWISE_CONSTANT_FWD", currency="EUR"),
    )
    run_ids["eur"] = str(eur["id"])
    curve_ids["eur"] = str(eur["curves"][0]["id"])

    staged_payload = staged_request(curve_ids["base"], curve_ids["eur"])
    staged_payload["name"] = "api_05_staged"
    staged_payload["solver"]["solve_mode"] = "APPROXIMATE"
    staged_payload["options"] = {
        "jacobian_mode": "ANALYTIC",
        "include_jacobian": False,
        "include_effective_inverse": False,
    }
    staged = _post_and_wait(
        client,
        "/api/calibrations/xccy/staged",
        staged_payload,
    )
    run_ids["staged"] = str(staged["id"])
    curve_ids["staged_basis"] = str(staged["curves"][0]["id"])

    joint = _post_and_wait(
        client,
        "/api/calibrations/xccy/joint",
        _joint_payload(),
    )
    run_ids["joint"] = str(joint["id"])
    for index, curve in enumerate(joint["curves"]):
        key = "joint_basis" if curve["role"] == "basis" else f"joint_{index}_{curve['name']}"
        curve_ids[key] = str(curve["id"])
    return {"run_ids": run_ids, "curve_ids": curve_ids}


def _collect_http_state(
    client,
    ids: dict[str, dict[str, str]],
) -> dict[str, object]:
    from app.services.calibrations import (
        CURVE_RESPONSE_ADAPTER,
        canonical_json_bytes,
    )
    from app.services.dal_gateway import get_gateway
    from app.services.store import get_store

    gateway = get_gateway()
    store = get_store()
    mutation_methods = (
        "mark_calibration_solving",
        "update_calibration_phase",
        "complete_calibration",
        "fail_calibration",
        "fail_knot_plan_integrity",
        "fail_expected_execution_identity_integrity",
    )
    with ExitStack() as stack:
        planner = stack.enter_context(
            mock.patch.object(
                gateway,
                "plan_single_admission",
                wraps=gateway.plan_single_admission,
            )
        )
        inspector = stack.enter_context(
            mock.patch.object(
                gateway,
                "_inspect_single_execution_identity",
                wraps=gateway._inspect_single_execution_identity,
            )
        )
        mutations = [
            stack.enter_context(
                mock.patch.object(
                    store,
                    name,
                    wraps=getattr(store, name),
                )
            )
            for name in mutation_methods
        ]
        run_payloads = {
            name: client.get(f"/api/calibrations/{run_id}")
            for name, run_id in ids["run_ids"].items()
        }
        curve_payloads = {
            name: client.get(f"/api/curves/{curve_id}")
            for name, curve_id in ids["curve_ids"].items()
        }
        for response in (*run_payloads.values(), *curve_payloads.values()):
            response.raise_for_status()

    anchor = gateway.make_date(2026, 1, 2)
    targets = (
        gateway.make_date(2026, 7, 2),
        gateway.make_date(2027, 1, 2),
        gateway.make_date(2028, 7, 2),
        gateway.make_date(2029, 1, 2),
    )
    values: dict[str, list[float]] = {}
    actual_schemes: dict[str, str | None] = {}
    for name, response in curve_payloads.items():
        payload = response.json()
        dto = CURVE_RESPONSE_ADAPTER.validate_python(payload)
        curve = gateway.rebuild_curve(dto)
        del dto
        gc.collect()
        values[name] = [curve(anchor, target) for target in targets]
        actual_schemes[name] = payload["log_df_scheme"]
        del curve
        gc.collect()

    runs = {name: response.json() for name, response in run_payloads.items()}
    plans = {
        name: payload["resolved_knot_plan"]
        for name, payload in runs.items()
        if payload["resolved_knot_plan"] is not None
    }
    hashes = {
        name: payload["resolved_knot_plan_hash"]
        for name, payload in runs.items()
        if payload["resolved_knot_plan_hash"] is not None
    }
    for name, plan in plans.items():
        import hashlib

        assert hashlib.sha256(canonical_json_bytes(plan)).hexdigest() == hashes[name]
    return {
        "curve_ids": ids["curve_ids"],
        "run_ids": ids["run_ids"],
        "values": values,
        "actual_schemes": actual_schemes,
        "canonical_runs": {
            name: canonical_json_bytes(payload).decode("utf-8") for name, payload in runs.items()
        },
        "canonical_plans": {
            name: canonical_json_bytes(plan).decode("utf-8") for name, plan in plans.items()
        },
        "canonical_plan_hashes": hashes,
        "http_reads": {
            "runs": len(run_payloads),
            "curves": len(curve_payloads),
        },
        "planner_calls": planner.call_count,
        "inspector_calls": inspector.call_count,
        "repair_writes": sum(item.call_count for item in mutations),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("write", "read"), required=True)
    parser.add_argument("--db", required=True)
    parser.add_argument("--ids-json")
    args = parser.parse_args()

    os.environ.pop("DAL_WEB_STORE", None)
    os.environ["DAL_WEB_DB_URL"] = f"sqlite:///{Path(args.db)}"
    from fastapi.testclient import TestClient

    from app.main import create_app
    from app.services import dal_gateway, store

    dal_gateway._gateway_box[0] = None
    store._store_box[0] = None
    with TestClient(create_app()) as client:
        ids = _create_http_state(client) if args.mode == "write" else json.loads(str(args.ids_json))
        result = _collect_http_state(client, ids)
    print(
        json.dumps(
            {
                "pid_mode": args.mode,
                **result,
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
