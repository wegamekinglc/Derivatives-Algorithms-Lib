"""Database-backed implementation of the ``Store`` surface.

``DbStore`` mirrors the public methods of the in-memory :class:`Store` but
persists every entity to a SQLAlchemy database. Each method opens a short-lived
session, does its work, commits, and closes -- no session is ever held across
the ``asyncio.to_thread`` boundary used by the async valuation path, which is
what makes this safe to use as a drop-in replacement for ``Store``.
"""

from __future__ import annotations

import json
from datetime import datetime

from sqlalchemy import select, update
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session, sessionmaker

from app.schemas import (
    ModelDefinition,
    Portfolio,
    ProductDefinition,
    Trade,
    ValuationResult,
)
from app.services.calibration_store import (
    CalibrationInstrumentRecord,
    CalibrationRunRecord,
    CurveDefinitionRecord,
    RawSingleWorkerAdmissionEvidence,
)
from app.services.calibrations import canonical_json_bytes
from app.services.db.models import (
    Base,
    CalibrationInstrumentDefinitionRow,
    CalibrationRunRow,
    CurveDefinitionRow,
    CurveLabAuditEventRow,
    CurveLabBuildRunRow,
    CurveLabDraftRow,
    CurveLabImportJobRow,
    CurveLabVersionRow,
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

    @staticmethod
    def _curve_lab_draft_dict(row: CurveLabDraftRow) -> dict:
        return {
            "id": row.id,
            "schema_version": row.schema_version,
            "revision": row.revision,
            "fingerprint": row.fingerprint,
            "state": row.state,
            "document": row.document_json,
            "created_at": row.created_at,
            "updated_at": row.updated_at,
        }

    @staticmethod
    def _curve_lab_build_dict(row: CurveLabBuildRunRow) -> dict:
        return {
            "id": row.id,
            "draft_id": row.draft_id,
            "draft_revision": row.draft_revision,
            "draft_fingerprint": row.draft_fingerprint,
            "state": row.state,
            "request": row.request_json,
            "native_payload": row.native_payload,
            "native_payload_hash": row.native_payload_hash,
            "error": row.error_json,
            "created_at": row.created_at,
            "finished_at": row.finished_at,
        }

    @staticmethod
    def _curve_lab_version_dict(row: CurveLabVersionRow) -> dict:
        metadata = row.metadata_json
        return {
            "id": row.id,
            "idempotency_key": row.idempotency_key,
            "source_kind": row.source_kind,
            "build_run_id": row.build_run_id,
            "import_job_id": row.import_job_id,
            "native_payload": row.native_payload,
            "native_payload_length": row.native_payload_length,
            "native_payload_hash": row.native_payload_hash,
            "archive_numeric_format": row.archive_numeric_format,
            "root_kind": row.root_kind,
            "build_validation_state": row.build_validation_state,
            "visibility_state": row.visibility_state,
            "name": metadata["name"],
            "version_note": metadata.get("version_note"),
            "tags": metadata.get("tags", []),
            "verification": row.verification_json,
            "created_at": row.created_at,
        }

    @staticmethod
    def _curve_lab_version_row(record: dict) -> CurveLabVersionRow:
        return CurveLabVersionRow(
            id=record["id"],
            idempotency_key=record["idempotency_key"],
            source_kind=record["source_kind"],
            build_run_id=record.get("build_run_id"),
            import_job_id=record.get("import_job_id"),
            native_payload=record["native_payload"],
            native_payload_length=record["native_payload_length"],
            native_payload_hash=record["native_payload_hash"],
            archive_numeric_format=record["archive_numeric_format"],
            root_kind=record["root_kind"],
            build_validation_state=record["build_validation_state"],
            visibility_state=record["visibility_state"],
            metadata_json={
                "name": record["name"],
                "version_note": record.get("version_note"),
                "tags": record.get("tags", []),
            },
            verification_json=record.get("verification", {}),
            created_at=record["created_at"],
        )

    @staticmethod
    def _curve_lab_import_job_row(record: dict) -> CurveLabImportJobRow:
        return CurveLabImportJobRow(
            id=record["id"],
            request_hash=record["request_hash"],
            compressed_payload_length=record["compressed_payload_length"],
            expanded_payload_length=record["expanded_payload_length"],
            state=record["state"],
            phase=record["phase"],
            error_json=record.get("error"),
            resulting_version_id=record.get("resulting_version_id"),
            created_at=record["created_at"],
            finished_at=record.get("finished_at"),
        )

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
            rows = session.scalars(select(TradeRow).where(TradeRow.id.in_(trade_ids))).all()
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
            # Mutate the relationship collection rather than session.delete():
            # cascade="all, delete-orphan" drops the row, and under
            # expire_on_commit=False the in-memory collection stays consistent
            # so to_schema() no longer reports the removed trade.
            for m in [m for m in pf.memberships if m.trade_id == trade_id]:
                pf.memberships.remove(m)
            for position, m in enumerate(pf.memberships):
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

    # -- curve calibration ----------------------------------------------

    def add_calibration_admission(
        self,
        run: CalibrationRunRecord,
        instruments: tuple[CalibrationInstrumentRecord, ...],
    ) -> CalibrationRunRecord:
        """Persist the admitted run and all normalized instruments atomically."""
        with self._session() as session:
            session.add(CalibrationRunRow.from_record(run))
            session.flush()
            session.add_all(
                CalibrationInstrumentDefinitionRow.from_record(instrument)
                for instrument in instruments
            )
            session.commit()
            return run

    def get_calibration_run(self, calibration_id: str) -> CalibrationRunRecord:
        with self._session() as session:
            row = session.get(CalibrationRunRow, calibration_id)
            if row is None:
                raise NotFoundError(f"calibration {calibration_id}")
            return row.to_record()

    def list_running_calibrations(self) -> list[CalibrationRunRecord]:
        with self._session() as session:
            rows = session.scalars(
                select(CalibrationRunRow)
                .where(CalibrationRunRow.status == "running")
                .order_by(CalibrationRunRow.created_at)
            ).all()
            return [row.to_record() for row in rows]

    def list_calibration_instruments(
        self, calibration_id: str
    ) -> list[CalibrationInstrumentRecord]:
        with self._session() as session:
            rows = session.scalars(
                select(CalibrationInstrumentDefinitionRow)
                .where(CalibrationInstrumentDefinitionRow.run_id == calibration_id)
                .order_by(
                    CalibrationInstrumentDefinitionRow.group_name,
                    CalibrationInstrumentDefinitionRow.calibration_index,
                )
            ).all()
            return [row.to_record() for row in rows]

    def mark_calibration_solving(
        self, calibration_id: str, started_at: datetime
    ) -> None:
        with self._session() as session:
            row = self._require_calibration_row(session, calibration_id)
            row.phase = "solving"
            row.started_at = started_at.isoformat()
            session.commit()

    def update_calibration_phase(self, calibration_id: str, phase: str) -> None:
        with self._session() as session:
            row = self._require_calibration_row(session, calibration_id)
            row.phase = phase
            session.commit()

    def complete_calibration(
        self,
        calibration_id: str,
        *,
        result_payload: dict,
        curves: tuple[CurveDefinitionRecord, ...],
        actual_jacobian_mode: str,
        actual_execution_identity: dict | None,
        actual_execution_identity_hash: str | None,
        native_solve_ms: float,
        serialization_ms: float,
        finished_at: datetime,
    ) -> None:
        """Insert output curves and terminalize success in one transaction."""
        with self._session() as session:
            row = self._require_calibration_row(session, calibration_id)
            for curve in curves:
                session.add(CurveDefinitionRow.from_record(curve))
                session.flush()
            row.result_payload = result_payload
            row.error_payload = None
            row.actual_jacobian_mode = actual_jacobian_mode
            row.actual_execution_identity = actual_execution_identity
            row.actual_execution_identity_hash = actual_execution_identity_hash
            row.native_solve_ms = native_solve_ms
            row.serialization_ms = serialization_ms
            row.finished_at = finished_at.isoformat()
            row.status = "completed"
            row.phase = "finished"
            session.commit()

    def fail_calibration(
        self,
        calibration_id: str,
        *,
        error_payload: dict,
        finished_at: datetime,
        actual_jacobian_mode: str | None = None,
        actual_execution_identity: dict | None = None,
        actual_execution_identity_hash: str | None = None,
        native_solve_ms: float | None = None,
        serialization_ms: float | None = None,
    ) -> None:
        with self._session() as session:
            row = self._require_calibration_row(session, calibration_id)
            row.status = "failed"
            row.phase = "finished"
            row.finished_at = finished_at.isoformat()
            row.error_payload = error_payload
            row.result_payload = None
            row.actual_jacobian_mode = actual_jacobian_mode
            row.actual_execution_identity = actual_execution_identity
            row.actual_execution_identity_hash = actual_execution_identity_hash
            row.native_solve_ms = native_solve_ms
            row.serialization_ms = serialization_ms
            session.commit()

    def load_single_worker_admission_evidence(
        self, calibration_id: str
    ) -> RawSingleWorkerAdmissionEvidence:
        with self._session() as session:
            row = self._require_calibration_row(session, calibration_id)
            if (
                row.resolved_knot_plan is None
                or row.resolved_knot_plan_hash is None
                or row.expected_execution_identity is None
                or row.expected_execution_identity_hash is None
            ):
                raise ValueError("single calibration admission evidence is incomplete")
            return RawSingleWorkerAdmissionEvidence(
                resolved_knot_plan_raw=row.resolved_knot_plan,
                resolved_knot_plan_hash=row.resolved_knot_plan_hash,
                expected_execution_identity_raw=row.expected_execution_identity,
                expected_execution_identity_hash=row.expected_execution_identity_hash,
            )

    def fail_knot_plan_integrity(
        self,
        calibration_id: str,
        finished_at: datetime,
        canonical_error_utf8: bytes,
    ) -> None:
        self._fail_integrity(calibration_id, finished_at, canonical_error_utf8)

    def fail_expected_execution_identity_integrity(
        self,
        calibration_id: str,
        finished_at: datetime,
        canonical_error_utf8: bytes,
    ) -> None:
        self._fail_integrity(calibration_id, finished_at, canonical_error_utf8)

    def get_curve_definition(self, curve_id: str) -> CurveDefinitionRecord:
        with self._session() as session:
            row = session.get(CurveDefinitionRow, curve_id)
            if row is None:
                raise NotFoundError(f"curve {curve_id}")
            return row.to_record()

    # -- Curve Lab V2 ---------------------------------------------------

    def add_curve_lab_draft(self, record: dict) -> dict:
        with self._session() as session:
            session.add(
                CurveLabDraftRow(
                    id=record["id"],
                    schema_version=record["schema_version"],
                    revision=record["revision"],
                    fingerprint=record["fingerprint"],
                    document_json=record["document"],
                    state=record["state"],
                    created_at=record["created_at"],
                    updated_at=record["updated_at"],
                )
            )
            session.commit()
            return record

    def get_curve_lab_draft(self, draft_id: str) -> dict:
        with self._session() as session:
            row = session.get(CurveLabDraftRow, draft_id)
            if row is None:
                raise NotFoundError(f"curve draft {draft_id}")
            return self._curve_lab_draft_dict(row)

    def update_curve_lab_draft(
        self, draft_id: str, expected_revision: int, record: dict
    ) -> dict:
        with self._session() as session:
            result = session.execute(
                update(CurveLabDraftRow)
                .where(
                    CurveLabDraftRow.id == draft_id,
                    CurveLabDraftRow.revision == expected_revision,
                )
                .values(
                    schema_version=record["schema_version"],
                    revision=record["revision"],
                    fingerprint=record["fingerprint"],
                    document_json=record["document"],
                    state=record["state"],
                    updated_at=record["updated_at"],
                )
            )
            if result.rowcount != 1:
                session.rollback()
                exists = session.scalar(
                    select(CurveLabDraftRow.id).where(
                        CurveLabDraftRow.id == draft_id
                    )
                )
                if exists is None:
                    raise NotFoundError(f"curve draft {draft_id}")
                raise ConflictError(
                    f"draft revision no longer equals {expected_revision}"
                )
            session.commit()
            return record

    def add_curve_lab_build_run(self, record: dict) -> dict:
        with self._session() as session:
            session.add(
                CurveLabBuildRunRow(
                    id=record["id"],
                    draft_id=record["draft_id"],
                    draft_revision=record["draft_revision"],
                    draft_fingerprint=record["draft_fingerprint"],
                    request_json=record["request"],
                    resolved_plan_json=record.get("resolved_plan"),
                    quote_axis_json=record.get("quote_axis"),
                    parameter_axis_json=record.get("parameter_axis"),
                    dependency_manifest_json=record.get("dependency_manifest", []),
                    state=record["state"],
                    native_payload=record.get("native_payload"),
                    native_payload_hash=record.get("native_payload_hash"),
                    diagnostics_json=record.get("diagnostics"),
                    error_json=record.get("error"),
                    created_at=record["created_at"],
                    finished_at=record.get("finished_at"),
                )
            )
            session.commit()
            return record

    def get_curve_lab_build_run(self, run_id: str) -> dict:
        with self._session() as session:
            row = session.get(CurveLabBuildRunRow, run_id)
            if row is None:
                raise NotFoundError(f"curve build run {run_id}")
            return self._curve_lab_build_dict(row)

    def add_curve_lab_version(self, record: dict) -> tuple[dict, bool]:
        with self._session() as session:
            existing = session.scalar(
                select(CurveLabVersionRow).where(
                    CurveLabVersionRow.idempotency_key == record["idempotency_key"]
                )
            )
            if existing is not None:
                return self._curve_lab_version_dict(existing), False
            session.add(self._curve_lab_version_row(record))
            try:
                session.commit()
            except IntegrityError:
                session.rollback()
                existing = session.scalar(
                    select(CurveLabVersionRow).where(
                        CurveLabVersionRow.idempotency_key
                        == record["idempotency_key"]
                    )
                )
                if existing is None:
                    raise
                return self._curve_lab_version_dict(existing), False
            return record, True

    def publish_curve_lab_version(
        self,
        record: dict,
        draft_id: str,
        draft_revision: int,
        draft_fingerprint: str,
        build_run_id: str,
    ) -> tuple[dict, bool]:
        with self._session() as session:
            draft = session.scalar(
                select(CurveLabDraftRow)
                .where(CurveLabDraftRow.id == draft_id)
                .with_for_update()
            )
            run = session.scalar(
                select(CurveLabBuildRunRow)
                .where(CurveLabBuildRunRow.id == build_run_id)
                .with_for_update()
            )
            if draft is None or run is None:
                raise NotFoundError("curve draft or build run")
            if (
                draft.revision != draft_revision
                or draft.fingerprint != draft_fingerprint
                or run.state != "SUCCEEDED"
                or run.draft_id != draft_id
                or run.draft_revision != draft_revision
                or run.draft_fingerprint != draft_fingerprint
            ):
                raise ConflictError("curve version publication CAS failed")
            existing = session.scalar(
                select(CurveLabVersionRow).where(
                    CurveLabVersionRow.idempotency_key == record["idempotency_key"]
                )
            )
            if existing is not None:
                return self._curve_lab_version_dict(existing), False
            session.add(self._curve_lab_version_row(record))
            try:
                session.commit()
            except IntegrityError:
                session.rollback()
                existing = session.scalar(
                    select(CurveLabVersionRow).where(
                        CurveLabVersionRow.idempotency_key
                        == record["idempotency_key"]
                    )
                )
                if existing is None:
                    raise
                return self._curve_lab_version_dict(existing), False
            return record, True

    def get_curve_lab_version(self, version_id: str) -> dict:
        with self._session() as session:
            row = session.get(CurveLabVersionRow, version_id)
            if row is None:
                raise NotFoundError(f"curve version {version_id}")
            return self._curve_lab_version_dict(row)

    def list_curve_lab_versions(self, include_archived: bool) -> list[dict]:
        with self._session() as session:
            statement = select(CurveLabVersionRow).order_by(
                CurveLabVersionRow.created_at
            )
            if not include_archived:
                statement = statement.where(
                    CurveLabVersionRow.visibility_state == "VISIBLE"
                )
            return [
                self._curve_lab_version_dict(row)
                for row in session.scalars(statement).all()
            ]

    def archive_curve_lab_version(self, version_id: str) -> dict:
        with self._session() as session:
            row = session.get(CurveLabVersionRow, version_id)
            if row is None:
                raise NotFoundError(f"curve version {version_id}")
            row.visibility_state = "ARCHIVED"
            session.commit()
            return self._curve_lab_version_dict(row)

    def add_curve_lab_import_job(self, record: dict) -> dict:
        with self._session() as session:
            session.add(self._curve_lab_import_job_row(record))
            session.commit()
            return record

    def publish_curve_lab_import(
        self, version_record: dict, job_record: dict
    ) -> tuple[dict, dict]:
        with self._session() as session:
            existing = session.scalar(
                select(CurveLabVersionRow).where(
                    CurveLabVersionRow.idempotency_key
                    == version_record["idempotency_key"]
                )
            )
            if existing is None:
                version = self._curve_lab_version_row(version_record)
                session.add(version)
            else:
                version = existing
            stored_job = {
                **job_record,
                "resulting_version_id": version.id,
            }
            session.add(self._curve_lab_import_job_row(stored_job))
            session.commit()
            return self._curve_lab_version_dict(version), stored_job

    def add_curve_lab_audit_event(self, record: dict) -> None:
        with self._session() as session:
            session.add(
                CurveLabAuditEventRow(
                    id=record["id"],
                    action=record["action"],
                    actor=record["actor"],
                    target_type=record["target_type"],
                    target_id=record["target_id"],
                    input_hash=record["input_hash"],
                    outcome=record["outcome"],
                    details_json=record.get("details", {}),
                    created_at=record["created_at"],
                )
            )
            session.commit()

    @staticmethod
    def _require_calibration_row(
        session: Session, calibration_id: str
    ) -> CalibrationRunRow:
        row = session.get(CalibrationRunRow, calibration_id)
        if row is None:
            raise NotFoundError(f"calibration {calibration_id}")
        return row

    def _fail_integrity(
        self,
        calibration_id: str,
        finished_at: datetime,
        canonical_error_utf8: bytes,
    ) -> None:
        """Parse and commit original canonical evidence in one short transaction."""
        parsed = json.loads(canonical_error_utf8)
        if not isinstance(parsed, dict):
            raise ValueError("integrity error evidence must encode one JSON object")
        if canonical_json_bytes(parsed) != canonical_error_utf8:
            raise ValueError("integrity error evidence is not canonical JSON")
        with self._session() as session:
            row = self._require_calibration_row(session, calibration_id)
            row.status = "failed"
            row.phase = "finished"
            row.finished_at = finished_at.isoformat()
            row.error_payload = parsed
            row.result_payload = None
            session.commit()
