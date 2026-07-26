"""Valuation history + system/market endpoints."""

from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException

from app.dependencies import gateway_dependency, store_dependency
from app.schemas import HealthResponse, ValuationResult
from app.services.dal_gateway import DalGateway
from app.services.store import NotFoundError, Store

router = APIRouter(prefix="/api", tags=["system"])


@router.get("/health", response_model=HealthResponse)
async def health(gateway: DalGateway = Depends(gateway_dependency)) -> HealthResponse:
    snapshot = gateway.health_snapshot()
    return HealthResponse(
        status="ok",
        backend=snapshot.backend,
        is_native=snapshot.is_native,
        evaluation_date=snapshot.evaluation_date or "",
    )


@router.get("/valuations", response_model=list[ValuationResult])
async def list_valuations(
    store: Store = Depends(store_dependency),
) -> list[ValuationResult]:
    return store.list_valuations()


@router.get("/valuations/{valuation_id}", response_model=ValuationResult)
async def get_valuation(
    valuation_id: str,
    store: Store = Depends(store_dependency),
) -> ValuationResult:
    try:
        return store.get_valuation(valuation_id)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
