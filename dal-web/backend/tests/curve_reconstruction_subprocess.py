"""Process-isolated writer/reader for persisted native curve reconstruction."""

from __future__ import annotations

import argparse
import gc
import json
from datetime import UTC, date, datetime

from app.services.calibration_store import CalibrationRunRecord, CurveDefinitionRecord
from app.services.calibrations import get_curve_response
from app.services.dal_gateway import DalGateway
from app.services.db.store_db import DbStore

RUN_ID = "1" * 32
CURVE_IDS = {
    "base": "2" * 32,
    "pwc": "3" * 32,
    "pwlf": "4" * 32,
    "zero": "5" * 32,
    "log": "6" * 32,
}


def _run() -> CalibrationRunRecord:
    now = datetime.now(UTC)
    return CalibrationRunRecord(
        id=RUN_ID,
        schema_version=1,
        kind="single",
        name="API-05 subprocess reconstruction",
        status="running",
        phase="queued",
        request_payload={"schema_version": 1},
        solver_payload={"solve_mode": "EXACT"},
        options_payload={"jacobian_mode": "ANALYTIC"},
        resolved_knot_plan=None,
        resolved_knot_plan_hash=None,
        expected_execution_identity=None,
        expected_execution_identity_hash=None,
        actual_jacobian_mode=None,
        actual_execution_identity=None,
        actual_execution_identity_hash=None,
        result_payload=None,
        error_payload=None,
        backend="dal",
        is_native=True,
        created_at=now,
        started_at=now,
        finished_at=None,
        native_solve_ms=None,
        serialization_ms=None,
    )


def _payload(
    parameterization: str,
    node_dates: list[str],
    parameters: dict[str, list[float]],
) -> dict:
    scheme = "MIXED" if parameterization in {"ZERO_RATE", "LOG_DISCOUNT"} else None
    return {
        "target": {"collateral": "OIS", "tenor": None},
        "parameterization": parameterization,
        "anchor_date": "2026-01-02",
        "day_count": "ACT_365F",
        "log_df_scheme": scheme,
        "node_dates": node_dates,
        "parameters": parameters,
    }


def _curves() -> tuple[CurveDefinitionRecord, ...]:
    created_at = datetime.now(UTC)
    future = ["2027-01-02", "2028-01-02", "2029-01-02"]
    definitions = (
        (
            "base",
            None,
            _payload(
                "PIECEWISE_CONSTANT_FWD",
                future,
                {"right_forwards": [0.0100, 0.0110, 0.0120]},
            ),
        ),
        (
            "pwc",
            CURVE_IDS["base"],
            _payload(
                "PIECEWISE_CONSTANT_FWD",
                future,
                {"right_forwards": [0.0010, 0.0011, 0.0012]},
            ),
        ),
        (
            "pwlf",
            CURVE_IDS["pwc"],
            _payload(
                "PIECEWISE_LINEAR_FWD",
                future,
                {
                    "left_forwards": [0.0020, 0.0021, 0.0022],
                    "right_forwards": [0.0025, 0.0026, 0.0027],
                },
            ),
        ),
        (
            "zero",
            CURVE_IDS["pwlf"],
            _payload(
                "ZERO_RATE",
                future,
                {"zero_rates": [0.0030, 0.0031, 0.0032]},
            ),
        ),
        (
            "log",
            CURVE_IDS["zero"],
            _payload(
                "LOG_DISCOUNT",
                ["2026-01-02", *future],
                {"log_discount_factors": [0.0, -0.0040, -0.0081, -0.0123]},
            ),
        ),
    )
    return tuple(
        CurveDefinitionRecord(
            id=CURVE_IDS[name],
            dto_version=1,
            name=name,
            currency="USD",
            role="base" if name == "base" else "discount",
            source_run_id=RUN_ID,
            base_curve_id=base_curve_id,
            payload=payload,
            created_at=created_at,
        )
        for name, base_curve_id, payload in definitions
    )


def _evaluate(store: DbStore) -> dict[str, list[float]]:
    gateway = DalGateway()
    anchor = gateway.make_date(2026, 1, 2)
    targets = (
        gateway.make_date(2026, 7, 2),
        gateway.make_date(2027, 1, 2),
        gateway.make_date(2028, 7, 2),
        gateway.make_date(2029, 1, 2),
    )
    values: dict[str, list[float]] = {}
    for name, curve_id in CURVE_IDS.items():
        dto = get_curve_response(store, curve_id)
        curve = gateway.rebuild_curve(dto)
        del dto
        gc.collect()
        values[name] = [curve(anchor, target) for target in targets]
        del curve
        gc.collect()
    return values


def _write(store: DbStore) -> dict[str, list[float]]:
    store.create_all()
    store.add_calibration_admission(_run(), ())
    curves = _curves()
    store.complete_calibration(
        RUN_ID,
        result_payload={"curve_ids": [curve.id for curve in curves]},
        curves=curves,
        actual_jacobian_mode="ANALYTIC",
        actual_execution_identity=None,
        actual_execution_identity_hash=None,
        native_solve_ms=1.0,
        serialization_ms=1.0,
        finished_at=datetime.now(UTC),
    )
    return _evaluate(store)


def _read(store: DbStore) -> dict[str, list[float]]:
    values = _evaluate(store)
    assert store.get_calibration_run(RUN_ID).status == "completed"
    return values


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("write", "read"), required=True)
    parser.add_argument("--db", required=True)
    args = parser.parse_args()
    store = DbStore(f"sqlite:///{args.db}")
    try:
        result = _write(store) if args.mode == "write" else _read(store)
    finally:
        store.close()
    print(
        json.dumps(
            {
                "pid_mode": args.mode,
                "curve_ids": CURVE_IDS,
                "values": result,
                "anchor": date(2026, 1, 2).isoformat(),
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
