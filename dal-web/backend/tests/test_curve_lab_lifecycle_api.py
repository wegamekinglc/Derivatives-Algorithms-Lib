"""Revision 8 draft/build/version lifecycle acceptance contract."""

from __future__ import annotations

import gzip
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from copy import deepcopy

import pytest
from sqlalchemy import create_engine, event, inspect
from sqlalchemy.orm import Session


def _document(raw_quote: str = "0.04") -> dict[str, object]:
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
                "start_date": "2026-01-16",
                "maturity_date": "2026-04-16",
                "currency_or_pair": "USD",
                "raw_quote": raw_quote,
                "source": "TEST",
                "observed_at": "2026-01-15T00:00:00Z",
                "included": True,
                "terms": {"index": "USD-SOFR"},
            }
        ],
        "dependency_version_ids": [],
        "solver": {
            "solve_mode": "EXACT",
            "parameterization": "PIECEWISE_CONSTANT_FWD",
        },
    }


def _create_draft(client, raw_quote: str = "0.04") -> dict[str, object]:
    response = client.post("/api/curve-lab/drafts", json=_document(raw_quote))
    assert response.status_code == 201, response.text
    return response.json()


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


def _completed_build(client, draft_id: str) -> dict[str, object]:
    response = client.post(f"/api/curve-lab/drafts/{draft_id}/build-runs")
    assert response.status_code == 202, response.text
    admitted = response.json()
    assert admitted["state"] == "QUEUED"
    completed = _wait_for_job(
        client,
        "build-runs",
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


def _completed_store_build(store, admitted: dict[str, object]) -> dict[str, object]:
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        completed = store.get_curve_lab_build_run(admitted["id"])
        if completed["state"] in {"SUCCEEDED", "FAILED", "TIMED_OUT"}:
            assert completed["state"] == "SUCCEEDED", completed
            return completed
        time.sleep(0.01)
    pytest.fail(f"build-runs/{admitted['id']} did not reach a terminal state")


def test_draft_create_get_and_restart_preserve_canonical_financial_document(
    client,
) -> None:
    created = _create_draft(client)
    instrument = created["document"]["instruments"][0]

    assert created["schema_version"] == 2
    assert created["revision"] == 1
    assert created["state"] == "READY_TO_BUILD"
    assert len(created["fingerprint"]) == 64
    assert len(instrument["instrument_id"]) == 32
    assert instrument["raw_quote"] == "0.04"
    assert instrument["normalized_quote"] == "0.04"
    assert instrument["quote_coordinate_kind"] == "RATE"
    assert instrument["exact_risk_raw_bump"] == "0.0001"

    restarted = client.get(f"/api/curve-lab/drafts/{created['id']}")
    assert restarted.status_code == 200
    assert restarted.content == client.get(f"/api/curve-lab/drafts/{created['id']}").content
    assert restarted.json() == created


def test_draft_rejects_percent_or_axis_override_before_any_row_or_audit(
    client,
) -> None:
    percent = _document("4")
    percent["instruments"][0]["input_convention"] = "PERCENT"
    override = _document()
    override["instruments"][0]["normalized_quote"] = "0.04"

    first = client.post("/api/curve-lab/drafts", json=percent)
    second = client.post("/api/curve-lab/drafts", json=override)

    assert first.status_code == 422
    assert second.status_code == 422
    assert first.json()["detail"]["code"] == "QUOTE_AXIS_OVERRIDE_FORBIDDEN"
    assert second.json()["detail"]["code"] == "QUOTE_AXIS_OVERRIDE_FORBIDDEN"


def test_draft_contract_rejects_empty_topology_and_open_solver_or_terms(client) -> None:
    empty = _document()
    empty["instruments"] = []
    open_solver = _document()
    open_solver["solver"]["surprise"] = True
    open_terms = _document()
    open_terms["instruments"][0]["terms"]["surprise"] = True

    empty_response = client.post("/api/curve-lab/drafts", json=empty)
    solver_response = client.post("/api/curve-lab/drafts", json=open_solver)
    terms_response = client.post("/api/curve-lab/drafts", json=open_terms)

    assert empty_response.status_code == 422
    assert solver_response.status_code == 422
    assert terms_response.status_code == 422
    assert empty_response.json()["detail"]["code"] == "DRAFT_TOPOLOGY_INVALID"
    assert solver_response.json()["detail"]["code"] == "REQUEST_VALIDATION_FAILED"
    assert terms_response.json()["detail"]["code"] == "REQUEST_VALIDATION_FAILED"


def test_draft_compare_and_swap_is_atomic_and_marks_old_run_stale(client) -> None:
    draft = _create_draft(client)
    build = client.post(f"/api/curve-lab/drafts/{draft['id']}/build-runs")
    assert build.status_code == 202, build.text
    admitted = build.json()
    assert admitted["state"] == "QUEUED"
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        old_run = client.get(f"/api/curve-lab/build-runs/{admitted['id']}").json()
        if old_run["state"] == "SUCCEEDED":
            break
        time.sleep(0.01)
    else:
        pytest.fail("build run did not reach SUCCEEDED")

    changed = _document("0.041")
    changed["instruments"][0]["instrument_id"] = draft["document"]["instruments"][0][
        "instrument_id"
    ]
    conflict = client.put(
        f"/api/curve-lab/drafts/{draft['id']}",
        headers={"If-Match": '"0"'},
        json=changed,
    )
    unchanged = client.get(f"/api/curve-lab/drafts/{draft['id']}").json()

    assert conflict.status_code == 409
    assert conflict.json()["detail"]["code"] == "DRAFT_REVISION_CONFLICT"
    assert unchanged == draft

    updated = client.put(
        f"/api/curve-lab/drafts/{draft['id']}",
        headers={"If-Match": '"1"'},
        json=changed,
    )
    assert updated.status_code == 200
    assert updated.json()["revision"] == 2
    assert updated.json()["fingerprint"] != draft["fingerprint"]
    assert (
        updated.json()["document"]["instruments"][0]["instrument_id"]
        == (draft["document"]["instruments"][0]["instrument_id"])
    )

    stale = client.get(f"/api/curve-lab/build-runs/{old_run['id']}")
    assert stale.status_code == 200
    assert stale.json()["stale"] is True
    assert stale.json()["draft_revision"] == 1


def test_concurrent_draft_updates_have_exactly_one_cas_winner(client) -> None:
    from app.services.db.store_db import DbStore
    from app.services.store import ConflictError, get_store

    draft = _create_draft(client)
    records = []
    for marker in ("a", "b"):
        record = deepcopy(draft)
        record["revision"] = 2
        record["fingerprint"] = marker * 64
        records.append(record)
    database_url = get_store().url
    stores = (DbStore(database_url), DbStore(database_url))
    barrier = threading.Barrier(2)

    def synchronize_draft_update(execute_state) -> None:
        statement = str(execute_state.statement)
        if execute_state.is_update and statement.startswith("UPDATE curve_drafts"):
            barrier.wait(timeout=5)

    def update(args) -> str:
        store, record = args
        try:
            store.update_curve_lab_draft(draft["id"], 1, record)
        except ConflictError:
            return "conflict"
        return "updated"

    event.listen(Session, "do_orm_execute", synchronize_draft_update)
    try:
        with ThreadPoolExecutor(max_workers=2) as pool:
            outcomes = list(pool.map(update, zip(stores, records, strict=True)))
    finally:
        event.remove(Session, "do_orm_execute", synchronize_draft_update)
        for store in stores:
            store.close()

    assert sorted(outcomes) == ["conflict", "updated"]
    persisted = client.get(f"/api/curve-lab/drafts/{draft['id']}").json()
    assert persisted["revision"] == 2
    assert persisted["fingerprint"] in {"a" * 64, "b" * 64}


def test_version_publication_is_cas_idempotent_immutable_and_archivable(client) -> None:
    draft = _create_draft(client)
    run = _completed_build(client, draft["id"])
    request = {
        "draft_id": draft["id"],
        "draft_revision": draft["revision"],
        "draft_fingerprint": draft["fingerprint"],
        "build_run_id": run["id"],
        "name": "USD SOFR",
        "version_note": "baseline",
        "tags": ["USD", "OIS"],
        "idempotency_key": "version-once",
    }

    created = client.post("/api/curve-lab/versions", json=request)
    replay = client.post("/api/curve-lab/versions", json=request)
    assert created.status_code == 201, created.text
    assert replay.status_code == 200
    assert replay.json() == created.json()
    version = created.json()

    native = client.get(f"/api/curve-lab/versions/{version['id']}/native-json")
    assert native.status_code == 200
    assert native.headers["content-type"].startswith("application/json")
    assert native.content
    assert version["native_payload_length"] == len(native.content)
    assert len(version["native_payload_hash"]) == 64

    archived = client.post(f"/api/curve-lab/versions/{version['id']}/archive")
    assert archived.status_code == 200
    assert archived.json()["visibility_state"] == "ARCHIVED"
    assert client.get("/api/curve-lab/versions").json() == []
    assert (
        client.get("/api/curve-lab/versions", params={"include_archived": True}).json()[0]["id"]
        == version["id"]
    )


def test_concurrent_version_publication_returns_one_immutable_version(client) -> None:
    from app.schemas.curve_lab import CurveVersionCreateRequest
    from app.services.curve_lab_lifecycle import create_version
    from app.services.db.store_db import DbStore
    from app.services.store import get_store

    draft = _create_draft(client)
    run = _completed_build(client, draft["id"])
    request = CurveVersionCreateRequest.model_validate(
        {
            "draft_id": draft["id"],
            "draft_revision": draft["revision"],
            "draft_fingerprint": draft["fingerprint"],
            "build_run_id": run["id"],
            "name": "concurrent",
            "idempotency_key": "concurrent-version-once",
        }
    )
    database_url = get_store().url
    stores = (DbStore(database_url), DbStore(database_url))
    barrier = threading.Barrier(2)
    counter_lock = threading.Lock()
    synchronized_reads = 0

    def synchronize_idempotency_read(execute_state) -> None:
        nonlocal synchronized_reads
        if execute_state.is_select and "curve_versions.idempotency_key" in str(
            execute_state.statement
        ):
            with counter_lock:
                participate = synchronized_reads < 2
                synchronized_reads += 1
            if participate:
                barrier.wait(timeout=5)

    event.listen(Session, "do_orm_execute", synchronize_idempotency_read)
    try:
        with ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(lambda store: create_version(store, request), stores))
    finally:
        event.remove(Session, "do_orm_execute", synchronize_idempotency_read)
        for store in stores:
            store.close()

    assert {result[0]["id"] for result in results} == {
        client.get("/api/curve-lab/versions").json()[0]["id"]
    }
    assert sorted(created for _, created in results) == [False, True]


def test_clone_rekeys_instruments_and_keeps_source_identity(client) -> None:
    draft = _create_draft(client)
    run = _completed_build(client, draft["id"])
    version = client.post(
        "/api/curve-lab/versions",
        json={
            "draft_id": draft["id"],
            "draft_revision": 1,
            "draft_fingerprint": draft["fingerprint"],
            "build_run_id": run["id"],
            "name": "clone-source",
            "idempotency_key": "clone-source",
        },
    ).json()

    clone = client.post(f"/api/curve-lab/versions/{version['id']}/clone")
    assert clone.status_code == 201
    source_instrument = draft["document"]["instruments"][0]
    cloned_instrument = clone.json()["document"]["instruments"][0]
    assert cloned_instrument["instrument_id"] != source_instrument["instrument_id"]
    assert cloned_instrument["source_instrument_id"] == source_instrument["instrument_id"]


def test_failed_version_cas_and_failed_import_publish_nothing(client) -> None:
    draft = _create_draft(client)
    run = _completed_build(client, draft["id"])
    bad = {
        "draft_id": draft["id"],
        "draft_revision": 999,
        "draft_fingerprint": draft["fingerprint"],
        "build_run_id": run["id"],
        "name": "bad",
        "idempotency_key": "bad",
    }
    failed_version = client.post("/api/curve-lab/versions", json=bad)
    failed_import = client.post(
        "/api/curve-lab/import-jobs",
        content=b'{"~type":"DefinitelyNotAllowed","$tag":"1"}',
        headers={"Content-Type": "application/json"},
    )

    assert failed_version.status_code == 409
    assert failed_version.json()["detail"]["code"] == "DRAFT_REVISION_CONFLICT"
    assert failed_import.status_code == 422
    assert failed_import.json()["detail"]["code"] == "IMPORT_ROOT_TYPE_FORBIDDEN"
    assert client.get("/api/curve-lab/versions").json() == []


def test_unknown_eighth_family_has_zero_durable_side_effects(client) -> None:
    payload = deepcopy(_document())
    payload["instruments"][0]["instrument_type"] = "SWAPTION"

    response = client.post("/api/curve-lab/drafts", json=payload)

    assert response.status_code == 422
    assert response.json()["detail"]["code"] == "UNSUPPORTED_PRODUCT"


def test_allowed_native_import_round_trips_and_publishes_one_version(client) -> None:
    draft = _create_draft(client)
    run = _completed_build(client, draft["id"])
    built = client.post(
        "/api/curve-lab/versions",
        json={
            "draft_id": draft["id"],
            "draft_revision": 1,
            "draft_fingerprint": draft["fingerprint"],
            "build_run_id": run["id"],
            "name": "export-source",
            "idempotency_key": "export-source",
        },
    ).json()
    payload = client.get(f"/api/curve-lab/versions/{built['id']}/native-json").content

    imported = client.post(
        "/api/curve-lab/import-jobs",
        content=payload,
        headers={"Content-Type": "application/json"},
    )

    completed_import = _completed_import(client, imported)
    imported_version = completed_import["resulting_version_id"]
    assert imported_version != built["id"]
    imported_payload = client.get(f"/api/curve-lab/versions/{imported_version}/native-json")
    assert imported_payload.status_code == 200
    assert imported_payload.content


def test_import_create_acknowledges_queued_before_native_reconstruction(
    client,
    monkeypatch,
) -> None:
    import app.services.dal_gateway as gateway_module

    gateway = gateway_module.get_gateway()
    entered = threading.Event()
    release = threading.Event()
    payload = b'{"~type":"Bag","name":"curves","keys":[]}'

    def blocked_import(archive: bytes) -> tuple[bytes, str]:
        entered.set()
        assert release.wait(timeout=5.0)
        return archive, "CURVE_SET"

    monkeypatch.setattr(gateway, "import_curve_lab_archive", blocked_import)

    started_at = time.monotonic()
    response = client.post(
        "/api/curve-lab/import-jobs",
        content=payload,
        headers={"Content-Type": "application/json"},
    )
    elapsed = time.monotonic() - started_at

    assert response.status_code == 202
    assert elapsed < 0.3
    assert response.json()["state"] == "QUEUED"
    assert entered.wait(timeout=1.0)

    release.set()
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        terminal = client.get(f"/api/curve-lab/import-jobs/{response.json()['id']}").json()
        if terminal["state"] == "SUCCEEDED":
            break
        time.sleep(0.01)
    else:
        pytest.fail("import job did not reach SUCCEEDED")


def test_import_allowlist_failure_never_calls_native_reader(client, monkeypatch) -> None:
    import app.services.dal_gateway as gateway_module

    gateway = gateway_module.get_gateway()
    called = False

    def forbidden(_payload: bytes):
        nonlocal called
        called = True
        raise AssertionError("native reader called before root allowlist")

    monkeypatch.setattr(gateway, "import_curve_lab_archive", forbidden)

    response = client.post(
        "/api/curve-lab/import-jobs",
        content=b'{"$tag":"1","~type":"Swaption"}',
        headers={"Content-Type": "application/json"},
    )

    assert response.status_code == 422
    assert response.json()["detail"]["code"] == "IMPORT_ROOT_TYPE_FORBIDDEN"
    assert called is False


@pytest.mark.parametrize(
    ("payload", "code"),
    [
        (b'{"~type":"Bag"} trailing', "ARCHIVE_JSON_TRAILING_BYTES"),
        (b'{"~type":"Bag","~type":"Bag"}', "ARCHIVE_JSON_DUPLICATE_KEY"),
        (b'{"~type":"Bag"}\x00', "ARCHIVE_PAYLOAD_NUL"),
        (b"\xff", "ARCHIVE_PAYLOAD_INVALID_UTF8"),
        (
            b'{"~type":"DiscountPWC_v1","name":"curve","ccy":{"$tag":"1"},'
            b'"knotDates":["2027-01-15"],"rightVals":[0.04]}',
            "ARCHIVE_FIELD_TYPE_INVALID",
        ),
        (
            b'{"~type":"Bag","name":"outer","keys":["nested"],'
            b'"contents0":{"~type":"Bag","name":"inner","keys":[]}}',
            "ARCHIVE_TYPE_POSITION_FORBIDDEN",
        ),
    ],
)
def test_classified_import_failure_is_persisted_without_calling_native(
    client,
    monkeypatch,
    payload: bytes,
    code: str,
) -> None:
    import app.services.dal_gateway as gateway_module

    gateway = gateway_module.get_gateway()
    called = False

    def forbidden(_payload: bytes):
        nonlocal called
        called = True
        raise AssertionError("native reader called before archive preflight")

    monkeypatch.setattr(gateway, "import_curve_lab_archive", forbidden)

    response = client.post(
        "/api/curve-lab/import-jobs",
        content=payload,
        headers={"Content-Type": "application/json"},
    )

    assert response.status_code == 422
    detail = response.json()["detail"]
    assert detail["code"] == code
    assert called is False
    persisted = client.get(f"/api/curve-lab/import-jobs/{detail['resource_id']}")
    assert persisted.status_code == 200
    assert persisted.json()["state"] == "FAILED"
    assert persisted.json()["phase"] == "PREFLIGHT"
    assert persisted.json()["error"]["code"] == code
    assert persisted.json()["resulting_version_id"] is None


def test_gzip_preflight_failure_persists_exact_wire_and_expanded_lengths(
    client,
    monkeypatch,
) -> None:
    import app.services.dal_gateway as gateway_module

    expanded = b'{"~type":"Bag"} trailing'
    payload = gzip.compress(expanded)
    gateway = gateway_module.get_gateway()
    called = False

    def forbidden(_payload: bytes):
        nonlocal called
        called = True
        raise AssertionError("native reader called before archive preflight")

    monkeypatch.setattr(gateway, "import_curve_lab_archive", forbidden)

    response = client.post(
        "/api/curve-lab/import-jobs",
        content=payload,
        headers={
            "Content-Type": "application/json",
            "Content-Encoding": "gzip",
        },
    )

    assert response.status_code == 422
    detail = response.json()["detail"]
    assert detail["code"] == "ARCHIVE_JSON_TRAILING_BYTES"
    assert called is False
    persisted = client.get(f"/api/curve-lab/import-jobs/{detail['resource_id']}").json()
    assert persisted["compressed_payload_length"] == len(payload)
    assert persisted["expanded_payload_length"] == len(expanded)


def test_import_publication_rolls_back_version_when_job_write_fails(
    client,
) -> None:
    from app.services.db.models import CurveLabImportJobRow

    draft = _create_draft(client)
    run = _completed_build(client, draft["id"])
    version = client.post(
        "/api/curve-lab/versions",
        json={
            "draft_id": draft["id"],
            "draft_revision": 1,
            "draft_fingerprint": draft["fingerprint"],
            "build_run_id": run["id"],
            "name": "import-transaction-source",
            "idempotency_key": "import-transaction-source",
        },
    ).json()
    payload = client.get(f"/api/curve-lab/versions/{version['id']}/native-json").content

    def fail_successful_job(_mapper, _connection, target) -> None:
        if target.state == "SUCCEEDED":
            raise RuntimeError("injected import job write failure")

    event.listen(CurveLabImportJobRow, "before_update", fail_successful_job)
    try:
        response = client.post(
            "/api/curve-lab/import-jobs",
            content=payload,
            headers={"Content-Type": "application/json"},
        )
        assert response.status_code == 202
        failed = _wait_for_job(
            client,
            "import-jobs",
            response.json()["id"],
            {"SUCCEEDED", "FAILED", "TIMED_OUT"},
        )
        assert failed["state"] == "FAILED"
        assert failed["error"]["code"] == "IMPORT_PUBLICATION_FAILED"
    finally:
        event.remove(CurveLabImportJobRow, "before_update", fail_successful_job)

    visible = client.get("/api/curve-lab/versions").json()
    assert [item["id"] for item in visible] == [version["id"]]


def test_database_restart_preserves_version_and_native_payload(tmp_path) -> None:
    from app.schemas.curve_lab import (
        CurveDraftDocumentInputV2,
        CurveVersionCreateRequest,
    )
    from app.services.curve_lab_lifecycle import (
        create_build_run,
        create_draft,
        create_version,
        native_payload,
    )
    from app.services.dal_gateway import DalGateway
    from app.services.db.store_db import DbStore

    database_url = f"sqlite:///{tmp_path / 'restart.db'}"
    first = DbStore(database_url)
    first.create_all()
    draft = create_draft(first, CurveDraftDocumentInputV2.model_validate(_document()))
    run = _completed_store_build(
        first,
        create_build_run(first, DalGateway(), draft["id"]),
    )
    version, created = create_version(
        first,
        CurveVersionCreateRequest(
            draft_id=draft["id"],
            draft_revision=draft["revision"],
            draft_fingerprint=draft["fingerprint"],
            build_run_id=run["id"],
            name="restart-proof",
            idempotency_key="restart-proof",
        ),
    )
    before_restart = native_payload(first, version["id"])
    first.close()

    restarted = DbStore(database_url)
    try:
        assert created is True
        assert restarted.get_curve_lab_draft(draft["id"]) == draft
        assert native_payload(restarted, version["id"]) == before_restart
    finally:
        restarted.close()


def test_database_restart_terminalizes_all_inflight_curve_lab_work(tmp_path) -> None:
    from app.schemas.curve_lab import (
        CurveDraftDocumentInputV2,
        CurveVersionCreateRequest,
    )
    from app.services.curve_lab_lifecycle import (
        create_build_run,
        create_draft,
        create_version,
    )
    from app.services.dal_gateway import DalGateway
    from app.services.db.store_db import DbStore

    database_url = f"sqlite:///{tmp_path / 'inflight-restart.db'}"
    first = DbStore(database_url)
    first.create_all()
    draft = create_draft(first, CurveDraftDocumentInputV2.model_validate(_document()))
    run = _completed_store_build(
        first,
        create_build_run(first, DalGateway(), draft["id"]),
    )
    version, _ = create_version(
        first,
        CurveVersionCreateRequest(
            draft_id=draft["id"],
            draft_revision=draft["revision"],
            draft_fingerprint=draft["fingerprint"],
            build_run_id=run["id"],
            name="restart-source",
            idempotency_key="restart-source",
        ),
    )
    first.update_curve_lab_build_run(
        run["id"],
        {
            **run,
            "state": "SOLVING",
            "error": None,
            "finished_at": None,
        },
    )
    first.add_curve_lab_import_job(
        {
            "id": "import-restart",
            "request_hash": "a" * 64,
            "compressed_payload_length": 1,
            "expanded_payload_length": 1,
            "state": "RUNNING",
            "phase": "DESERIALIZING",
            "error": None,
            "resulting_version_id": None,
            "created_at": "2026-01-15T00:00:00+00:00",
            "deadline_at": "2026-01-15T00:00:30+00:00",
            "finished_at": None,
        }
    )
    first.publish_curve_lab_risk_run(
        {
            "id": "risk-restart",
            "curve_version_id": version["id"],
            "calibration_run_id": None,
            "import_job_id": None,
            "source_kind": "VERSION",
            "request": {},
            "fixing_snapshot_hash": "b" * 64,
            "target_fingerprint": "c" * 64,
            "quote_axis": None,
            "parameter_axis": [],
            "estimated_work": {},
            "state": "QUEUED",
            "result": None,
            "error": None,
            "created_at": "2026-01-15T00:00:00+00:00",
            "deadline_at": "2026-01-15T00:15:00+00:00",
            "finished_at": None,
        },
        [],
    )
    first.close()

    restarted = DbStore(database_url)
    try:
        finished_at = "2026-01-15T00:01:00+00:00"
        assert restarted.reconcile_curve_lab_inflight(finished_at) == 3
        records = (
            restarted.get_curve_lab_build_run(run["id"]),
            restarted.get_curve_lab_import_job("import-restart"),
            restarted.get_curve_lab_risk_run("risk-restart"),
        )
        for record in (records[0], records[2]):
            assert record["state"] == "FAILED"
            assert record["error"]["code"] == "SERVER_RESTARTED"
            assert record["finished_at"] == finished_at
        assert records[1]["state"] == "TIMED_OUT"
        assert records[1]["error"] == {
            "code": "SOFT_DEADLINE_EXCEEDED",
            "message": "Curve Lab work exceeded its persisted soft deadline.",
            "field": "deadline_at",
            "value": "2026-01-15T00:00:30+00:00",
            "resource_id": "import-restart",
            "details": {},
        }
        assert records[1]["finished_at"] == finished_at
    finally:
        restarted.close()


def test_curve_lab_migration_upgrade_downgrade_upgrade(tmp_path, monkeypatch) -> None:
    from alembic import command
    from alembic.config import Config

    database = tmp_path / "curve-lab-migration.db"
    config = Config("alembic.ini")
    monkeypatch.setenv("DAL_WEB_DB_URL", f"sqlite:///{database}")

    command.upgrade(config, "head")
    names = set(inspect(create_engine(f"sqlite:///{database}")).get_table_names())
    assert {
        "curve_drafts",
        "curve_build_runs",
        "curve_versions",
        "curve_import_jobs",
        "curve_risk_runs",
        "curve_matrix_blobs",
        "curve_audit_events",
    } <= names

    command.downgrade(config, "c2d8f43a9e71")
    names = set(inspect(create_engine(f"sqlite:///{database}")).get_table_names())
    assert "curve_drafts" not in names

    command.upgrade(config, "head")
    names = set(inspect(create_engine(f"sqlite:///{database}")).get_table_names())
    assert "curve_versions" in names


def test_native_build_failure_is_persisted_and_restart_readable(client, monkeypatch) -> None:
    import app.services.dal_gateway as gateway_module

    draft = client.post("/api/curve-lab/drafts", json=_document()).json()
    gateway = gateway_module.get_gateway()

    def fail_native_build(_document) -> bytes:
        raise ValueError("deliberate native failure")

    monkeypatch.setattr(gateway, "build_curve_lab_archive", fail_native_build)

    response = client.post(f"/api/curve-lab/drafts/{draft['id']}/build-runs")

    assert response.status_code == 202
    assert response.json()["state"] == "QUEUED"
    run = _wait_for_job(
        client,
        "build-runs",
        response.json()["id"],
        {"SUCCEEDED", "FAILED", "TIMED_OUT"},
    )
    assert run["state"] == "FAILED"
    assert run["native_payload_hash"] is None
    assert run["error"]["code"] == "NATIVE_BUILD_FAILED"
    assert "deliberate native failure" not in run["error"]["message"]
    assert run["resolved_plan"]["mode"] == "SINGLE"
    assert run["diagnostics"]["fit_state"] == "FAILED"
    assert client.get(f"/api/curve-lab/build-runs/{run['id']}").json() == run
    assert client.get("/api/curve-lab/versions").json() == []


def test_build_create_acknowledges_queued_before_blocking_native_work(
    client,
    monkeypatch,
) -> None:
    import app.services.dal_gateway as gateway_module

    draft = client.post("/api/curve-lab/drafts", json=_document()).json()
    gateway = gateway_module.get_gateway()
    original = gateway.build_curve_lab_archive
    entered = threading.Event()
    release = threading.Event()

    def slow_build(document, dependencies=()) -> bytes:
        entered.set()
        assert release.wait(timeout=5)
        return original(document, dependencies)

    monkeypatch.setattr(gateway, "build_curve_lab_archive", slow_build)

    started = time.monotonic()
    response = client.post(f"/api/curve-lab/drafts/{draft['id']}/build-runs")
    elapsed = time.monotonic() - started

    assert response.status_code == 202
    assert response.json()["state"] == "QUEUED"
    assert elapsed < 0.3
    assert entered.wait(timeout=1)
    release.set()
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        completed = client.get(f"/api/curve-lab/build-runs/{response.json()['id']}").json()
        if completed["state"] in {"SUCCEEDED", "FAILED", "TIMED_OUT"}:
            break
        time.sleep(0.01)
    assert completed["state"] == "SUCCEEDED"


def test_build_rejects_missing_dependency_before_native_work(client, monkeypatch) -> None:
    """Skipping dependency resolution recreates DAL-23's false success."""
    import app.services.dal_gateway as gateway_module

    missing_id = "f" * 32
    document = _document()
    document["dependency_version_ids"] = [missing_id]
    draft = client.post("/api/curve-lab/drafts", json=document).json()
    gateway = gateway_module.get_gateway()
    called = False

    def forbidden_native_build(_document, _dependencies=()) -> bytes:
        nonlocal called
        called = True
        raise AssertionError("native build started before dependency admission")

    monkeypatch.setattr(
        gateway,
        "build_curve_lab_archive",
        forbidden_native_build,
    )

    response = client.post(f"/api/curve-lab/drafts/{draft['id']}/build-runs")

    assert response.status_code == 202
    assert response.json()["state"] == "QUEUED"
    run = _wait_for_job(
        client,
        "build-runs",
        response.json()["id"],
        {"SUCCEEDED", "FAILED", "TIMED_OUT"},
    )
    assert run["state"] == "FAILED"
    assert run["error"]["code"] == "DEPENDENCY_VERSION_NOT_FOUND"
    assert run["error"]["value"] == missing_id
    assert run["dependency_manifest"] == []
    assert called is False
    assert client.get(f"/api/curve-lab/build-runs/{run['id']}").json() == run


def test_build_pins_resolved_dependency_identity_hash_and_root_kind(client) -> None:
    source_draft = _create_draft(client)
    source_run = _completed_build(client, source_draft["id"])
    source_version = client.post(
        "/api/curve-lab/versions",
        json={
            "draft_id": source_draft["id"],
            "draft_revision": source_draft["revision"],
            "draft_fingerprint": source_draft["fingerprint"],
            "build_run_id": source_run["id"],
            "name": "dependency",
            "idempotency_key": "dependency",
        },
    ).json()
    document = _document("0.041")
    document["dependency_version_ids"] = [source_version["id"]]
    dependent_draft = client.post("/api/curve-lab/drafts", json=document).json()

    run = _completed_build(client, dependent_draft["id"])
    assert run["dependency_manifest"] == [
        {
            "version_id": source_version["id"],
            "content_hash": source_version["native_payload_hash"],
            "root_kind": source_version["root_kind"],
        }
    ]


def test_build_rejects_archived_dependency_before_native_work(
    client,
    monkeypatch,
) -> None:
    import app.services.dal_gateway as gateway_module

    source_draft = _create_draft(client)
    source_run = _completed_build(client, source_draft["id"])
    source_version = client.post(
        "/api/curve-lab/versions",
        json={
            "draft_id": source_draft["id"],
            "draft_revision": source_draft["revision"],
            "draft_fingerprint": source_draft["fingerprint"],
            "build_run_id": source_run["id"],
            "name": "archived dependency",
            "idempotency_key": "archived-dependency",
        },
    ).json()
    client.post(f"/api/curve-lab/versions/{source_version['id']}/archive")
    document = _document("0.041")
    document["dependency_version_ids"] = [source_version["id"]]
    dependent_draft = client.post("/api/curve-lab/drafts", json=document).json()
    gateway = gateway_module.get_gateway()
    called = False

    def forbidden_native_build(_document, _dependencies=()) -> bytes:
        nonlocal called
        called = True
        raise AssertionError("native build started with an archived dependency")

    monkeypatch.setattr(
        gateway,
        "build_curve_lab_archive",
        forbidden_native_build,
    )

    response = client.post(f"/api/curve-lab/drafts/{dependent_draft['id']}/build-runs")

    assert response.status_code == 202
    assert response.json()["state"] == "QUEUED"
    run = _wait_for_job(
        client,
        "build-runs",
        response.json()["id"],
        {"SUCCEEDED", "FAILED", "TIMED_OUT"},
    )
    assert run["state"] == "FAILED"
    assert run["error"]["code"] == "DEPENDENCY_VERSION_ARCHIVED"
    assert run["dependency_manifest"] == []
    assert called is False


def test_publication_rejects_dependency_archived_after_successful_build(
    client,
) -> None:
    source_draft = _create_draft(client)
    source_run = _completed_build(client, source_draft["id"])
    source_version = client.post(
        "/api/curve-lab/versions",
        json={
            "draft_id": source_draft["id"],
            "draft_revision": source_draft["revision"],
            "draft_fingerprint": source_draft["fingerprint"],
            "build_run_id": source_run["id"],
            "name": "publication dependency",
            "idempotency_key": "publication-dependency",
        },
    ).json()
    dependent_document = _document("0.041")
    dependent_document["dependency_version_ids"] = [source_version["id"]]
    dependent_draft = client.post(
        "/api/curve-lab/drafts",
        json=dependent_document,
    ).json()
    dependent_run = _completed_build(client, dependent_draft["id"])

    archived = client.post(f"/api/curve-lab/versions/{source_version['id']}/archive")
    published = client.post(
        "/api/curve-lab/versions",
        json={
            "draft_id": dependent_draft["id"],
            "draft_revision": dependent_draft["revision"],
            "draft_fingerprint": dependent_draft["fingerprint"],
            "build_run_id": dependent_run["id"],
            "name": "must not publish",
            "idempotency_key": "dependency-archived-before-publication",
        },
    )

    assert archived.status_code == 200
    assert published.status_code == 409
    assert published.json()["detail"]["code"] == "DEPENDENCY_VERSION_ARCHIVED"
    versions = client.get(
        "/api/curve-lab/versions",
        params={"include_archived": True},
    ).json()
    assert [version["id"] for version in versions] == [source_version["id"]]
