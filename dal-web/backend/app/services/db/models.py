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
    CheckConstraint,
    Float,
    ForeignKey,
    Index,
    Integer,
    LargeBinary,
    String,
    UniqueConstraint,
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
from app.services.calibration_store import (
    CalibrationInstrumentRecord,
    CalibrationRunRecord,
    CurveDefinitionRecord,
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


class CalibrationRunRow(Base):
    __tablename__ = "calibration_run"
    __table_args__ = (
        CheckConstraint(
            "kind IN ('single','xccy_staged','xccy_joint')",
            name="ck_calibration_run_kind",
        ),
        CheckConstraint(
            "status IN ('running','completed','failed')",
            name="ck_calibration_run_status",
        ),
        CheckConstraint(
            "(status = 'running' AND phase IN "
            "('queued','solving','serializing','persisting')) OR "
            "(status IN ('completed','failed') AND phase = 'finished')",
            name="ck_calibration_run_status_phase",
        ),
        CheckConstraint(
            "actual_jacobian_mode IS NULL OR actual_jacobian_mode IN ('ANALYTIC','BUMPED')",
            name="ck_calibration_run_actual_jacobian_mode",
        ),
        Index("ix_calibration_run_status_created_at", "status", "created_at"),
    )

    id: Mapped[str] = mapped_column(String(32), primary_key=True)
    schema_version: Mapped[int] = mapped_column(Integer, nullable=False)
    kind: Mapped[str] = mapped_column(String(16), nullable=False)
    name: Mapped[str] = mapped_column(String(128), nullable=False)
    status: Mapped[str] = mapped_column(String(16), nullable=False)
    phase: Mapped[str] = mapped_column(String(16), nullable=False)
    request_payload: Mapped[dict] = mapped_column(JSON, nullable=False)
    solver_payload: Mapped[dict] = mapped_column(JSON, nullable=False)
    options_payload: Mapped[dict] = mapped_column(JSON, nullable=False)
    resolved_knot_plan: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    resolved_knot_plan_hash: Mapped[str | None] = mapped_column(String(64), nullable=True)
    expected_execution_identity: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    expected_execution_identity_hash: Mapped[str | None] = mapped_column(String(64), nullable=True)
    actual_jacobian_mode: Mapped[str | None] = mapped_column(String(16), nullable=True)
    actual_execution_identity: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    actual_execution_identity_hash: Mapped[str | None] = mapped_column(String(64), nullable=True)
    result_payload: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    error_payload: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    backend: Mapped[str] = mapped_column(String(64), nullable=False)
    is_native: Mapped[bool] = mapped_column(Boolean, nullable=False)
    created_at: Mapped[str] = mapped_column(String(40), nullable=False)
    started_at: Mapped[str | None] = mapped_column(String(40), nullable=True)
    finished_at: Mapped[str | None] = mapped_column(String(40), nullable=True)
    native_solve_ms: Mapped[float | None] = mapped_column(Float, nullable=True)
    serialization_ms: Mapped[float | None] = mapped_column(Float, nullable=True)

    @classmethod
    def from_record(cls, record: CalibrationRunRecord) -> CalibrationRunRow:
        return cls(
            id=record.id,
            schema_version=record.schema_version,
            kind=record.kind,
            name=record.name,
            status=record.status,
            phase=record.phase,
            request_payload=record.request_payload,
            solver_payload=record.solver_payload,
            options_payload=record.options_payload,
            resolved_knot_plan=record.resolved_knot_plan,
            resolved_knot_plan_hash=record.resolved_knot_plan_hash,
            expected_execution_identity=record.expected_execution_identity,
            expected_execution_identity_hash=record.expected_execution_identity_hash,
            actual_jacobian_mode=record.actual_jacobian_mode,
            actual_execution_identity=record.actual_execution_identity,
            actual_execution_identity_hash=record.actual_execution_identity_hash,
            result_payload=record.result_payload,
            error_payload=record.error_payload,
            backend=record.backend,
            is_native=record.is_native,
            created_at=record.created_at.isoformat(),
            started_at=record.started_at.isoformat() if record.started_at else None,
            finished_at=record.finished_at.isoformat() if record.finished_at else None,
            native_solve_ms=record.native_solve_ms,
            serialization_ms=record.serialization_ms,
        )

    def to_record(self) -> CalibrationRunRecord:
        from datetime import datetime

        return CalibrationRunRecord(
            id=self.id,
            schema_version=self.schema_version,
            kind=self.kind,
            name=self.name,
            status=self.status,
            phase=self.phase,
            request_payload=self.request_payload,
            solver_payload=self.solver_payload,
            options_payload=self.options_payload,
            resolved_knot_plan=self.resolved_knot_plan,
            resolved_knot_plan_hash=self.resolved_knot_plan_hash,
            expected_execution_identity=self.expected_execution_identity,
            expected_execution_identity_hash=self.expected_execution_identity_hash,
            actual_jacobian_mode=self.actual_jacobian_mode,
            actual_execution_identity=self.actual_execution_identity,
            actual_execution_identity_hash=self.actual_execution_identity_hash,
            result_payload=self.result_payload,
            error_payload=self.error_payload,
            backend=self.backend,
            is_native=self.is_native,
            created_at=datetime.fromisoformat(self.created_at),
            started_at=datetime.fromisoformat(self.started_at) if self.started_at else None,
            finished_at=datetime.fromisoformat(self.finished_at) if self.finished_at else None,
            native_solve_ms=self.native_solve_ms,
            serialization_ms=self.serialization_ms,
        )


class CurveDefinitionRow(Base):
    __tablename__ = "curve_definition"
    __table_args__ = (
        CheckConstraint(
            "role IN ('discount','forward','basis','base')",
            name="ck_curve_definition_role",
        ),
        Index("ix_curve_definition_source_run_id", "source_run_id"),
        Index("ix_curve_definition_base_curve_id", "base_curve_id"),
    )

    id: Mapped[str] = mapped_column(String(32), primary_key=True)
    dto_version: Mapped[int] = mapped_column(Integer, nullable=False)
    name: Mapped[str] = mapped_column(String(128), nullable=False)
    currency: Mapped[str] = mapped_column(String(128), nullable=False)
    role: Mapped[str] = mapped_column(String(16), nullable=False)
    source_run_id: Mapped[str] = mapped_column(
        ForeignKey("calibration_run.id", ondelete="RESTRICT"), nullable=False
    )
    base_curve_id: Mapped[str | None] = mapped_column(
        ForeignKey("curve_definition.id", ondelete="RESTRICT"), nullable=True
    )
    payload: Mapped[dict] = mapped_column(JSON, nullable=False)
    created_at: Mapped[str] = mapped_column(String(40), nullable=False)

    @classmethod
    def from_record(cls, record: CurveDefinitionRecord) -> CurveDefinitionRow:
        return cls(
            id=record.id,
            dto_version=record.dto_version,
            name=record.name,
            currency=record.currency,
            role=record.role,
            source_run_id=record.source_run_id,
            base_curve_id=record.base_curve_id,
            payload=record.payload,
            created_at=record.created_at.isoformat(),
        )

    def to_record(self) -> CurveDefinitionRecord:
        from datetime import datetime

        return CurveDefinitionRecord(
            id=self.id,
            dto_version=self.dto_version,
            name=self.name,
            currency=self.currency,
            role=self.role,
            source_run_id=self.source_run_id,
            base_curve_id=self.base_curve_id,
            payload=self.payload,
            created_at=datetime.fromisoformat(self.created_at),
        )


class CalibrationInstrumentDefinitionRow(Base):
    __tablename__ = "calibration_instrument_definition"
    __table_args__ = (
        UniqueConstraint(
            "run_id",
            "group_name",
            "input_index",
            name="uq_calibration_instrument_run_group_input",
        ),
        UniqueConstraint(
            "run_id",
            "group_name",
            "calibration_index",
            name="uq_calibration_instrument_run_group_calibration",
        ),
        CheckConstraint("input_index >= 0", name="ck_calibration_instrument_input_index"),
        CheckConstraint(
            "calibration_index >= 0",
            name="ck_calibration_instrument_calibration_index",
        ),
    )

    id: Mapped[str] = mapped_column(String(32), primary_key=True)
    run_id: Mapped[str] = mapped_column(
        ForeignKey("calibration_run.id", ondelete="CASCADE"), nullable=False
    )
    group_name: Mapped[str] = mapped_column(String(160), nullable=False)
    input_index: Mapped[int] = mapped_column(Integer, nullable=False)
    calibration_index: Mapped[int] = mapped_column(Integer, nullable=False)
    kind: Mapped[str] = mapped_column(String(32), nullable=False)
    label: Mapped[str] = mapped_column(String(128), nullable=False)
    native_name: Mapped[str] = mapped_column(String(128), nullable=False)
    payload: Mapped[dict] = mapped_column(JSON, nullable=False)

    @classmethod
    def from_record(cls, record: CalibrationInstrumentRecord) -> CalibrationInstrumentDefinitionRow:
        return cls(
            id=record.id,
            run_id=record.run_id,
            group_name=record.group_name,
            input_index=record.input_index,
            calibration_index=record.calibration_index,
            kind=record.kind,
            label=record.label,
            native_name=record.native_name,
            payload=record.payload,
        )

    def to_record(self) -> CalibrationInstrumentRecord:
        return CalibrationInstrumentRecord(
            id=self.id,
            run_id=self.run_id,
            group_name=self.group_name,
            input_index=self.input_index,
            calibration_index=self.calibration_index,
            kind=self.kind,
            label=self.label,
            native_name=self.native_name,
            payload=self.payload,
        )


class CurveLabDraftRow(Base):
    __tablename__ = "curve_drafts"

    id: Mapped[str] = mapped_column(String(32), primary_key=True)
    schema_version: Mapped[int] = mapped_column(Integer, nullable=False)
    revision: Mapped[int] = mapped_column(Integer, nullable=False)
    fingerprint: Mapped[str] = mapped_column(String(64), nullable=False)
    document_json: Mapped[dict] = mapped_column(JSON, nullable=False)
    state: Mapped[str] = mapped_column(String(32), nullable=False)
    created_at: Mapped[str] = mapped_column(String(40), nullable=False)
    updated_at: Mapped[str] = mapped_column(String(40), nullable=False)


class CurveLabBuildRunRow(Base):
    __tablename__ = "curve_build_runs"

    id: Mapped[str] = mapped_column(String(32), primary_key=True)
    draft_id: Mapped[str] = mapped_column(
        ForeignKey("curve_drafts.id", ondelete="RESTRICT"), nullable=False
    )
    draft_revision: Mapped[int] = mapped_column(Integer, nullable=False)
    draft_fingerprint: Mapped[str] = mapped_column(String(64), nullable=False)
    request_json: Mapped[dict] = mapped_column(JSON, nullable=False)
    resolved_plan_json: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    quote_axis_json: Mapped[list | None] = mapped_column(JSON, nullable=True)
    parameter_axis_json: Mapped[list | None] = mapped_column(JSON, nullable=True)
    dependency_manifest_json: Mapped[list] = mapped_column(JSON, nullable=False)
    state: Mapped[str] = mapped_column(String(32), nullable=False)
    native_payload: Mapped[bytes | None] = mapped_column(LargeBinary, nullable=True)
    native_payload_hash: Mapped[str | None] = mapped_column(String(64), nullable=True)
    diagnostics_json: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    error_json: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    created_at: Mapped[str] = mapped_column(String(40), nullable=False)
    deadline_at: Mapped[str] = mapped_column(String(40), nullable=False)
    finished_at: Mapped[str | None] = mapped_column(String(40), nullable=True)


class CurveLabVersionRow(Base):
    __tablename__ = "curve_versions"
    __table_args__ = (UniqueConstraint("idempotency_key", name="uq_curve_version_idempotency_key"),)

    id: Mapped[str] = mapped_column(String(32), primary_key=True)
    idempotency_key: Mapped[str] = mapped_column(String(256), nullable=False)
    source_kind: Mapped[str] = mapped_column(String(16), nullable=False)
    build_run_id: Mapped[str | None] = mapped_column(
        ForeignKey("curve_build_runs.id", ondelete="RESTRICT"), nullable=True
    )
    import_job_id: Mapped[str | None] = mapped_column(String(32), nullable=True)
    native_payload: Mapped[bytes] = mapped_column(LargeBinary, nullable=False)
    native_payload_length: Mapped[int] = mapped_column(Integer, nullable=False)
    native_payload_hash: Mapped[str] = mapped_column(String(64), nullable=False)
    archive_numeric_format: Mapped[str] = mapped_column(String(32), nullable=False)
    root_kind: Mapped[str] = mapped_column(String(32), nullable=False)
    build_validation_state: Mapped[str] = mapped_column(String(32), nullable=False)
    visibility_state: Mapped[str] = mapped_column(String(16), nullable=False)
    metadata_json: Mapped[dict] = mapped_column(JSON, nullable=False)
    verification_json: Mapped[dict] = mapped_column(JSON, nullable=False)
    created_at: Mapped[str] = mapped_column(String(40), nullable=False)


class CurveLabImportJobRow(Base):
    __tablename__ = "curve_import_jobs"

    id: Mapped[str] = mapped_column(String(32), primary_key=True)
    request_hash: Mapped[str] = mapped_column(String(64), nullable=False)
    compressed_payload_length: Mapped[int] = mapped_column(Integer, nullable=False)
    expanded_payload_length: Mapped[int] = mapped_column(Integer, nullable=False)
    state: Mapped[str] = mapped_column(String(16), nullable=False)
    phase: Mapped[str] = mapped_column(String(64), nullable=False)
    error_json: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    resulting_version_id: Mapped[str | None] = mapped_column(String(32), nullable=True)
    created_at: Mapped[str] = mapped_column(String(40), nullable=False)
    deadline_at: Mapped[str] = mapped_column(String(40), nullable=False)
    finished_at: Mapped[str | None] = mapped_column(String(40), nullable=True)


class CurveLabFixingSnapshotRow(Base):
    __tablename__ = "curve_fixing_snapshots"

    id: Mapped[str] = mapped_column(String(256), primary_key=True)
    observations_json: Mapped[list] = mapped_column(JSON, nullable=False)
    content_hash: Mapped[str] = mapped_column(String(64), nullable=False)
    created_at: Mapped[str] = mapped_column(String(40), nullable=False)


class CurveLabRiskRunRow(Base):
    __tablename__ = "curve_risk_runs"

    id: Mapped[str] = mapped_column(String(32), primary_key=True)
    curve_version_id: Mapped[str] = mapped_column(
        ForeignKey("curve_versions.id", ondelete="RESTRICT"), nullable=False
    )
    calibration_run_id: Mapped[str | None] = mapped_column(String(32), nullable=True)
    import_job_id: Mapped[str | None] = mapped_column(String(32), nullable=True)
    source_kind: Mapped[str] = mapped_column(String(32), nullable=False)
    request_json: Mapped[dict] = mapped_column(JSON, nullable=False)
    fixing_snapshot_hash: Mapped[str] = mapped_column(String(64), nullable=False)
    target_fingerprint: Mapped[str] = mapped_column(String(64), nullable=False)
    quote_axis_json: Mapped[list | None] = mapped_column(JSON, nullable=True)
    parameter_axis_json: Mapped[list] = mapped_column(JSON, nullable=False)
    estimated_work_json: Mapped[dict] = mapped_column(JSON, nullable=False)
    state: Mapped[str] = mapped_column(String(32), nullable=False)
    result_json: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    error_json: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    created_at: Mapped[str] = mapped_column(String(40), nullable=False)
    deadline_at: Mapped[str] = mapped_column(String(40), nullable=False)
    finished_at: Mapped[str | None] = mapped_column(String(40), nullable=True)


class CurveLabMatrixBlobRow(Base):
    __tablename__ = "curve_matrix_blobs"

    risk_run_id: Mapped[str] = mapped_column(
        ForeignKey("curve_risk_runs.id", ondelete="CASCADE"), primary_key=True
    )
    matrix_id: Mapped[str] = mapped_column(String(128), primary_key=True)
    envelope_json: Mapped[dict] = mapped_column(JSON, nullable=False)
    values_blob: Mapped[bytes | None] = mapped_column(LargeBinary, nullable=True)


class CurveLabAuditEventRow(Base):
    __tablename__ = "curve_audit_events"

    id: Mapped[str] = mapped_column(String(32), primary_key=True)
    action: Mapped[str] = mapped_column(String(64), nullable=False)
    actor: Mapped[str] = mapped_column(String(128), nullable=False)
    target_type: Mapped[str] = mapped_column(String(64), nullable=False)
    target_id: Mapped[str] = mapped_column(String(32), nullable=False)
    input_hash: Mapped[str] = mapped_column(String(64), nullable=False)
    outcome: Mapped[str] = mapped_column(String(32), nullable=False)
    details_json: Mapped[dict] = mapped_column(JSON, nullable=False)
    created_at: Mapped[str] = mapped_column(String(40), nullable=False)
