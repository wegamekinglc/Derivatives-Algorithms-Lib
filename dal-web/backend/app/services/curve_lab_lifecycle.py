"""Transactional Curve Lab V2 draft, build, version, and import lifecycle."""

from __future__ import annotations

import hashlib
import json
from datetime import UTC, datetime
from typing import TYPE_CHECKING
from uuid import uuid4

from app.schemas.curve_lab import (
    CURVE_LAB_V1_SUCCESS_REGISTRY,
    CurveDraftDocumentInputV2,
    CurveVersionCreateRequest,
)
from app.services.archive_preflight import ArchivePreflightError, preflight_archive
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
        **details: object,
    ) -> None:
        super().__init__(message)
        self.status_code = status_code
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


def _canonical_bytes(value: object) -> bytes:
    return json.dumps(
        value,
        ensure_ascii=True,
        allow_nan=False,
        separators=(",", ":"),
        sort_keys=True,
    ).encode("ascii")


def _hash(value: object) -> str:
    return hashlib.sha256(_canonical_bytes(value)).hexdigest()


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
    parameter_coordinates = parameter_axis(document)
    resolved_plan = {
        "schema_version": 1,
        "mode": document["mode"],
        "component_order": [
            declaration["component_key"] for declaration in document["declarations"]
        ],
        "quote_count": len(quote_coordinates),
        "parameter_count": len(parameter_coordinates),
    }
    now = _now()
    run_id = uuid4().hex
    dependency_records, dependency_manifest, dependency_error = _resolve_build_dependencies(
        store,
        list(document["dependency_version_ids"]),
        run_id,
    )
    if dependency_error is not None:
        record = _failed_build_record(
            run_id,
            draft,
            resolved_plan,
            quote_coordinates,
            parameter_coordinates,
            dependency_manifest,
            dependency_error,
            now,
        )
        stored = store.add_curve_lab_build_run(record)
        _audit(
            store,
            "BUILD_FAILED",
            "curve_build_run",
            run_id,
            document,
            outcome="FAILED",
            error_code=dependency_error["code"],
        )
        return _build_public(store, stored)
    try:
        native_payload = gateway.build_curve_lab_archive(
            document,
            dependency_records,
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
            run_id,
            draft,
            resolved_plan,
            quote_coordinates,
            parameter_coordinates,
            dependency_manifest,
            error,
            now,
        )
        stored = store.add_curve_lab_build_run(record)
        _audit(
            store,
            "BUILD_FAILED",
            "curve_build_run",
            run_id,
            document,
            outcome="FAILED",
            error_code=error["code"],
        )
        return _build_public(store, stored)
    record = {
        "id": run_id,
        "draft_id": draft_id,
        "draft_revision": draft["revision"],
        "draft_fingerprint": draft["fingerprint"],
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
        },
        "error": None,
        "created_at": now,
        "finished_at": now,
    }
    stored = store.add_curve_lab_build_run(record)
    _audit(store, "BUILD_SUCCEEDED", "curve_build_run", record["id"], record["request"])
    return _build_public(store, stored)


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
    run_id: str,
    draft: dict,
    resolved_plan: dict,
    quote_coordinates: list[dict],
    parameter_coordinates: list[dict],
    dependency_manifest: list[dict],
    error: dict,
    created_at: str,
) -> dict:
    return {
        "id": run_id,
        "draft_id": draft["id"],
        "draft_revision": draft["revision"],
        "draft_fingerprint": draft["fingerprint"],
        "state": "FAILED",
        "request": draft["document"],
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
        "created_at": created_at,
        "finished_at": _now(),
    }


def quote_axis(document: dict) -> list[dict]:
    declarations = document["declarations"]
    default_component = declarations[0]["component_key"]
    local_indices: dict[str, int] = {}
    result: list[dict] = []
    for item in document["instruments"]:
        if not item["included"]:
            continue
        component_key = str(item["terms"].get("component_key", default_component))
        local_index = local_indices.get(component_key, 0)
        local_indices[component_key] = local_index + 1
        result.append(
            {
                "global_quote_index": len(result),
                "quote_id": item["instrument_id"],
                "instrument_id": item["instrument_id"],
                "component_key": component_key,
                "stage_id": "stage-0",
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


def parameter_axis(document: dict) -> list[dict]:
    included = [item for item in document["instruments"] if item["included"]]
    default_component = document["declarations"][0]["component_key"]
    result: list[dict] = []
    stage_local_index = 0
    for declaration in document["declarations"]:
        component_key = declaration["component_key"]
        dates = sorted(
            {
                item["maturity_date"]
                for item in included
                if item["terms"].get("component_key", default_component) == component_key
            }
        )
        representation = declaration["parameterization"]
        coordinates: list[tuple[str, str | None]] = []
        if representation == "PIECEWISE_LINEAR_FWD":
            coordinates = [(date_value, side) for date_value in dates for side in ("LEFT", "RIGHT")]
        elif representation == "PIECEWISE_CONSTANT_FWD":
            coordinates = [(date_value, "RIGHT") for date_value in dates]
        elif representation == "LOG_DISCOUNT":
            coordinates = [(date_value, None) for date_value in dates[1:]]
        else:
            coordinates = [(date_value, None) for date_value in dates]
        for component_local_index, (node_date, side) in enumerate(coordinates):
            side_token = side or "SINGLE"
            display_suffix = component_key.rsplit("/", 1)[-1]
            result.append(
                {
                    "global_parameter_index": len(result),
                    "parameter_id": (f"{component_key}:{representation}:{node_date}:{side_token}"),
                    "component_key": component_key,
                    "stage_id": "stage-0",
                    "stage_local_parameter_index": stage_local_index,
                    "component_local_parameter_index": component_local_index,
                    "coordinate_kind": representation,
                    "node_date": node_date,
                    "side": side,
                    "native_parameter_unit": (
                        "LOG_DISCOUNT_FACTOR"
                        if representation == "LOG_DISCOUNT"
                        else "DECIMAL_RATE"
                    ),
                    "display_label": (
                        f"{declaration['currency']} {display_suffix} {node_date} {side_token}"
                    ),
                }
            )
            stage_local_index += 1
    return result


def _build_public(store: StoreProtocol, record: dict) -> dict:
    current = get_draft(store, record["draft_id"])
    return {
        key: value
        for key, value in {
            **record,
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
) -> dict:
    now = _now()
    job_id = uuid4().hex
    request_hash = hashlib.sha256(payload).hexdigest()
    try:
        preflight = preflight_archive(
            payload,
            content_encoding=content_encoding,
        )
    except ArchivePreflightError as exc:
        error = {
            "code": exc.code,
            "message": exc.message,
            "field": exc.field,
            "value": exc.value,
            "resource_id": job_id,
            "details": exc.details,
        }
        store.add_curve_lab_import_job(
            {
                "id": job_id,
                "request_hash": request_hash,
                "compressed_payload_length": (
                    exc.wire_length
                    if exc.wire_length is not None
                    else len(payload)
                ),
                "expanded_payload_length": (
                    exc.expanded_length
                    if exc.expanded_length is not None
                    else 0
                ),
                "state": "FAILED",
                "phase": "PREFLIGHT",
                "error": error,
                "resulting_version_id": None,
                "created_at": now,
                "finished_at": now,
            }
        )
        raise CurveLabLifecycleError(
            422,
            error["code"],
            error["message"],
            error["field"],
            error["value"],
            resource_id=job_id,
            **error["details"],
        ) from exc
    try:
        canonical, root_kind = gateway.import_curve_lab_archive(preflight.payload)
    except Exception as exc:
        error = {
            "code": "IMPORT_NATIVE_RECONSTRUCTION_FAILED",
            "message": "Native archive reconstruction failed.",
            "field": "payload",
            "value": None,
            "resource_id": job_id,
            "details": {},
        }
        store.add_curve_lab_import_job(
            {
                "id": job_id,
                "request_hash": request_hash,
                "compressed_payload_length": len(payload),
                "expanded_payload_length": preflight.expanded_length,
                "state": "FAILED",
                "phase": "NATIVE_RECONSTRUCTION",
                "error": error,
                "resulting_version_id": None,
                "created_at": now,
                "finished_at": _now(),
            }
        )
        raise CurveLabLifecycleError(
            422,
            error["code"],
            error["message"],
            error["field"],
            resource_id=job_id,
        ) from exc
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
        "verification": {
            "request_hash": request_hash,
            "source_payload_hash": request_hash,
        },
        "created_at": now,
    }
    job = {
        "id": job_id,
        "request_hash": request_hash,
        "compressed_payload_length": len(payload),
        "expanded_payload_length": preflight.expanded_length,
        "state": "SUCCEEDED",
        "phase": "PUBLISHED",
        "error": None,
        "resulting_version_id": version_record["id"],
        "created_at": now,
        "finished_at": _now(),
    }
    _, stored_job = store.publish_curve_lab_import(version_record, job)
    _audit(store, "IMPORT_SUCCEEDED", "curve_import_job", job_id, stored_job)
    return stored_job
