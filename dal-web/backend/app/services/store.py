"""Persistence seam for portfolios, trades, products, models and valuations.

Two implementations live behind this seam: the in-memory :class:`Store` (no
external dependencies, lost on restart) and the database-backed
:class:`~app.services.db.store_db.DbStore` (SQLAlchemy, persists to disk).
Routers depend only on :class:`StoreProtocol`, so either implementation can be
selected at startup via environment variables without touching router code.
"""

from __future__ import annotations

import os
import threading
from typing import Protocol, runtime_checkable

from app.schemas import (
    ModelDefinition,
    Portfolio,
    ProductDefinition,
    Trade,
    ValuationResult,
)


class NotFoundError(KeyError):
    """Raised when an entity id cannot be resolved."""


class ConflictError(Exception):
    """Raised when an operation conflicts with existing state (e.g., references)."""


@runtime_checkable
class StoreProtocol(Protocol):
    """The router-facing surface every store implementation must provide."""

    # products
    def add_product(self, product: ProductDefinition) -> ProductDefinition: ...
    def list_products(self) -> list[ProductDefinition]: ...
    def get_product(self, product_id: str) -> ProductDefinition: ...
    def delete_product(self, product_id: str) -> None: ...
    def update_product(self, product_id: str, patch: dict) -> ProductDefinition: ...

    # models
    def add_model(self, model: ModelDefinition) -> ModelDefinition: ...
    def list_models(self) -> list[ModelDefinition]: ...
    def get_model(self, model_id: str) -> ModelDefinition: ...
    def delete_model(self, model_id: str) -> None: ...
    def update_model(self, model_id: str, patch: dict) -> ModelDefinition: ...

    # trades
    def add_trade(self, trade: Trade) -> Trade: ...
    def list_trades(self) -> list[Trade]: ...
    def get_trade(self, trade_id: str) -> Trade: ...
    def delete_trade(self, trade_id: str) -> None: ...
    def update_trade(self, trade_id: str, patch: dict) -> Trade: ...

    # portfolios
    def add_portfolio(self, portfolio: Portfolio) -> Portfolio: ...
    def list_portfolios(self) -> list[Portfolio]: ...
    def get_portfolio(self, portfolio_id: str) -> Portfolio: ...
    def delete_portfolio(self, portfolio_id: str) -> None: ...
    def portfolio_trades(self, portfolio_id: str) -> list[Trade]: ...
    def add_trade_to_portfolio(self, portfolio_id: str, trade_id: str) -> Portfolio: ...
    def remove_trade_from_portfolio(self, portfolio_id: str, trade_id: str) -> Portfolio: ...

    # valuations
    def add_valuation(self, result: ValuationResult) -> ValuationResult: ...
    def list_valuations(self) -> list[ValuationResult]: ...
    def get_valuation(self, valuation_id: str) -> ValuationResult: ...
    def update_valuation(self, valuation_id: str, patch: dict) -> ValuationResult: ...


class Store:
    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._products: dict[str, ProductDefinition] = {}
        self._models: dict[str, ModelDefinition] = {}
        self._trades: dict[str, Trade] = {}
        self._portfolios: dict[str, Portfolio] = {}
        self._valuations: dict[str, ValuationResult] = {}

    # -- products --------------------------------------------------------

    def add_product(self, product: ProductDefinition) -> ProductDefinition:
        with self._lock:
            self._products[product.id] = product
            return product

    def list_products(self) -> list[ProductDefinition]:
        with self._lock:
            return list(self._products.values())

    def get_product(self, product_id: str) -> ProductDefinition:
        with self._lock:
            try:
                return self._products[product_id]
            except KeyError as exc:
                raise NotFoundError(f"product {product_id}") from exc

    def delete_product(self, product_id: str) -> None:
        with self._lock:
            if product_id not in self._products:
                return
            # Guard against orphaning trades that still reference this product.
            for trade in self._trades.values():
                if trade.product_id == product_id:
                    raise ConflictError(
                        f"Cannot delete product {product_id}: still referenced by trade {trade.id}"
                    )
            del self._products[product_id]

    def update_product(self, product_id: str, patch: dict) -> ProductDefinition:
        """Merge a partial update into an existing product definition."""
        with self._lock:
            product = self.get_product(product_id)
            updated_data = product.model_dump()
            updated_data.update(patch)
            updated = ProductDefinition(**updated_data)
            self._products[product_id] = updated
            return updated

    # -- models ----------------------------------------------------------

    def add_model(self, model: ModelDefinition) -> ModelDefinition:
        with self._lock:
            self._models[model.id] = model
            return model

    def list_models(self) -> list[ModelDefinition]:
        with self._lock:
            return list(self._models.values())

    def get_model(self, model_id: str) -> ModelDefinition:
        with self._lock:
            try:
                return self._models[model_id]
            except KeyError as exc:
                raise NotFoundError(f"model {model_id}") from exc

    def delete_model(self, model_id: str) -> None:
        with self._lock:
            if model_id not in self._models:
                return
            # Guard against orphaning trades that still reference this model.
            for trade in self._trades.values():
                if trade.model_id == model_id:
                    raise ConflictError(
                        f"Cannot delete model {model_id}: still referenced by trade {trade.id}"
                    )
            del self._models[model_id]

    def update_model(self, model_id: str, patch: dict) -> ModelDefinition:
        """Merge a partial update into an existing model definition."""
        with self._lock:
            model = self.get_model(model_id)
            updated_data = model.model_dump()
            updated_data.update(patch)
            updated = ModelDefinition(**updated_data)
            # Validate params match kind (same check as create_model)
            updated.dal_kind_and_params()
            self._models[model_id] = updated
            return updated

    # -- trades ----------------------------------------------------------

    def add_trade(self, trade: Trade) -> Trade:
        with self._lock:
            # validate references eagerly
            self.get_product(trade.product_id)
            self.get_model(trade.model_id)
            self._trades[trade.id] = trade
            return trade

    def list_trades(self) -> list[Trade]:
        with self._lock:
            return list(self._trades.values())

    def get_trade(self, trade_id: str) -> Trade:
        with self._lock:
            try:
                return self._trades[trade_id]
            except KeyError as exc:
                raise NotFoundError(f"trade {trade_id}") from exc

    def delete_trade(self, trade_id: str) -> None:
        with self._lock:
            if trade_id not in self._trades:
                return
            del self._trades[trade_id]
            # Cascade: remove from any portfolios that still reference it.
            for pf in self._portfolios.values():
                if trade_id in pf.trade_ids:
                    pf.trade_ids.remove(trade_id)

    def update_trade(self, trade_id: str, patch: dict) -> Trade:
        """Merge a partial update into an existing trade.

        If product_id or model_id are changed, validates they exist.
        """
        with self._lock:
            trade = self.get_trade(trade_id)
            if "product_id" in patch:
                self.get_product(patch["product_id"])
            if "model_id" in patch:
                self.get_model(patch["model_id"])
            updated_data = trade.model_dump()
            updated_data.update(patch)
            updated = Trade(**updated_data)
            self._trades[trade_id] = updated
            return updated

    # -- portfolios ------------------------------------------------------

    def add_portfolio(self, portfolio: Portfolio) -> Portfolio:
        with self._lock:
            self._portfolios[portfolio.id] = portfolio
            return portfolio

    def list_portfolios(self) -> list[Portfolio]:
        with self._lock:
            return list(self._portfolios.values())

    def get_portfolio(self, portfolio_id: str) -> Portfolio:
        with self._lock:
            try:
                return self._portfolios[portfolio_id]
            except KeyError as exc:
                raise NotFoundError(f"portfolio {portfolio_id}") from exc

    def delete_portfolio(self, portfolio_id: str) -> None:
        with self._lock:
            self._portfolios.pop(portfolio_id, None)

    def portfolio_trades(self, portfolio_id: str) -> list[Trade]:
        with self._lock:
            pf = self.get_portfolio(portfolio_id)
            return [self._trades[tid] for tid in pf.trade_ids if tid in self._trades]

    def add_trade_to_portfolio(self, portfolio_id: str, trade_id: str) -> Portfolio:
        with self._lock:
            pf = self.get_portfolio(portfolio_id)
            self.get_trade(trade_id)
            if trade_id not in pf.trade_ids:
                pf.trade_ids.append(trade_id)
            return pf

    def remove_trade_from_portfolio(self, portfolio_id: str, trade_id: str) -> Portfolio:
        with self._lock:
            pf = self.get_portfolio(portfolio_id)
            if trade_id in pf.trade_ids:
                pf.trade_ids.remove(trade_id)
            return pf

    # -- valuation history ----------------------------------------------

    def add_valuation(self, result: ValuationResult) -> ValuationResult:
        with self._lock:
            self._valuations[result.id] = result
            return result

    def list_valuations(self) -> list[ValuationResult]:
        with self._lock:
            return sorted(
                self._valuations.values(), key=lambda r: r.created_at, reverse=True
            )

    def get_valuation(self, valuation_id: str) -> ValuationResult:
        with self._lock:
            try:
                return self._valuations[valuation_id]
            except KeyError as exc:
                raise NotFoundError(f"valuation {valuation_id}") from exc

    def update_valuation(self, valuation_id: str, patch: dict) -> ValuationResult:
        """Atomically update fields on a valuation (used by async pricing)."""
        with self._lock:
            valuation = self.get_valuation(valuation_id)
            updated_data = valuation.model_dump()
            updated_data.update(patch)
            updated = ValuationResult(**updated_data)
            self._valuations[valuation_id] = updated
            return updated


# Process-wide singleton stored in a mutable container so get_store()
# does not need a `global` statement.
_store_box: list[StoreProtocol | None] = [None]
_store_lock = threading.Lock()


def is_memory_mode() -> bool:
    """``DAL_WEB_STORE=memory`` opts into the legacy in-memory store."""
    return os.environ.get("DAL_WEB_STORE", "").strip().lower() == "memory"


def get_store() -> StoreProtocol:
    """Return the process-wide store, building it on first use.

    By default a :class:`~app.services.db.store_db.DbStore` is built against
    ``DAL_WEB_DB_URL`` (or the local default SQLite file). Setting
    ``DAL_WEB_STORE=memory`` returns the legacy in-memory :class:`Store` instead
    -- the escape hatch for read-only environments and smoke tests.
    """
    if _store_box[0] is None:
        with _store_lock:
            if _store_box[0] is None:
                _store_box[0] = _build_store()
    return _store_box[0]


def _build_store() -> StoreProtocol:
    """Construct a fresh store based on the current environment."""
    if is_memory_mode():
        return Store()
    # Imported lazily so the in-memory path never depends on SQLAlchemy.
    from app.services.db.session import default_db_url
    from app.services.db.store_db import DbStore

    url = os.environ.get("DAL_WEB_DB_URL") or default_db_url()
    store = DbStore(url=url)
    store.create_all()
    return store
