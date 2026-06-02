"""Trade endpoints, including single-trade valuation."""

from __future__ import annotations

from typing import List

from fastapi import APIRouter, Depends, HTTPException

from app.dependencies import gateway_dependency, store_dependency
from app.schemas import (
    Trade,
    TradeCreate,
    ValuationConfig,
    ValuationResult,
)
from app.services.dal_gateway import DalGateway
from app.services.store import NotFoundError, Store
from app.services.valuation import value_single_trade

router = APIRouter(prefix="/api/trades", tags=["trades"])


@router.get("", response_model=List[Trade])
def list_trades(store: Store = Depends(store_dependency)) -> List[Trade]:
    return store.list_trades()


@router.post("", response_model=Trade, status_code=201)
def create_trade(
    payload: TradeCreate,
    store: Store = Depends(store_dependency),
) -> Trade:
    trade = Trade(**payload.model_dump())
    try:
        return store.add_trade(trade)
    except NotFoundError as exc:
        raise HTTPException(status_code=422, detail=str(exc)) from exc


@router.get("/{trade_id}", response_model=Trade)
def get_trade(trade_id: str, store: Store = Depends(store_dependency)) -> Trade:
    try:
        return store.get_trade(trade_id)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.delete("/{trade_id}", status_code=204)
def delete_trade(trade_id: str, store: Store = Depends(store_dependency)) -> None:
    store.delete_trade(trade_id)


@router.post("/{trade_id}/value", response_model=ValuationResult)
def value_trade_endpoint(
    trade_id: str,
    config: ValuationConfig,
    store: Store = Depends(store_dependency),
    gateway: DalGateway = Depends(gateway_dependency),
) -> ValuationResult:
    try:
        return value_single_trade(store, gateway, trade_id, config)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
