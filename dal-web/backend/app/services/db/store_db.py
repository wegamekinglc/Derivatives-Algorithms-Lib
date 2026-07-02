"""Database-backed implementation of the ``Store`` surface.

``DbStore`` mirrors the public methods of the in-memory :class:`Store` but
persists every entity to a SQLAlchemy database. Each method opens a short-lived
session, does its work, commits, and closes -- no session is ever held across
the ``asyncio.to_thread`` boundary used by the async valuation path, which is
what makes this safe to use as a drop-in replacement for ``Store``.
"""

from __future__ import annotations

from sqlalchemy import select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session, sessionmaker

from app.schemas import (
    ModelDefinition,
    Portfolio,
    ProductDefinition,
    Trade,
    ValuationResult,
)
from app.services.db.models import (
    Base,
    ModelRow,
    PortfolioRow,
    PortfolioTradeRow,
    ProductRow,
    TradeRow,
    ValuationRow,
)
from app.services.db.session import engine_from_url
from app.services.store import ConflictError, NotFoundError


class DbStore:
    """A :class:`Store`-compatible facade backed by a SQLAlchemy database."""

    def __init__(self, url: str) -> None:
        self._url = url
        self._engine = engine_from_url(self._url)
        self._session_factory: sessionmaker[Session] = sessionmaker(
            bind=self._engine, expire_on_commit=False, future=True
        )

    # -- schema / lifecycle ---------------------------------------------

    def create_all(self) -> None:
        """Create all tables. Dev / test fast path; production uses Alembic."""
        Base.metadata.create_all(self._engine)

    def close(self) -> None:
        """Release the connection pool. Tests use this between instances."""
        self._engine.dispose()

    @property
    def url(self) -> str:
        return self._url

    def _session(self) -> Session:
        return self._session_factory()

    # -- products --------------------------------------------------------

    def add_product(self, product: ProductDefinition) -> ProductDefinition:
        with self._session() as session:
            session.add(ProductRow.from_schema(product))
            session.commit()
            return product

    def list_products(self) -> list[ProductDefinition]:
        with self._session() as session:
            rows = session.scalars(select(ProductRow)).all()
            return [r.to_schema() for r in rows]

    def get_product(self, product_id: str) -> ProductDefinition:
        with self._session() as session:
            row = session.get(ProductRow, product_id)
            if row is None:
                raise NotFoundError(f"product {product_id}")
            return row.to_schema()

    def delete_product(self, product_id: str) -> None:
        with self._session() as session:
            row = session.get(ProductRow, product_id)
            if row is None:
                return
            ref = session.scalars(
                select(TradeRow).where(TradeRow.product_id == product_id).limit(1)
            ).first()
            if ref is not None:
                raise ConflictError(
                    f"Cannot delete product {product_id}: still referenced by trade {ref.id}"
                )
            session.delete(row)
            session.commit()

    def update_product(self, product_id: str, patch: dict) -> ProductDefinition:
        with self._session() as session:
            row = session.get(ProductRow, product_id)
            if row is None:
                raise NotFoundError(f"product {product_id}")
            updated = row.to_schema()
            data = updated.model_dump()
            data.update(patch)
            updated = ProductDefinition(**data)
            merged = ProductRow.from_schema(updated)
            row.name = merged.name
            row.description = merged.description
            row.template = merged.template
            row.rows = merged.rows
            session.commit()
            return updated

    # -- models ----------------------------------------------------------

    def add_model(self, model: ModelDefinition) -> ModelDefinition:
        with self._session() as session:
            session.add(ModelRow.from_schema(model))
            session.commit()
            return model

    def list_models(self) -> list[ModelDefinition]:
        with self._session() as session:
            rows = session.scalars(select(ModelRow)).all()
            return [r.to_schema() for r in rows]

    def get_model(self, model_id: str) -> ModelDefinition:
        with self._session() as session:
            row = session.get(ModelRow, model_id)
            if row is None:
                raise NotFoundError(f"model {model_id}")
            return row.to_schema()

    def delete_model(self, model_id: str) -> None:
        with self._session() as session:
            row = session.get(ModelRow, model_id)
            if row is None:
                return
            ref = session.scalars(
                select(TradeRow).where(TradeRow.model_id == model_id).limit(1)
            ).first()
            if ref is not None:
                raise ConflictError(
                    f"Cannot delete model {model_id}: still referenced by trade {ref.id}"
                )
            session.delete(row)
            session.commit()

    def update_model(self, model_id: str, patch: dict) -> ModelDefinition:
        with self._session() as session:
            row = session.get(ModelRow, model_id)
            if row is None:
                raise NotFoundError(f"model {model_id}")
            updated = row.to_schema()
            data = updated.model_dump()
            data.update(patch)
            updated = ModelDefinition(**data)
            # Mirror Store.update_model: validate params match the declared kind.
            updated.dal_kind_and_params()
            merged = ModelRow.from_schema(updated)
            row.name = merged.name
            row.kind = merged.kind
            row.params = merged.params
            session.commit()
            return updated

    # -- trades ----------------------------------------------------------

    def add_trade(self, trade: Trade) -> Trade:
        # Validate references up front so a missing product / model surfaces as
        # NotFoundError (matching Store) rather than a raw FK IntegrityError.
        self.get_product(trade.product_id)
        self.get_model(trade.model_id)
        with self._session() as session:
            session.add(TradeRow.from_schema(trade))
            try:
                session.commit()
            except IntegrityError as exc:  # pragma: no cover - belt and braces
                session.rollback()
                raise NotFoundError(
                    f"product {trade.product_id} or model {trade.model_id}"
                ) from exc
            return trade

    def list_trades(self) -> list[Trade]:
        with self._session() as session:
            rows = session.scalars(select(TradeRow)).all()
            return [r.to_schema() for r in rows]

    def get_trade(self, trade_id: str) -> Trade:
        with self._session() as session:
            row = session.get(TradeRow, trade_id)
            if row is None:
                raise NotFoundError(f"trade {trade_id}")
            return row.to_schema()

    def delete_trade(self, trade_id: str) -> None:
        with self._session() as session:
            row = session.get(TradeRow, trade_id)
            if row is None:
                return
            session.delete(row)
            # portfolio_trade rows cascade on delete; commit flushes them too.
            session.commit()

    def update_trade(self, trade_id: str, patch: dict) -> Trade:
        if "product_id" in patch:
            self.get_product(patch["product_id"])
        if "model_id" in patch:
            self.get_model(patch["model_id"])
        with self._session() as session:
            row = session.get(TradeRow, trade_id)
            if row is None:
                raise NotFoundError(f"trade {trade_id}")
            updated = row.to_schema()
            data = updated.model_dump()
            data.update(patch)
            updated = Trade(**data)
            merged = TradeRow.from_schema(updated)
            row.name = merged.name
            row.book = merged.book
            row.counterparty = merged.counterparty
            row.notional = merged.notional
            row.quantity = merged.quantity
            row.product_id = merged.product_id
            row.model_id = merged.model_id
            row.tags = merged.tags
            try:
                session.commit()
            except IntegrityError as exc:  # pragma: no cover - belt and braces
                session.rollback()
                raise NotFoundError(f"trade {trade_id} reference") from exc
            return updated

    # -- portfolios ------------------------------------------------------

    def add_portfolio(self, portfolio: Portfolio) -> Portfolio:
        with self._session() as session:
            session.add(PortfolioRow.from_schema(portfolio))
            for position, trade_id in enumerate(portfolio.trade_ids):
                session.add(
                    PortfolioTradeRow(
                        portfolio_id=portfolio.id, trade_id=trade_id, position=position
                    )
                )
            session.commit()
            return portfolio

    def list_portfolios(self) -> list[Portfolio]:
        with self._session() as session:
            rows = session.scalars(select(PortfolioRow)).all()
            return [r.to_schema() for r in rows]

    def get_portfolio(self, portfolio_id: str) -> Portfolio:
        with self._session() as session:
            row = session.get(PortfolioRow, portfolio_id)
            if row is None:
                raise NotFoundError(f"portfolio {portfolio_id}")
            return row.to_schema()

    def delete_portfolio(self, portfolio_id: str) -> None:
        with self._session() as session:
            row = session.get(PortfolioRow, portfolio_id)
            if row is None:
                return
            session.delete(row)
            session.commit()

    def portfolio_trades(self, portfolio_id: str) -> list[Trade]:
        with self._session() as session:
            pf = session.get(PortfolioRow, portfolio_id)
            if pf is None:
                raise NotFoundError(f"portfolio {portfolio_id}")
            trade_ids = [m.trade_id for m in pf.memberships]
            if not trade_ids:
                return []
            rows = session.scalars(
                select(TradeRow).where(TradeRow.id.in_(trade_ids))
            ).all()
            by_id = {r.id: r.to_schema() for r in rows}
            return [by_id[tid] for tid in trade_ids if tid in by_id]

    def add_trade_to_portfolio(self, portfolio_id: str, trade_id: str) -> Portfolio:
        with self._session() as session:
            pf = session.get(PortfolioRow, portfolio_id)
            if pf is None:
                raise NotFoundError(f"portfolio {portfolio_id}")
            if session.get(TradeRow, trade_id) is None:
                raise NotFoundError(f"trade {trade_id}")
            existing_ids = {m.trade_id for m in pf.memberships}
            if trade_id not in existing_ids:
                # position stays gap-free: remove_trade_from_portfolio renumbers
                # surviving rows, so len(memberships) is the next free index.
                pf.memberships.append(
                    PortfolioTradeRow(
                        portfolio_id=portfolio_id,
                        trade_id=trade_id,
                        position=len(pf.memberships),
                    )
                )
            session.commit()
            return pf.to_schema()

    def remove_trade_from_portfolio(self, portfolio_id: str, trade_id: str) -> Portfolio:
        with self._session() as session:
            pf = session.get(PortfolioRow, portfolio_id)
            if pf is None:
                raise NotFoundError(f"portfolio {portfolio_id}")
            kept = [m for m in pf.memberships if m.trade_id != trade_id]
            for m in pf.memberships:
                if m.trade_id == trade_id:
                    session.delete(m)
            for position, m in enumerate(kept):
                m.position = position
            session.commit()
            return pf.to_schema()

    # -- valuation history ----------------------------------------------

    def add_valuation(self, result: ValuationResult) -> ValuationResult:
        with self._session() as session:
            session.add(ValuationRow.from_schema(result))
            session.commit()
            return result

    def list_valuations(self) -> list[ValuationResult]:
        with self._session() as session:
            rows = session.scalars(
                select(ValuationRow).order_by(ValuationRow.created_at.desc())
            ).all()
            return [r.to_schema() for r in rows]

    def get_valuation(self, valuation_id: str) -> ValuationResult:
        with self._session() as session:
            row = session.get(ValuationRow, valuation_id)
            if row is None:
                raise NotFoundError(f"valuation {valuation_id}")
            return row.to_schema()

    def update_valuation(self, valuation_id: str, patch: dict) -> ValuationResult:
        with self._session() as session:
            row = session.get(ValuationRow, valuation_id)
            if row is None:
                raise NotFoundError(f"valuation {valuation_id}")
            updated = row.to_schema()
            data = updated.model_dump()
            data.update(patch)
            updated = ValuationResult(**data)
            merged = ValuationRow.from_schema(updated)
            row.target_kind = merged.target_kind
            row.target_id = merged.target_id
            row.backend = merged.backend
            row.is_native = merged.is_native
            row.config = merged.config
            row.total_pv = merged.total_pv
            row.total_greeks = merged.total_greeks
            row.trades = merged.trades
            row.created_at = merged.created_at
            row.status = merged.status
            row.error_message = merged.error_message
            session.commit()
            return updated
