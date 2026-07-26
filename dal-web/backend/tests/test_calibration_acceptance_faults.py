from __future__ import annotations

import asyncio
import copy
import inspect
import json
import threading
from collections.abc import Callable
from dataclasses import FrozenInstanceError, fields, is_dataclass
from unittest import mock

import pytest
from sqlalchemy import func, select

from app.services import calibrations as calibration_service
from app.services import dal_gateway as gateway_module
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


def test_fix_bk06_static_worker_carriers_and_gateway_signatures_are_exact() -> None:
    """FIX-BK06-LOCK-TO-NATIVE-EVIDENCE — API-14 typed public boundary."""
    pre_lock = calibration_service.SingleGatewayPreLockRequest
    evidence = calibration_service.VerifiedSingleWorkerAdmissionEvidence
    verified = calibration_service.VerifiedSingleGatewayRequest

    assert pre_lock._fields == ("request", "referenced_curves")
    assert evidence._fields == (
        "resolved_knot_plan",
        "resolved_knot_plan_hash",
        "expected_execution_identity",
        "expected_execution_identity_hash",
    )
    assert verified._fields == ("pre_lock_request", "evidence")

    single = inspect.signature(gateway_module.DalGateway.calibrate_single)
    assert tuple(single.parameters) == (
        "self",
        "pre_lock_request",
        "on_lock_acquired",
        "verify_pre_native_admission_evidence",
        "on_execution_identity_inspected",
    )
    for method_name in ("calibrate_staged_xccy", "calibrate_joint_xccy"):
        signature = inspect.signature(getattr(gateway_module.DalGateway, method_name))
        assert "verify_pre_native_admission_evidence" not in signature.parameters

    gateway_source = inspect.getsource(gateway_module)
    assert "services.store" not in gateway_source
    assert "services.db" not in gateway_source


def test_fix_bk07_frozen_integrity_error_evidence_is_deep_and_byte_stable() -> None:
    """FIX-BK07-FROZEN-INTEGRITY-ERROR-EVIDENCE — exact D-22 factory."""
    source_location: list[str | int] = ["body", "integrity", 0]
    source_context = {
        "empty_array": [],
        "empty_object": {},
        "nested": {"array": [{"leaf": "before"}]},
    }
    evidence = calibration_service.freeze_integrity_error_evidence(
        "PERSISTED_EXPECTED_EXECUTION_IDENTITY_HASH_MISMATCH",
        (
            "persisted expected single-knot execution identity failed "
            "canonical hash verification"
        ),
        source_location,
        source_context,
    )
    source_location.append("changed")
    source_context["nested"]["array"][0]["leaf"] = "after"

    assert is_dataclass(evidence)
    assert tuple(field.name for field in fields(evidence)) == (
        "code",
        "message",
        "location",
        "context",
        "canonical_error_utf8",
    )
    assert evidence.location == ("body", "integrity", 0)
    entries = dict(evidence.context.entries)
    assert tuple(key for key, _ in evidence.context.entries) == (
        "empty_array",
        "empty_object",
        "nested",
    )
    assert entries["empty_array"] == calibration_service.FrozenJsonArray(())
    assert entries["empty_object"] == calibration_service.FrozenJsonObject(())

    with pytest.raises(FrozenInstanceError):
        evidence.message = "changed"
    with pytest.raises(TypeError):
        evidence.location[0] = "changed"
    with pytest.raises(FrozenInstanceError):
        evidence.context.entries = ()
    nested = dict(entries["nested"].entries)
    with pytest.raises(FrozenInstanceError):
        nested["array"].items = ()
    with pytest.raises(TypeError):
        nested["array"].items[0] = "changed"

    first_wire = calibration_service.to_api_error_dto(evidence)
    first_wire.message = "wire changed"
    first_wire.location[0] = "wire changed"
    first_wire.context["nested"]["array"][0]["leaf"] = "wire changed"
    second_wire = calibration_service.to_api_error_dto(evidence)
    decoded_wire = json.loads(evidence.canonical_error_utf8)
    assert calibration_service.canonical_json_bytes(
        second_wire.model_dump(mode="json")
    ) == evidence.canonical_error_utf8
    assert calibration_service.canonical_json_bytes(decoded_wire) == (
        evidence.canonical_error_utf8
    )
    assert second_wire.context["nested"]["array"][0]["leaf"] == "before"
    assert source_context["nested"] is not second_wire.context["nested"]


def test_fix_bk06_untyped_callback_fault_is_a_lifecycle_failure(
    client,
    monkeypatch,
) -> None:
    """FIX-BK06 — untyped D-21 callback never starts timer/spec/native work."""
    gateway = get_gateway()
    monkeypatch.setattr(
        calibration_service,
        "_verify_single_evidence",
        lambda *_args: object(),
    )
    with (
        mock.patch.object(
            gateway,
            "_build_single_execution_spec",
            wraps=gateway._build_single_execution_spec,
        ) as build,
        mock.patch.object(
            gateway,
            "_inspect_single_execution_identity",
            wraps=gateway._inspect_single_execution_identity,
        ) as inspect_identity,
        mock.patch.object(
            gateway,
            "_calibrate_single_verified",
            wraps=gateway._calibrate_single_verified,
        ) as native,
    ):
        submitted = client.post(
            "/api/calibrations/single",
            json=single_request("USD", 0.04),
        )
        terminal = wait_for_terminal(
            client,
            {"location": submitted.headers["location"]},
        )

    assert terminal["status"] == "failed"
    assert terminal["error"]["code"] == "LIFECYCLE_TRANSITION_FAILED"
    assert terminal["error"]["context"]["transition"] == (
        "verify_pre_native_admission_evidence"
    )
    assert build.call_count == inspect_identity.call_count == native.call_count == 0
    assert terminal["actual_execution_identity"] is None
    assert terminal["timings"] == {
        "native_solve_ms": None,
        "serialization_ms": None,
    }


def test_fix_b5_on_lock_fault_suppresses_all_evidence_and_native_events(
    client,
    monkeypatch,
) -> None:
    """FIX-B5-LOCK-HANDSHAKE — on-lock failure has strict precedence."""
    gateway = get_gateway()
    store = get_store()
    monkeypatch.setattr(
        store,
        "mark_calibration_solving",
        mock.Mock(side_effect=RuntimeError("injected on-lock failure")),
    )
    with (
        mock.patch.object(
            store,
            "load_single_worker_admission_evidence",
            wraps=store.load_single_worker_admission_evidence,
        ) as load,
        mock.patch.object(
            gateway,
            "_build_single_execution_spec",
            wraps=gateway._build_single_execution_spec,
        ) as build,
        mock.patch.object(
            gateway,
            "_calibrate_single_verified",
            wraps=gateway._calibrate_single_verified,
        ) as native,
    ):
        submitted = client.post(
            "/api/calibrations/single",
            json=single_request("USD", 0.04),
        )
        terminal = wait_for_terminal(
            client,
            {"location": submitted.headers["location"]},
        )

    assert terminal["status"] == "failed"
    assert terminal["error"]["code"] == "LIFECYCLE_TRANSITION_FAILED"
    assert terminal["error"]["context"]["transition"] == "mark_calibration_solving"
    assert load.call_count == build.call_count == native.call_count == 0


@pytest.mark.parametrize("kind", ("xccy_staged", "xccy_joint"))
def test_reviewer_xccy_on_lock_fault_is_a_lifecycle_failure(kind: str) -> None:
    """D10 — staged/joint on-lock faults use the lifecycle envelope before native."""
    gateway = get_gateway()
    store = mock.Mock()
    store.mark_calibration_solving.side_effect = RuntimeError("injected on-lock failure")

    with (
        mock.patch.object(
            gateway,
            "_calibrate_xccy_fallback",
            wraps=gateway._calibrate_xccy_fallback,
        ) as native,
        mock.patch.object(
            gateway_module.time,
            "perf_counter",
            wraps=gateway_module.time.perf_counter,
        ) as timer,
    ):
        asyncio.run(
            calibration_service._run_xccy_worker(
                store,
                gateway,
                "a" * 32,
                object(),
                (),
                kind,
            )
        )

    assert timer.call_count == native.call_count == 0
    assert store.fail_calibration.call_count == 1
    error = store.fail_calibration.call_args.kwargs["error_payload"]
    assert error["code"] == "LIFECYCLE_TRANSITION_FAILED"
    assert error["context"]["transition"] == "mark_calibration_solving"


@pytest.mark.parametrize(
    ("fault_stage", "expected_code", "actual_expected"),
    (
        ("worker_spec", "NATIVE_CALIBRATION_FAILED", False),
        ("worker_inspector", "NATIVE_CALIBRATION_FAILED", False),
        ("native_solve", "NATIVE_CALIBRATION_FAILED", False),
        ("post_solve_storage", "NATIVE_KNOT_PLAN_MISMATCH", True),
    ),
)
def test_fix_worker_execution_identity_faults_are_atomic_and_never_repaired(
    client,
    monkeypatch,
    fault_stage: str,
    expected_code: str,
    actual_expected: bool,
) -> None:
    """FIX-WORKER-EXECUTION-IDENTITY — API-13 worker/post-solve faults."""
    gateway = get_gateway()
    store = get_store()
    original_build = gateway._build_single_execution_spec
    original_inspect = gateway._inspect_single_execution_identity
    original_native = gateway._calibrate_single_verified

    def build(*args, **kwargs):
        if fault_stage == "worker_spec":
            raise RuntimeError("injected worker spec fault")
        return original_build(*args, **kwargs)

    def inspect_identity(*args, **kwargs):
        if fault_stage == "worker_inspector":
            raise RuntimeError("injected worker inspector fault")
        return original_inspect(*args, **kwargs)

    def native(*args, **kwargs):
        if fault_stage == "native_solve":
            raise RuntimeError("injected native solve fault")
        return original_native(*args, **kwargs)

    def terminal_identity(expected, result, elapsed_ms):
        if fault_stage != "post_solve_storage":
            return gateway_module._require_terminal_single_identity(
                expected, result, elapsed_ms
            )
        actual = expected.model_copy(
            update={"today": expected.today.replace(day=3)}
        )
        raise calibration_service.NativeExecutionIdentityMismatchError(
            expected,
            actual,
            comparison_stage="post_solve_storage",
            actual_jacobian_mode=result.actual_jacobian_mode,
            native_solve_ms=elapsed_ms,
        )

    monkeypatch.setattr(gateway, "_build_single_execution_spec", build)
    monkeypatch.setattr(
        gateway,
        "_inspect_single_execution_identity",
        inspect_identity,
    )
    monkeypatch.setattr(gateway, "_calibrate_single_verified", native)
    if fault_stage == "post_solve_storage":
        monkeypatch.setattr(
            gateway_module,
            "_require_terminal_single_identity",
            terminal_identity,
        )

    submitted = client.post(
        "/api/calibrations/single",
        json=single_request("USD", 0.04),
    )
    run_id = submitted.json()["id"]
    terminal = wait_for_terminal(
        client,
        {"location": submitted.headers["location"]},
    )

    assert terminal["status"] == "failed"
    assert terminal["error"]["code"] == expected_code
    assert (terminal["actual_execution_identity"] is not None) is actual_expected
    assert (terminal["actual_execution_identity_hash"] is not None) is actual_expected
    assert terminal["timings"]["serialization_ms"] is None
    first = client.get(f"/api/calibrations/{run_id}")
    second = client.get(f"/api/calibrations/{run_id}")
    assert first.content == second.content
    with store._session() as session:
        curve_count = session.scalar(
            select(func.count())
            .select_from(CurveDefinitionRow)
            .where(CurveDefinitionRow.source_run_id == run_id)
        )
    assert curve_count == 0


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

    original_notify = gateway._notify_lock_acquired

    def notify(callback: Callable):
        recorder.record("lock_acquired")

        def observed(acquired_at):
            recorder.record("on_lock_callback_entered")
            callback(acquired_at)

        return original_notify(observed)

    monkeypatch.setattr(gateway, "_notify_lock_acquired", notify)

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
    """FIX-PERSISTED-EXPECTED-IDENTITY-INTEGRITY — atomic plan/identity faults."""
    store = get_store()
    gateway = get_gateway()
    load_entered = threading.Event()
    release_load = threading.Event()
    frozen_evidence: list[calibration_service.FrozenIntegrityErrorEvidence] = []
    gateway_exceptions: list[Exception] = []
    terminal_exceptions: list[Exception] = []
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
        frozen_evidence.append(frozen)
        return frozen

    monkeypatch.setattr(
        calibration_service,
        "freeze_integrity_error_evidence",
        observe_freeze,
    )

    original_gateway_verify = gateway._verify_single_admission

    def observe_gateway_verify(*args, **kwargs):
        try:
            return original_gateway_verify(*args, **kwargs)
        except (
            calibration_service.PersistedKnotPlanIntegrityError,
            calibration_service.PersistedExpectedExecutionIdentityIntegrityError,
        ) as exception:
            gateway_exceptions.append(exception)
            raise

    monkeypatch.setattr(
        gateway,
        "_verify_single_admission",
        observe_gateway_verify,
    )

    original_terminalize = calibration_service._terminalize_integrity_error

    def observe_terminalize(*args, **kwargs):
        terminal_exceptions.append(args[2])
        return original_terminalize(*args, **kwargs)

    monkeypatch.setattr(
        calibration_service,
        "_terminalize_integrity_error",
        observe_terminalize,
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
        mock.patch.object(
            calibration_service,
            "to_api_error_dto",
            wraps=calibration_service.to_api_error_dto,
        ) as project,
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
    assert project.call_count == 1
    assert len(frozen_evidence) == len(persisted_bytes) == 1
    assert len(gateway_exceptions) == len(terminal_exceptions) == 1
    assert gateway_exceptions[0] is terminal_exceptions[0]
    assert gateway_exceptions[0].error is frozen_evidence[0]
    assert persisted_bytes[0] is frozen_evidence[0].canonical_error_utf8

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


@pytest.mark.parametrize("domain", ("plan", "expected"))
@pytest.mark.parametrize(
    "fault_stage",
    ("factory", "terminal_projection", "terminal_bytes", "store_commit"),
)
def test_fix_bk07_integrity_terminal_fault_matrix_uses_atomic_fallback(
    client,
    monkeypatch,
    domain: str,
    fault_stage: str,
) -> None:
    """FIX-BK07-FROZEN-INTEGRITY-ERROR-EVIDENCE — D-22 fault matrix."""
    store = get_store()
    gateway = get_gateway()
    load_entered = threading.Event()
    release_load = threading.Event()
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

    integrity_store_calls = 0
    projection_calls = 0
    bytes_guard_calls = 0
    projection_returned = False
    original_freeze = calibration_service.freeze_integrity_error_evidence
    original_project = calibration_service.to_api_error_dto
    original_json_bytes = calibration_service.canonical_json_bytes
    fail_name = (
        "fail_knot_plan_integrity"
        if domain == "plan"
        else "fail_expected_execution_identity_integrity"
    )
    original_integrity_store = getattr(store, fail_name)

    def freeze(*args, **kwargs):
        if fault_stage == "factory":
            raise RuntimeError("injected evidence factory fault")
        return original_freeze(*args, **kwargs)

    def project(error):
        nonlocal projection_calls, projection_returned
        projection_calls += 1
        if fault_stage == "terminal_projection":
            raise RuntimeError("injected terminal projection fault")
        dto = original_project(error)
        if fault_stage == "terminal_bytes":
            dto.message += " mutated"
        projection_returned = True
        return dto

    def json_bytes(value):
        nonlocal bytes_guard_calls
        if (
            projection_returned
            and isinstance(value, dict)
            and value.get("code", "").startswith("PERSISTED_")
        ):
            bytes_guard_calls += 1
        return original_json_bytes(value)

    def fail_integrity(*args, **kwargs):
        nonlocal integrity_store_calls
        integrity_store_calls += 1
        if fault_stage == "store_commit":
            raise RuntimeError("injected integrity commit fault")
        return original_integrity_store(*args, **kwargs)

    monkeypatch.setattr(
        calibration_service,
        "freeze_integrity_error_evidence",
        freeze,
    )
    monkeypatch.setattr(calibration_service, "to_api_error_dto", project)
    monkeypatch.setattr(calibration_service, "canonical_json_bytes", json_bytes)
    monkeypatch.setattr(store, fail_name, fail_integrity)

    with (
        mock.patch.object(
            gateway,
            "_build_single_execution_spec",
            wraps=gateway._build_single_execution_spec,
        ) as build,
        mock.patch.object(
            gateway,
            "_inspect_single_execution_identity",
            wraps=gateway._inspect_single_execution_identity,
        ) as inspect_identity,
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
        run_id = submitted.json()["id"]
        assert load_entered.wait(timeout=5.0)
        with store._session() as session:
            row = session.get(CalibrationRunRow, run_id)
            assert row is not None
            if domain == "plan":
                corrupted = copy.deepcopy(row.resolved_knot_plan)
                corrupted["requested_policy"] = "INSTRUMENTS"
                row.resolved_knot_plan = corrupted
            else:
                corrupted = copy.deepcopy(row.expected_execution_identity)
                corrupted["today"] = "2026-01-03"
                row.expected_execution_identity = corrupted
            session.commit()
        release_load.set()
        terminal = wait_for_terminal(
            client,
            {"location": submitted.headers["location"]},
        )

    assert terminal["status"] == "failed"
    assert terminal["error"]["code"] == "LIFECYCLE_TRANSITION_FAILED"
    assert terminal["error"]["context"]["transition"] == (
        "verify_pre_native_admission_evidence"
        if fault_stage == "factory"
        else (
            "persist_knot_plan_integrity_failure"
            if domain == "plan"
            else "persist_expected_identity_integrity_failure"
        )
    )
    assert build.call_count == inspect_identity.call_count == native.call_count == 0
    assert complete.call_count == 0
    assert projection_calls == int(fault_stage != "factory")
    assert bytes_guard_calls == int(
        fault_stage in {"terminal_bytes", "store_commit"}
    )
    assert integrity_store_calls == int(fault_stage == "store_commit")
    with store._session() as session:
        curve_count = session.scalar(
            select(func.count())
            .select_from(CurveDefinitionRow)
            .where(CurveDefinitionRow.source_run_id == run_id)
        )
    assert curve_count == 0


@pytest.mark.parametrize("domain", ("plan", "expected"))
def test_fix_bk07_double_terminal_fault_leaves_complete_running_row(
    client,
    monkeypatch,
    domain: str,
) -> None:
    """FIX-BK07 — integrity rollback plus lifecycle rollback has no partial row."""
    store = get_store()
    gateway = get_gateway()
    load_entered = threading.Event()
    release_load = threading.Event()
    lifecycle_rolled_back = threading.Event()
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
    fail_name = (
        "fail_knot_plan_integrity"
        if domain == "plan"
        else "fail_expected_execution_identity_integrity"
    )
    monkeypatch.setattr(
        store,
        fail_name,
        mock.Mock(side_effect=RuntimeError("injected integrity commit fault")),
    )

    def fail_lifecycle(*_args, **_kwargs):
        lifecycle_rolled_back.set()
        raise RuntimeError("injected lifecycle terminal fault")

    monkeypatch.setattr(store, "fail_calibration", fail_lifecycle)
    with (
        mock.patch.object(
            gateway,
            "_build_single_execution_spec",
            wraps=gateway._build_single_execution_spec,
        ) as build,
        mock.patch.object(
            gateway,
            "_calibrate_single_verified",
            wraps=gateway._calibrate_single_verified,
        ) as native,
    ):
        submitted = client.post(
            "/api/calibrations/single",
            json=single_request("USD", 0.04),
        )
        run_id = submitted.json()["id"]
        assert load_entered.wait(timeout=5.0)
        with store._session() as session:
            row = session.get(CalibrationRunRow, run_id)
            assert row is not None
            if domain == "plan":
                corrupted = copy.deepcopy(row.resolved_knot_plan)
                corrupted["requested_policy"] = "INSTRUMENTS"
                row.resolved_knot_plan = corrupted
            else:
                corrupted = copy.deepcopy(row.expected_execution_identity)
                corrupted["today"] = "2026-01-03"
                row.expected_execution_identity = corrupted
            session.commit()
        release_load.set()
        assert lifecycle_rolled_back.wait(timeout=5.0)
        visible = client.get(submitted.headers["location"]).json()

    assert visible["status"] == "running"
    assert visible["phase"] == "solving"
    assert visible["finished_at"] is None
    assert visible["error"] is None
    assert visible["actual_execution_identity"] is None
    assert build.call_count == native.call_count == 0
    with store._session() as session:
        curve_count = session.scalar(
            select(func.count())
            .select_from(CurveDefinitionRow)
            .where(CurveDefinitionRow.source_run_id == run_id)
        )
    assert curve_count == 0
