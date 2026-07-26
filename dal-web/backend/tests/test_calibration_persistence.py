from __future__ import annotations

from datetime import UTC, datetime

import pytest
from sqlalchemy import func, select
from sqlalchemy.exc import IntegrityError

from app.services.calibration_store import (
    CalibrationInstrumentRecord,
    CalibrationRunRecord,
    CurveDefinitionRecord,
)
from app.services.calibrations import canonical_json_bytes
from app.services.db.models import CurveDefinitionRow


def _run(run_id: str = "a" * 32) -> CalibrationRunRecord:
    plan = {"planner_version": 1, "execution_policy": "INPUT"}
    identity = {"identity_version": 1, "execution_policy": "INPUT"}
    return CalibrationRunRecord(
        id=run_id,
        schema_version=1,
        kind="single",
        name="test",
        status="running",
        phase="queued",
        request_payload={"schema_version": 1},
        solver_payload={"solve_mode": "EXACT"},
        options_payload={"jacobian_mode": "ANALYTIC"},
        resolved_knot_plan=plan,
        resolved_knot_plan_hash="b" * 64,
        expected_execution_identity=identity,
        expected_execution_identity_hash="c" * 64,
        actual_jacobian_mode=None,
        actual_execution_identity=None,
        actual_execution_identity_hash=None,
        result_payload=None,
        error_payload=None,
        backend="fake",
        is_native=True,
        created_at=datetime.now(UTC),
        started_at=None,
        finished_at=None,
        native_solve_ms=None,
        serialization_ms=None,
    )


def test_admission_and_terminal_success_are_atomic(store):
    run = _run()
    instrument = CalibrationInstrumentRecord(
        id="d" * 32,
        run_id=run.id,
        group_name="single:curve",
        input_index=0,
        calibration_index=0,
        kind="DEPOSIT",
        label="deposit",
        native_name="Deposit",
        payload={"kind": "DEPOSIT"},
    )
    store.add_calibration_admission(run, (instrument,))
    store.mark_calibration_solving(run.id, datetime.now(UTC))

    curve = CurveDefinitionRecord(
        id="e" * 32,
        dto_version=1,
        name="curve",
        currency="USD",
        role="discount",
        source_run_id=run.id,
        base_curve_id=None,
        payload={
            "target": {"collateral": "OIS", "tenor": None},
            "parameterization": "PIECEWISE_CONSTANT_FWD",
            "anchor_date": "2026-01-01",
            "day_count": "ACT_365F",
            "log_df_scheme": None,
            "node_dates": ["2027-01-01"],
            "parameters": {"right_forwards": [0.04]},
        },
        created_at=datetime.now(UTC),
    )
    store.complete_calibration(
        run.id,
        result_payload={"curve_ids": [curve.id]},
        curves=(curve,),
        actual_jacobian_mode="ANALYTIC",
        actual_execution_identity={"identity_version": 1},
        actual_execution_identity_hash="f" * 64,
        native_solve_ms=1.0,
        serialization_ms=2.0,
        finished_at=datetime.now(UTC),
    )

    loaded = store.get_calibration_run(run.id)
    assert loaded.status == "completed"
    assert loaded.result_payload == {"curve_ids": [curve.id]}
    assert loaded.actual_jacobian_mode == "ANALYTIC"
    assert loaded.actual_execution_identity_hash == "f" * 64
    assert store.get_curve_definition(curve.id).payload == curve.payload


def test_fix_bk07_completion_flush_fault_leaves_no_partial_curves(store):
    """DB-03/DB-05 — a mid-curve flush fault rolls back the whole terminal write."""
    run = _run("7" * 32)
    store.add_calibration_admission(run, ())
    payload = {
        "target": {"collateral": "OIS", "tenor": None},
        "parameterization": "PIECEWISE_CONSTANT_FWD",
        "anchor_date": "2026-01-01",
        "day_count": "ACT_365F",
        "log_df_scheme": None,
        "node_dates": ["2027-01-01"],
        "parameters": {"right_forwards": [0.04]},
    }
    first = CurveDefinitionRecord(
        id="8" * 32,
        dto_version=1,
        name="first",
        currency="USD",
        role="discount",
        source_run_id=run.id,
        base_curve_id=None,
        payload=payload,
        created_at=datetime.now(UTC),
    )
    duplicate = CurveDefinitionRecord(
        id=first.id,
        dto_version=1,
        name="duplicate",
        currency="USD",
        role="discount",
        source_run_id=run.id,
        base_curve_id=None,
        payload=payload,
        created_at=datetime.now(UTC),
    )

    with pytest.raises(IntegrityError):
        store.complete_calibration(
            run.id,
            result_payload={"curve_ids": [first.id, duplicate.id]},
            curves=(first, duplicate),
            actual_jacobian_mode="ANALYTIC",
            actual_execution_identity=None,
            actual_execution_identity_hash=None,
            native_solve_ms=1.0,
            serialization_ms=2.0,
            finished_at=datetime.now(UTC),
        )

    loaded = store.get_calibration_run(run.id)
    assert loaded.status == "running"
    assert loaded.result_payload is None
    with store._session() as session:
        curve_count = session.scalar(
            select(func.count())
            .select_from(CurveDefinitionRow)
            .where(CurveDefinitionRow.source_run_id == run.id)
        )
    assert curve_count == 0


def test_integrity_failure_consumes_canonical_bytes_and_preserves_evidence(store):
    run = _run("1" * 32)
    store.add_calibration_admission(run, ())
    store.mark_calibration_solving(run.id, datetime.now(UTC))
    error = {
        "code": "PERSISTED_KNOT_PLAN_HASH_MISMATCH",
        "message": "persisted single-knot plan failed canonical hash verification",
        "location": None,
        "context": {
            "stored_plan_hash": "b" * 64,
            "actual_plan_hash": "0" * 64,
            "first_difference": None,
        },
    }
    payload = canonical_json_bytes(error)

    store.fail_knot_plan_integrity(run.id, datetime.now(UTC), payload)

    loaded = store.get_calibration_run(run.id)
    assert loaded.status == "failed"
    assert loaded.error_payload == error
    assert loaded.resolved_knot_plan == run.resolved_knot_plan
    assert loaded.actual_execution_identity is None
    assert canonical_json_bytes(loaded.error_payload) == payload


def test_load_worker_evidence_returns_raw_pairs(store):
    run = _run("2" * 32)
    store.add_calibration_admission(run, ())

    evidence = store.load_single_worker_admission_evidence(run.id)

    assert evidence.resolved_knot_plan_raw == run.resolved_knot_plan
    assert evidence.expected_execution_identity_raw == run.expected_execution_identity
    assert evidence.resolved_knot_plan_hash == "b" * 64
