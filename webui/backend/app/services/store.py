"""Thread-safe in-memory store for portfolios, trades, products and models.

This keeps the example self-contained (no external database).  The store is
deliberately small and could be swapped for a real persistence layer without
touching the routers, which depend only on its public methods.
"""

from __future__ import annotations

import threading

from app.schemas import (
    ModelDefinition,
    Portfolio,
    ProductDefinition,
    Trade,
    ValuationResult,
)


class NotFoundError(KeyError):
    """Raised when an entity id cannot be resolved."""


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
            self._products.pop(product_id, None)

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
            self._models.pop(model_id, None)

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
            self._trades.pop(trade_id, None)
            for pf in self._portfolios.values():
                if trade_id in pf.trade_ids:
                    pf.trade_ids.remove(trade_id)

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


# Process-wide singleton stored in a mutable container so get_store()
# does not need a `global` statement.
_store_box: list[Store | None] = [None]
_store_lock = threading.Lock()


def get_store() -> Store:
    if _store_box[0] is None:
        with _store_lock:
            if _store_box[0] is None:
                _store_box[0] = Store()
    return _store_box[0]
