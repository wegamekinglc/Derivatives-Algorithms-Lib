"""Stateless Curve Lab authoring and capability resources."""

from __future__ import annotations

from typing import Annotated

from fastapi import APIRouter, Depends, Header, HTTPException, Query, Request, Response
from fastapi.exception_handlers import request_validation_exception_handler
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse
from pydantic import ValidationError

from app.dependencies import gateway_dependency, store_dependency
from app.schemas.curve_lab import (
    CURVE_LAB_V1_SUCCESS_FAMILIES,
    CURVE_LAB_V1_SUCCESS_REGISTRY,
    CurveBuildRunResponse,
    CurveDraftDocumentInputV2,
    CurveDraftResponse,
    CurveImportJobResponse,
    CurveLabCapabilitiesResponse,
    CurveLabErrorResponse,
    CurveLabQuoteCanonicalizationRequest,
    CurveLabQuoteCanonicalizationResponse,
    CurveLabRegistryEntryDTO,
    CurveRuntimeManifestV1,
    CurveVersionCreateRequest,
    CurveVersionResponse,
    FixingSnapshotCreateV1,
    FixingSnapshotResponseV1,
    MatrixResultV2,
    RiskRunRequestV2,
    RiskRunResponseV2,
)
from app.services.archive_preflight import ArchiveLimits
from app.services.curve_lab_lifecycle import (
    CurveLabLifecycleError,
    archive_version,
    clone_version,
    create_build_run,
    create_draft,
    create_version,
    get_build_run,
    get_draft,
    get_import_job,
    import_native_json,
    list_versions,
    native_payload,
    update_draft,
    version_runtime_manifest,
)
from app.services.curve_risk import (
    create_fixing_snapshot,
    create_risk_run,
    get_fixing_snapshot,
    get_matrix,
    get_risk_run,
)
from app.services.quote_canonicalization import (
    QuoteCanonicalizationError,
    canonicalize_quote,
)

router = APIRouter(prefix="/api/curve-lab", tags=["curve-lab"])


async def _read_bounded_request_body(
    request: Request,
    *,
    wire_bytes: int = ArchiveLimits().wire_bytes,
) -> bytes:
    """Buffer no more than the preflight cap plus one sentinel byte."""

    chunks: list[bytes] = []
    length = 0
    async for chunk in request.stream():
        remaining = wire_bytes + 1 - length
        if remaining <= 0:
            break
        bounded = chunk[:remaining]
        chunks.append(bounded)
        length += len(bounded)
        if length > wire_bytes:
            break
    return b"".join(chunks)


def _raise_lifecycle(exc: CurveLabLifecycleError) -> None:
    raise HTTPException(
        status_code=exc.status_code,
        detail=exc.detail,
        headers=exc.headers,
    ) from exc


@router.get("/capabilities", response_model=CurveLabCapabilitiesResponse)
async def get_curve_lab_capabilities() -> CurveLabCapabilitiesResponse:
    return CurveLabCapabilitiesResponse(
        success_families=CURVE_LAB_V1_SUCCESS_FAMILIES,
        registry=tuple(
            CurveLabRegistryEntryDTO(
                instrument_type=row.instrument_type,
                quote_coordinate_kind=row.quote_coordinate_kind,
                canonical_raw_unit=row.canonical_raw_unit,
                exact_risk_raw_bump=row.exact_risk_raw_bump,
                normalized_risk_bump=row.normalized_risk_bump,
            )
            for row in CURVE_LAB_V1_SUCCESS_REGISTRY
        ),
    )


@router.post(
    "/quote-canonicalizations",
    response_model=CurveLabQuoteCanonicalizationResponse,
    responses={422: {"model": CurveLabErrorResponse}},
)
async def canonicalize_authoring_quote(
    request: CurveLabQuoteCanonicalizationRequest,
) -> CurveLabQuoteCanonicalizationResponse:
    try:
        return canonicalize_quote(
            request.instrument_type,
            request.input_lexeme,
            request.input_convention,
        )
    except QuoteCanonicalizationError as exc:
        raise HTTPException(status_code=422, detail=exc.as_detail()) from exc


@router.post("/drafts", response_model=CurveDraftResponse, status_code=201)
async def create_curve_draft(
    request: CurveDraftDocumentInputV2,
    store=Depends(store_dependency),
) -> dict:
    try:
        return create_draft(store, request)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


@router.get("/drafts/{draft_id}", response_model=CurveDraftResponse)
async def get_curve_draft(draft_id: str, store=Depends(store_dependency)) -> dict:
    try:
        return get_draft(store, draft_id)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


@router.put("/drafts/{draft_id}", response_model=CurveDraftResponse)
async def update_curve_draft(
    draft_id: str,
    request: CurveDraftDocumentInputV2,
    if_match: Annotated[str, Header(alias="If-Match")],
    store=Depends(store_dependency),
) -> dict:
    try:
        revision = int(if_match.strip().strip('"'))
    except ValueError as exc:
        raise HTTPException(
            status_code=422,
            detail={
                "code": "DRAFT_REVISION_INVALID",
                "message": "If-Match must contain one quoted integer revision.",
                "field": "If-Match",
                "value": if_match,
                "resource_id": draft_id,
                "details": {},
            },
        ) from exc
    try:
        return update_draft(store, draft_id, revision, request)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


@router.post(
    "/drafts/{draft_id}/build-runs",
    response_model=CurveBuildRunResponse,
    status_code=202,
)
async def create_curve_build_run(
    draft_id: str,
    store=Depends(store_dependency),
    gateway=Depends(gateway_dependency),
) -> dict:
    try:
        return create_build_run(store, gateway, draft_id)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


@router.get("/build-runs/{run_id}", response_model=CurveBuildRunResponse)
async def get_curve_build_run(run_id: str, store=Depends(store_dependency)) -> dict:
    try:
        return get_build_run(store, run_id)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


@router.post(
    "/versions",
    response_model=CurveVersionResponse,
    responses={201: {"model": CurveVersionResponse}},
)
async def create_curve_version(
    request: CurveVersionCreateRequest,
    response: Response,
    store=Depends(store_dependency),
) -> dict:
    try:
        result, created = create_version(store, request)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)
    response.status_code = 201 if created else 200
    return result


@router.get("/versions", response_model=list[CurveVersionResponse])
async def list_curve_versions(
    include_archived: Annotated[bool, Query()] = False,
    store=Depends(store_dependency),
) -> list[dict]:
    return list_versions(store, include_archived)


@router.get("/versions/{version_id}", response_model=CurveVersionResponse)
async def get_curve_version(version_id: str, store=Depends(store_dependency)) -> dict:
    from app.services.curve_lab_lifecycle import get_version

    try:
        record = get_version(store, version_id)
        return {
            key: value
            for key, value in record.items()
            if key not in {"native_payload", "idempotency_key", "verification"}
        }
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


@router.post("/versions/{version_id}/archive", response_model=CurveVersionResponse)
async def archive_curve_version(version_id: str, store=Depends(store_dependency)) -> dict:
    try:
        return archive_version(store, version_id)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


@router.post(
    "/versions/{version_id}/clone",
    response_model=CurveDraftResponse,
    status_code=201,
)
async def clone_curve_version(version_id: str, store=Depends(store_dependency)) -> dict:
    try:
        return clone_version(store, version_id)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


@router.get("/versions/{version_id}/native-json")
async def get_curve_version_native_json(
    version_id: str, store=Depends(store_dependency)
) -> Response:
    try:
        payload = native_payload(store, version_id)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)
    return Response(content=payload, media_type="application/json")


@router.get(
    "/versions/{version_id}/runtime-manifest",
    response_model=CurveRuntimeManifestV1,
)
async def get_curve_version_runtime_manifest(
    version_id: str,
    store=Depends(store_dependency),
) -> dict:
    try:
        return version_runtime_manifest(store, version_id)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


@router.post(
    "/import-jobs",
    response_model=CurveImportJobResponse,
    status_code=202,
)
async def create_curve_import_job(
    request: Request,
    content_encoding: Annotated[str | None, Header(alias="Content-Encoding")] = None,
    runtime_manifest_json: Annotated[
        str | None,
        Header(alias="X-Curve-Lab-Runtime-Manifest"),
    ] = None,
    store=Depends(store_dependency),
    gateway=Depends(gateway_dependency),
) -> dict:
    payload = await _read_bounded_request_body(request)
    try:
        runtime_manifest = (
            CurveRuntimeManifestV1.model_validate_json(runtime_manifest_json)
            if runtime_manifest_json is not None
            else None
        )
    except ValidationError as exc:
        raise HTTPException(
            status_code=422,
            detail={
                "code": "IMPORT_RUNTIME_MANIFEST_INVALID",
                "message": "Runtime manifest is not a closed CurveRuntimeManifestV1.",
                "field": "X-Curve-Lab-Runtime-Manifest",
                "value": None,
                "resource_id": None,
                "details": {"errors": exc.error_count()},
            },
        ) from exc
    try:
        return import_native_json(
            store,
            gateway,
            payload,
            content_encoding,
            runtime_manifest,
        )
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


@router.get(
    "/import-jobs/{job_id}",
    response_model=CurveImportJobResponse,
)
async def get_curve_import_job(job_id: str, store=Depends(store_dependency)) -> dict:
    try:
        return get_import_job(store, job_id)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


@router.post(
    "/fixing-snapshots",
    response_model=FixingSnapshotResponseV1,
    status_code=201,
)
async def post_fixing_snapshot(
    request: FixingSnapshotCreateV1,
    store=Depends(store_dependency),
) -> dict:
    try:
        return create_fixing_snapshot(store, request)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


@router.get(
    "/fixing-snapshots/{snapshot_id}",
    response_model=FixingSnapshotResponseV1,
)
async def read_fixing_snapshot(
    snapshot_id: str,
    store=Depends(store_dependency),
) -> dict:
    try:
        return get_fixing_snapshot(store, snapshot_id)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


@router.post(
    "/risk-runs",
    response_model=RiskRunResponseV2,
    response_model_exclude_unset=True,
    status_code=202,
)
async def create_curve_risk_run(
    request: RiskRunRequestV2,
    store=Depends(store_dependency),
    gateway=Depends(gateway_dependency),
) -> dict:
    try:
        return create_risk_run(store, gateway, request)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


@router.get(
    "/risk-runs/{run_id}",
    response_model=RiskRunResponseV2,
    response_model_exclude_unset=True,
)
async def get_curve_risk_run(run_id: str, store=Depends(store_dependency)) -> dict:
    try:
        return get_risk_run(store, run_id)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


@router.get(
    "/risk-runs/{run_id}/matrices/{matrix_id}",
    response_model=MatrixResultV2,
)
async def get_curve_risk_matrix(
    run_id: str,
    matrix_id: str,
    store=Depends(store_dependency),
) -> dict:
    try:
        return get_matrix(store, run_id, matrix_id)
    except CurveLabLifecycleError as exc:
        _raise_lifecycle(exc)


async def curve_lab_validation_exception_handler(
    request: Request,
    exc: RequestValidationError,
) -> JSONResponse:
    """Give pre-handler axis overrides the same stable Curve Lab envelope."""

    if request.url.path.startswith("/api/curve-lab/"):
        for error in exc.errors():
            if error["type"] == "quote_axis_override_forbidden":
                field = str(error.get("ctx", {}).get("field", "request"))
                return JSONResponse(
                    status_code=422,
                    content={
                        "detail": {
                            "code": "QUOTE_AXIS_OVERRIDE_FORBIDDEN",
                            "message": "Quote-axis values are derived from the family registry.",
                            "field": field,
                            "value": None,
                            "resource_id": None,
                            "details": {"constraint": "server_derived"},
                        }
                    },
                )
            if error["type"] == "draft_topology_invalid":
                field = str(error.get("ctx", {}).get("field", "document"))
                return JSONResponse(
                    status_code=422,
                    content={
                        "detail": {
                            "code": "DRAFT_TOPOLOGY_INVALID",
                            "message": error["msg"],
                            "field": field,
                            "value": None,
                            "resource_id": None,
                            "details": {"constraint": "closed_curve_topology"},
                        }
                    },
                )
        first = exc.errors()[0]
        return JSONResponse(
            status_code=422,
            content={
                "detail": {
                    "code": "REQUEST_VALIDATION_FAILED",
                    "message": first["msg"],
                    "field": ".".join(str(item) for item in first["loc"][1:]),
                    "value": None,
                    "resource_id": None,
                    "details": {"type": first["type"]},
                }
            },
        )
    return await request_validation_exception_handler(request, exc)
