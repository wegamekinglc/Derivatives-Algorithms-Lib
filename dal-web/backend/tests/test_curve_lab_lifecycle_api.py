"""Revision 8 draft/build/version lifecycle acceptance contract."""

from __future__ import annotations

import threading
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
    assert restarted.content == client.get(
        f"/api/curve-lab/drafts/{created['id']}"
    ).content
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


def test_draft_compare_and_swap_is_atomic_and_marks_old_run_stale(client) -> None:
    draft = _create_draft(client)
    build = client.post(f"/api/curve-lab/drafts/{draft['id']}/build-runs")
    assert build.status_code == 202, build.text
    old_run = build.json()
    assert old_run["state"] == "SUCCEEDED"

    changed = _document("0.041")
    changed["instruments"][0]["instrument_id"] = draft["document"]["instruments"][
        0
    ]["instrument_id"]
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
    assert updated.json()["document"]["instruments"][0]["instrument_id"] == (
        draft["document"]["instruments"][0]["instrument_id"]
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
        if (
            execute_state.is_update
            and statement.startswith("UPDATE curve_drafts")
        ):
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
    run = client.post(f"/api/curve-lab/drafts/{draft['id']}/build-runs").json()
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
    assert client.get(
        "/api/curve-lab/versions", params={"include_archived": True}
    ).json()[0]["id"] == version["id"]


def test_concurrent_version_publication_returns_one_immutable_version(client) -> None:
    from app.schemas.curve_lab import CurveVersionCreateRequest
    from app.services.curve_lab_lifecycle import create_version
    from app.services.db.store_db import DbStore
    from app.services.store import get_store

    draft = _create_draft(client)
    run = client.post(f"/api/curve-lab/drafts/{draft['id']}/build-runs").json()
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
        if (
            execute_state.is_select
            and "curve_versions.idempotency_key" in str(execute_state.statement)
        ):
            with counter_lock:
                participate = synchronized_reads < 2
                synchronized_reads += 1
            if participate:
                barrier.wait(timeout=5)

    event.listen(Session, "do_orm_execute", synchronize_idempotency_read)
    try:
        with ThreadPoolExecutor(max_workers=2) as pool:
            results = list(
                pool.map(lambda store: create_version(store, request), stores)
            )
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
    run = client.post(f"/api/curve-lab/drafts/{draft['id']}/build-runs").json()
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
    run = client.post(f"/api/curve-lab/drafts/{draft['id']}/build-runs").json()
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
    run = client.post(f"/api/curve-lab/drafts/{draft['id']}/build-runs").json()
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
    payload = client.get(
        f"/api/curve-lab/versions/{built['id']}/native-json"
    ).content

    imported = client.post(
        "/api/curve-lab/import-jobs",
        content=payload,
        headers={"Content-Type": "application/json"},
    )

    assert imported.status_code == 202, imported.text
    assert imported.json()["state"] == "SUCCEEDED"
    imported_version = imported.json()["resulting_version_id"]
    assert imported_version != built["id"]
    imported_payload = client.get(
        f"/api/curve-lab/versions/{imported_version}/native-json"
    )
    assert imported_payload.status_code == 200
    assert imported_payload.content


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


def test_import_publication_rolls_back_version_when_job_write_fails(
    client,
) -> None:
    from app.services.db.models import CurveLabImportJobRow

    draft = _create_draft(client)
    run = client.post(f"/api/curve-lab/drafts/{draft['id']}/build-runs").json()
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
    payload = client.get(
        f"/api/curve-lab/versions/{version['id']}/native-json"
    ).content
    def fail_successful_job(_mapper, _connection, target) -> None:
        if target.state == "SUCCEEDED":
            raise RuntimeError("injected import job write failure")

    event.listen(CurveLabImportJobRow, "before_insert", fail_successful_job)
    try:
        with pytest.raises(RuntimeError, match="injected import job write failure"):
            client.post(
                "/api/curve-lab/import-jobs",
                content=payload,
                headers={"Content-Type": "application/json"},
            )
    finally:
        event.remove(CurveLabImportJobRow, "before_insert", fail_successful_job)

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
    draft = create_draft(
        first, CurveDraftDocumentInputV2.model_validate(_document())
    )
    run = create_build_run(first, DalGateway(), draft["id"])
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


def test_curve_lab_migration_upgrade_downgrade_upgrade(
    tmp_path, monkeypatch
) -> None:
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


def test_native_build_failure_is_persisted_and_restart_readable(
    client, monkeypatch
) -> None:
    import app.services.dal_gateway as gateway_module

    draft = client.post("/api/curve-lab/drafts", json=_document()).json()
    gateway = gateway_module.get_gateway()

    def fail_native_build(_document) -> bytes:
        raise ValueError("deliberate native failure")

    monkeypatch.setattr(gateway, "build_curve_lab_archive", fail_native_build)

    response = client.post(f"/api/curve-lab/drafts/{draft['id']}/build-runs")

    assert response.status_code == 202
    run = response.json()
    assert run["state"] == "FAILED"
    assert run["native_payload_hash"] is None
    assert run["error"]["code"] == "NATIVE_BUILD_FAILED"
    assert "deliberate native failure" not in run["error"]["message"]
    assert run["resolved_plan"]["mode"] == "SINGLE"
    assert run["diagnostics"]["fit_state"] == "FAILED"
    assert client.get(f"/api/curve-lab/build-runs/{run['id']}").json() == run
    assert client.get("/api/curve-lab/versions").json() == []
