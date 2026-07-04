"""SQLAlchemy ORM models for the five persisted entities.

These are intentionally separate from the Pydantic schemas in
``app.schemas``: a flat row-oriented representation suits a relational store
better than the nested API shapes, and the two are bridged with
``to_schema()`` / ``from_schema()`` mappers.

Hybrid column mapping: scalar fields are real columns; nested or variable
structures (product rows, model params, valuation ``config`` / ``trades`` /
``total_greeks``) live in JSON columns so the schema stays portable across
SQLite (JSON1) and Postgres (JSONB) without per-field table proliferation.
"""

from __future__ import annotations

from sqlalchemy import (
    Boolean,
    Float,
    ForeignKey,
    Integer,
    String,
)
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column, relationship
from sqlalchemy.types import JSON

from app.schemas import (
    BSModelParams,
    DupireModelParams,
    EventRow,
    ModelDefinition,
    Portfolio,
    ProductDefinition,
    Trade,
    TradeValuation,
    ValuationConfig,
    ValuationResult,
)


class Base(DeclarativeBase):
    """Declarative base for all ORM models."""


class ProductRow(Base):
    __tablename__ = "product"

    id: Mapped[str] = mapped_column(String, primary_key=True)
    name: Mapped[str] = mapped_column(String, nullable=False)
    description: Mapped[str] = mapped_column(String, default="", nullable=False)
    template: Mapped[str | None] = mapped_column(String, nullable=True)
    rows: Mapped[list] = mapped_column(JSON, nullable=False)

    @classmethod
    def from_schema(cls, product: ProductDefinition) -> ProductRow:
        return cls(
            id=product.id,
            name=product.name,
            description=product.description,
            template=product.template,
            rows=[r.model_dump(mode="json") for r in product.rows],
        )

    def to_schema(self) -> ProductDefinition:
        return ProductDefinition(
            id=self.id,
            name=self.name,
            description=self.description,
            template=self.template,
            rows=[EventRow(**r) for r in self.rows],
        )


class ModelRow(Base):
    __tablename__ = "model"

    id: Mapped[str] = mapped_column(String, primary_key=True)
    name: Mapped[str] = mapped_column(String, nullable=False)
    kind: Mapped[str] = mapped_column(String, nullable=False)
    params: Mapped[dict] = mapped_column(JSON, nullable=False)

    @classmethod
    def from_schema(cls, model: ModelDefinition) -> ModelRow:
        kind, params = model.dal_kind_and_params()
        return cls(id=model.id, name=model.name, kind=kind, params=params)

    def to_schema(self) -> ModelDefinition:
        if self.kind == "BSModelData_":
            bs = BSModelParams(**self.params)
            return ModelDefinition(id=self.id, name=self.name, kind=self.kind, bs=bs)
        if self.kind == "DupireModelData_":
            dupire = DupireModelParams(**self.params)
            return ModelDefinition(id=self.id, name=self.name, kind=self.kind, dupire=dupire)
        return ModelDefinition(id=self.id, name=self.name, kind=self.kind)


class TradeRow(Base):
    __tablename__ = "trade"

    id: Mapped[str] = mapped_column(String, primary_key=True)
    name: Mapped[str] = mapped_column(String, nullable=False)
    book: Mapped[str] = mapped_column(String, default="DEFAULT", nullable=False)
    counterparty: Mapped[str] = mapped_column(String, default="", nullable=False)
    notional: Mapped[float] = mapped_column(Float, default=1.0, nullable=False)
    quantity: Mapped[float] = mapped_column(Float, default=1.0, nullable=False)
    product_id: Mapped[str] = mapped_column(
        ForeignKey("product.id", ondelete="RESTRICT"), nullable=False
    )
    model_id: Mapped[str] = mapped_column(
        ForeignKey("model.id", ondelete="RESTRICT"), nullable=False
    )
    tags: Mapped[list] = mapped_column(JSON, default=list, nullable=False)

    @classmethod
    def from_schema(cls, trade: Trade) -> TradeRow:
        return cls(
            id=trade.id,
            name=trade.name,
            book=trade.book,
            counterparty=trade.counterparty,
            notional=trade.notional,
            quantity=trade.quantity,
            product_id=trade.product_id,
            model_id=trade.model_id,
            tags=list(trade.tags),
        )

    def to_schema(self) -> Trade:
        return Trade(
            id=self.id,
            name=self.name,
            book=self.book,
            counterparty=self.counterparty,
            notional=self.notional,
            quantity=self.quantity,
            product_id=self.product_id,
            model_id=self.model_id,
            tags=list(self.tags),
        )


class PortfolioRow(Base):
    __tablename__ = "portfolio"

    id: Mapped[str] = mapped_column(String, primary_key=True)
    name: Mapped[str] = mapped_column(String, nullable=False)
    description: Mapped[str] = mapped_column(String, default="", nullable=False)

    memberships: Mapped[list[PortfolioTradeRow]] = relationship(
        back_populates="portfolio",
        cascade="all, delete-orphan",
        order_by="PortfolioTradeRow.position",
    )

    @classmethod
    def from_schema(cls, portfolio: Portfolio) -> PortfolioRow:
        return cls(id=portfolio.id, name=portfolio.name, description=portfolio.description)

    def to_schema(self) -> Portfolio:
        return Portfolio(
            id=self.id,
            name=self.name,
            description=self.description,
            trade_ids=[m.trade_id for m in self.memberships],
        )


class PortfolioTradeRow(Base):
    """Association table replacing the in-memory ``trade_ids`` list.

    Composite PK (portfolio, trade) gives stable ordering via ``position`` and
    lets a trade removal cascade its membership rows.
    """

    __tablename__ = "portfolio_trade"

    portfolio_id: Mapped[str] = mapped_column(
        ForeignKey("portfolio.id", ondelete="CASCADE"), primary_key=True
    )
    trade_id: Mapped[str] = mapped_column(
        ForeignKey("trade.id", ondelete="CASCADE"), primary_key=True
    )
    position: Mapped[int] = mapped_column(Integer, nullable=False)

    portfolio: Mapped[PortfolioRow] = relationship(back_populates="memberships")


class ValuationRow(Base):
    """Valuation result / history row.

    Holds no foreign key to its target trade or portfolio: valuation results are
    an audit trail and must survive the deletion of the entity they priced.
    """

    __tablename__ = "valuation"

    id: Mapped[str] = mapped_column(String, primary_key=True)
    target_kind: Mapped[str] = mapped_column(String, nullable=False)
    target_id: Mapped[str] = mapped_column(String, nullable=False)
    backend: Mapped[str] = mapped_column(String, nullable=False)
    is_native: Mapped[bool] = mapped_column(Boolean, nullable=False)
    config: Mapped[dict] = mapped_column(JSON, nullable=False)
    total_pv: Mapped[float] = mapped_column(Float, nullable=False)
    total_greeks: Mapped[dict] = mapped_column(JSON, nullable=False)
    trades: Mapped[list] = mapped_column(JSON, nullable=False)
    created_at: Mapped[str] = mapped_column(String, nullable=False)
    status: Mapped[str] = mapped_column(String, nullable=False)
    error_message: Mapped[str | None] = mapped_column(String, nullable=True)

    @classmethod
    def from_schema(cls, result: ValuationResult) -> ValuationRow:
        return cls(
            id=result.id,
            target_kind=result.target_kind,
            target_id=result.target_id,
            backend=result.backend,
            is_native=result.is_native,
            config=result.config.model_dump(mode="json"),
            total_pv=result.total_pv,
            total_greeks=dict(result.total_greeks),
            trades=[t.model_dump(mode="json") for t in result.trades],
            created_at=result.created_at,
            status=result.status,
            error_message=result.error_message,
        )

    def to_schema(self) -> ValuationResult:
        return ValuationResult(
            id=self.id,
            target_kind=self.target_kind,
            target_id=self.target_id,
            backend=self.backend,
            is_native=self.is_native,
            config=ValuationConfig(**self.config),
            total_pv=self.total_pv,
            total_greeks=dict(self.total_greeks),
            trades=[TradeValuation(**t) for t in self.trades],
            created_at=self.created_at,
            status=self.status,
            error_message=self.error_message,
        )
