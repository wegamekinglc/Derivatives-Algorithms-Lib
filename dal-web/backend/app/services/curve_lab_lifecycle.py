"""Transactional Curve Lab V2 draft, build, version, and import lifecycle."""

from __future__ import annotations

import hashlib
from datetime import UTC, datetime
from threading import Lock
from typing import TYPE_CHECKING
from uuid import uuid4

from app.schemas.curve_lab import (
    CURVE_LAB_V1_SUCCESS_REGISTRY,
    CurveDraftDocumentInputV2,
    CurveRuntimeManifestV1,
    CurveVersionCreateRequest,
)
from app.services.archive_preflight import (
    ArchivePreflightError,
    ArchivePreflightResult,
    preflight_archive,
)
from app.services.canonical_json import canonical_json_hash
from app.services.curve_lab_jobs import (
    CURVE_LAB_JOBS,
    CurveLabQueueFullError,
    deadline_expired,
    new_deadline,
    soft_deadline_error,
)
from app.services.curve_lab_plan import resolved_declaration_order, stage_id
from app.services.quote_canonicalization import (
    QuoteCanonicalizationError,
    canonicalize_quote,
)
from app.services.store import (
    ConflictError,
    CurveLabDependencyConflictError,
    NotFoundError,
)

if TYPE_CHECKING:
    from app.services.dal_gateway import DalGateway
    from app.services.store import StoreProtocol


_BUILD_FUTURES: dict[str, object] = {}
_BUILD_FUTURES_LOCK = Lock()

_REGISTRY = {row.instrument_type: row for row in CURVE_LAB_V1_SUCCESS_REGISTRY}


class CurveLabLifecycleError(ValueError):
    def __init__(
        self,
        status_code: int,
        code: str,
        message: str,
        field: str,
        value: object = None,
        *,
        resource_id: str | None = None,
        constraint: str | None = None,
        headers: dict[str, str] | None = None,
        **details: object,
    ) -> None:
        super().__init__(message)
        self.status_code = status_code
        self.headers = headers
        self.detail = {
            "code": code,
            "message": message,
            "field": field,
            "value": value,
            "resource_id": resource_id,
            "details": {
                **({"constraint": constraint} if constraint else {}),
                **details,
            },
        }


def _now() -> str:
    return datetime.now(UTC).isoformat().replace("+00:00", "Z")


def _reserve_job():
    try:
        return CURVE_LAB_JOBS.reserve()
    except CurveLabQueueFullError as exc:
        raise CurveLabLifecycleError(
            429,
            "CURVE_LAB_QUEUE_FULL",
            "The Curve Lab worker queue is full.",
            "queue",
            None,
            constraint="at most 2 running and 100 queued jobs",
            headers={"Retry-After": "1"},
        ) from exc


def _project_import_preflight_error(
    error: ArchivePreflightError,
    *,
    job_id: str,
    payload_length: int,
) -> tuple[dict[str, object], int, int]:
    return (
        {
            "code": error.code,
            "message": error.message,
            "field": error.field,
            "value": error.value,
            "resource_id": job_id,
            "details": error.details,
        },
        error.wire_length if error.wire_length is not None else payload_length,
        error.expanded_length if error.expanded_length is not None else 0,
    )


def _hash(value: object) -> str:
    return canonical_json_hash(value)


def _audit(
    store: StoreProtocol,
    action: str,
    target_type: str,
    target_id: str,
    input_value: object,
    outcome: str = "SUCCEEDED",
    **details: object,
) -> None:
    store.add_curve_lab_audit_event(
        {
            "id": uuid4().hex,
            "action": action,
            "actor": "dal-web",
            "target_type": target_type,
            "target_id": target_id,
            "input_hash": _hash(input_value),
            "outcome": outcome,
            "details": details,
            "created_at": _now(),
        }
    )


def _canonical_document(
    request: CurveDraftDocumentInputV2,
    *,
    rekey: bool = False,
) -> dict:
    seen: dict[str, int] = {}
    instruments: list[dict] = []
    for index, authored in enumerate(request.instruments):
        source = authored.model_dump(mode="json")
        source["terms"] = authored.terms.model_dump(mode="json", exclude_none=True)
        supplied_id = source.pop("instrument_id")
        source_id = source.pop("source_instrument_id")
        if rekey:
            source_id = supplied_id or source_id
            instrument_id = uuid4().hex
        else:
            instrument_id = supplied_id or uuid4().hex
        if instrument_id in seen:
            raise CurveLabLifecycleError(
                422,
                "INSTRUMENT_ID_DUPLICATE",
                "Instrument ids must be unique within a draft.",
                f"instruments[{index}].instrument_id",
                instrument_id,
                constraint="unique_within_draft",
                conflicts_with=f"instruments[{seen[instrument_id]}].instrument_id",
            )
        seen[instrument_id] = index
        family = source["instrument_type"]
        row = _REGISTRY.get(family)
        if row is None:
            raise CurveLabLifecycleError(
                422,
                "UNSUPPORTED_PRODUCT",
                "Instrument family is outside the Curve Lab V1 success registry.",
                f"instruments[{index}].instrument_type",
                family,
                constraint="CurveLabV1SuccessFamily",
            )
        try:
            quote = canonicalize_quote(
                family,
                source["raw_quote"],
                row.canonical_raw_unit,
            )
        except QuoteCanonicalizationError as exc:
            raise CurveLabLifecycleError(
                422,
                exc.code,
                exc.message,
                f"instruments[{index}].{exc.field}",
                exc.value,
                **exc.details,
            ) from exc
        if quote.raw_quote != source["raw_quote"]:
            raise CurveLabLifecycleError(
                422,
                "QUOTE_PERSISTED_BYTES_NOT_CANONICAL",
                "Durable raw_quote must already use canonical plain-decimal bytes.",
                f"instruments[{index}].raw_quote",
                source["raw_quote"],
                constraint=f"must equal {quote.raw_quote}",
            )
        instruments.append(
            {
                "instrument_id": instrument_id,
                "source_instrument_id": source_id,
                "instrument_type": family,
                "trade_date": source["trade_date"],
                "start_date": source["start_date"],
                "maturity_date": source["maturity_date"],
                "currency_or_pair": source["currency_or_pair"],
                "quote_coordinate_kind": quote.quote_coordinate_kind,
                "canonical_raw_unit": quote.canonical_raw_unit,
                "raw_quote": quote.raw_quote,
                "normalized_quote": quote.normalized_quote,
                "exact_risk_raw_bump": quote.exact_risk_raw_bump,
                "normalized_risk_bump": quote.normalized_risk_bump,
                "source": source["source"],
                "observed_at": source["observed_at"],
                "included": source["included"],
                "terms": source["terms"],
            }
        )
    payload = request.model_dump(mode="json")
    payload["instruments"] = instruments
    return payload


def create_draft(
    store: StoreProtocol,
    request: CurveDraftDocumentInputV2,
) -> dict:
    document = _canonical_document(request)
    now = _now()
    record = {
        "id": uuid4().hex,
        "schema_version": 2,
        "revision": 1,
        "fingerprint": _hash(document),
        "state": "READY_TO_BUILD",
        "document": document,
        "created_at": now,
        "updated_at": now,
    }
    stored = store.add_curve_lab_draft(record)
    _audit(store, "DRAFT_CREATED", "curve_draft", record["id"], document)
    return stored


def get_draft(store: StoreProtocol, draft_id: str) -> dict:
    try:
        return store.get_curve_lab_draft(draft_id)
    except NotFoundError as exc:
        raise CurveLabLifecycleError(
            404,
            "CURVE_DRAFT_NOT_FOUND",
            "Curve draft was not found.",
            "draft_id",
            draft_id,
        ) from exc


def update_draft(
    store: StoreProtocol,
    draft_id: str,
    expected_revision: int,
    request: CurveDraftDocumentInputV2,
) -> dict:
    current = get_draft(store, draft_id)
    if current["revision"] != expected_revision:
        raise _revision_conflict(draft_id, expected_revision, current["revision"])
    document = _canonical_document(request)
    record = {
        **current,
        "revision": current["revision"] + 1,
        "fingerprint": _hash(document),
        "state": "MODIFIED",
        "document": document,
        "updated_at": _now(),
    }
    try:
        stored = store.update_curve_lab_draft(draft_id, expected_revision, record)
    except ConflictError as exc:
        actual = get_draft(store, draft_id)["revision"]
        raise _revision_conflict(draft_id, expected_revision, actual) from exc
    _audit(store, "DRAFT_UPDATED", "curve_draft", draft_id, document)
    return stored


def _revision_conflict(
    draft_id: str, expected_revision: int, actual_revision: int
) -> CurveLabLifecycleError:
    return CurveLabLifecycleError(
        409,
        "DRAFT_REVISION_CONFLICT",
        "Draft revision compare-and-swap failed.",
        "If-Match",
        str(expected_revision),
        resource_id=draft_id,
        constraint=f"must equal current draft revision {actual_revision}",
    )


def create_build_run(store: StoreProtocol, gateway: DalGateway, draft_id: str) -> dict:
    draft = get_draft(store, draft_id)
    document = draft["document"]
    quote_coordinates = quote_axis(document)
    parameter_coordinates: list[dict] = []
    ordered_declarations = resolved_declaration_order(document)
    resolved_plan = {
        "schema_version": 1,
        "mode": document["mode"],
        "component_order": [declaration["component_key"] for declaration in ordered_declarations],
        "stages": [
            {
                "stage_id": stage_id(document, index),
                "component_keys": [declaration["component_key"]],
            }
            for index, declaration in enumerate(ordered_declarations)
        ]
        if document["mode"] == "STAGED_XCCY"
        else [
            {
                "stage_id": "stage-0",
                "component_keys": [
                    declaration["component_key"] for declaration in ordered_declarations
                ],
            }
        ],
        "quote_count": len(quote_coordinates),
        "parameter_count": 0,
        "parameter_axis_source": "PENDING_NATIVE_CALIBRATION",
    }
    now = _now()
    reservation = _reserve_job()
    run_id = uuid4().hex
    queued = {
        "id": run_id,
        "draft_id": draft_id,
        "draft_revision": draft["revision"],
        "draft_fingerprint": draft["fingerprint"],
        "state": "QUEUED",
        "request": document,
        "resolved_plan": resolved_plan,
        "quote_axis": quote_coordinates,
        "parameter_axis": parameter_coordinates,
        "dependency_manifest": [],
        "native_payload": None,
        "native_payload_hash": None,
        "diagnostics": {
            "fit_state": "QUEUED",
            "quote_count": len(quote_coordinates),
            "parameter_count": 0,
        },
        "error": None,
        "created_at": now,
        "deadline_at": new_deadline(datetime.fromisoformat(now.replace("Z", "+00:00"))),
        "finished_at": None,
    }
    try:
        store.add_curve_lab_build_run(queued)
        future = reservation.submit(
            _execute_build_run,
            store,
            gateway,
            run_id,
        )
    except Exception:
        reservation.cancel()
        raise
    with _BUILD_FUTURES_LOCK:
        _BUILD_FUTURES[run_id] = future
    future.add_done_callback(lambda _future: _forget_build_future(run_id))
    return _build_public(store, queued)


def _forget_build_future(run_id: str) -> None:
    with _BUILD_FUTURES_LOCK:
        _BUILD_FUTURES.pop(run_id, None)


def _execute_build_run(
    store: StoreProtocol,
    gateway: DalGateway,
    run_id: str,
) -> None:
    queued = store.get_curve_lab_build_run(run_id)
    draft_id = queued["draft_id"]
    document = queued["request"]
    quote_coordinates = list(queued["quote_axis"])
    parameter_coordinates: list[dict] = []
    resolved_plan = queued["resolved_plan"]
    if deadline_expired(queued["deadline_at"]):
        store.update_curve_lab_build_run(
            run_id,
            _timed_out_build_record(queued),
        )
        return
    store.update_curve_lab_build_run(
        run_id,
        {
            **queued,
            "state": "RESOLVING_DEPENDENCIES",
            "diagnostics": {
                **queued["diagnostics"],
                "fit_state": "RESOLVING_DEPENDENCIES",
            },
        },
    )
    dependency_records, dependency_manifest, dependency_error = _resolve_build_dependencies(
        store,
        list(document["dependency_version_ids"]),
        run_id,
    )
    if deadline_expired(queued["deadline_at"]):
        store.update_curve_lab_build_run(
            run_id,
            _timed_out_build_record(
                {
                    **queued,
                    "dependency_manifest": dependency_manifest,
                }
            ),
        )
        return
    if dependency_error is not None:
        record = _failed_build_record(
            queued,
            resolved_plan,
            quote_coordinates,
            parameter_coordinates,
            dependency_manifest,
            dependency_error,
        )
        store.update_curve_lab_build_run(run_id, record)
        _audit(
            store,
            "BUILD_FAILED",
            "curve_build_run",
            run_id,
            document,
            outcome="FAILED",
            error_code=dependency_error["code"],
        )
        return
    running = {
        **queued,
        "state": "SOLVING",
        "dependency_manifest": dependency_manifest,
        "diagnostics": {
            **queued["diagnostics"],
            "fit_state": "SOLVING",
        },
    }
    store.update_curve_lab_build_run(run_id, running)
    try:
        native_payload = gateway.build_curve_lab_archive(
            document,
            dependency_records,
        )
        if deadline_expired(queued["deadline_at"]):
            store.update_curve_lab_build_run(
                run_id,
                _timed_out_build_record(
                    {
                        **queued,
                        "dependency_manifest": dependency_manifest,
                    }
                ),
            )
            return
        parameter_coordinates = gateway.curve_lab_archive_parameter_axis(
            document,
            native_payload,
        )
        curve_views = gateway.curve_lab_archive_curve_views(
            document,
            native_payload,
            parameter_coordinates,
        )
    except Exception:  # noqa: BLE001 - native failure becomes immutable evidence
        error = {
            "code": "NATIVE_BUILD_FAILED",
            "message": "Native curve construction failed.",
            "field": "draft_id",
            "value": draft_id,
            "resource_id": run_id,
            "details": {},
        }
        record = _failed_build_record(
            queued,
            resolved_plan,
            quote_coordinates,
            parameter_coordinates,
            dependency_manifest,
            error,
        )
        store.update_curve_lab_build_run(run_id, record)
        _audit(
            store,
            "BUILD_FAILED",
            "curve_build_run",
            run_id,
            document,
            outcome="FAILED",
            error_code=error["code"],
        )
        return
    resolved_plan = {
        **resolved_plan,
        "parameter_count": len(parameter_coordinates),
        "parameter_axis_source": "NATIVE_ARCHIVE_LAYOUT",
        "runtime_manifest": {
            "schema_version": 1,
            "mode": document["mode"],
            "as_of_date": document["as_of_date"],
            "market_snapshot_id": document["market_snapshot_id"],
            "components": [
                {
                    "component_key": declaration["component_key"],
                    "role": declaration["role"],
                    "currency": declaration["currency"],
                    "parameterization": declaration["parameterization"],
                    "parameter_ids": [
                        item["parameter_id"]
                        for item in parameter_coordinates
                        if item["component_key"] == declaration["component_key"]
                    ],
                }
                for declaration in resolved_declaration_order(document)
            ],
        },
    }
    record = {
        "id": run_id,
        "draft_id": draft_id,
        "draft_revision": queued["draft_revision"],
        "draft_fingerprint": queued["draft_fingerprint"],
        "state": "SUCCEEDED",
        "request": document,
        "resolved_plan": resolved_plan,
        "quote_axis": quote_coordinates,
        "parameter_axis": parameter_coordinates,
        "dependency_manifest": dependency_manifest,
        "native_payload": native_payload,
        "native_payload_hash": hashlib.sha256(native_payload).hexdigest(),
        "diagnostics": {
            "fit_state": "NATIVE_ARCHIVE_VALIDATED",
            "quote_count": len(quote_coordinates),
            "parameter_count": len(parameter_coordinates),
            "payload_bytes": len(native_payload),
            "curve_views": curve_views,
        },
        "error": None,
        "created_at": queued["created_at"],
        "deadline_at": queued["deadline_at"],
        "finished_at": _now(),
    }
    store.update_curve_lab_build_run(run_id, record)
    _audit(store, "BUILD_SUCCEEDED", "curve_build_run", record["id"], record["request"])


def _resolve_build_dependencies(
    store: StoreProtocol,
    requested_ids: list[str],
    run_id: str,
) -> tuple[list[dict], list[dict], dict | None]:
    resolved = store.resolve_curve_lab_versions(requested_ids)
    by_id = {record["id"]: record for record in resolved}
    accepted: list[dict] = []
    manifest: list[dict] = []
    for version_id in requested_ids:
        version = by_id.get(version_id)
        if version is None:
            return (
                accepted,
                manifest,
                {
                    "code": "DEPENDENCY_VERSION_NOT_FOUND",
                    "message": "Curve dependency version was not found.",
                    "field": "dependency_version_ids",
                    "value": version_id,
                    "resource_id": run_id,
                    "details": {},
                },
            )
        if version["visibility_state"] == "ARCHIVED":
            return (
                accepted,
                manifest,
                {
                    "code": "DEPENDENCY_VERSION_ARCHIVED",
                    "message": "Archived curve dependency versions cannot enter a new build.",
                    "field": "dependency_version_ids",
                    "value": version_id,
                    "resource_id": run_id,
                    "details": {},
                },
            )
        verification = version.get("verification")
        if not isinstance(verification, dict) or not isinstance(
            verification.get("document"),
            dict,
        ):
            return (
                accepted,
                manifest,
                {
                    "code": "DEPENDENCY_CONTEXT_UNAVAILABLE",
                    "message": "Curve dependency lacks reconstructible component context.",
                    "field": "dependency_version_ids",
                    "value": version_id,
                    "resource_id": run_id,
                    "details": {},
                },
            )
        accepted.append(version)
        manifest.append(
            {
                "version_id": version_id,
                "content_hash": version["native_payload_hash"],
                "root_kind": version["root_kind"],
            }
        )
    return accepted, manifest, None


def _failed_build_record(
    queued: dict,
    resolved_plan: dict,
    quote_coordinates: list[dict],
    parameter_coordinates: list[dict],
    dependency_manifest: list[dict],
    error: dict,
) -> dict:
    return {
        "id": queued["id"],
        "draft_id": queued["draft_id"],
        "draft_revision": queued["draft_revision"],
        "draft_fingerprint": queued["draft_fingerprint"],
        "state": "FAILED",
        "request": queued["request"],
        "resolved_plan": resolved_plan,
        "quote_axis": quote_coordinates,
        "parameter_axis": parameter_coordinates,
        "dependency_manifest": dependency_manifest,
        "native_payload": None,
        "native_payload_hash": None,
        "diagnostics": {
            "fit_state": "FAILED",
            "quote_count": len(quote_coordinates),
            "parameter_count": len(parameter_coordinates),
        },
        "error": error,
        "created_at": queued["created_at"],
        "deadline_at": queued["deadline_at"],
        "finished_at": _now(),
    }


def _timed_out_build_record(record: dict) -> dict:
    return {
        **record,
        "state": "TIMED_OUT",
        "native_payload": None,
        "native_payload_hash": None,
        "diagnostics": {
            **(record.get("diagnostics") or {}),
            "fit_state": "TIMED_OUT",
        },
        "error": soft_deadline_error(record),
        "finished_at": _now(),
    }


def quote_axis(document: dict) -> list[dict]:
    declarations = resolved_declaration_order(document)
    default_component = declarations[0]["component_key"]
    declaration_index = {
        declaration["component_key"]: index for index, declaration in enumerate(declarations)
    }
    instruments_by_component: dict[str, list[dict]] = {
        str(declaration["component_key"]): [] for declaration in declarations
    }
    for item in document["instruments"]:
        if not item["included"]:
            continue
        component_key = str(item["terms"].get("component_key", default_component))
        instruments_by_component[component_key].append(item)
    stage_offsets: dict[str, int] = {}
    result: list[dict] = []
    for declaration in declarations:
        component_key = str(declaration["component_key"])
        stage = stage_id(document, declaration_index[component_key])
        for item in instruments_by_component[component_key]:
            local_index = stage_offsets.get(stage, 0)
            stage_offsets[stage] = local_index + 1
            result.append(
                {
                    "global_quote_index": len(result),
                    "quote_id": item["instrument_id"],
                    "instrument_id": item["instrument_id"],
                    "component_key": component_key,
                    "stage_id": stage,
                    "group_id": component_key,
                    "stage_local_quote_index": local_index,
                    "quote_coordinate_kind": item["quote_coordinate_kind"],
                    "canonical_raw_unit": item["canonical_raw_unit"],
                    "raw_quote": item["raw_quote"],
                    "normalized_quote": item["normalized_quote"],
                    "normalized_unit": "DECIMAL_RATE",
                    "exact_risk_raw_bump": item["exact_risk_raw_bump"],
                    "normalized_risk_bump": item["normalized_risk_bump"],
                    "display_label": (f"{item['instrument_type']} {item['maturity_date']}"),
                }
            )
    return result


def _build_public(store: StoreProtocol, record: dict) -> dict:
    current = get_draft(store, record["draft_id"])
    diagnostics = record.get("diagnostics")
    curve_views = diagnostics.get("curve_views", []) if diagnostics else []
    public_diagnostics = (
        {key: value for key, value in diagnostics.items() if key != "curve_views"}
        if diagnostics is not None
        else None
    )
    return {
        key: value
        for key, value in {
            **record,
            "curve_views": curve_views,
            "diagnostics": public_diagnostics,
            "stale": (
                current["revision"] != record["draft_revision"]
                or current["fingerprint"] != record["draft_fingerprint"]
            ),
        }.items()
        if key
        not in {
            "native_payload",
        }
    }


def get_build_run(store: StoreProtocol, run_id: str) -> dict:
    try:
        return _build_public(store, store.get_curve_lab_build_run(run_id))
    except NotFoundError as exc:
        raise CurveLabLifecycleError(
            404,
            "CURVE_BUILD_RUN_NOT_FOUND",
            "Curve build run was not found.",
            "build_run_id",
            run_id,
        ) from exc


def get_import_job(store: StoreProtocol, job_id: str) -> dict:
    try:
        return store.get_curve_lab_import_job(job_id)
    except NotFoundError as exc:
        raise CurveLabLifecycleError(
            404,
            "CURVE_IMPORT_JOB_NOT_FOUND",
            "Curve import job was not found.",
            "import_job_id",
            job_id,
        ) from exc


def create_version(
    store: StoreProtocol,
    request: CurveVersionCreateRequest,
) -> tuple[dict, bool]:
    draft = get_draft(store, request.draft_id)
    if draft["revision"] != request.draft_revision:
        raise _revision_conflict(request.draft_id, request.draft_revision, draft["revision"])
    if draft["fingerprint"] != request.draft_fingerprint:
        raise CurveLabLifecycleError(
            409,
            "DRAFT_FINGERPRINT_CONFLICT",
            "Draft fingerprint compare-and-swap failed.",
            "draft_fingerprint",
            request.draft_fingerprint,
            resource_id=request.draft_id,
            constraint=f"must equal {draft['fingerprint']}",
        )
    try:
        run = store.get_curve_lab_build_run(request.build_run_id)
    except NotFoundError as exc:
        raise CurveLabLifecycleError(
            404,
            "CURVE_BUILD_RUN_NOT_FOUND",
            "Curve build run was not found.",
            "build_run_id",
            request.build_run_id,
        ) from exc
    if (
        run["state"] != "SUCCEEDED"
        or run["draft_id"] != request.draft_id
        or run["draft_revision"] != request.draft_revision
        or run["draft_fingerprint"] != request.draft_fingerprint
    ):
        raise CurveLabLifecycleError(
            409,
            "BUILD_RUN_STALE",
            "Build run does not match the immutable draft snapshot.",
            "build_run_id",
            request.build_run_id,
            resource_id=request.build_run_id,
        )
    payload = run["native_payload"]
    record = {
        "id": uuid4().hex,
        "idempotency_key": request.idempotency_key,
        "source_kind": "BUILD",
        "build_run_id": request.build_run_id,
        "import_job_id": None,
        "native_payload": payload,
        "native_payload_length": len(payload),
        "native_payload_hash": hashlib.sha256(payload).hexdigest(),
        "archive_numeric_format": "JSON_MAX_DIGITS10_V1",
        "root_kind": ("DISCOUNT_CURVE" if draft["document"]["mode"] == "SINGLE" else "CURVE_SET"),
        "build_validation_state": "VERIFIED",
        "visibility_state": "VISIBLE",
        "name": request.name,
        "version_note": request.version_note,
        "tags": list(request.tags),
        "verification": {
            "draft_id": request.draft_id,
            "draft_revision": request.draft_revision,
            "draft_fingerprint": request.draft_fingerprint,
            "document": draft["document"],
            "dependency_manifest": run["dependency_manifest"],
            "resolved_plan": run["resolved_plan"],
            "quote_axis": run["quote_axis"],
            "parameter_axis": run["parameter_axis"],
        },
        "created_at": _now(),
    }
    try:
        stored, created = store.publish_curve_lab_version(
            record,
            request.draft_id,
            request.draft_revision,
            request.draft_fingerprint,
            request.build_run_id,
        )
    except CurveLabDependencyConflictError as exc:
        code, message = {
            "NOT_FOUND": (
                "DEPENDENCY_VERSION_NOT_FOUND",
                "Curve dependency version was not found.",
            ),
            "ARCHIVED": (
                "DEPENDENCY_VERSION_ARCHIVED",
                "Archived curve dependency versions cannot enter publication.",
            ),
            "CONTEXT_CHANGED": (
                "DEPENDENCY_CONTEXT_CHANGED",
                "Curve dependency evidence changed before publication.",
            ),
        }[exc.reason]
        raise CurveLabLifecycleError(
            409,
            code,
            message,
            "dependency_version_ids",
            exc.version_id or None,
            resource_id=exc.version_id or request.build_run_id,
        ) from exc
    except ConflictError as exc:
        current = get_draft(store, request.draft_id)
        if current["revision"] != request.draft_revision:
            raise _revision_conflict(
                request.draft_id,
                request.draft_revision,
                current["revision"],
            ) from exc
        raise CurveLabLifecycleError(
            409,
            "BUILD_RUN_STALE",
            "Build run became stale before immutable publication.",
            "build_run_id",
            request.build_run_id,
            resource_id=request.build_run_id,
        ) from exc
    if created:
        _audit(
            store,
            "VERSION_CREATED",
            "curve_version",
            stored["id"],
            {
                **_version_public(record),
                "native_payload_hash": record["native_payload_hash"],
            },
        )
    return _version_public(stored), created


def _version_public(record: dict) -> dict:
    return {
        key: value
        for key, value in record.items()
        if key not in {"native_payload", "idempotency_key", "verification"}
    }


def get_version(store: StoreProtocol, version_id: str) -> dict:
    try:
        return store.get_curve_lab_version(version_id)
    except NotFoundError as exc:
        raise CurveLabLifecycleError(
            404,
            "CURVE_VERSION_NOT_FOUND",
            "Curve version was not found.",
            "curve_version_id",
            version_id,
        ) from exc


def list_versions(store: StoreProtocol, include_archived: bool) -> list[dict]:
    return [_version_public(item) for item in store.list_curve_lab_versions(include_archived)]


def archive_version(store: StoreProtocol, version_id: str) -> dict:
    get_version(store, version_id)
    stored = store.archive_curve_lab_version(version_id)
    _audit(store, "VERSION_ARCHIVED", "curve_version", version_id, {"id": version_id})
    return _version_public(stored)


def native_payload(store: StoreProtocol, version_id: str) -> bytes:
    return get_version(store, version_id)["native_payload"]


def version_runtime_manifest(store: StoreProtocol, version_id: str) -> dict:
    version = get_version(store, version_id)
    verification = version.get("verification", {})
    manifest = verification.get("runtime_manifest")
    if not isinstance(manifest, dict):
        resolved_plan = verification.get("resolved_plan", {})
        manifest = (
            resolved_plan.get("runtime_manifest") if isinstance(resolved_plan, dict) else None
        )
    if not isinstance(manifest, dict):
        raise CurveLabLifecycleError(
            409,
            "RUNTIME_MANIFEST_UNAVAILABLE",
            "Curve version has no replayable runtime manifest.",
            "curve_version_id",
            version_id,
            resource_id=version_id,
        )
    return manifest


def clone_version(store: StoreProtocol, version_id: str) -> dict:
    version = get_version(store, version_id)
    document = version["verification"].get("document")
    if not isinstance(document, dict):
        raise CurveLabLifecycleError(
            409,
            "VERSION_CLONE_UNAVAILABLE",
            "Version does not carry a canonical source document.",
            "curve_version_id",
            version_id,
        )
    authored = {
        **document,
        "instruments": [
            {key: value for key, value in item.items() if key not in _DERIVED_FOR_CLONE}
            for item in document["instruments"]
        ],
    }
    request = CurveDraftDocumentInputV2.model_validate(authored)
    cloned = _canonical_document(request, rekey=True)
    now = _now()
    record = {
        "id": uuid4().hex,
        "schema_version": 2,
        "revision": 1,
        "fingerprint": _hash(cloned),
        "state": "READY_TO_BUILD",
        "document": cloned,
        "created_at": now,
        "updated_at": now,
    }
    stored = store.add_curve_lab_draft(record)
    _audit(store, "VERSION_CLONED", "curve_draft", record["id"], cloned)
    return stored


_DERIVED_FOR_CLONE = frozenset(
    (
        "quote_coordinate_kind",
        "canonical_raw_unit",
        "normalized_quote",
        "exact_risk_raw_bump",
        "normalized_risk_bump",
    )
)


def import_native_json(
    store: StoreProtocol,
    gateway: DalGateway,
    payload: bytes,
    content_encoding: str | None = None,
    runtime_manifest: CurveRuntimeManifestV1 | None = None,
) -> dict:
    now = _now()
    job_id = uuid4().hex
    request_hash = hashlib.sha256(payload).hexdigest()
    try:
        admitted = preflight_archive(
            payload,
            content_encoding=content_encoding,
        )
    except ArchivePreflightError as exc:
        error, wire_length, expanded_length = _project_import_preflight_error(
            exc,
            job_id=job_id,
            payload_length=len(payload),
        )
        failed = {
            "id": job_id,
            "request_hash": request_hash,
            "compressed_payload_length": wire_length,
            "expanded_payload_length": expanded_length,
            "state": "FAILED",
            "phase": "PREFLIGHT",
            "error": error,
            "resulting_version_id": None,
            "created_at": now,
            "deadline_at": new_deadline(datetime.fromisoformat(now.replace("Z", "+00:00"))),
            "finished_at": now,
        }
        store.add_curve_lab_import_job(failed)
        raise CurveLabLifecycleError(
            422,
            exc.code,
            exc.message,
            exc.field,
            exc.value,
            resource_id=job_id,
            **exc.details,
        ) from exc
    queued = {
        "id": job_id,
        "request_hash": request_hash,
        "compressed_payload_length": len(payload),
        "expanded_payload_length": admitted.expanded_length,
        "state": "QUEUED",
        "phase": "QUEUED",
        "error": None,
        "resulting_version_id": None,
        "created_at": now,
        "deadline_at": new_deadline(datetime.fromisoformat(now.replace("Z", "+00:00"))),
        "finished_at": None,
    }
    reservation = _reserve_job()
    try:
        store.add_curve_lab_import_job(queued)
        reservation.submit(
            _execute_import_job,
            store,
            gateway,
            queued,
            admitted,
            runtime_manifest,
        )
    except Exception:
        reservation.cancel()
        raise
    return queued


def _execute_import_job(
    store: StoreProtocol,
    gateway: DalGateway,
    queued: dict,
    preflight: ArchivePreflightResult,
    runtime_manifest: CurveRuntimeManifestV1 | None,
) -> None:
    job_id = queued["id"]
    request_hash = queued["request_hash"]
    now = queued["created_at"]
    if deadline_expired(queued["deadline_at"]):
        store.update_curve_lab_import_job(
            job_id,
            _timed_out_import_record(queued),
        )
        return
    running = {
        **queued,
        "state": "RUNNING",
        "phase": "PREFLIGHT",
    }
    store.update_curve_lab_import_job(job_id, running)
    native_running = {
        **running,
        "compressed_payload_length": preflight.wire_length,
        "expanded_payload_length": preflight.expanded_length,
        "phase": "NATIVE_RECONSTRUCTION",
    }
    store.update_curve_lab_import_job(job_id, native_running)
    if deadline_expired(queued["deadline_at"]):
        store.update_curve_lab_import_job(
            job_id,
            _timed_out_import_record(native_running),
        )
        return
    try:
        canonical, root_kind = gateway.import_curve_lab_archive(preflight.payload)
    except Exception:
        error = {
            "code": "IMPORT_NATIVE_RECONSTRUCTION_FAILED",
            "message": "Native archive reconstruction failed.",
            "field": "payload",
            "value": None,
            "resource_id": job_id,
            "details": {},
        }
        store.update_curve_lab_import_job(
            job_id,
            {
                **native_running,
                "id": job_id,
                "request_hash": request_hash,
                "compressed_payload_length": preflight.wire_length,
                "expanded_payload_length": preflight.expanded_length,
                "state": "FAILED",
                "phase": "NATIVE_RECONSTRUCTION",
                "error": error,
                "resulting_version_id": None,
                "created_at": now,
                "finished_at": _now(),
            },
        )
        return
    if deadline_expired(queued["deadline_at"]):
        store.update_curve_lab_import_job(
            job_id,
            _timed_out_import_record(native_running),
        )
        return
    verification: dict[str, object] = {
        "request_hash": request_hash,
        "source_payload_hash": request_hash,
    }
    if runtime_manifest is not None:
        manifest = runtime_manifest.model_dump(mode="json")
        expected_root_kind = "DISCOUNT_CURVE" if manifest["mode"] == "SINGLE" else "CURVE_SET"
        if root_kind != expected_root_kind:
            error = {
                "code": "IMPORT_RUNTIME_MANIFEST_MISMATCH",
                "message": "Runtime manifest mode does not match the native archive root.",
                "field": "runtime_manifest.mode",
                "value": manifest["mode"],
                "resource_id": job_id,
                "details": {
                    "expected_root_kind": expected_root_kind,
                    "actual_root_kind": root_kind,
                },
            }
            store.update_curve_lab_import_job(
                job_id,
                {
                    **native_running,
                    "state": "FAILED",
                    "phase": "POST_VALIDATE",
                    "error": error,
                    "finished_at": _now(),
                },
            )
            return
        document = {
            "schema_version": 2,
            "mode": manifest["mode"],
            "as_of_date": manifest["as_of_date"],
            "market_snapshot_id": manifest["market_snapshot_id"],
            "declarations": [
                {
                    "component_key": component["component_key"],
                    "role": component["role"],
                    "currency": component["currency"],
                    "parameterization": component["parameterization"],
                }
                for component in manifest["components"]
            ],
            "instruments": [],
            "dependency_version_ids": [],
            "solver": {
                "solve_mode": "EXACT",
                "parameterization": manifest["components"][0]["parameterization"],
            },
        }
        try:
            parameter_coordinates = gateway.curve_lab_archive_parameter_axis(
                document,
                canonical,
            )
        except Exception:
            error = {
                "code": "IMPORT_RUNTIME_MANIFEST_MISMATCH",
                "message": "Runtime manifest components cannot reconstruct the native archive.",
                "field": "runtime_manifest.components",
                "value": None,
                "resource_id": job_id,
                "details": {},
            }
            store.update_curve_lab_import_job(
                job_id,
                {
                    **native_running,
                    "state": "FAILED",
                    "phase": "POST_VALIDATE",
                    "error": error,
                    "finished_at": _now(),
                },
            )
            return
        if deadline_expired(queued["deadline_at"]):
            store.update_curve_lab_import_job(
                job_id,
                _timed_out_import_record(native_running),
            )
            return
        expected_parameter_ids = [
            parameter_id
            for component in manifest["components"]
            for parameter_id in component["parameter_ids"]
        ]
        actual_parameter_ids = [coordinate["parameter_id"] for coordinate in parameter_coordinates]
        if expected_parameter_ids != actual_parameter_ids:
            error = {
                "code": "IMPORT_RUNTIME_MANIFEST_MISMATCH",
                "message": "Runtime manifest parameter layout does not match the native archive.",
                "field": "runtime_manifest.components",
                "value": None,
                "resource_id": job_id,
                "details": {
                    "expected_parameter_ids": expected_parameter_ids,
                    "actual_parameter_ids": actual_parameter_ids,
                },
            }
            store.update_curve_lab_import_job(
                job_id,
                {
                    **native_running,
                    "state": "FAILED",
                    "phase": "POST_VALIDATE",
                    "error": error,
                    "finished_at": _now(),
                },
            )
            return
        verification.update(
            {
                "document": document,
                "runtime_manifest": manifest,
                "dependency_manifest": [],
                "quote_axis": [],
                "parameter_axis": parameter_coordinates,
            }
        )
    version_record = {
        "id": uuid4().hex,
        "idempotency_key": f"import:{request_hash}",
        "source_kind": "IMPORT",
        "build_run_id": None,
        "import_job_id": job_id,
        "native_payload": canonical,
        "native_payload_length": len(canonical),
        "native_payload_hash": hashlib.sha256(canonical).hexdigest(),
        "archive_numeric_format": "JSON_MAX_DIGITS10_V1",
        "root_kind": root_kind,
        "build_validation_state": "IMPORT_RECONSTRUCTED",
        "visibility_state": "VISIBLE",
        "name": f"Imported {preflight.root_type}",
        "version_note": None,
        "tags": [],
        "verification": verification,
        "created_at": now,
    }
    job = {
        "id": job_id,
        "request_hash": request_hash,
        "compressed_payload_length": preflight.wire_length,
        "expanded_payload_length": preflight.expanded_length,
        "state": "SUCCEEDED",
        "phase": "PUBLISHED",
        "error": None,
        "resulting_version_id": version_record["id"],
        "created_at": now,
        "deadline_at": queued["deadline_at"],
        "finished_at": _now(),
    }
    try:
        _, stored_job = store.publish_curve_lab_import(version_record, job)
    except Exception:
        error = {
            "code": "IMPORT_PUBLICATION_FAILED",
            "message": "Imported curve version could not be published atomically.",
            "field": "state",
            "value": "SUCCEEDED",
            "resource_id": job_id,
            "details": {},
        }
        store.update_curve_lab_import_job(
            job_id,
            {
                **native_running,
                "state": "FAILED",
                "phase": "PUBLISH",
                "error": error,
                "resulting_version_id": None,
                "finished_at": _now(),
            },
        )
        return
    _audit(store, "IMPORT_SUCCEEDED", "curve_import_job", job_id, stored_job)


def _timed_out_import_record(record: dict) -> dict:
    return {
        **record,
        "state": "TIMED_OUT",
        "phase": "TIMED_OUT",
        "error": soft_deadline_error(record),
        "resulting_version_id": None,
        "finished_at": _now(),
    }
