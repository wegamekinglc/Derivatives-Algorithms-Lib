"""The one and only integration surface with the DAL library.

The web backend talks to the Derivatives Algorithms Library exclusively through
its Python public API -- the compiled ``dal`` package (the dal-python pybind11
bindings; see ``dal-python/src/bindings/value.cpp``).  No router, schema or
service module imports ``dal`` directly -- they all go through :class:`DalGateway`.
"""

from __future__ import annotations

import threading
import time
from collections.abc import Callable, Mapping
from dataclasses import dataclass
from datetime import UTC, date, datetime
from typing import Any, NamedTuple

from app.native_runtime import load_native_dal
from app.schemas.calibrations import (
    ExecutionSingleKnotIdentityDTO,
    FreeParameterDTO,
    FxForwardDTO,
    InstrumentDiagnosticDTO,
    KnotCandidateDTO,
    MatrixDTO,
    NamedRangeDTO,
    NamedRangesDTO,
    ResolvedKnotNodeDTO,
    ResolvedSingleKnotPlanDTO,
    SolverDiagnosticsDTO,
)
from app.services.calibrations import (
    NativeExecutionIdentityMismatchError,
    NativeSolverDidNotConvergeError,
    PersistedExpectedExecutionIdentityIntegrityError,
    PersistedKnotPlanIntegrityError,
    SingleGatewayPreLockRequest,
    VerifiedSingleGatewayRequest,
    VerifiedSingleWorkerAdmissionEvidence,
)

dal = load_native_dal()

_NATIVE_RNG_METHODS = {
    "sobol": "sobol",
    "pseudo": "mrg32",
}


@dataclass(frozen=True)
class ValuationRequest:
    """Normalised inputs for a Monte Carlo valuation."""

    event_dates: list[Any]
    events: list[str]
    model_kind: str
    model_params: dict[str, Any]
    num_paths: int = 1 << 16
    method: str = "sobol"
    use_brownian_bridge: bool = False
    enable_aad: bool = True
    smooth: float = 0.01
    evaluation_date: tuple[int, int, int] | None = None


class SingleGatewayAdmissionRequest(NamedTuple):
    request: object
    referenced_curves: Mapping[str, object]


@dataclass(frozen=True, slots=True)
class GatewayResolvedKnotCounts:
    submitted_knots: int
    instrument_candidates: int
    resolved_declared_nodes: int
    storage_nodes: int
    free_parameters: int


@dataclass(frozen=True, slots=True)
class GatewayRequiredHistoricalFixing:
    instrument_index: int
    index_name: str
    timestamp: datetime


@dataclass(frozen=True, slots=True)
class GatewayResolvedSingleKnotPlan:
    """Unbounded pre-admission carrier; bounded only after semantic gates."""

    planner_version: int
    requested_policy: str
    execution_policy: str
    submitted_knot_dates: tuple[date, ...]
    candidate_trace: tuple[KnotCandidateDTO, ...]
    resolved_declared_nodes: tuple[ResolvedKnotNodeDTO, ...]
    storage_nodes: tuple[ResolvedKnotNodeDTO, ...]
    free_parameters: tuple[FreeParameterDTO, ...]
    anchor_added: bool
    counts: GatewayResolvedKnotCounts

    def to_bounded_dto(self) -> ResolvedSingleKnotPlanDTO:
        return ResolvedSingleKnotPlanDTO.model_validate(
            {
                "planner_version": self.planner_version,
                "requested_policy": self.requested_policy,
                "execution_policy": self.execution_policy,
                "submitted_knot_dates": self.submitted_knot_dates,
                "candidate_trace": self.candidate_trace,
                "resolved_declared_nodes": self.resolved_declared_nodes,
                "storage_nodes": self.storage_nodes,
                "free_parameters": self.free_parameters,
                "anchor_added": self.anchor_added,
                "counts": {
                    "submitted_knots": self.counts.submitted_knots,
                    "instrument_candidates": self.counts.instrument_candidates,
                    "resolved_declared_nodes": (self.counts.resolved_declared_nodes),
                    "storage_nodes": self.counts.storage_nodes,
                    "free_parameters": self.counts.free_parameters,
                },
            }
        )


class SingleGatewayAdmissionResult(NamedTuple):
    resolved_knot_plan: GatewayResolvedSingleKnotPlan
    native_names_by_input: tuple[str, ...]
    analytic_eligibility: object | None
    latest_instrument_end: date
    resolved_initial_guess_per_node: tuple[float, ...]


class StagedXccyGatewayRequest(NamedTuple):
    request: object
    referenced_curves: Mapping[str, object]


class JointXccyGatewayRequest(NamedTuple):
    request: object


class GatewayCalibrationResult(NamedTuple):
    actual_jacobian_mode: str
    actual_execution_identity: ExecutionSingleKnotIdentityDTO | None
    curves: tuple[dict[str, object], ...]
    instrument_diagnostics: tuple[InstrumentDiagnosticDTO, ...]
    solver_diagnostics: SolverDiagnosticsDTO
    fx_forwards: FxForwardDTO | None
    named_ranges: NamedRangesDTO
    jacobian: MatrixDTO
    effective_inverse: MatrixDTO
    native_solve_ms: float


@dataclass(frozen=True, slots=True)
class HealthSnapshot:
    backend: str
    is_native: bool
    evaluation_date: str | None


class GatewayLifecycleTransitionError(Exception):
    def __init__(self, transition: str) -> None:
        super().__init__(transition)
        self.transition = transition


class DalGateway:
    """Thin, thread-safe adapter over the DAL public Python API."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._calibration_lock = self._lock
        self._health_lock = threading.Lock()
        self._dal = dal
        self._health_snapshot = HealthSnapshot(
            backend="dal",
            is_native=True,
            evaluation_date=repr(self._dal.EvaluationDate_Get()),
        )

    @property
    def is_native(self) -> bool:
        return True

    @property
    def backend_name(self) -> str:
        return "dal"

    # -- primitive constructors ------------------------------------------

    def make_date(self, year: int, month: int, day: int) -> Any:
        return self._dal.Date_(year, month, day)

    def _to_cell(self, value: Any) -> Any:
        """Wrap a python value as a ``dal.Cell_`` unless it already is one."""
        cell_cls = self._dal.Cell_
        if isinstance(value, cell_cls):
            return value
        return cell_cls(value)

    def set_evaluation_date(self, year: int, month: int, day: int) -> None:
        self._dal.EvaluationDate_Set(self.make_date(year, month, day))

    def get_evaluation_date(self) -> str:
        return repr(self._dal.EvaluationDate_Get())

    def health_snapshot(self) -> HealthSnapshot:
        with self._health_lock:
            return self._health_snapshot

    def _refresh_health_snapshot(self) -> None:
        snapshot = HealthSnapshot(
            backend=self.backend_name,
            is_native=self.is_native,
            evaluation_date=repr(self._dal.EvaluationDate_Get()),
        )
        with self._health_lock:
            self._health_snapshot = snapshot

    # -- product / model -------------------------------------------------

    def _coerce_event_date(self, raw: Any) -> Any:
        """Convert a JSON-friendly event-date token into a ``Cell_``.

        Accepted forms:
        * ``{"date": "YYYY-MM-DD"}``  -> Cell_ wrapping a Date_
        * a plain string label/schedule (e.g. ``"STRIKE"`` or
          ``"START: ... END: ... FREQ: 1W"``) -> Cell_ wrapping the string
        """
        if isinstance(raw, dict) and "date" in raw:
            y, m, d = (int(x) for x in str(raw["date"]).split("-"))
            return self._to_cell(self.make_date(y, m, d))
        return self._to_cell(str(raw))

    def build_product(self, event_dates: list[Any], events: list[str]) -> Any:
        cells = [self._coerce_event_date(d) for d in event_dates]
        return self._dal.Product_New(cells, [str(e) for e in events])

    def debug_product(self, event_dates: list[Any], events: list[str]) -> str:
        product = self.build_product(event_dates, events)
        return self._dal.Product_Debug(product)

    def build_model(self, kind: str, params: dict[str, Any]) -> Any:
        if kind == "BSModelData_":
            return self._dal.BSModelData_New(
                float(params["spot"]),
                float(params["vol"]),
                float(params["rate"]),
                float(params["div"]),
            )
        if kind == "DupireModelData_":
            spots = [float(x) for x in params["spots"]]
            times = [float(x) for x in params["times"]]
            vols = self._build_matrix(spots, times, params["vols"])
            return self._dal.DupireModelData_New(
                float(params["spot"]),
                float(params["rate"]),
                float(params["repo"]),
                spots,
                times,
                vols,
            )
        raise ValueError(f"Unsupported model kind: {kind!r}")

    def _build_matrix(self, spots: list[float], times: list[float], vols: Any) -> Any:
        """Build a ``dal.DoubleMatrix_`` from a spots-by-times nested sequence."""
        matrix_cls = getattr(self._dal, "DoubleMatrix_", None)
        n_rows, n_cols = len(spots), len(times)
        if len(vols) != n_rows or any(len(row) != n_cols for row in vols):
            raise ValueError("Dupire vols must be a rectangular matrix matching spots x times")

        rows = [[float(value) for value in row] for row in vols]
        if matrix_cls is None:
            return rows
        return matrix_cls(rows)

    # -- valuation -------------------------------------------------------

    def value(self, request: ValuationRequest) -> dict[str, float]:
        with self._lock:
            try:
                native_method = _NATIVE_RNG_METHODS[request.method]
            except KeyError as exc:
                raise ValueError(f"Unsupported Monte Carlo method: {request.method!r}") from exc
            previous_date = None
            if request.evaluation_date is not None:
                previous_date = self._dal.EvaluationDate_Get()
                self.set_evaluation_date(*request.evaluation_date)
            try:
                product = self.build_product(request.event_dates, request.events)
                model = self.build_model(request.model_kind, request.model_params)
                raw = self._dal.MonteCarlo_Value(
                    product,
                    model,
                    int(request.num_paths),
                    native_method,
                    bool(request.use_brownian_bridge),
                    bool(request.enable_aad),
                    float(request.smooth),
                )
                return {str(k): float(v) for k, v in dict(raw).items()}
            finally:
                if previous_date is not None:
                    self._dal.EvaluationDate_Set(previous_date)
                self._refresh_health_snapshot()

    # -- curve calibration ----------------------------------------------

    def plan_single_admission(
        self,
        request: SingleGatewayAdmissionRequest,
        on_unbounded_plan_inspected: Callable[[ResolvedSingleKnotPlanDTO], None],
    ) -> SingleGatewayAdmissionResult:
        """Resolve the single-knot plan once under the calibration lock."""
        with self._calibration_lock:
            plan = self._plan_single(request)
            on_unbounded_plan_inspected(plan)
            normalized = request.request
            instruments = normalized.instruments
            native_names = tuple(_native_instrument_name(item.kind) for item in instruments)
            latest_end = max(item.maturity for item in instruments)
            execution_spec = self._build_single_spec(normalized, request.referenced_curves, plan)
            eligibility = (
                self._dal.ValidateSingleCurveAnalyticEligibility(execution_spec)
                if execution_spec is not None
                and hasattr(self._dal, "ValidateSingleCurveAnalyticEligibility")
                else None
            )
            resolved_guess = (
                tuple(
                    float(value)
                    for value in self._dal.ResolveCurveCalibrationInitialGuess(execution_spec)
                )
                if execution_spec is not None
                and hasattr(self._dal, "ResolveCurveCalibrationInitialGuess")
                else _fallback_resolved_initial_guess(normalized, plan)
            )
            return SingleGatewayAdmissionResult(
                resolved_knot_plan=plan,
                native_names_by_input=native_names,
                analytic_eligibility=eligibility,
                latest_instrument_end=latest_end,
                resolved_initial_guess_per_node=resolved_guess,
            )

    def calibrate_single(
        self,
        pre_lock_request: SingleGatewayPreLockRequest,
        on_lock_acquired: Callable[[datetime], None],
        verify_pre_native_admission_evidence: Callable[
            [SingleGatewayPreLockRequest], VerifiedSingleWorkerAdmissionEvidence
        ],
        on_execution_identity_inspected: Callable[[ExecutionSingleKnotIdentityDTO], None],
    ) -> GatewayCalibrationResult:
        """Hold one lock continuously through callbacks and native extraction."""
        with self._calibration_lock:
            self._notify_single_lock_acquired(on_lock_acquired)
            evidence = self._verify_single_admission(
                pre_lock_request, verify_pre_native_admission_evidence
            )
            verified = VerifiedSingleGatewayRequest(pre_lock_request, evidence)
            started = time.perf_counter()
            native_spec = self._build_single_execution_spec(verified)
            actual = self._inspect_single_execution_identity(
                native_spec, evidence.expected_execution_identity
            )
            _require_single_execution_identity(
                evidence.expected_execution_identity,
                actual,
                comparison_stage="pre_solve_execution_identity",
            )
            on_execution_identity_inspected(actual)
            result = self._calibrate_single_verified(verified, actual, native_spec)
            elapsed_ms = (time.perf_counter() - started) * 1000.0
            _require_terminal_single_identity(
                evidence.expected_execution_identity, result, elapsed_ms
            )
            self._refresh_health_snapshot()
            return result._replace(native_solve_ms=elapsed_ms)

    @staticmethod
    def _notify_single_lock_acquired(
        callback: Callable[[datetime], None],
    ) -> None:
        try:
            callback(datetime.now(UTC))
        except Exception as exc:
            raise GatewayLifecycleTransitionError("mark_calibration_solving") from exc

    @staticmethod
    def _verify_single_admission(
        request: SingleGatewayPreLockRequest,
        callback: Callable[[SingleGatewayPreLockRequest], VerifiedSingleWorkerAdmissionEvidence],
    ) -> VerifiedSingleWorkerAdmissionEvidence:
        try:
            evidence = callback(request)
        except (
            PersistedKnotPlanIntegrityError,
            PersistedExpectedExecutionIdentityIntegrityError,
        ):
            raise
        except Exception as exc:
            raise GatewayLifecycleTransitionError("verify_pre_native_admission_evidence") from exc
        if not isinstance(evidence, VerifiedSingleWorkerAdmissionEvidence):
            raise TypeError("pre-native evidence callback returned the wrong carrier type")
        return evidence

    def _inspect_single_execution_identity(
        self,
        native_spec: object | None,
        expected: ExecutionSingleKnotIdentityDTO,
    ) -> ExecutionSingleKnotIdentityDTO:
        if native_spec is None or not hasattr(
            self._dal, "InspectCurveCalibrationExecutionIdentity"
        ):
            return expected.model_copy(deep=True)
        return _native_identity_to_dto(
            self._dal.InspectCurveCalibrationExecutionIdentity(native_spec)
        )

    def calibrate_staged_xccy(
        self,
        request: StagedXccyGatewayRequest,
        on_lock_acquired: Callable[[datetime], None],
    ) -> GatewayCalibrationResult:
        with self._calibration_lock:
            on_lock_acquired(datetime.now(UTC))
            started = time.perf_counter()
            if hasattr(self._dal, "CrossCurrencyCalibrationSpecBuilder_"):
                native_spec = self._build_staged_xccy_spec(
                    request.request, request.referenced_curves
                )
                native_result = self._call_native_calibration(
                    self._dal.CalibrateXccyMarket,
                    request.request,
                    native_spec,
                    self._build_xccy_options(request.request),
                )
                result = _native_staged_result_to_gateway(request.request, native_result)
            else:
                result = self._calibrate_xccy_fallback(request.request, "xccy_staged")
            self._refresh_health_snapshot()
            return result._replace(native_solve_ms=(time.perf_counter() - started) * 1000.0)

    def validate_staged_xccy_admission(self, request: StagedXccyGatewayRequest) -> object | None:
        with self._calibration_lock:
            if not hasattr(self._dal, "ValidateCrossCurrencyAnalyticEligibility"):
                return None
            native_spec = self._build_staged_xccy_spec(request.request, request.referenced_curves)
            return self._dal.ValidateCrossCurrencyAnalyticEligibility(native_spec)

    def required_historical_xccy_fixings(
        self, request: StagedXccyGatewayRequest | JointXccyGatewayRequest
    ) -> tuple[GatewayRequiredHistoricalFixing, ...]:
        """Resolve required observations from DAL's native cashflow schedules."""
        extension = getattr(self._dal, "_dal", self._dal)
        preflight = getattr(extension, "_RequiredHistoricalXccyFixings", None)
        if preflight is None:
            return ()
        with self._calibration_lock:
            instruments = [
                self._build_xccy_instrument(item) for item in request.request.basis.instruments
            ]
            rows = preflight(
                instruments,
                self._native_datetime(request.request.valuation_time),
            )
        return tuple(
            GatewayRequiredHistoricalFixing(
                instrument_index=int(instrument_index),
                index_name=str(index_name),
                timestamp=datetime.fromisoformat(repr(timestamp)),
            )
            for instrument_index, index_name, timestamp in rows
        )

    def calibrate_joint_xccy(
        self,
        request: JointXccyGatewayRequest,
        on_lock_acquired: Callable[[datetime], None],
    ) -> GatewayCalibrationResult:
        with self._calibration_lock:
            on_lock_acquired(datetime.now(UTC))
            started = time.perf_counter()
            if hasattr(self._dal, "JointXccyCalibrationSpecBuilder_"):
                native_spec = self._build_joint_xccy_spec(request.request)
                native_result = self._call_native_calibration(
                    self._dal.CalibrateJointXccyMarket,
                    request.request,
                    native_spec,
                    self._build_joint_xccy_options(request.request),
                )
                result = _native_joint_result_to_gateway(request.request, native_result)
            else:
                result = self._calibrate_xccy_fallback(request.request, "xccy_joint")
            self._refresh_health_snapshot()
            return result._replace(native_solve_ms=(time.perf_counter() - started) * 1000.0)

    def validate_joint_xccy_admission(self, request: JointXccyGatewayRequest) -> object | None:
        with self._calibration_lock:
            if not hasattr(self._dal, "ValidateJointXccyAnalyticEligibility"):
                return None
            return self._dal.ValidateJointXccyAnalyticEligibility(
                self._build_joint_xccy_spec(request.request)
            )

    def rebuild_curve(self, dto: object) -> Any:
        if dto.dto_version != 1:
            raise ValueError(f"unsupported curve DTO version {dto.dto_version}")
        base = self.rebuild_curve(dto.base) if dto.base is not None else None
        dates = [self._native_date(value) for value in dto.node_dates]
        if dto.parameterization == "PIECEWISE_CONSTANT_FWD":
            return self._dal.DiscountPWC_New(
                dto.name,
                dto.currency,
                dates,
                dto.parameters.right_forwards,
                base,
            )
        if dto.parameterization == "PIECEWISE_LINEAR_FWD":
            return self._dal.DiscountPWLF_New(
                dto.name,
                dto.currency,
                dates,
                dto.parameters.left_forwards,
                dto.parameters.right_forwards,
                base,
            )
        day_count = self._dal.DayBasis_New(dto.day_count)
        scheme = getattr(self._dal.LogDfScheme, dto.log_df_scheme)
        if dto.parameterization == "ZERO_RATE":
            return self._dal.DiscountZeroRate_New(
                dto.name,
                dto.currency,
                self._native_date(dto.anchor_date),
                dates,
                dto.parameters.zero_rates,
                day_count,
                scheme,
                base,
            )
        if dto.parameterization == "LOG_DISCOUNT":
            return self._dal.DiscountLogDF_New(
                dto.name,
                dto.currency,
                dates,
                dto.parameters.log_discount_factors,
                day_count,
                scheme,
                base,
            )
        raise ValueError(f"unsupported curve parameterization {dto.parameterization}")

    def _native_date(self, value: date) -> Any:
        return self.make_date(value.year, value.month, value.day)

    def _plan_single(
        self, admission: SingleGatewayAdmissionRequest
    ) -> GatewayResolvedSingleKnotPlan:
        request = admission.request
        if hasattr(self._dal, "PlanCurveCalibrationKnots"):
            instruments = [self._build_rate_instrument(item) for item in request.instruments]
            native = self._dal.PlanCurveCalibrationKnots(
                self._native_date(request.today),
                instruments,
                [self._native_date(value) for value in request.declaration.knot_dates],
                getattr(
                    self._dal.CurveKnotPolicy,
                    request.declaration.knot_policy,
                ),
                getattr(
                    self._dal.CurveParameterization,
                    request.declaration.parameterization,
                ),
            )
            return _native_plan_to_dto(native)
        return _fallback_single_plan(request)

    def _build_rate_instrument(self, instrument: object) -> Any:
        trade_date = self._native_date(instrument.trade_date)
        start = self._native_date(instrument.start)
        maturity = self._native_date(instrument.maturity)
        if instrument.kind in {"DEPOSIT", "FRA", "FUTURE"}:
            index = self._build_rate_index(instrument.index)
            function = {
                "DEPOSIT": self._dal.Deposit_New,
                "FRA": self._dal.FRA_New,
                "FUTURE": self._dal.Future_New,
            }[instrument.kind]
            arguments = [
                trade_date,
                start,
                maturity,
                instrument.market_rate,
                index,
            ]
            if instrument.kind == "FUTURE":
                arguments.append(instrument.convexity_adjustment)
            return function(*arguments)
        if instrument.kind in {"SWAP", "OIS_SWAP"}:
            index_field = (
                instrument.float_index if instrument.kind == "SWAP" else instrument.overnight_index
            )
            function = self._dal.Swap_New if instrument.kind == "SWAP" else self._dal.OISSwap_New
            return function(
                trade_date,
                start,
                maturity,
                instrument.market_rate,
                self._build_rate_leg(instrument.fixed_leg),
                self._build_rate_index(index_field),
                self._build_rate_leg(instrument.float_leg),
            )
        if instrument.kind == "BASIS_SWAP":
            return self._dal.BasisSwap_New(
                trade_date,
                start,
                maturity,
                instrument.market_rate,
                self._build_rate_index(instrument.spread_index),
                self._build_rate_leg(instrument.spread_leg),
                self._build_rate_index(instrument.reference_index),
                self._build_rate_leg(instrument.reference_leg),
            )
        raise ValueError(f"unsupported rate instrument {instrument.kind}")

    def _native_datetime(self, value: datetime) -> Any:
        return self._dal.DateTime_(
            self._native_date(value.date()),
            value.hour,
            value.minute,
            value.second,
        )

    def _build_fixings(self, observations: object) -> Any:
        values: dict[str, dict[object, float]] = {}
        for observation in observations:
            values.setdefault(observation.index_name, {})[
                self._native_datetime(observation.timestamp)
            ] = observation.value
        return self._dal.MarketFixingSnapshot_New(values)

    def _build_xccy_config(self, value: object) -> Any:
        convention = self._dal.CrossCurrencyConvention_()
        convention.initial_notional_exchange = value.convention.initial_notional_exchange
        convention.final_notional_exchange = value.convention.final_notional_exchange
        convention.spread_on_foreign_leg = value.convention.spread_on_foreign_leg
        convention.domestic_index = self._build_rate_index(value.convention.domestic_index)
        convention.domestic_leg = self._build_rate_leg(value.convention.domestic_leg)
        convention.foreign_index = self._build_rate_index(value.convention.foreign_index)
        convention.foreign_leg = self._build_rate_leg(value.convention.foreign_leg)

        def fixing_identity(dto: object) -> Any:
            identity = self._dal.FixingIdentity_()
            identity.index_name = dto.index_name
            identity.fixing_hour = dto.fixing_hour
            identity.fixing_minute = dto.fixing_minute
            return identity

        reset = value.fx_reset
        builder = self._dal.CrossCurrencySwapConfigBuilder_()
        builder.pair = self._dal.CurrencyPair_New(value.pair.domestic, value.pair.foreign)
        builder.domestic_notional = value.domestic_notional
        builder.foreign_notional = value.foreign_notional
        builder.convention = convention
        builder.notional_mode = getattr(self._dal.XccyNotionalMode, value.notional_mode)
        builder.fx_reset = self._dal.FxResetConvention_New(
            reset.fixing_lag,
            self._dal.Holidays_(reset.fixing_holidays),
            _native_business_day_convention(self._dal, reset.fixing_convention),
            reset.fixing_hour,
            reset.fixing_minute,
        )
        builder.domestic_rate_fixing = fixing_identity(value.domestic_rate_fixing)
        builder.foreign_rate_fixing = fixing_identity(value.foreign_rate_fixing)
        return builder.Build()

    def _build_xccy_instrument(self, instrument: object) -> Any:
        return self._dal.CrossCurrencySwap_New(
            self._native_date(instrument.trade_date),
            self._native_date(instrument.start),
            self._native_date(instrument.maturity),
            instrument.market_rate,
            self._build_xccy_config(instrument.config),
        )

    def _build_curve_block(self, block: object, referenced: Mapping[str, object]) -> Any:
        discounts = {
            self._dal.CollateralType_(slot): self.rebuild_curve(referenced[curve_id])
            for slot, curve_id in block.discount_curve_ids.items()
        }
        forwards = {
            self._dal.PeriodLength_New(_native_period(slot)): self.rebuild_curve(
                referenced[curve_id]
            )
            for slot, curve_id in block.forward_curve_ids.items()
        }
        return self._dal.CurveBlock_New(
            block.name,
            block.currency,
            discounts,
            forwards,
            self._dal.DayBasis_New(block.libor_basis),
        )

    def _build_staged_xccy_spec(self, request: object, referenced: Mapping[str, object]) -> Any:
        builder = self._dal.CrossCurrencyCalibrationSpecBuilder_()
        builder.today = self._native_date(request.valuation_time.date())
        builder.valuation_time = self._native_datetime(request.valuation_time)
        builder.collateral_currency = self._dal.Ccy_(request.collateral_currency)
        builder.fixings = self._build_fixings(request.fixings)
        builder.basis_pair = self._dal.CurrencyPair_New(request.pair.domestic, request.pair.foreign)
        builder.domestic_curve_block = self._build_curve_block(
            request.domestic_curve_block, referenced
        )
        builder.foreign_curve_block = self._build_curve_block(
            request.foreign_curve_block, referenced
        )
        builder.fx_spot = request.fx_spot
        builder.fx_forward_collateral = self._dal.CollateralType_(request.fx_forward_collateral)
        builder.instruments = [
            self._build_xccy_instrument(item) for item in request.basis.instruments
        ]
        builder.knot_dates = [self._native_date(value) for value in request.basis.knot_dates]
        builder.smoothing_weight = request.solver.smoothing_weight
        builder.tolerance = request.solver.tolerance
        builder.fit_tolerance = request.solver.fit_tolerance
        builder.initial_guess = request.solver.initial_guess
        builder.initial_guess_per_node = list(request.basis.initial_guess_per_node) or [
            request.solver.initial_guess
        ] * len(request.basis.knot_dates)
        builder.max_evaluations = request.solver.max_evaluations
        builder.max_restarts = request.solver.max_restarts
        builder.solve_mode = getattr(self._dal.CurveSolveMode, request.solver.solve_mode)
        return builder.Build()

    def _build_xccy_options(self, request: object) -> Any:
        options = self._dal.CrossCurrencyCalibrationOptions_()
        options.jacobian_mode = getattr(self._dal.CurveJacobianMode, request.options.jacobian_mode)
        options.compute_forward_jacobian = request.options.include_jacobian
        options.compute_eff_jacobian_inverse = request.options.include_effective_inverse
        return options

    def _build_joint_curve_declaration(self, value: object) -> Any:
        declaration = self._dal.JointCurveDeclaration_()
        declaration.curve_name = value.curve_name
        declaration.instruments = [self._build_rate_instrument(item) for item in value.instruments]
        declaration.knot_dates = [self._native_date(item) for item in value.knot_dates]
        declaration.target_collateral = self._dal.CollateralType_(value.target_collateral)
        if value.target_tenor is not None:
            declaration.target_tenor = self._dal.PeriodLength_New(
                _native_period(value.target_tenor)
            )
        declaration.calibrate_discount_curve = value.calibrate_discount_curve
        declaration.base_layered_over_discount = value.base_layered_over_discount
        declaration.parameterization = getattr(
            self._dal.CurveParameterization, value.parameterization
        )
        declaration.log_df_scheme = getattr(
            self._dal.LogDfScheme, value.log_df_scheme or "LOG_LINEAR"
        )
        declaration.smoothing_weight = (
            value.smoothing_weight if value.smoothing_weight is not None else 1.0
        )
        declaration.initial_guess_per_node = list(value.initial_guess_per_node)
        return declaration

    def _build_joint_currency(self, value: object) -> Any:
        result = self._dal.JointCurrencyCurveSpec_()
        result.ccy = self._dal.Ccy_(value.currency)
        result.libor_basis = self._dal.DayBasis_New(value.libor_basis)
        result.curves = [self._build_joint_curve_declaration(item) for item in value.declarations]
        return result

    def _build_joint_xccy_spec(self, request: object) -> Any:
        basis = self._dal.XccyBasisCurveDeclaration_()
        basis.curve_name = request.basis.curve_name
        basis.instruments = [
            self._build_xccy_instrument(item) for item in request.basis.instruments
        ]
        basis.knot_dates = [self._native_date(item) for item in request.basis.knot_dates]
        basis.parameterization = getattr(
            self._dal.CurveParameterization, request.basis.parameterization
        )
        basis.log_df_scheme = getattr(
            self._dal.LogDfScheme, request.basis.log_df_scheme or "LOG_LINEAR"
        )
        basis.smoothing_weight = (
            request.basis.smoothing_weight
            if request.basis.smoothing_weight is not None
            else request.solver.smoothing_weight
        )
        basis.initial_guess_per_node = list(request.basis.initial_guess_per_node)

        solver = self._dal.CurveSolverOptions_()
        solver.smoothing_weight = request.solver.smoothing_weight
        solver.tolerance = request.solver.tolerance
        solver.fit_tolerance = request.solver.fit_tolerance
        solver.initial_guess = request.solver.initial_guess
        solver.max_evaluations = request.solver.max_evaluations
        solver.max_restarts = request.solver.max_restarts
        solver.solve_mode = getattr(self._dal.CurveSolveMode, request.solver.solve_mode)

        builder = self._dal.JointXccyCalibrationSpecBuilder_()
        builder.valuation_time = self._native_datetime(request.valuation_time)
        builder.pair = self._dal.CurrencyPair_New(request.pair.domestic, request.pair.foreign)
        builder.collateral_currency = self._dal.Ccy_(request.collateral_currency)
        builder.fx_spot = request.fx_spot
        builder.domestic = self._build_joint_currency(request.domestic)
        builder.foreign = self._build_joint_currency(request.foreign)
        builder.basis = basis
        builder.fixings = self._build_fixings(request.fixings)
        builder.solver_options = solver
        return builder.Build()

    def _build_joint_xccy_options(self, request: object) -> Any:
        options = self._dal.JointXccyCalibrationOptions_()
        options.jacobian_mode = getattr(self._dal.CurveJacobianMode, request.options.jacobian_mode)
        options.compute_forward_jacobian = request.options.include_jacobian
        options.compute_eff_jacobian_inverse = request.options.include_effective_inverse
        return options

    def _build_rate_index(self, value: object) -> Any:
        result = self._dal.RateIndexConvention_New(
            self._dal.PeriodLength_New(_native_period(value.forecast_tenor)),
            self._dal.DayBasis_New(value.day_basis),
            self._dal.CollateralType_(value.collateral),
            value.use_projection_curve,
        )
        result.spot_lag = value.spot_lag
        result.fixing_lag = value.fixing_lag
        result.business_day_convention = _native_business_day_convention(
            self._dal, value.business_day_convention
        )
        result.fixing_holidays = self._dal.Holidays_(value.fixing_holidays)
        result.accrual_holidays = self._dal.Holidays_(value.accrual_holidays)
        result.end_of_month = value.end_of_month
        return result

    def _build_rate_leg(self, value: object) -> Any:
        result = self._dal.RateLegConvention_New(
            self._dal.PeriodLength_New(_native_period(value.payment_frequency)),
            self._dal.DayBasis_New(value.day_basis),
        )
        result.payment_lag = value.payment_lag
        result.business_day_convention = _native_business_day_convention(
            self._dal, value.business_day_convention
        )
        result.payment_convention = _native_business_day_convention(
            self._dal, value.payment_convention
        )
        result.accrual_holidays = self._dal.Holidays_(value.accrual_holidays)
        result.payment_holidays = self._dal.Holidays_(value.payment_holidays)
        result.end_of_month = value.end_of_month
        return result

    def _call_native_calibration(
        self,
        function: Callable[..., object],
        request: object,
        *arguments: object,
    ) -> object:
        try:
            return function(*arguments)
        except Exception as exc:
            extension = getattr(self._dal, "_dal", self._dal)
            convergence_error = getattr(extension, "_CalibrationConvergenceError", None)
            if convergence_error is None or not isinstance(exc, convergence_error):
                raise
            raise NativeSolverDidNotConvergeError(
                max_abs_residual=None,
                rms_residual=None,
                evaluations=request.solver.max_evaluations,
                max_evaluations=request.solver.max_evaluations,
            ) from exc

    def _calibrate_single_verified(
        self,
        verified: VerifiedSingleGatewayRequest,
        actual: ExecutionSingleKnotIdentityDTO,
        native_spec: object | None,
    ) -> GatewayCalibrationResult:
        """Solve through DAL when available, retaining a minimal test-double path."""
        request = verified.pre_lock_request.request
        if native_spec is not None and hasattr(self._dal, "CalibrateSingleCurve"):
            options = self._dal.CurveCalibrationOptions_()
            options.jacobian_mode = getattr(
                self._dal.CurveJacobianMode, request.options.jacobian_mode
            )
            options.compute_forward_jacobian = request.options.include_jacobian
            options.compute_eff_jacobian_inverse = request.options.include_effective_inverse
            native_result = self._call_native_calibration(
                self._dal.CalibrateSingleCurve,
                request,
                native_spec,
                options,
            )
            return _native_single_result_to_gateway(
                request,
                verified.evidence.resolved_knot_plan,
                native_result,
                actual,
            )
        plan = verified.evidence.resolved_knot_plan
        declaration = request.declaration
        values = (
            declaration.initial_guess_per_node
            or [request.solver.initial_guess] * plan.counts.free_parameters
        )
        curve = _fallback_curve_payload(
            request,
            plan,
            values,
            role="discount" if declaration.calibrate_discount_curve else "forward",
        )
        return _fallback_result(
            request,
            (curve,),
            _terminal_identity_from_curve_payload(actual, curve),
        )

    def _build_single_execution_spec(self, verified: VerifiedSingleGatewayRequest) -> object | None:
        return self._build_single_spec(
            verified.pre_lock_request.request,
            verified.pre_lock_request.referenced_curves,
            verified.evidence.resolved_knot_plan,
        )

    def _build_single_spec(
        self,
        request: object,
        referenced: Mapping[str, object],
        plan: ResolvedSingleKnotPlanDTO,
    ) -> object | None:
        if not hasattr(self._dal, "CurveCalibrationSpecBuilder_"):
            return None
        declaration = request.declaration
        builder = self._dal.CurveCalibrationSpecBuilder_()
        self._configure_single_spec_builder(builder, request, plan)
        self._configure_single_reference_curves(builder, declaration, referenced)
        return builder.Build()

    def _configure_single_spec_builder(
        self,
        builder: object,
        request: object,
        plan: ResolvedSingleKnotPlanDTO,
    ) -> None:
        declaration = request.declaration
        builder.today_ = self._native_date(request.today)
        builder.ccy_ = self._dal.String_(request.currency)
        builder.curveName_ = self._dal.String_(declaration.curve_name)
        builder.targetCollateral_ = self._dal.CollateralType_(declaration.target_collateral)
        if declaration.target_tenor is not None:
            builder.targetTenor_ = self._dal.PeriodLength_New(
                _native_period(declaration.target_tenor)
            )
        builder.calibrateDiscountCurve_ = declaration.calibrate_discount_curve
        builder.liborBasis_ = self._dal.DayBasis_New(declaration.libor_basis)
        builder.smoothingWeight_ = request.solver.smoothing_weight
        builder.tolerance_ = request.solver.tolerance
        builder.fitTolerance_ = request.solver.fit_tolerance
        builder.maxEvaluations_ = request.solver.max_evaluations
        builder.maxRestarts_ = request.solver.max_restarts
        builder.initialGuess_ = request.solver.initial_guess
        builder.solveMode_ = getattr(self._dal.CurveSolveMode, request.solver.solve_mode)
        builder.parameterization_ = getattr(
            self._dal.CurveParameterization, declaration.parameterization
        )
        builder.knotPolicy_ = self._dal.CurveKnotPolicy.INPUT
        builder.logDfScheme_ = getattr(
            self._dal.LogDfScheme, declaration.log_df_scheme or "LOG_LINEAR"
        )
        canonical_instruments = sorted(
            request.instruments,
            key=lambda item: (
                item.maturity,
                item.start,
                _native_instrument_name(item.kind),
            ),
        )
        builder.instruments_ = [self._build_rate_instrument(item) for item in canonical_instruments]
        builder.knotDates_ = [
            self._native_date(node.date)
            for node in plan.storage_nodes
            if node.date != request.today or declaration.parameterization == "LOG_DISCOUNT"
        ]
        builder.initialGuessPerNode_ = (
            list(declaration.initial_guess_per_node)
            or [request.solver.initial_guess] * plan.counts.free_parameters
        )

    def _configure_single_reference_curves(
        self,
        builder: object,
        declaration: object,
        referenced: Mapping[str, object],
    ) -> None:
        if declaration.base_curve_id is not None:
            builder.baseCurve_ = self.rebuild_curve(referenced[declaration.base_curve_id])
        builder.discountCurves_ = {
            self._dal.CollateralType_(slot): self.rebuild_curve(referenced[curve_id])
            for slot, curve_id in declaration.discount_curve_ids.items()
        }
        builder.forwardCurves_ = {
            self._dal.PeriodLength_New(_native_period(slot)): self.rebuild_curve(
                referenced[curve_id]
            )
            for slot, curve_id in declaration.forward_curve_ids.items()
        }

    def _calibrate_xccy_fallback(self, request: object, kind: str) -> GatewayCalibrationResult:
        curve = {
            "name": request.basis.curve_name,
            "currency": request.pair.domestic,
            "role": "basis",
            "target": {"collateral": request.collateral_currency, "tenor": None},
            "parameterization": "PIECEWISE_CONSTANT_FWD",
            "anchor_date": request.valuation_time.date(),
            "day_count": "ACT_365F",
            "log_df_scheme": None,
            "node_dates": list(request.basis.knot_dates),
            "parameters": {
                "right_forwards": (
                    list(request.basis.initial_guess_per_node)
                    or [request.solver.initial_guess] * len(request.basis.knot_dates)
                )
            },
            "base_curve_id": None,
        }
        return _fallback_result(request, (curve,), None, xccy=True)


def _require_single_execution_identity(
    expected: ExecutionSingleKnotIdentityDTO,
    actual: ExecutionSingleKnotIdentityDTO,
    *,
    comparison_stage: str,
    actual_jacobian_mode: str | None = None,
    native_solve_ms: float | None = None,
) -> None:
    if actual == expected:
        return
    raise NativeExecutionIdentityMismatchError(
        expected,
        actual,
        comparison_stage=comparison_stage,
        actual_jacobian_mode=actual_jacobian_mode,
        native_solve_ms=native_solve_ms,
    )


def _require_terminal_single_identity(
    expected: ExecutionSingleKnotIdentityDTO,
    result: GatewayCalibrationResult,
    elapsed_ms: float,
) -> None:
    actual = result.actual_execution_identity
    if actual is None:
        raise RuntimeError("single calibration omitted terminal execution identity")
    _require_single_execution_identity(
        expected,
        actual,
        comparison_stage="post_solve_storage",
        actual_jacobian_mode=result.actual_jacobian_mode,
        native_solve_ms=elapsed_ms,
    )


def _enum_name(value: object) -> str:
    name = getattr(value, "name", None)
    if name is not None:
        return str(name)
    return str(value).rsplit(".", 1)[-1]


def _native_period(value: str) -> str:
    """Translate ISO-8601 Web tenors to DAL's legacy tenor spelling."""
    return value[1:] if value.startswith("P") else value


def _native_business_day_convention(module: object, value: str) -> object:
    enum_name = value.replace("-", "_").replace(" ", "_").upper()
    return getattr(module.BizDayConvention_, enum_name)


def _native_date_to_python(value: object) -> date:
    return date.fromisoformat(repr(value))


def _native_origin_to_dict(origin: object) -> dict[str, object]:
    kind = _enum_name(origin.kind)
    if kind == "INPUT":
        return {"kind": kind, "input_knot_index": origin.input_knot_index}
    if kind in {"INSTRUMENT_START", "INSTRUMENT_END"}:
        return {
            "kind": kind,
            "instrument_input_index": origin.instrument_input_index,
        }
    return {"kind": "SYNTHETIC_ANCHOR"}


def _native_candidate_to_dto(candidate: object) -> KnotCandidateDTO:
    resolved_index = candidate.resolved_index
    return KnotCandidateDTO.model_validate(
        {
            "ordinal": candidate.ordinal,
            "date": _native_date_to_python(candidate.date),
            "origin": _native_origin_to_dict(candidate.origin),
            "disposition": _enum_name(candidate.disposition),
            "resolved_index": resolved_index if resolved_index >= 0 else None,
        }
    )


def _native_node_to_dto(item: object) -> ResolvedKnotNodeDTO:
    return ResolvedKnotNodeDTO.model_validate(
        {
            "date": _native_date_to_python(item.date),
            "origins": [_native_origin_to_dict(origin) for origin in item.origins],
        }
    )


def _native_free_parameter_to_dto(item: object) -> FreeParameterDTO:
    return FreeParameterDTO.model_validate(
        {
            "date": _native_date_to_python(item.date),
            "component": _enum_name(item.component).lower(),
        }
    )


def _native_plan_counts(value: object) -> GatewayResolvedKnotCounts:
    counts = value.counts
    return GatewayResolvedKnotCounts(
        submitted_knots=counts.submitted_knots,
        instrument_candidates=counts.instrument_candidates,
        resolved_declared_nodes=counts.resolved_declared_nodes,
        storage_nodes=counts.storage_nodes,
        free_parameters=counts.free_parameters,
    )


def _native_plan_to_dto(value: object) -> GatewayResolvedSingleKnotPlan:
    return GatewayResolvedSingleKnotPlan(
        planner_version=value.planner_version,
        requested_policy=_enum_name(value.requested_policy),
        execution_policy=_enum_name(value.execution_policy),
        submitted_knot_dates=tuple(
            _native_date_to_python(item) for item in value.submitted_knot_dates
        ),
        candidate_trace=tuple(_native_candidate_to_dto(item) for item in value.candidate_trace),
        resolved_declared_nodes=tuple(
            _native_node_to_dto(item) for item in value.resolved_declared_nodes
        ),
        storage_nodes=tuple(_native_node_to_dto(item) for item in value.storage_nodes),
        free_parameters=tuple(
            _native_free_parameter_to_dto(item) for item in value.free_parameters
        ),
        anchor_added=value.anchor_added,
        counts=_native_plan_counts(value),
    )


def _native_identity_to_dto(value: object) -> ExecutionSingleKnotIdentityDTO:
    scheme = getattr(value, "log_df_scheme", None)
    counts = value.counts
    return ExecutionSingleKnotIdentityDTO.model_validate(
        {
            "identity_version": value.identity_version,
            "execution_policy": _enum_name(value.execution_policy),
            "today": _native_date_to_python(value.today),
            "parameterization": _enum_name(value.parameterization),
            "log_df_scheme": _enum_name(scheme) if scheme is not None else None,
            "resolved_declared_dates": [
                _native_date_to_python(item) for item in value.resolved_declared_dates
            ],
            "storage_dates": [_native_date_to_python(item) for item in value.storage_dates],
            "free_parameters": [
                {
                    "date": _native_date_to_python(item.date),
                    "component": _enum_name(item.component).lower(),
                }
                for item in value.free_parameters
            ],
            "counts": {
                "resolved_declared_nodes": counts.resolved_declared_nodes,
                "storage_nodes": counts.storage_nodes,
                "free_parameters": counts.free_parameters,
            },
        }
    )


def _native_instrument_diagnostics(
    market_rates: object,
    model_rates: object,
    residuals: object,
    *,
    group: str,
) -> tuple[InstrumentDiagnosticDTO, ...]:
    return tuple(
        InstrumentDiagnosticDTO(
            instrument_id=f"{index + 1:032x}",
            group=group,
            calibration_index=index,
            market_rate=float(market_rate),
            model_rate=float(model_rate),
            residual=float(residual),
        )
        for index, (market_rate, model_rate, residual) in enumerate(
            zip(market_rates, model_rates, residuals, strict=True)
        )
    )


def _native_solver_diagnostics(
    diagnostics: object,
    request: object,
    *,
    evaluations: int | None = None,
) -> SolverDiagnosticsDTO:
    approximate = bool(diagnostics.used_approximate_fit)
    return SolverDiagnosticsDTO(
        status="approximate_fit" if approximate else "converged",
        solve_mode=request.solver.solve_mode,
        used_approximate_fit=approximate,
        tolerance=request.solver.tolerance,
        fit_tolerance=request.solver.fit_tolerance,
        max_abs_residual=float(diagnostics.max_abs_residual),
        rms_residual=float(diagnostics.rms_residual),
        evaluations=evaluations,
    )


def _single_parameter_axis(
    request: object,
    actual: ExecutionSingleKnotIdentityDTO,
) -> list[str]:
    return [
        "parameter:"
        f"{request.declaration.curve_name}:"
        f"{parameter.date.isoformat()}:{parameter.component}"
        for parameter in actual.free_parameters
    ]


def _native_single_result_to_gateway(
    request: object,
    plan: ResolvedSingleKnotPlanDTO,
    native_result: object,
    actual: ExecutionSingleKnotIdentityDTO,
) -> GatewayCalibrationResult:
    diagnostics = native_result.diagnostics_
    instrument_diagnostics = _native_instrument_diagnostics(
        diagnostics.market_rates,
        diagnostics.model_rates,
        diagnostics.residuals,
        group="single:curve",
    )
    parameter_axis = _single_parameter_axis(request, actual)
    residual_axis = [
        f"residual:{diagnostic.instrument_id}" for diagnostic in instrument_diagnostics
    ]
    exact = request.solver.solve_mode == "EXACT"
    jacobian_available = (
        request.options.include_jacobian and exact and request.options.jacobian_mode == "ANALYTIC"
    )
    inverse_available = request.options.include_effective_inverse and exact
    jacobian = _native_matrix_dto(
        diagnostics.jacobian,
        available=jacobian_available,
        requested=request.options.include_jacobian,
        row_axis=residual_axis,
        column_axis=parameter_axis,
        scaling="unscaled",
        residual_tolerance=None,
    )
    inverse = _native_matrix_dto(
        diagnostics.eff_jacobian_inverse,
        available=inverse_available,
        requested=request.options.include_effective_inverse,
        row_axis=parameter_axis,
        column_axis=residual_axis,
        scaling="solver_scaled",
        residual_tolerance=request.solver.tolerance,
    )
    curve_payload = _native_single_curve_payload(request, plan, native_result.curve_)
    terminal_actual = _terminal_identity_from_curve_payload(actual, curve_payload)
    return GatewayCalibrationResult(
        actual_jacobian_mode=request.options.jacobian_mode,
        actual_execution_identity=terminal_actual,
        curves=(curve_payload,),
        instrument_diagnostics=instrument_diagnostics,
        solver_diagnostics=_native_solver_diagnostics(diagnostics, request),
        fx_forwards=None,
        named_ranges=NamedRangesDTO(
            parameters=[
                NamedRangeDTO(
                    name=f"single:{request.declaration.curve_name}",
                    offset=0,
                    size=len(parameter_axis),
                )
            ],
            residuals=[
                NamedRangeDTO(
                    name=f"single:{request.declaration.curve_name}",
                    offset=0,
                    size=len(residual_axis),
                )
            ],
        ),
        jacobian=jacobian,
        effective_inverse=inverse,
        native_solve_ms=0.0,
    )


def _native_staged_result_to_gateway(
    request: object, native_result: object
) -> GatewayCalibrationResult:
    diagnostics = native_result.diagnostics
    instrument_diagnostics = _native_instrument_diagnostics(
        diagnostics.market_rates,
        diagnostics.model_rates,
        diagnostics.residuals,
        group="basis:curve",
    )
    parameter_axis = [
        f"parameter:{request.basis.curve_name}:{value.isoformat()}:right_forward"
        for value in request.basis.knot_dates
    ]
    residual_axis = [f"residual:{value.instrument_id}" for value in instrument_diagnostics]
    jacobian_available, inverse_available = _staged_matrix_availability(request, diagnostics)
    curve = native_result.basis_curve
    fx = native_result.fx_forward_curve
    return GatewayCalibrationResult(
        actual_jacobian_mode=request.options.jacobian_mode,
        actual_execution_identity=None,
        curves=(
            {
                "name": request.basis.curve_name,
                "currency": request.pair.domestic,
                "role": "basis",
                "target": {
                    "collateral": request.collateral_currency,
                    "tenor": None,
                },
                "parameterization": "PIECEWISE_CONSTANT_FWD",
                "anchor_date": request.valuation_time.date(),
                "day_count": "ACT_365F",
                "log_df_scheme": None,
                "node_dates": [_native_date_to_python(value) for value in curve.knot_dates],
                "parameters": {"right_forwards": [float(value) for value in curve.right_forwards]},
                "base_curve_id": None,
            },
        ),
        instrument_diagnostics=instrument_diagnostics,
        solver_diagnostics=_native_solver_diagnostics(diagnostics, request),
        fx_forwards=FxForwardDTO(
            pair={
                "domestic": request.pair.domestic,
                "foreign": request.pair.foreign,
            },
            dates=[_native_date_to_python(value) for value in fx.dates],
            forwards=[float(value) for value in fx.forwards],
        ),
        named_ranges=NamedRangesDTO(
            parameters=[
                NamedRangeDTO(
                    name=f"basis:{request.basis.curve_name}",
                    offset=0,
                    size=len(parameter_axis),
                )
            ],
            residuals=[
                NamedRangeDTO(
                    name="basis:curve",
                    offset=0,
                    size=len(residual_axis),
                )
            ],
        ),
        jacobian=_native_matrix_dto(
            diagnostics.jacobian,
            available=jacobian_available,
            requested=request.options.include_jacobian,
            row_axis=residual_axis,
            column_axis=parameter_axis,
            scaling="unscaled",
            residual_tolerance=None,
        ),
        effective_inverse=_native_matrix_dto(
            diagnostics.eff_jacobian_inverse,
            available=inverse_available,
            requested=request.options.include_effective_inverse,
            row_axis=parameter_axis,
            column_axis=residual_axis,
            scaling="solver_scaled",
            residual_tolerance=request.solver.tolerance,
        ),
        native_solve_ms=0.0,
    )


def _staged_matrix_availability(request: object, diagnostics: object) -> tuple[bool, bool]:
    jacobian_available = diagnostics.jacobian_availability == "available"
    inverse_available = diagnostics.eff_jacobian_inverse_availability == "available"
    exact = request.solver.solve_mode == "EXACT"
    expected_jacobian = (
        request.options.include_jacobian and exact and request.options.jacobian_mode == "ANALYTIC"
    )
    expected_inverse = request.options.include_effective_inverse and exact
    if jacobian_available != expected_jacobian:
        raise RuntimeError("native staged Jacobian availability violates the request")
    if inverse_available != expected_inverse:
        raise RuntimeError("native staged effective-inverse availability violates the request")
    return jacobian_available, inverse_available


def _native_joint_curve_payload(
    declaration: object,
    *,
    currency: str,
    collateral_currency: str,
    anchor_date: date,
    curve: object,
    role: str,
) -> dict[str, object]:
    parameterization = declaration.parameterization
    node_dates, parameters, day_count = _native_curve_state(parameterization, curve)
    return {
        "name": declaration.curve_name,
        "currency": currency,
        "role": role,
        "target": {
            "collateral": getattr(declaration, "target_collateral", collateral_currency),
            "tenor": getattr(declaration, "target_tenor", None),
        },
        "parameterization": parameterization,
        "anchor_date": anchor_date,
        "day_count": day_count,
        "log_df_scheme": declaration.log_df_scheme,
        "node_dates": node_dates,
        "parameters": parameters,
        "base_curve_id": None,
    }


def _native_piecewise_constant_state(
    curve: object,
) -> tuple[list[date], dict[str, object], str]:
    return (
        [_native_date_to_python(value) for value in curve.knot_dates],
        {"right_forwards": [float(value) for value in curve.right_forwards]},
        "ACT_365F",
    )


def _native_piecewise_linear_state(
    curve: object,
) -> tuple[list[date], dict[str, object], str]:
    return (
        [_native_date_to_python(value) for value in curve.knot_dates],
        {
            "left_forwards": [float(value) for value in curve.left_forwards],
            "right_forwards": [float(value) for value in curve.right_forwards],
        },
        "ACT_365F",
    )


def _native_zero_rate_state(
    curve: object,
) -> tuple[list[date], dict[str, object], str]:
    return (
        [_native_date_to_python(value) for value in curve.node_dates],
        {"zero_rates": [float(value) for value in curve.zero_rates]},
        str(curve.day_count),
    )


def _native_log_discount_state(
    curve: object,
) -> tuple[list[date], dict[str, object], str]:
    return (
        [_native_date_to_python(value) for value in curve.node_dates],
        {"log_discount_factors": [float(value) for value in curve.log_discount_factors]},
        str(curve.day_count),
    )


def _native_curve_state(
    parameterization: str, curve: object
) -> tuple[list[date], dict[str, object], str]:
    projectors = {
        "PIECEWISE_CONSTANT_FWD": _native_piecewise_constant_state,
        "PIECEWISE_LINEAR_FWD": _native_piecewise_linear_state,
        "ZERO_RATE": _native_zero_rate_state,
        "LOG_DISCOUNT": _native_log_discount_state,
    }
    try:
        projector = projectors[parameterization]
    except KeyError as exc:
        raise RuntimeError(f"unsupported native curve parameterization {parameterization}") from exc
    return projector(curve)


def _joint_parameter_axis(request: object, parameter_ranges: list[NamedRangeDTO]) -> list[str]:
    declarations = [
        *request.domestic.declarations,
        *request.foreign.declarations,
        request.basis,
    ]
    if len(declarations) != len(parameter_ranges):
        raise RuntimeError("native joint parameter ranges do not match submitted declarations")

    axis: list[str] = []
    for declaration, native_range in zip(declarations, parameter_ranges, strict=True):
        entries = _joint_parameter_entries(declaration)
        if len(entries) != native_range.size:
            raise RuntimeError(
                "native joint parameter range size does not match its "
                f"declaration: {native_range.name}"
            )
        axis.extend(
            f"parameter:{native_range.name}:{node_date.isoformat()}:{component}"
            for node_date, component in entries
        )
    return axis


def _joint_parameter_entries(declaration: object) -> list[tuple[date, str]]:
    dates = list(declaration.knot_dates)
    parameterization = declaration.parameterization
    components = {
        "PIECEWISE_CONSTANT_FWD": ("right_forward",),
        "PIECEWISE_LINEAR_FWD": ("left_forward", "right_forward"),
        "ZERO_RATE": ("zero_rate",),
        "LOG_DISCOUNT": ("log_discount_factor",),
    }.get(parameterization)
    if components is None:
        raise RuntimeError(f"unsupported native joint parameterization {parameterization}")
    return [(node_date, component) for node_date in dates for component in components]


def _joint_block_curve(declaration: object, block: object) -> tuple[object, str]:
    if declaration.calibrate_discount_curve:
        return (
            next(
                value
                for key, value in block.discount_curves.items()
                if str(key) == declaration.target_collateral
            ),
            "discount",
        )
    target_tenor = _native_period(declaration.target_tenor)
    return (
        next(value for key, value in block.forward_curves.items() if str(key) == target_tenor),
        "forward",
    )


def _native_joint_curve_payloads(
    request: object, native_result: object
) -> tuple[dict[str, object], ...]:
    anchor_date = request.valuation_time.date()
    curves: list[dict[str, object]] = []
    for group, block in (
        (request.domestic, native_result.domestic_curve_block),
        (request.foreign, native_result.foreign_curve_block),
    ):
        for declaration in group.declarations:
            curve, role = _joint_block_curve(declaration, block)
            curves.append(
                _native_joint_curve_payload(
                    declaration,
                    currency=group.currency,
                    collateral_currency=request.collateral_currency,
                    anchor_date=anchor_date,
                    curve=curve,
                    role=role,
                )
            )
    curves.append(
        _native_joint_curve_payload(
            request.basis,
            currency=request.pair.domestic,
            collateral_currency=request.collateral_currency,
            anchor_date=anchor_date,
            curve=native_result.basis_curve,
            role="basis",
        )
    )
    return tuple(curves)


def _native_named_ranges(values: object) -> list[NamedRangeDTO]:
    return [
        NamedRangeDTO(name=value.name, offset=value.offset, size=value.size) for value in values
    ]


def _residual_group(index: int, residual_ranges: list[NamedRangeDTO]) -> str:
    return next(
        value.name for value in residual_ranges if value.offset <= index < value.offset + value.size
    )


def _native_joint_instrument_diagnostics(
    native_result: object, residual_ranges: list[NamedRangeDTO]
) -> tuple[InstrumentDiagnosticDTO, ...]:
    return tuple(
        InstrumentDiagnosticDTO(
            instrument_id=f"{index + 1:032x}",
            group=_residual_group(index, residual_ranges),
            calibration_index=index,
            market_rate=float(market_rate),
            model_rate=float(model_rate),
            residual=float(residual),
        )
        for index, (market_rate, model_rate, residual) in enumerate(
            zip(
                native_result.market_rates,
                native_result.model_rates,
                native_result.residuals,
                strict=True,
            )
        )
    )


def _native_joint_solver_diagnostics(
    native_result: object, request: object
) -> SolverDiagnosticsDTO:
    approximate = bool(native_result.used_approximate_fit)
    return SolverDiagnosticsDTO(
        status="approximate_fit" if approximate else "converged",
        solve_mode=request.solver.solve_mode,
        used_approximate_fit=approximate,
        tolerance=request.solver.tolerance,
        fit_tolerance=request.solver.fit_tolerance,
        max_abs_residual=float(native_result.joint_max_abs_residual),
        rms_residual=float(native_result.joint_rms_residual),
        evaluations=int(native_result.solver_evaluations),
    )


def _native_joint_result_to_gateway(
    request: object, native_result: object
) -> GatewayCalibrationResult:
    curves = _native_joint_curve_payloads(request, native_result)
    parameter_ranges = _native_named_ranges(native_result.parameter_ranges)
    residual_ranges = _native_named_ranges(native_result.residual_ranges)
    parameter_axis = _joint_parameter_axis(request, parameter_ranges)
    instrument_diagnostics = _native_joint_instrument_diagnostics(native_result, residual_ranges)
    residual_axis = [
        f"residual:{diagnostic.instrument_id}" for diagnostic in instrument_diagnostics
    ]
    exact = request.solver.solve_mode == "EXACT"
    jacobian_available = (
        request.options.include_jacobian and exact and request.options.jacobian_mode == "ANALYTIC"
    )
    inverse_available = request.options.include_effective_inverse and exact
    fx = native_result.fx_forward_curve
    return GatewayCalibrationResult(
        actual_jacobian_mode=request.options.jacobian_mode,
        actual_execution_identity=None,
        curves=curves,
        instrument_diagnostics=instrument_diagnostics,
        solver_diagnostics=_native_joint_solver_diagnostics(native_result, request),
        fx_forwards=FxForwardDTO(
            pair={
                "domestic": request.pair.domestic,
                "foreign": request.pair.foreign,
            },
            dates=[_native_date_to_python(value) for value in fx.dates],
            forwards=[float(value) for value in fx.forwards],
        ),
        named_ranges=NamedRangesDTO(
            parameters=parameter_ranges,
            residuals=residual_ranges,
        ),
        jacobian=_native_matrix_dto(
            native_result.jacobian_at_solution,
            available=jacobian_available,
            requested=request.options.include_jacobian,
            row_axis=residual_axis,
            column_axis=parameter_axis,
            scaling="unscaled",
            residual_tolerance=None,
        ),
        effective_inverse=_native_matrix_dto(
            native_result.eff_jacobian_inverse,
            available=inverse_available,
            requested=request.options.include_effective_inverse,
            row_axis=parameter_axis,
            column_axis=residual_axis,
            scaling="solver_scaled",
            residual_tolerance=request.solver.tolerance,
        ),
        native_solve_ms=0.0,
    )


def _native_matrix_dto(
    matrix: object,
    *,
    available: bool,
    requested: bool,
    row_axis: list[str],
    column_axis: list[str],
    scaling: str,
    residual_tolerance: float | None,
) -> MatrixDTO:
    values = matrix.to_rows() if available else None
    if available and (
        len(values) != len(row_axis) or any(len(row) != len(column_axis) for row in values)
    ):
        raise RuntimeError("native matrix shape does not match its response axes")
    return MatrixDTO(
        availability=(
            "available"
            if available
            else "not_requested"
            if not requested
            else "not_available_for_mode"
        ),
        shape=(len(row_axis), len(column_axis)),
        row_axis=row_axis,
        column_axis=column_axis,
        scaling=scaling,
        residual_tolerance=residual_tolerance,
        values=values,
    )


def _native_single_curve_payload(
    request: object, plan: ResolvedSingleKnotPlanDTO, curve: object
) -> dict[str, object]:
    parameterization = request.declaration.parameterization
    node_dates, parameters, day_count = _native_curve_state(parameterization, curve)
    return {
        "name": request.declaration.curve_name,
        "currency": request.currency,
        "role": ("discount" if request.declaration.calibrate_discount_curve else "forward"),
        "target": {
            "collateral": request.declaration.target_collateral,
            "tenor": request.declaration.target_tenor,
        },
        "parameterization": parameterization,
        "anchor_date": request.today,
        "day_count": day_count,
        "log_df_scheme": request.declaration.log_df_scheme,
        "node_dates": node_dates,
        "parameters": parameters,
        "base_curve_id": request.declaration.base_curve_id,
    }


def _terminal_identity_from_curve_payload(
    inspected: ExecutionSingleKnotIdentityDTO,
    curve_payload: Mapping[str, object],
) -> ExecutionSingleKnotIdentityDTO:
    node_dates = tuple(curve_payload["node_dates"])
    if curve_payload["parameterization"] == "ZERO_RATE":
        node_dates = (curve_payload["anchor_date"], *node_dates)
    return inspected.model_copy(
        update={
            "storage_dates": node_dates,
            "counts": inspected.counts.model_copy(update={"storage_nodes": len(node_dates)}),
        }
    )


def _fallback_plan_candidates(
    request: object,
) -> list[tuple[date, dict[str, object]]]:
    declaration = request.declaration
    candidates: list[tuple[date, dict[str, object]]] = []
    if declaration.knot_policy in {"INPUT", "AUGMENTED"}:
        candidates.extend(
            (value, {"kind": "INPUT", "input_knot_index": index})
            for index, value in enumerate(declaration.knot_dates)
        )
    if declaration.knot_policy in {"INSTRUMENTS", "AUGMENTED"}:
        for index, instrument in enumerate(request.instruments):
            candidates.extend(
                (
                    (
                        instrument.start,
                        {
                            "kind": "INSTRUMENT_START",
                            "instrument_input_index": index,
                        },
                    ),
                    (
                        instrument.maturity,
                        {
                            "kind": "INSTRUMENT_END",
                            "instrument_input_index": index,
                        },
                    ),
                )
            )
    return candidates


def _resolved_candidate_origins(
    request: object,
    candidates: list[tuple[date, dict[str, object]]],
) -> dict[date, list[dict[str, object]]]:
    resolved_origins: dict[date, list[dict[str, object]]] = {}
    for candidate_date, origin in candidates:
        if candidate_date > request.today:
            resolved_origins.setdefault(candidate_date, []).append(origin)
    return resolved_origins


def _fallback_candidate_trace(
    request: object,
    candidates: list[tuple[date, dict[str, object]]],
    resolved_origins: dict[date, list[dict[str, object]]],
) -> tuple[KnotCandidateDTO, ...]:
    final_index = {
        candidate_date: index for index, candidate_date in enumerate(sorted(resolved_origins))
    }
    trace: list[KnotCandidateDTO] = []
    seen_dates: set[date] = set()
    for ordinal, (candidate_date, origin) in enumerate(candidates):
        if candidate_date <= request.today:
            disposition = "FILTERED_NOT_AFTER_TODAY"
            resolved_index = None
        else:
            disposition = "DUPLICATE" if candidate_date in seen_dates else "ADDED"
            seen_dates.add(candidate_date)
            resolved_index = final_index[candidate_date]
        trace.append(
            KnotCandidateDTO(
                ordinal=ordinal,
                date=candidate_date,
                origin=origin,
                disposition=disposition,
                resolved_index=resolved_index,
            )
        )
    return tuple(trace)


def _fallback_resolved_nodes(
    resolved_origins: dict[date, list[dict[str, object]]],
) -> list[ResolvedKnotNodeDTO]:
    return [
        ResolvedKnotNodeDTO(date=value, origins=tuple(resolved_origins[value]))
        for value in sorted(resolved_origins)
    ]


def _fallback_storage_nodes(
    request: object, resolved: list[ResolvedKnotNodeDTO]
) -> tuple[list[ResolvedKnotNodeDTO], bool]:
    declaration = request.declaration
    storage = list(resolved)
    anchor_added = declaration.parameterization == "ZERO_RATE"
    if anchor_added:
        storage.insert(
            0,
            ResolvedKnotNodeDTO(date=request.today, origins=({"kind": "SYNTHETIC_ANCHOR"},)),
        )
    if declaration.parameterization == "LOG_DISCOUNT":
        submitted_anchor = ResolvedKnotNodeDTO(
            date=request.today,
            origins=({"kind": "INPUT", "input_knot_index": 0},),
        )
        storage.insert(0, submitted_anchor)
    return storage, anchor_added


def _fallback_free_parameters(
    declaration: object, resolved: list[ResolvedKnotNodeDTO]
) -> tuple[FreeParameterDTO, ...]:
    components = {
        "PIECEWISE_CONSTANT_FWD": ("right_forward",),
        "PIECEWISE_LINEAR_FWD": ("left_forward", "right_forward"),
        "ZERO_RATE": ("zero_rate",),
        "LOG_DISCOUNT": ("log_discount_factor",),
    }[declaration.parameterization]
    return tuple(
        FreeParameterDTO(date=node.date, component=component)
        for node in resolved
        for component in components
    )


def _fallback_instrument_candidate_count(request: object) -> int:
    if request.declaration.knot_policy in {"INSTRUMENTS", "AUGMENTED"}:
        return 2 * len(request.instruments)
    return 0


def _fallback_single_plan(request: object) -> GatewayResolvedSingleKnotPlan:
    declaration = request.declaration
    candidates = _fallback_plan_candidates(request)
    resolved_origins = _resolved_candidate_origins(request, candidates)
    resolved = _fallback_resolved_nodes(resolved_origins)
    storage, anchor_added = _fallback_storage_nodes(request, resolved)
    free = _fallback_free_parameters(declaration, resolved)
    return GatewayResolvedSingleKnotPlan(
        planner_version=1,
        requested_policy=declaration.knot_policy,
        execution_policy="INPUT",
        submitted_knot_dates=tuple(declaration.knot_dates),
        candidate_trace=_fallback_candidate_trace(request, candidates, resolved_origins),
        resolved_declared_nodes=tuple(resolved),
        storage_nodes=tuple(storage),
        free_parameters=free,
        anchor_added=anchor_added,
        counts=GatewayResolvedKnotCounts(
            submitted_knots=len(declaration.knot_dates),
            instrument_candidates=_fallback_instrument_candidate_count(request),
            resolved_declared_nodes=len(resolved),
            storage_nodes=len(storage),
            free_parameters=len(free),
        ),
    )


def _fallback_resolved_initial_guess(
    request: object, plan: GatewayResolvedSingleKnotPlan
) -> tuple[float, ...]:
    explicit = tuple(request.declaration.initial_guess_per_node)
    if explicit:
        return explicit
    if request.declaration.parameterization != "LOG_DISCOUNT":
        return (request.solver.initial_guess,) * plan.counts.free_parameters
    denominator = 365.0
    if request.declaration.libor_basis == "ACT_360":
        denominator = 360.0
    return tuple(
        -request.solver.initial_guess * ((parameter.date - request.today).days / denominator)
        for parameter in plan.free_parameters
    )


def _native_instrument_name(kind: str) -> str:
    return {
        "DEPOSIT": "Deposit",
        "FRA": "FRA",
        "FUTURE": "Future",
        "SWAP": "Swap",
        "OIS_SWAP": "OISSwap",
        "BASIS_SWAP": "BasisSwap",
        "XCCY_SWAP": "CrossCurrencySwap",
    }[kind]


def _fallback_linear_parameters(values: list[float], node_count: int) -> dict[str, object]:
    if len(values) == 2 * node_count:
        return {
            "left_forwards": values[0::2],
            "right_forwards": values[1::2],
        }
    split = len(values) // 2
    return {
        "left_forwards": values[:split],
        "right_forwards": values[split:],
    }


def _fallback_curve_state(
    parameterization: str,
    plan: ResolvedSingleKnotPlanDTO,
    values: list[float],
) -> tuple[list[date], dict[str, object]]:
    if parameterization == "ZERO_RATE":
        return (
            [node.date for node in plan.resolved_declared_nodes],
            {"zero_rates": values},
        )
    node_dates = [node.date for node in plan.storage_nodes]
    if parameterization == "PIECEWISE_CONSTANT_FWD":
        return node_dates, {"right_forwards": values}
    if parameterization == "PIECEWISE_LINEAR_FWD":
        return node_dates, _fallback_linear_parameters(values, len(node_dates))
    return node_dates, {"log_discount_factors": [0.0, *values]}


def _fallback_day_count(declaration: object) -> str:
    if declaration.parameterization in {
        "PIECEWISE_CONSTANT_FWD",
        "PIECEWISE_LINEAR_FWD",
    }:
        return "ACT_365F"
    return declaration.libor_basis


def _fallback_curve_payload(
    request: object,
    plan: ResolvedSingleKnotPlanDTO,
    values: list[float],
    *,
    role: str,
) -> dict[str, object]:
    declaration = request.declaration
    parameterization = declaration.parameterization
    node_dates, parameters = _fallback_curve_state(parameterization, plan, values)
    return {
        "name": declaration.curve_name,
        "currency": request.currency,
        "role": role,
        "target": {
            "collateral": declaration.target_collateral,
            "tenor": declaration.target_tenor,
        },
        "parameterization": parameterization,
        "anchor_date": request.today,
        "day_count": _fallback_day_count(declaration),
        "log_df_scheme": declaration.log_df_scheme,
        "node_dates": node_dates,
        "parameters": parameters,
        "base_curve_id": declaration.base_curve_id,
    }


def _fallback_instruments(request: object) -> list[object]:
    if hasattr(request, "instruments"):
        return list(request.instruments)
    return list(request.basis.instruments)


def _fallback_diagnostics(request: object, *, xccy: bool) -> tuple[InstrumentDiagnosticDTO, ...]:
    group = "basis" if xccy else "single"
    return tuple(
        InstrumentDiagnosticDTO(
            instrument_id=f"{index + 1:032x}",
            group=group,
            calibration_index=index,
            market_rate=instrument.market_rate,
            model_rate=instrument.market_rate,
            residual=0.0,
        )
        for index, instrument in enumerate(_fallback_instruments(request))
    )


def _fallback_matrix_availability(
    request: object,
) -> tuple[bool, bool]:
    exact = request.solver.solve_mode == "EXACT"
    jacobian_available = (
        request.options.include_jacobian and exact and request.options.jacobian_mode == "ANALYTIC"
    )
    inverse_available = request.options.include_effective_inverse and exact
    return jacobian_available, inverse_available


def _matrix_availability(available: bool, requested: bool) -> str:
    if available:
        return "available"
    if requested:
        return "not_available_for_mode"
    return "not_requested"


def _fallback_identity_values(rows: int, columns: int, diagonal: float) -> list[list[float]]:
    return [
        [diagonal if row == column else 0.0 for column in range(columns)] for row in range(rows)
    ]


def _fallback_matrices(
    request: object,
    parameter_axis: list[str],
    residual_axis: list[str],
) -> tuple[MatrixDTO, MatrixDTO]:
    jacobian_available, inverse_available = _fallback_matrix_availability(request)
    jacobian_values = None
    if jacobian_available:
        jacobian_values = _fallback_identity_values(len(residual_axis), len(parameter_axis), 1.0)
    inverse_values = None
    if inverse_available:
        inverse_values = _fallback_identity_values(
            len(parameter_axis),
            len(residual_axis),
            request.solver.tolerance,
        )
    return (
        MatrixDTO(
            availability=_matrix_availability(jacobian_available, request.options.include_jacobian),
            shape=(len(residual_axis), len(parameter_axis)),
            row_axis=residual_axis,
            column_axis=parameter_axis,
            scaling="unscaled",
            residual_tolerance=None,
            values=jacobian_values,
        ),
        MatrixDTO(
            availability=_matrix_availability(
                inverse_available,
                request.options.include_effective_inverse,
            ),
            shape=(len(parameter_axis), len(residual_axis)),
            row_axis=parameter_axis,
            column_axis=residual_axis,
            scaling="solver_scaled",
            residual_tolerance=request.solver.tolerance,
            values=inverse_values,
        ),
    )


def _fallback_fx_forwards(request: object, *, xccy: bool) -> FxForwardDTO | None:
    if not xccy:
        return None
    dates = list(request.basis.knot_dates)
    return FxForwardDTO(
        pair=request.pair,
        dates=dates,
        forwards=[request.fx_spot] * len(dates),
    )


def _fallback_named_ranges(
    parameter_count: int,
    residual_count: int,
    *,
    xccy: bool,
) -> NamedRangesDTO:
    name = "basis" if xccy else "single"
    return NamedRangesDTO(
        parameters=[
            NamedRangeDTO(
                name=name,
                offset=0,
                size=max(1, parameter_count),
            )
        ],
        residuals=[
            NamedRangeDTO(
                name=name,
                offset=0,
                size=max(1, residual_count),
            )
        ],
    )


def _fallback_result(
    request: object,
    curves: tuple[dict[str, object], ...],
    actual: ExecutionSingleKnotIdentityDTO | None,
    *,
    xccy: bool = False,
) -> GatewayCalibrationResult:
    diagnostics = _fallback_diagnostics(request, xccy=xccy)
    parameter_count = sum(len(values) for values in curves[0]["parameters"].values())
    parameter_axis = [f"parameter:{index}" for index in range(parameter_count)]
    residual_axis = [f"residual:{item.instrument_id}" for item in diagnostics]
    jacobian, inverse = _fallback_matrices(request, parameter_axis, residual_axis)
    return GatewayCalibrationResult(
        actual_jacobian_mode=request.options.jacobian_mode,
        actual_execution_identity=actual,
        curves=curves,
        instrument_diagnostics=diagnostics,
        solver_diagnostics=SolverDiagnosticsDTO(
            status="converged",
            solve_mode=request.solver.solve_mode,
            used_approximate_fit=False,
            tolerance=request.solver.tolerance,
            fit_tolerance=request.solver.fit_tolerance,
            max_abs_residual=0.0,
            rms_residual=0.0,
            evaluations=1,
        ),
        fx_forwards=_fallback_fx_forwards(request, xccy=xccy),
        named_ranges=_fallback_named_ranges(
            parameter_count,
            len(diagnostics),
            xccy=xccy,
        ),
        jacobian=jacobian,
        effective_inverse=inverse,
        native_solve_ms=0.0,
    )


# Process-wide singleton stored in a mutable container so get_gateway()
# does not need a `global` statement.
_gateway_box: list[DalGateway | None] = [None]
_gateway_lock = threading.Lock()


def get_gateway() -> DalGateway:
    """Return a process-wide :class:`DalGateway` singleton."""
    if _gateway_box[0] is None:
        with _gateway_lock:
            if _gateway_box[0] is None:
                _gateway_box[0] = DalGateway()
    return _gateway_box[0]
