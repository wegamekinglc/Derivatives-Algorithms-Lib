"""Pydantic schemas for the portfolio-management API."""

from __future__ import annotations

from datetime import date as date_type
from typing import Literal
from uuid import uuid4

from pydantic import BaseModel, Field, model_validator


def _new_id() -> str:
    return uuid4().hex


# ---------------------------------------------------------------------------
# Product definition (maps onto DAL scripted products)
# ---------------------------------------------------------------------------


class EventRow(BaseModel):
    """A single (date / schedule label, event-script) pair.

    ``date_kind`` distinguishes a concrete calendar date from a textual label
    such as a constant macro (``STRIKE``) or an observation schedule
    (``START: ... END: ... FREQ: 1W``).
    """

    date_kind: Literal["date", "label"] = "date"
    date: date_type | None = None
    label: str | None = None
    event: str = Field(..., description="DAL event script for this row")

    @model_validator(mode="after")
    def _check_row_consistency(self) -> "EventRow":
        if self.date_kind == "date" and self.date is None:
            raise ValueError("date_kind='date' requires a date value")
        if self.date_kind == "label" and not self.label:
            raise ValueError("date_kind='label' requires a non-empty label")
        return self

    def to_event_date_token(self) -> dict | str:
        if self.date_kind == "date":
            return {"date": self.date.isoformat()}
        return self.label


class ProductDefinition(BaseModel):
    id: str = Field(default_factory=_new_id)
    name: str
    description: str = ""
    template: str | None = None
    rows: list[EventRow]

    def event_dates_and_events(self) -> tuple[list, list[str]]:
        dates = [r.to_event_date_token() for r in self.rows]
        events = [r.event for r in self.rows]
        return dates, events


class ProductCreate(BaseModel):
    name: str
    description: str = ""
    template: str | None = None
    rows: list[EventRow]


class ProductUpdate(BaseModel):
    name: str | None = None
    description: str | None = None
    template: str | None = None
    rows: list[EventRow] | None = None


# ---------------------------------------------------------------------------
# Model definitions (map onto DAL model data)
# ---------------------------------------------------------------------------


class BSModelParams(BaseModel):
    spot: float
    vol: float = Field(..., ge=0.0)
    rate: float
    div: float


class DupireModelParams(BaseModel):
    spot: float
    rate: float
    repo: float
    spots: list[float]
    times: list[float]
    vols: list[list[float]]


class ModelDefinition(BaseModel):
    id: str = Field(default_factory=_new_id)
    name: str
    kind: Literal["BSModelData_", "DupireModelData_"]
    bs: BSModelParams | None = None
    dupire: DupireModelParams | None = None

    def dal_kind_and_params(self) -> tuple[str, dict]:
        if self.kind == "BSModelData_":
            if self.bs is None:
                raise ValueError("Black-Scholes model requires 'bs' parameters")
            return self.kind, self.bs.model_dump()
        if self.dupire is None:
            raise ValueError("Dupire model requires 'dupire' parameters")
        return self.kind, self.dupire.model_dump()


class ModelCreate(BaseModel):
    name: str
    kind: Literal["BSModelData_", "DupireModelData_"]
    bs: BSModelParams | None = None
    dupire: DupireModelParams | None = None


class ModelUpdate(BaseModel):
    name: str | None = None
    kind: Literal["BSModelData_", "DupireModelData_"] | None = None
    bs: BSModelParams | None = None
    dupire: DupireModelParams | None = None


# ---------------------------------------------------------------------------
# Portfolio / trade hierarchy
# ---------------------------------------------------------------------------


class Trade(BaseModel):
    id: str = Field(default_factory=_new_id)
    name: str
    book: str = "DEFAULT"
    counterparty: str = ""
    notional: float = 1.0
    quantity: float = 1.0
    product_id: str
    model_id: str
    tags: list[str] = Field(default_factory=list)


class TradeCreate(BaseModel):
    name: str
    book: str = "DEFAULT"
    counterparty: str = ""
    notional: float = 1.0
    quantity: float = 1.0
    product_id: str
    model_id: str
    tags: list[str] = Field(default_factory=list)


class TradeUpdate(BaseModel):
    name: str | None = None
    book: str | None = None
    counterparty: str | None = None
    notional: float | None = None
    quantity: float | None = None
    product_id: str | None = None
    model_id: str | None = None
    tags: list[str] | None = None


class Portfolio(BaseModel):
    id: str = Field(default_factory=_new_id)
    name: str
    description: str = ""
    trade_ids: list[str] = Field(default_factory=list)


class PortfolioCreate(BaseModel):
    name: str
    description: str = ""


# ---------------------------------------------------------------------------
# Valuation
# ---------------------------------------------------------------------------


class ValuationConfig(BaseModel):
    num_paths: int = Field(default=1 << 16, ge=1)
    method: Literal["sobol", "pseudo"] = "sobol"
    use_brownian_bridge: bool = False
    enable_aad: bool = True
    smooth: float = Field(default=0.01, ge=0.0)
    evaluation_date: date_type | None = None


class TradeValuation(BaseModel):
    trade_id: str
    trade_name: str
    pv: float
    scaled_pv: float
    greeks: dict[str, float] = Field(default_factory=dict)
    error: str | None = None


class ValuationResult(BaseModel):
    id: str = Field(default_factory=_new_id)
    target_kind: Literal["trade", "portfolio"]
    target_id: str
    backend: str
    is_native: bool
    config: ValuationConfig
    total_pv: float
    total_greeks: dict[str, float] = Field(default_factory=dict)
    trades: list[TradeValuation] = Field(default_factory=list)
    created_at: str
    status: Literal["running", "completed", "failed"] = "completed"


class ProductDebugRequest(BaseModel):
    rows: list[EventRow]


class ProductDebugResponse(BaseModel):
    debug: str


class HealthResponse(BaseModel):
    status: str
    backend: str
    is_native: bool
    evaluation_date: str
