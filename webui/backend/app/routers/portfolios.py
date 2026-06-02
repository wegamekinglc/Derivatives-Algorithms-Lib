"""Portfolio endpoints, including portfolio-level valuation."""

from __future__ import annotations

from typing import List

from fastapi import APIRouter, Depends, HTTPException

from app.dependencies import gateway_dependency, store_dependency
from app.schemas import (
    Portfolio,
    PortfolioCreate,
    Trade,
    ValuationConfig,
    ValuationResult,
)
from app.services.dal_gateway import DalGateway
from app.services.store import NotFoundError, Store
from app.services.valuation import value_portfolio

router = APIRouter(prefix="/api/portfolios", tags=["portfolios"])


@router.get("", response_model=List[Portfolio])
def list_portfolios(store: Store = Depends(store_dependency)) -> List[Portfolio]:
    return store.list_portfolios()


@router.post("", response_model=Portfolio, status_code=201)
def create_portfolio(
    payload: PortfolioCreate,
    store: Store = Depends(store_dependency),
) -> Portfolio:
    portfolio = Portfolio(**payload.model_dump())
    return store.add_portfolio(portfolio)


@router.get("/{portfolio_id}", response_model=Portfolio)
def get_portfolio(
    portfolio_id: str,
    store: Store = Depends(store_dependency),
) -> Portfolio:
    try:
        return store.get_portfolio(portfolio_id)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.delete("/{portfolio_id}", status_code=204)
def delete_portfolio(
    portfolio_id: str, store: Store = Depends(store_dependency)
) -> None:
    store.delete_portfolio(portfolio_id)


@router.get("/{portfolio_id}/trades", response_model=List[Trade])
def portfolio_trades(
    portfolio_id: str,
    store: Store = Depends(store_dependency),
) -> List[Trade]:
    try:
        return store.portfolio_trades(portfolio_id)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.post("/{portfolio_id}/trades/{trade_id}", response_model=Portfolio)
def add_trade(
    portfolio_id: str,
    trade_id: str,
    store: Store = Depends(store_dependency),
) -> Portfolio:
    try:
        return store.add_trade_to_portfolio(portfolio_id, trade_id)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.delete("/{portfolio_id}/trades/{trade_id}", response_model=Portfolio)
def remove_trade(
    portfolio_id: str,
    trade_id: str,
    store: Store = Depends(store_dependency),
) -> Portfolio:
    try:
        return store.remove_trade_from_portfolio(portfolio_id, trade_id)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.post("/{portfolio_id}/value", response_model=ValuationResult)
def value_portfolio_endpoint(
    portfolio_id: str,
    config: ValuationConfig,
    store: Store = Depends(store_dependency),
    gateway: DalGateway = Depends(gateway_dependency),
) -> ValuationResult:
    try:
        store.get_portfolio(portfolio_id)
    except NotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    return value_portfolio(store, gateway, portfolio_id, config)
