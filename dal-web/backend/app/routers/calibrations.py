"""Curve-calibration and persisted-curve REST endpoints."""

from __future__ import annotations

import logging
from typing import Annotated
from uuid import uuid4

from fastapi import APIRouter, Depends, Path, Query, Request, Response
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse
from fastapi.routing import APIRoute
from pydantic import ValidationError

from app.dependencies import gateway_dependency, store_dependency
from app.schemas.calibrations import (
    MAX_RESPONSE_BYTES,
    ApiErrorDTO,
    ApiErrorResponse,
    CalibrationRunResponse,
    CurveReconstructionDTO,
    JointXccyCalibrationRequest,
    QuoteBumpQueryDTO,
    RunningCalibrationRunResponse,
    SingleCalibrationRequest,
    StagedXccyCalibrationRequest,
)
from app.services.calibrations import (
    CURVE_RESPONSE_ADAPTER,
    RUN_RESPONSE_ADAPTER,
    CalibrationHttpError,
    get_calibration_response,
    get_curve_response,
    submit_joint_xccy_calibration,
    submit_single_calibration,
    submit_staged_xccy_calibration,
)
from app.services.dal_gateway import DalGateway
from app.services.store import StoreProtocol

logger = logging.getLogger(__name__)

_ERROR_RESPONSES = {
    422: {"model": ApiErrorResponse},
    500: {"model": ApiErrorResponse},
}
_ENTITY_ID = r"^[0-9a-f]{32}$"
_JOINT_CAPACITY_EXAMPLE = {
    "error": {
        "code": "JOINT_FREE_PARAMETER_LIMIT_EXCEEDED",
        "message": "joint calibration has 201 free parameters; maximum is 200",
        "location": ["body", "basis", "parameterization"],
        "context": {
            "total_free_parameters": 201,
            "max_total_free_parameters": 200,
            "cumulative_before": 199,
            "cumulative_after": 201,
            "offending_group": "basis",
            "offending_declaration_index": None,
            "offending_parameterization": "PIECEWISE_CONSTANT_FWD",
            "offending_storage_nodes": 2,
        },
    }
}
_FAILED_EXPECTED_IDENTITY_EXAMPLE = {
    "id": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "kind": "single",
    "name": "usd_ois_2026_01_02",
    "schema_version": 1,
    "status": "failed",
    "phase": "finished",
    "created_at": "2026-01-02T02:00:00Z",
    "started_at": "2026-01-02T02:00:01Z",
    "finished_at": "2026-01-02T02:00:01Z",
    "backend": "dal",
    "is_native": True,
    "solver": {
        "solve_mode": "EXACT",
        "smoothing_weight": 1.0,
        "tolerance": 1e-8,
        "fit_tolerance": 1e-6,
        "initial_guess": 0.05,
        "max_evaluations": 200,
        "max_restarts": 20,
    },
    "options": {
        "jacobian_mode": "ANALYTIC",
        "include_jacobian": True,
        "include_effective_inverse": True,
    },
    "requested_jacobian_mode": "ANALYTIC",
    "actual_jacobian_mode": None,
    "solver_diagnostics": None,
    "curves": [],
    "instrument_diagnostics": [],
    "fx_forwards": None,
    "named_ranges": None,
    "jacobian": None,
    "effective_inverse": None,
    "quote_bump_preview": None,
    "resolved_knot_plan": {
        "planner_version": 1,
        "requested_policy": "INPUT",
        "execution_policy": "INPUT",
        "submitted_knot_dates": ["2027-01-02"],
        "candidate_trace": [
            {
                "ordinal": 0,
                "date": "2027-01-02",
                "origin": {"kind": "INPUT", "input_knot_index": 0},
                "disposition": "ADDED",
                "resolved_index": 0,
            }
        ],
        "resolved_declared_nodes": [
            {
                "date": "2027-01-02",
                "origins": [{"kind": "INPUT", "input_knot_index": 0}],
            }
        ],
        "storage_nodes": [
            {
                "date": "2027-01-02",
                "origins": [{"kind": "INPUT", "input_knot_index": 0}],
            }
        ],
        "free_parameters": [{"date": "2027-01-02", "component": "right_forward"}],
        "anchor_added": False,
        "counts": {
            "submitted_knots": 1,
            "instrument_candidates": 0,
            "resolved_declared_nodes": 1,
            "storage_nodes": 1,
            "free_parameters": 1,
        },
    },
    "resolved_knot_plan_hash": ("e79378f7c5d80b13e2087bb5db77418334b35fe7b8f9de409dab8305d74455a8"),
    "expected_execution_identity": {
        "identity_version": 1,
        "execution_policy": "INPUT",
        "today": "2026-01-03",
        "parameterization": "PIECEWISE_CONSTANT_FWD",
        "log_df_scheme": None,
        "resolved_declared_dates": ["2027-01-02"],
        "storage_dates": ["2027-01-02"],
        "free_parameters": [{"date": "2027-01-02", "component": "right_forward"}],
        "counts": {
            "resolved_declared_nodes": 1,
            "storage_nodes": 1,
            "free_parameters": 1,
        },
    },
    "expected_execution_identity_hash": (
        "c0fe9beaba4f64bd527df72c2ca00612a607b15d602d85513157b6e4590fb2ab"
    ),
    "actual_execution_identity": None,
    "actual_execution_identity_hash": None,
    "timings": {"native_solve_ms": None, "serialization_ms": None},
    "error": {
        "code": "PERSISTED_EXPECTED_EXECUTION_IDENTITY_HASH_MISMATCH",
        "message": (
            "persisted expected single-knot execution identity failed canonical hash verification"
        ),
        "location": None,
        "context": {
            "integrity_domain": "expected_execution_identity",
            "stored_expected_execution_identity_hash": (
                "c0fe9beaba4f64bd527df72c2ca00612a607b15d602d85513157b6e4590fb2ab"
            ),
            "actual_expected_execution_identity_hash": (
                "365aacee6fef1ebb789657ac953d39c1fdf3efbd243b0551ac8f6b98fcf45422"
            ),
            "first_difference": None,
        },
    },
}


def patch_openapi_examples(document: dict[str, object]) -> None:
    """Restore contract-significant nulls stripped by OpenAPI encoding."""
    paths = document["paths"]
    joint_examples = paths["/api/calibrations/xccy/joint"]["post"]["responses"]["422"]["content"][
        "application/json"
    ]["examples"]
    joint_examples["joint_free_parameter_limit_exceeded"]["value"] = _JOINT_CAPACITY_EXAMPLE
    run_examples = paths["/api/calibrations/{calibration_id}"]["get"]["responses"]["200"][
        "content"
    ]["application/json"]["examples"]
    run_examples["persisted_expected_execution_identity_hash_mismatch"]["value"] = (
        _FAILED_EXPECTED_IDENTITY_EXAMPLE
    )


def _sanitize(value: object, *, depth: int = 0) -> object:
    if depth > 4:
        return "validation context omitted"
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, (list, tuple)):
        return [_sanitize(item, depth=depth + 1) for item in value[:20]]
    if isinstance(value, dict):
        return {
            str(key)[:128]: _sanitize(item, depth=depth + 1)
            for key, item in list(value.items())[:20]
        }
    return str(value)[:256]


def _error_response(status_code: int, error: ApiErrorDTO) -> JSONResponse:
    return JSONResponse(
        status_code=status_code,
        content=ApiErrorResponse(error=error).model_dump(mode="json"),
    )


class CalibrationAPIRoute(APIRoute):
    """Router-local stable errors without affecting the legacy endpoints."""

    def get_route_handler(self):
        original = super().get_route_handler()

        async def handler(request: Request) -> Response:
            try:
                return await original(request)
            except (RequestValidationError, ValidationError) as exc:
                first = exc.errors()[0]
                context = {
                    "type": first["type"],
                    **_sanitize(first.get("ctx", {})),
                }
                return _error_response(
                    422,
                    ApiErrorDTO(
                        code="VALIDATION_ERROR",
                        message=first["msg"],
                        location=list(first["loc"]),
                        context=context,
                    ),
                )
            except CalibrationHttpError as exc:
                return _error_response(exc.status_code, exc.error)
            except Exception:  # noqa: BLE001 - deliberately sanitized boundary
                incident_id = uuid4().hex
                logger.exception("Unhandled calibration API error; incident_id=%s", incident_id)
                return _error_response(
                    500,
                    ApiErrorDTO(
                        code="INTERNAL_SERVER_ERROR",
                        message="Internal server error",
                        location=None,
                        context={"incident_id": incident_id},
                    ),
                )

        return handler


router = APIRouter(
    prefix="/api/calibrations",
    tags=["calibrations"],
    route_class=CalibrationAPIRoute,
)
curve_router = APIRouter(
    prefix="/api/curves",
    tags=["curves"],
    route_class=CalibrationAPIRoute,
)


@router.post(
    "/single",
    status_code=202,
    response_model=RunningCalibrationRunResponse,
    responses=_ERROR_RESPONSES,
)
async def submit_single(
    request: SingleCalibrationRequest,
    response: Response,
    store: StoreProtocol = Depends(store_dependency),
    gateway: DalGateway = Depends(gateway_dependency),
) -> RunningCalibrationRunResponse:
    result = await submit_single_calibration(store, gateway, request)
    response.headers["Location"] = f"/api/calibrations/{result.id}"
    return result


@router.post(
    "/xccy/staged",
    status_code=202,
    response_model=RunningCalibrationRunResponse,
    responses=_ERROR_RESPONSES,
)
async def submit_staged(
    request: StagedXccyCalibrationRequest,
    response: Response,
    store: StoreProtocol = Depends(store_dependency),
    gateway: DalGateway = Depends(gateway_dependency),
) -> RunningCalibrationRunResponse:
    result = await submit_staged_xccy_calibration(store, gateway, request)
    response.headers["Location"] = f"/api/calibrations/{result.id}"
    return result


@router.post(
    "/xccy/joint",
    status_code=202,
    response_model=RunningCalibrationRunResponse,
    responses={
        422: {
            "model": ApiErrorResponse,
            "content": {
                "application/json": {
                    "examples": {
                        "joint_free_parameter_limit_exceeded": {
                            "summary": "Joint aggregate free-parameter cap",
                            "value": _JOINT_CAPACITY_EXAMPLE,
                        }
                    }
                }
            },
        },
        500: {"model": ApiErrorResponse},
    },
)
async def submit_joint(
    request: JointXccyCalibrationRequest,
    response: Response,
    store: StoreProtocol = Depends(store_dependency),
    gateway: DalGateway = Depends(gateway_dependency),
) -> RunningCalibrationRunResponse:
    result = await submit_joint_xccy_calibration(store, gateway, request)
    response.headers["Location"] = f"/api/calibrations/{result.id}"
    return result


@router.get(
    "/{calibration_id}",
    response_model=CalibrationRunResponse,
    responses={
        200: {
            "content": {
                "application/json": {
                    "examples": {
                        "persisted_expected_execution_identity_hash_mismatch": {
                            "summary": "Persisted expected identity integrity failure",
                            "value": _FAILED_EXPECTED_IDENTITY_EXAMPLE,
                        }
                    }
                }
            }
        },
        404: {"model": ApiErrorResponse},
        409: {"model": ApiErrorResponse},
        422: {"model": ApiErrorResponse},
        500: {"model": ApiErrorResponse},
    },
)
async def get_calibration(
    calibration_id: Annotated[str, Path(pattern=_ENTITY_ID)],
    quote_bump_index: Annotated[int | None, Query(ge=0)] = None,
    quote_bump_size: Annotated[float | None, Query(ge=-0.01, le=0.01)] = None,
    store: StoreProtocol = Depends(store_dependency),
) -> Response:
    query = QuoteBumpQueryDTO(
        quote_bump_index=quote_bump_index,
        quote_bump_size=quote_bump_size,
    )
    result = get_calibration_response(store, calibration_id, query)
    body = RUN_RESPONSE_ADAPTER.dump_json(result)
    if len(body) > MAX_RESPONSE_BYTES:
        incident_id = uuid4().hex
        error = ApiErrorDTO(
            code="RESPONSE_LIMIT_GUARD_BREACH",
            message=f"serialized response is {len(body)} bytes; "
            f"limit is {MAX_RESPONSE_BYTES} bytes",
            location=None,
            context={
                "actual_bytes": len(body),
                "limit_bytes": MAX_RESPONSE_BYTES,
                "incident_id": incident_id,
            },
        )
        return _error_response(500, error)
    return _exact_json_response(body)


@curve_router.get(
    "/{curve_id}",
    response_model=CurveReconstructionDTO,
    responses={
        404: {"model": ApiErrorResponse},
        409: {"model": ApiErrorResponse},
        422: {"model": ApiErrorResponse},
        500: {"model": ApiErrorResponse},
    },
)
async def get_curve(
    curve_id: Annotated[str, Path(pattern=_ENTITY_ID)],
    store: StoreProtocol = Depends(store_dependency),
) -> Response:
    body = CURVE_RESPONSE_ADAPTER.dump_json(get_curve_response(store, curve_id))
    if len(body) > MAX_RESPONSE_BYTES:
        raise RuntimeError("persisted curve response exceeds the response limit")
    return _exact_json_response(body)


def _exact_json_response(body: bytes) -> Response:
    return Response(
        content=body,
        media_type="application/json",
        headers={
            "Content-Length": str(len(body)),
            "X-DAL-Response-Bytes": str(len(body)),
            "X-DAL-Response-Limit": str(MAX_RESPONSE_BYTES),
        },
    )
