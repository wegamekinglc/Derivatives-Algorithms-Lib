"""Valuation service: orchestrates trade / portfolio pricing via the gateway."""

from __future__ import annotations

from datetime import datetime, timezone
from typing import Dict

from app.schemas import (
    Trade,
    TradeValuation,
    ValuationConfig,
    ValuationResult,
)
from app.services.dal_gateway import DalGateway, ValuationRequest
from app.services.store import Store


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def value_trade(
    store: Store,
    gateway: DalGateway,
    trade: Trade,
    config: ValuationConfig,
) -> TradeValuation:
    """Price a single trade and scale the PV/Greeks by notional * quantity."""
    product = store.get_product(trade.product_id)
    model = store.get_model(trade.model_id)
    event_dates, events = product.event_dates_and_events()
    model_kind, model_params = model.dal_kind_and_params()

    eval_date = None
    if config.evaluation_date is not None:
        d = config.evaluation_date
        eval_date = (d.year, d.month, d.day)

    request = ValuationRequest(
        event_dates=event_dates,
        events=events,
        model_kind=model_kind,
        model_params=model_params,
        num_paths=config.num_paths,
        method=config.method,
        use_brownian_bridge=config.use_brownian_bridge,
        enable_aad=config.enable_aad,
        smooth=config.smooth,
        evaluation_date=eval_date,
    )

    scale = trade.notional * trade.quantity
    try:
        raw = gateway.value(request)
    except Exception as exc:  # noqa: BLE001 - surface per-trade failures
        return TradeValuation(
            trade_id=trade.id,
            trade_name=trade.name,
            pv=0.0,
            scaled_pv=0.0,
            greeks={},
            error=str(exc),
        )

    pv = raw.pop("PV", 0.0)
    greeks = {k: v * scale for k, v in raw.items()}
    return TradeValuation(
        trade_id=trade.id,
        trade_name=trade.name,
        pv=pv,
        scaled_pv=pv * scale,
        greeks=greeks,
    )


def value_portfolio(
    store: Store,
    gateway: DalGateway,
    portfolio_id: str,
    config: ValuationConfig,
) -> ValuationResult:
    trades = store.portfolio_trades(portfolio_id)
    trade_valuations = [value_trade(store, gateway, t, config) for t in trades]
    total_pv = sum(tv.scaled_pv for tv in trade_valuations)
    total_greeks: Dict[str, float] = {}
    for tv in trade_valuations:
        for name, value in tv.greeks.items():
            total_greeks[name] = total_greeks.get(name, 0.0) + value

    result = ValuationResult(
        target_kind="portfolio",
        target_id=portfolio_id,
        backend=gateway.backend_name,
        is_native=gateway.is_native,
        config=config,
        total_pv=total_pv,
        total_greeks=total_greeks,
        trades=trade_valuations,
        created_at=_utc_now(),
    )
    return store.add_valuation(result)


def value_single_trade(
    store: Store,
    gateway: DalGateway,
    trade_id: str,
    config: ValuationConfig,
) -> ValuationResult:
    trade = store.get_trade(trade_id)
    tv = value_trade(store, gateway, trade, config)
    result = ValuationResult(
        target_kind="trade",
        target_id=trade_id,
        backend=gateway.backend_name,
        is_native=gateway.is_native,
        config=config,
        total_pv=tv.scaled_pv,
        total_greeks=dict(tv.greeks),
        trades=[tv],
        created_at=_utc_now(),
    )
    return store.add_valuation(result)
