"""Stateless Curve Lab authoring and capability resources."""

from __future__ import annotations

from fastapi import APIRouter, HTTPException, Request
from fastapi.exception_handlers import request_validation_exception_handler
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse

from app.schemas.curve_lab import (
    CURVE_LAB_V1_SUCCESS_FAMILIES,
    CURVE_LAB_V1_SUCCESS_REGISTRY,
    CurveLabCapabilitiesResponse,
    CurveLabErrorResponse,
    CurveLabQuoteCanonicalizationRequest,
    CurveLabQuoteCanonicalizationResponse,
    CurveLabRegistryEntryDTO,
)
from app.services.quote_canonicalization import (
    QuoteCanonicalizationError,
    canonicalize_quote,
)

router = APIRouter(prefix="/api/curve-lab", tags=["curve-lab"])


@router.get("/capabilities", response_model=CurveLabCapabilitiesResponse)
def get_curve_lab_capabilities() -> CurveLabCapabilitiesResponse:
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
def canonicalize_authoring_quote(
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


async def curve_lab_validation_exception_handler(
    request: Request,
    exc: RequestValidationError,
) -> JSONResponse:
    """Give pre-handler axis overrides the same stable Curve Lab envelope."""

    if request.url.path == "/api/curve-lab/quote-canonicalizations":
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
    return await request_validation_exception_handler(request, exc)
