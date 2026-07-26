from __future__ import annotations

import copy
import threading
from collections.abc import Callable
from unittest import mock

import pytest
from sqlalchemy import func, select

from app.services import calibrations as calibration_service
from app.services.dal_gateway import get_gateway
from app.services.db.models import CalibrationRunRow, CurveDefinitionRow
from app.services.db.store_db import DbStore
from app.services.store import get_store
from tests.calibration_contract_fixtures import (
    single_request,
    wait_for_terminal,
)


class _EventRecorder:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self.events: list[str] = []

    def record(self, event: str) -> None:
        with self._lock:
            self.events.append(event)


def test_fix_b5_bk06_shared_recorder_proves_conc_01_06_production_order(
    client,
    monkeypatch,
) -> None:
    """FIX-B5/FIX-BK06 — CONC-01/06 use poll and health barriers."""
    gateway = get_gateway()
    store = get_store()
    recorder = _EventRecorder()
    evidence_loaded = threading.Event()
    release_evidence = threading.Event()

    original_insert = store.add_calibration_admission

    def insert(*args, **kwargs):
        result = original_insert(*args, **kwargs)
        recorder.record("queued")
        return result

    monkeypatch.setattr(store, "add_calibration_admission", insert)

    original_load = store.load_single_worker_admission_evidence

    def load(calibration_id: str):
        result = original_load(calibration_id)
        recorder.record("evidence_loaded")
        evidence_loaded.set()
        assert release_evidence.wait(timeout=5.0)
        return result

    monkeypatch.setattr(
        store,
        "load_single_worker_admission_evidence",
        load,
    )

    original_mark = store.mark_calibration_solving

    def mark(calibration_id, acquired_at):
        result = original_mark(calibration_id, acquired_at)
        recorder.record("solving_committed")
        return result

    monkeypatch.setattr(store, "mark_calibration_solving", mark)

    original_phase = store.update_calibration_phase

    def phase(calibration_id: str, value: str):
        result = original_phase(calibration_id, value)
        recorder.record(f"{value}_committed")
        return result

    monkeypatch.setattr(store, "update_calibration_phase", phase)

    original_complete = store.complete_calibration

    def complete(*args, **kwargs):
        result = original_complete(*args, **kwargs)
        recorder.record("terminal_committed")
        return result

    monkeypatch.setattr(store, "complete_calibration", complete)

    original_hash = calibration_service.canonical_model_hash
    worker_hash_calls = 0

    def canonical_hash(value):
        nonlocal worker_hash_calls
        result = original_hash(value)
        if evidence_loaded.is_set() and worker_hash_calls < 2:
            recorder.record(
                "plan_hash_verified"
                if worker_hash_calls == 0
                else "expected_identity_hash_verified"
            )
            worker_hash_calls += 1
        return result

    monkeypatch.setattr(
        calibration_service,
        "canonical_model_hash",
        canonical_hash,
    )

    original_validate = (
        calibration_service._validate_single_worker_admission_context
    )

    def validate(*args, **kwargs):
        result = original_validate(*args, **kwargs)
        recorder.record("evidence_bounded_validated")
        return result

    monkeypatch.setattr(
        calibration_service,
        "_validate_single_worker_admission_context",
        validate,
    )

    original_notify = gateway._notify_single_lock_acquired

    def notify(callback: Callable):
        recorder.record("lock_acquired")

        def observed(acquired_at):
            recorder.record("on_lock_callback_entered")
            callback(acquired_at)

        return original_notify(observed)

    monkeypatch.setattr(gateway, "_notify_single_lock_acquired", notify)

    original_verify = gateway._verify_single_admission

    def verify(request, callback: Callable):
        recorder.record("evidence_callback_entered")
        result = original_verify(request, callback)
        recorder.record("evidence_callback_returned")
        return result

    monkeypatch.setattr(gateway, "_verify_single_admission", verify)

    original_build = gateway._build_single_execution_spec

    def build(*args, **kwargs):
        recorder.record("native_timer_started")
        result = original_build(*args, **kwargs)
        recorder.record("native_spec_constructed")
        return result

    monkeypatch.setattr(gateway, "_build_single_execution_spec", build)

    original_inspect = gateway._inspect_single_execution_identity

    def inspect(*args, **kwargs):
        result = original_inspect(*args, **kwargs)
        recorder.record("execution_identity_inspected")
        return result

    monkeypatch.setattr(
        gateway,
        "_inspect_single_execution_identity",
        inspect,
    )

    original_native = gateway._calibrate_single_verified

    def native(*args, **kwargs):
        recorder.record("native_entered")
        return original_native(*args, **kwargs)

    monkeypatch.setattr(gateway, "_calibrate_single_verified", native)

    original_calibrate = gateway.calibrate_single

    def calibrate(*args, **kwargs):
        recorder.record("lock_waiting")
        try:
            return original_calibrate(*args, **kwargs)
        finally:
            recorder.record("lock_released")

    monkeypatch.setattr(gateway, "calibrate_single", calibrate)

    submitted = client.post(
        "/api/calibrations/single",
        json=single_request("USD", 0.04),
    )
    assert submitted.status_code == 202
    run_id = submitted.json()["id"]
    assert evidence_loaded.wait(timeout=5.0)

    poll = client.get(f"/api/calibrations/{run_id}")
    health = client.get("/api/health")

    assert poll.status_code == health.status_code == 200
    assert poll.json()["status"] == "running"
    assert poll.json()["phase"] == "solving"
    assert poll.json()["started_at"] is not None
    assert health.json()["status"] == "ok"
    assert gateway._calibration_lock.acquire(blocking=False) is False

    release_evidence.set()
    terminal = wait_for_terminal(
        client,
        {"location": f"/api/calibrations/{run_id}"},
    )
    assert terminal["status"] == "completed"
    assert recorder.events == [
        "queued",
        "lock_waiting",
        "lock_acquired",
        "on_lock_callback_entered",
        "solving_committed",
        "evidence_callback_entered",
        "evidence_loaded",
        "plan_hash_verified",
        "expected_identity_hash_verified",
        "evidence_bounded_validated",
        "evidence_callback_returned",
        "native_timer_started",
        "native_spec_constructed",
        "execution_identity_inspected",
        "native_entered",
        "lock_released",
        "serializing_committed",
        "persisting_committed",
        "terminal_committed",
    ]


@pytest.mark.parametrize(
    ("domain", "mutation", "expected_code"),
    (
        (
            "plan",
            "body",
            "PERSISTED_KNOT_PLAN_HASH_MISMATCH",
        ),
        (
            "plan",
            "hash",
            "PERSISTED_KNOT_PLAN_HASH_MISMATCH",
        ),
        (
            "expected",
            "body",
            "PERSISTED_EXPECTED_EXECUTION_IDENTITY_HASH_MISMATCH",
        ),
        (
            "expected",
            "hash",
            "PERSISTED_EXPECTED_EXECUTION_IDENTITY_HASH_MISMATCH",
        ),
    ),
)
def test_fix_cb1_bk01_bk03_integrity_faults_are_atomic_and_never_repaired(
    client,
    monkeypatch,
    domain: str,
    mutation: str,
    expected_code: str,
) -> None:
    """FIX-CB1/BK01/BK03 — production worker detects every evidence fault."""
    store = get_store()
    gateway = get_gateway()
    load_entered = threading.Event()
    release_load = threading.Event()
    frozen_bytes: list[bytes] = []
    persisted_bytes: list[bytes] = []

    original_load = store.load_single_worker_admission_evidence

    def delayed_load(calibration_id: str):
        load_entered.set()
        assert release_load.wait(timeout=5.0)
        return original_load(calibration_id)

    monkeypatch.setattr(
        store,
        "load_single_worker_admission_evidence",
        delayed_load,
    )

    original_freeze = calibration_service.freeze_integrity_error_evidence

    def observe_freeze(*args, **kwargs):
        frozen = original_freeze(*args, **kwargs)
        frozen_bytes.append(frozen.canonical_error_utf8)
        return frozen

    monkeypatch.setattr(
        calibration_service,
        "freeze_integrity_error_evidence",
        observe_freeze,
    )

    fail_name = (
        "fail_knot_plan_integrity"
        if domain == "plan"
        else "fail_expected_execution_identity_integrity"
    )
    original_fail = getattr(store, fail_name)

    def observe_fail(
        calibration_id: str,
        finished_at,
        canonical_error_utf8: bytes,
    ):
        persisted_bytes.append(canonical_error_utf8)
        return original_fail(
            calibration_id,
            finished_at,
            canonical_error_utf8,
        )

    monkeypatch.setattr(store, fail_name, observe_fail)

    with (
        mock.patch.object(
            store,
            "add_calibration_admission",
            wraps=store.add_calibration_admission,
        ) as admission,
        mock.patch.object(
            gateway,
            "_calibrate_single_verified",
            wraps=gateway._calibrate_single_verified,
        ) as native,
        mock.patch.object(
            store,
            "complete_calibration",
            wraps=store.complete_calibration,
        ) as complete,
    ):
        submitted = client.post(
            "/api/calibrations/single",
            json=single_request("USD", 0.04),
        )
        assert submitted.status_code == 202
        run_id = submitted.json()["id"]
        assert load_entered.wait(timeout=5.0)

        field = (
            "resolved_knot_plan"
            if domain == "plan"
            else "expected_execution_identity"
        )
        hash_field = f"{field}_hash"
        with store._session() as session:
            row = session.get(CalibrationRunRow, run_id)
            assert row is not None
            if mutation == "body":
                corrupted = copy.deepcopy(getattr(row, field))
                if domain == "plan":
                    corrupted["requested_policy"] = "INSTRUMENTS"
                else:
                    corrupted["today"] = "2026-01-03"
                setattr(row, field, corrupted)
            else:
                setattr(row, hash_field, "0" * 64)
            session.commit()
            corrupted_body = copy.deepcopy(getattr(row, field))
            corrupted_hash = getattr(row, hash_field)

        release_load.set()
        terminal = wait_for_terminal(
            client,
            {"location": f"/api/calibrations/{run_id}"},
        )

    assert terminal["status"] == "failed"
    assert terminal["error"]["code"] == expected_code
    assert terminal["actual_jacobian_mode"] is None
    assert terminal["actual_execution_identity"] is None
    assert terminal["actual_execution_identity_hash"] is None
    assert terminal["timings"] == {
        "native_solve_ms": None,
        "serialization_ms": None,
    }
    assert admission.call_count == 1
    assert native.call_count == complete.call_count == 0
    assert len(frozen_bytes) == len(persisted_bytes) == 1
    assert persisted_bytes[0] is frozen_bytes[0]

    first = client.get(f"/api/calibrations/{run_id}")
    second = client.get(f"/api/calibrations/{run_id}")
    assert first.status_code == second.status_code == 200
    assert first.content == second.content

    reopened = DbStore(store.url)
    try:
        persisted = reopened.get_calibration_run(run_id)
        assert getattr(persisted, field) == corrupted_body
        assert getattr(persisted, hash_field) == corrupted_hash
        assert persisted.result_payload is None
        with reopened._session() as session:
            curve_count = session.scalar(
                select(func.count())
                .select_from(CurveDefinitionRow)
                .where(CurveDefinitionRow.source_run_id == run_id)
            )
        assert curve_count == 0
    finally:
        reopened.close()
