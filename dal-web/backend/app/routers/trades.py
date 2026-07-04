"""Trade endpoints, including single-trade valuation."""

from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException

from app.dependencies import gateway_dependency, store_dependency
from app.schemas import (
    Trade,
    TradeCreate,
    TradeUpdate,
    ValuationConfig,
    ValuationResult,
)
from app.services.dal_gateway import DalGateway
from app.services.store import NotFoundError, Store
from app.services.valuation import value_single_trade_async

router = APIRouter(prefix="/api/trades", tags=["trades"])


@router.get("", response_model=list[Trade])
async def list_trades(store: Store = Depends(store_dependency)) -> list[Trade]:
    return store.list_trades()


@router.post("", response_model=Trade, status_code=201)
async def create_trade(
    payload: TradeCreate,
    store: Store = Depends(store_dependency),
) -> Trade:
    trade = Trade(**payload.model_dump())
    try:
        return store.add_trade(trade)
    except NotFoundError as exc:
        raise HTTPException(status_code=422, detail=str(exc)) from exc


@router.get("/{trade_id}", response_model=Trade)
async def get_trade(trade_id: str, store: Store = Depends(store_dependency)) -> Trade:
    try:
        return store.get_trade(trade_id)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.put("/{trade_id}", response_model=Trade)
async def update_trade(
    trade_id: str,
    payload: TradeUpdate,
    store: Store = Depends(store_dependency),
) -> Trade:
    patch = payload.model_dump(exclude_none=True)
    try:
        return store.update_trade(trade_id, patch)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.delete("/{trade_id}", status_code=204)
async def delete_trade(trade_id: str, store: Store = Depends(store_dependency)) -> None:
    store.delete_trade(trade_id)


@router.post("/{trade_id}/value", response_model=ValuationResult)
async def value_trade_endpoint(
    trade_id: str,
    config: ValuationConfig,
    store: Store = Depends(store_dependency),
    gateway: DalGateway = Depends(gateway_dependency),
) -> ValuationResult:
    """Start a single-trade valuation asynchronously.

    Returns a pending ``ValuationResult`` immediately with ``status="running"``.
    Pricing runs as an ``asyncio`` task that offloads the blocking C++ call to a
    worker thread; poll ``GET /api/valuations/{id}`` until ``status`` becomes
    ``"completed"`` or ``"failed"``.
    """
    try:
        store.get_trade(trade_id)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc

    return await value_single_trade_async(store, gateway, trade_id, config)
