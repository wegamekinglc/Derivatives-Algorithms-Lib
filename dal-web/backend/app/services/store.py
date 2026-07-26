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
from copy import deepcopy
from datetime import datetime
from typing import Protocol, runtime_checkable

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

    # calibrations
    def add_calibration_admission(
        self,
        run: CalibrationRunRecord,
        instruments: tuple[CalibrationInstrumentRecord, ...],
    ) -> CalibrationRunRecord: ...
    def get_calibration_run(self, calibration_id: str) -> CalibrationRunRecord: ...
    def list_running_calibrations(self) -> list[CalibrationRunRecord]: ...
    def list_calibration_instruments(
        self, calibration_id: str
    ) -> list[CalibrationInstrumentRecord]: ...
    def mark_calibration_solving(
        self, calibration_id: str, started_at: datetime
    ) -> None: ...
    def update_calibration_phase(self, calibration_id: str, phase: str) -> None: ...
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
    ) -> None: ...
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
    ) -> None: ...
    def load_single_worker_admission_evidence(
        self, calibration_id: str
    ) -> RawSingleWorkerAdmissionEvidence: ...
    def fail_knot_plan_integrity(
        self,
        calibration_id: str,
        finished_at: datetime,
        canonical_error_utf8: bytes,
    ) -> None: ...
    def fail_expected_execution_identity_integrity(
        self,
        calibration_id: str,
        finished_at: datetime,
        canonical_error_utf8: bytes,
    ) -> None: ...
    def get_curve_definition(self, curve_id: str) -> CurveDefinitionRecord: ...


class Store:
    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._products: dict[str, ProductDefinition] = {}
        self._models: dict[str, ModelDefinition] = {}
        self._trades: dict[str, Trade] = {}
        self._portfolios: dict[str, Portfolio] = {}
        self._valuations: dict[str, ValuationResult] = {}
        self._calibration_runs: dict[str, CalibrationRunRecord] = {}
        self._calibration_instruments: dict[str, list[CalibrationInstrumentRecord]] = {}
        self._curve_definitions: dict[str, CurveDefinitionRecord] = {}

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
            return sorted(self._valuations.values(), key=lambda r: r.created_at, reverse=True)

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

    # -- curve calibration ----------------------------------------------

    def add_calibration_admission(
        self,
        run: CalibrationRunRecord,
        instruments: tuple[CalibrationInstrumentRecord, ...],
    ) -> CalibrationRunRecord:
        with self._lock:
            stored = deepcopy(run)
            self._calibration_runs[run.id] = stored
            self._calibration_instruments[run.id] = deepcopy(list(instruments))
            return deepcopy(stored)

    def get_calibration_run(self, calibration_id: str) -> CalibrationRunRecord:
        with self._lock:
            try:
                return deepcopy(self._calibration_runs[calibration_id])
            except KeyError as exc:
                raise NotFoundError(f"calibration {calibration_id}") from exc

    def list_running_calibrations(self) -> list[CalibrationRunRecord]:
        with self._lock:
            return deepcopy(
                sorted(
                    (
                        run
                        for run in self._calibration_runs.values()
                        if run.status == "running"
                    ),
                    key=lambda run: run.created_at,
                )
            )

    def list_calibration_instruments(
        self, calibration_id: str
    ) -> list[CalibrationInstrumentRecord]:
        with self._lock:
            if calibration_id not in self._calibration_runs:
                raise NotFoundError(f"calibration {calibration_id}")
            return deepcopy(
                sorted(
                    self._calibration_instruments.get(calibration_id, []),
                    key=lambda item: (item.group_name, item.calibration_index),
                )
            )

    def mark_calibration_solving(
        self, calibration_id: str, started_at: datetime
    ) -> None:
        self._update_calibration(
            calibration_id, phase="solving", started_at=started_at
        )

    def update_calibration_phase(self, calibration_id: str, phase: str) -> None:
        self._update_calibration(calibration_id, phase=phase)

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
        with self._lock:
            run = self.get_calibration_run(calibration_id)
            new_curve_ids = {curve.id for curve in curves}
            if new_curve_ids & self._curve_definitions.keys():
                raise ConflictError("curve definition already exists")
            from dataclasses import replace

            updated = replace(
                run,
                status="completed",
                phase="finished",
                result_payload=deepcopy(result_payload),
                error_payload=None,
                actual_jacobian_mode=actual_jacobian_mode,
                actual_execution_identity=deepcopy(actual_execution_identity),
                actual_execution_identity_hash=actual_execution_identity_hash,
                native_solve_ms=native_solve_ms,
                serialization_ms=serialization_ms,
                finished_at=finished_at,
            )
            for curve in curves:
                self._curve_definitions[curve.id] = deepcopy(curve)
            self._calibration_runs[calibration_id] = updated

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
        self._update_calibration(
            calibration_id,
            status="failed",
            phase="finished",
            finished_at=finished_at,
            result_payload=None,
            error_payload=deepcopy(error_payload),
            actual_jacobian_mode=actual_jacobian_mode,
            actual_execution_identity=deepcopy(actual_execution_identity),
            actual_execution_identity_hash=actual_execution_identity_hash,
            native_solve_ms=native_solve_ms,
            serialization_ms=serialization_ms,
        )

    def load_single_worker_admission_evidence(
        self, calibration_id: str
    ) -> RawSingleWorkerAdmissionEvidence:
        run = self.get_calibration_run(calibration_id)
        if (
            run.resolved_knot_plan is None
            or run.resolved_knot_plan_hash is None
            or run.expected_execution_identity is None
            or run.expected_execution_identity_hash is None
        ):
            raise ValueError("single calibration admission evidence is incomplete")
        return RawSingleWorkerAdmissionEvidence(
            resolved_knot_plan_raw=run.resolved_knot_plan,
            resolved_knot_plan_hash=run.resolved_knot_plan_hash,
            expected_execution_identity_raw=run.expected_execution_identity,
            expected_execution_identity_hash=run.expected_execution_identity_hash,
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
        with self._lock:
            try:
                return deepcopy(self._curve_definitions[curve_id])
            except KeyError as exc:
                raise NotFoundError(f"curve {curve_id}") from exc

    def _update_calibration(self, calibration_id: str, **patch: object) -> None:
        from dataclasses import replace

        with self._lock:
            try:
                run = self._calibration_runs[calibration_id]
            except KeyError as exc:
                raise NotFoundError(f"calibration {calibration_id}") from exc
            self._calibration_runs[calibration_id] = replace(run, **patch)

    def _fail_integrity(
        self,
        calibration_id: str,
        finished_at: datetime,
        canonical_error_utf8: bytes,
    ) -> None:
        import json

        parsed = json.loads(canonical_error_utf8)
        if not isinstance(parsed, dict):
            raise ValueError("integrity error evidence must encode one JSON object")
        if canonical_json_bytes(parsed) != canonical_error_utf8:
            raise ValueError("integrity error evidence is not canonical JSON")
        self._update_calibration(
            calibration_id,
            status="failed",
            phase="finished",
            finished_at=finished_at,
            result_payload=None,
            error_payload=parsed,
        )


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
    """Construct a fresh store based on the current environment.

    Schema creation is deliberately *not* done here -- it lives in
    :func:`app.main._init_database`, so ``DAL_WEB_AUTO_MIGRATE=1`` can build the
    schema via Alembic without ``create_all()`` having already populated it.
    """
    if is_memory_mode():
        return Store()
    # Imported lazily so the in-memory path never depends on SQLAlchemy.
    from app.services.db.session import default_db_url
    from app.services.db.store_db import DbStore

    url = os.environ.get("DAL_WEB_DB_URL") or default_db_url()
    return DbStore(url=url)
