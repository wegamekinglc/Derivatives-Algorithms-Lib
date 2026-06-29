"""Valuation service: orchestrates trade / portfolio pricing via the gateway.

The blocking path (``value_trade`` / ``value_portfolio`` / ``value_single_trade``)
calls ``gateway.value`` directly and returns the completed result.  The
asynchronous path (``value_portfolio_async`` / ``value_single_trade_async``)
is a coroutine that creates a pending ``ValuationResult`` in the store,
schedules pricing as an ``asyncio`` task, and returns immediately.  The task
offloads the blocking C++ ``gateway.value`` call to a worker thread via
``asyncio.to_thread`` and then writes the final result back to the store so
the polling endpoint observes ``status`` transitioning from ``running`` to
``completed`` or ``failed``.
"""

from __future__ import annotations

import asyncio
import logging
from datetime import datetime, timezone
from typing import Any, Coroutine

from app.schemas import (
    Trade,
    TradeValuation,
    ValuationConfig,
    ValuationResult,
)
from app.services.dal_gateway import DalGateway, ValuationRequest
from app.services.store import Store

logger = logging.getLogger(__name__)

# Strong references to in-flight pricing tasks.  asyncio drops tasks whose only
# reference is the event loop's weak set, which would silently cancel pricing
# mid-flight; holding them here keeps the coroutines alive until they complete.
_BACKGROUND_TASKS: set[asyncio.Task[None]] = set()


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


def _schedule_pricing(coro: Coroutine[Any, Any, None]) -> "asyncio.Task[None]":
    """Create a pricing task and retain it until it finishes.

    The task is added to ``_BACKGROUND_TASKS`` so the event loop cannot garbage
    collect it mid-pricing, then removed via ``done_callback`` once it settles.
    """
    task = asyncio.create_task(coro)
    _BACKGROUND_TASKS.add(task)
    task.add_done_callback(_BACKGROUND_TASKS.discard)
    return task


async def _run_portfolio_pricing_async(
    store: Store,
    gateway: DalGateway,
    valuation_id: str,
    portfolio_id: str,
    config: ValuationConfig,
) -> None:
    """Price a portfolio and update the ValuationResult in-place.

    Each per-trade C++ pricing call is offloaded to a worker thread via
    ``asyncio.to_thread`` so the event loop stays responsive; ``gateway.value``
    holds the GIL for the whole Monte Carlo run.  ``DalGateway._lock`` keeps
    concurrent dispatch serial inside the gateway, so the trades are priced
    sequentially -- running them concurrently would only spawn worker threads
    that block on the lock and risk exhausting the default thread pool.
    """
    try:
        trades = store.portfolio_trades(portfolio_id)
        trade_valuations = [
            await asyncio.to_thread(_price_trade, store, gateway, t, config) for t in trades
        ]
        total_pv, total_greeks = _aggregate_trade_valuations(trade_valuations)
        store.update_valuation(
            valuation_id,
            {
                "total_pv": total_pv,
                "total_greeks": total_greeks,
                "trades": list(trade_valuations),
                "status": "completed",
            },
        )
    except Exception as exc:  # noqa: BLE001 - mark the valuation as failed
        logger.exception("Portfolio valuation %s failed", valuation_id)
        store.update_valuation(
            valuation_id,
            {
                "status": "failed",
                "error_message": str(exc),
                "total_pv": 0.0,
                "total_greeks": {},
            },
        )


async def _run_trade_pricing_async(
    store: Store,
    gateway: DalGateway,
    valuation_id: str,
    trade_id: str,
    config: ValuationConfig,
) -> None:
    """Price a single trade and update the ValuationResult in-place.

    The blocking C++ ``gateway.value`` call is offloaded via ``asyncio.to_thread``.
    """
    try:
        trade = store.get_trade(trade_id)
        tv = await asyncio.to_thread(_price_trade, store, gateway, trade, config)
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
        logger.exception("Trade valuation %s failed", valuation_id)
        store.update_valuation(
            valuation_id,
            {
                "status": "failed",
                "error_message": str(exc),
                "total_pv": 0.0,
                "total_greeks": {},
            },
        )


async def value_portfolio_async(
    store: Store,
    gateway: DalGateway,
    portfolio_id: str,
    config: ValuationConfig,
) -> ValuationResult:
    """Create a pending ValuationResult, schedule pricing, and return immediately.

    Pricing runs as an ``asyncio`` task that offloads ``gateway.value`` to a
    worker thread.  The caller polls ``GET /api/valuations/{id}`` until
    ``status`` becomes ``"completed"`` or ``"failed"``.
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
    added = store.add_valuation(pending)
    _schedule_pricing(
        _run_portfolio_pricing_async(store, gateway, added.id, portfolio_id, config)
    )
    return added


async def value_single_trade_async(
    store: Store,
    gateway: DalGateway,
    trade_id: str,
    config: ValuationConfig,
) -> ValuationResult:
    """Create a pending ValuationResult, schedule pricing, and return immediately.

    Pricing runs as an ``asyncio`` task that offloads ``gateway.value`` to a
    worker thread.  The caller polls ``GET /api/valuations/{id}`` until
    ``status`` becomes ``"completed"`` or ``"failed"``.
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
    added = store.add_valuation(pending)
    _schedule_pricing(
        _run_trade_pricing_async(store, gateway, added.id, trade_id, config)
    )
    return added
