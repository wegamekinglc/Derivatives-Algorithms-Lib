"""Valuation service: orchestrates trade / portfolio pricing via the gateway.

Supports both synchronous and asynchronous pricing.  The synchronous path
(``value_trade``, ``value_portfolio``, ``value_single_trade``) blocks until
pricing completes and returns the result directly.  The asynchronous path
(``value_portfolio_async``, ``value_single_trade_async``) creates a pending
``ValuationResult`` in the store, runs pricing in a background task, and
updates the result in-place when done (status: running -> completed / failed).
"""

from __future__ import annotations

from datetime import datetime, timezone

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


def _price_trade(
    store: Store,
    gateway: DalGateway,
    trade: Trade,
    config: ValuationConfig,
) -> TradeValuation:
    """Price a single trade and scale the PV/Greeks by notional * quantity.

    Per-trade failures are caught and returned as ``TradeValuation(error=...)``
    so that a single failing trade does not abort an entire portfolio valuation.
    """
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


# Backwards-compatible alias (the old public name).
value_trade = _price_trade


def _aggregate_trade_valuations(
    trade_valuations: list[TradeValuation],
) -> tuple[float, dict[str, float]]:
    """Sum scaled PVs and merge per-trade Greeks into portfolio totals."""
    total_pv = sum(tv.scaled_pv for tv in trade_valuations)
    total_greeks: dict[str, float] = {}
    for tv in trade_valuations:
        for name, value in tv.greeks.items():
            total_greeks[name] = total_greeks.get(name, 0.0) + value
    return total_pv, total_greeks


# ---------------------------------------------------------------------------
# Synchronous pricing (legacy path — blocks until done)
# ---------------------------------------------------------------------------


def value_portfolio(
    store: Store,
    gateway: DalGateway,
    portfolio_id: str,
    config: ValuationConfig,
) -> ValuationResult:
    trades = store.portfolio_trades(portfolio_id)
    trade_valuations = [_price_trade(store, gateway, t, config) for t in trades]
    total_pv, total_greeks = _aggregate_trade_valuations(trade_valuations)

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
    tv = _price_trade(store, gateway, trade, config)
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


# ---------------------------------------------------------------------------
# Asynchronous pricing (creates a pending result, runs in background task)
# ---------------------------------------------------------------------------


def _run_portfolio_pricing(
    store: Store,
    gateway: DalGateway,
    valuation_id: str,
    portfolio_id: str,
    config: ValuationConfig,
) -> None:
    """Background task: price a portfolio and update the ValuationResult in-place."""
    try:
        trades = store.portfolio_trades(portfolio_id)
        trade_valuations = [_price_trade(store, gateway, t, config) for t in trades]
        total_pv, total_greeks = _aggregate_trade_valuations(trade_valuations)
        store.update_valuation(
            valuation_id,
            {
                "total_pv": total_pv,
                "total_greeks": total_greeks,
                "trades": trade_valuations,
                "status": "completed",
            },
        )
    except Exception as exc:  # noqa: BLE001 - mark the valuation as failed
        store.update_valuation(
            valuation_id,
            {"status": "failed", "total_greeks": {"error": 1.0}, "total_pv": 0.0},
        )


def _run_trade_pricing(
    store: Store,
    gateway: DalGateway,
    valuation_id: str,
    trade_id: str,
    config: ValuationConfig,
) -> None:
    """Background task: price a single trade and update the ValuationResult in-place."""
    try:
        trade = store.get_trade(trade_id)
        tv = _price_trade(store, gateway, trade, config)
        store.update_valuation(
            valuation_id,
            {
                "total_pv": tv.scaled_pv,
                "total_greeks": dict(tv.greeks),
                "trades": [tv],
                "status": "completed",
            },
        )
    except Exception as exc:  # noqa: BLE001 - mark the valuation as failed
        store.update_valuation(
            valuation_id,
            {"status": "failed", "total_greeks": {"error": 1.0}, "total_pv": 0.0},
        )


def value_portfolio_async(
    store: Store,
    gateway: DalGateway,
    portfolio_id: str,
    config: ValuationConfig,
) -> ValuationResult:
    """Create a pending ValuationResult and return immediately.

    Pricing runs in a FastAPI BackgroundTasks task.  The caller polls
    ``GET /api/valuations/{id}`` until ``status == "completed"`` or ``"failed"``.
    """
    pending = ValuationResult(
        target_kind="portfolio",
        target_id=portfolio_id,
        backend=gateway.backend_name,
        is_native=gateway.is_native,
        config=config,
        total_pv=0.0,
        trades=[],
        created_at=_utc_now(),
        status="running",
    )
    return store.add_valuation(pending)


def value_single_trade_async(
    store: Store,
    gateway: DalGateway,
    trade_id: str,
    config: ValuationConfig,
) -> ValuationResult:
    """Create a pending ValuationResult and return immediately.

    Pricing runs in a FastAPI BackgroundTasks task.  The caller polls
    ``GET /api/valuations/{id}`` until ``status == "completed"`` or ``"failed"``.
    """
    pending = ValuationResult(
        target_kind="trade",
        target_id=trade_id,
        backend=gateway.backend_name,
        is_native=gateway.is_native,
        config=config,
        total_pv=0.0,
        trades=[],
        created_at=_utc_now(),
        status="running",
    )
    return store.add_valuation(pending)
