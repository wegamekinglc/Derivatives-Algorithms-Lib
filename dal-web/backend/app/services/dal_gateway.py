"""The one and only integration surface with the DAL library.

The web backend talks to the Derivatives Algorithms Library exclusively through
its Python public API -- the compiled ``dal`` package (the dal-python pybind11
bindings; see ``dal-python/src/bindings/value.cpp``).  No router, schema or
service module imports ``dal`` directly -- they all go through :class:`DalGateway`.
"""

from __future__ import annotations

import hashlib
import math
import threading
import time
from collections.abc import Callable, Mapping, Sequence
from dataclasses import dataclass
from datetime import UTC, date, datetime, timedelta
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
from app.services.archive_preflight import preflight_archive
from app.services.calibrations import (
    NativeExecutionIdentityMismatchError,
    NativeSolverDidNotConvergeError,
    PersistedExpectedExecutionIdentityIntegrityError,
    PersistedKnotPlanIntegrityError,
    SingleGatewayPreLockRequest,
    VerifiedSingleGatewayRequest,
    VerifiedSingleWorkerAdmissionEvidence,
)
from app.services.curve_lab_fixings import canonical_utc_datetime
from app.services.curve_lab_plan import resolved_declaration_order

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


class NativeDalCapabilityError(RuntimeError):
    """Raised when a production DAL module lacks a required web capability."""


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
            if execution_spec is not None and hasattr(
                self._dal, "ValidateSingleCurveAnalyticEligibility"
            ):
                eligibility = self._dal.ValidateSingleCurveAnalyticEligibility(execution_spec)
            else:
                self._require_test_double_fallback("single-curve analytic eligibility validation")
                eligibility = None
            if execution_spec is not None and hasattr(
                self._dal, "ResolveCurveCalibrationInitialGuess"
            ):
                resolved_guess = tuple(
                    float(value)
                    for value in self._dal.ResolveCurveCalibrationInitialGuess(execution_spec)
                )
            else:
                self._require_test_double_fallback("single-curve initial-guess resolution")
                resolved_guess = _fallback_resolved_initial_guess(normalized, plan)
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
            self._notify_lock_acquired(on_lock_acquired)
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
    def _notify_lock_acquired(
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
            if not isinstance(evidence, VerifiedSingleWorkerAdmissionEvidence):
                raise TypeError("pre-native evidence callback returned the wrong carrier type")
        except (
            PersistedKnotPlanIntegrityError,
            PersistedExpectedExecutionIdentityIntegrityError,
        ):
            raise
        except Exception as exc:
            raise GatewayLifecycleTransitionError("verify_pre_native_admission_evidence") from exc
        return evidence

    def _inspect_single_execution_identity(
        self,
        native_spec: object | None,
        expected: ExecutionSingleKnotIdentityDTO,
    ) -> ExecutionSingleKnotIdentityDTO:
        if native_spec is None or not hasattr(
            self._dal, "InspectCurveCalibrationExecutionIdentity"
        ):
            self._require_test_double_fallback("single-curve execution identity inspection")
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
            self._notify_lock_acquired(on_lock_acquired)
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
                self._require_test_double_fallback("staged cross-currency calibration")
                result = self._calibrate_xccy_fallback(request.request, "xccy_staged")
            self._refresh_health_snapshot()
            return result._replace(native_solve_ms=(time.perf_counter() - started) * 1000.0)

    def validate_staged_xccy_admission(self, request: StagedXccyGatewayRequest) -> object | None:
        with self._calibration_lock:
            if not hasattr(self._dal, "ValidateCrossCurrencyAnalyticEligibility"):
                self._require_test_double_fallback(
                    "staged cross-currency analytic eligibility validation"
                )
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
            self._require_test_double_fallback("cross-currency historical fixing planning")
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
            self._notify_lock_acquired(on_lock_acquired)
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
                self._require_test_double_fallback("joint cross-currency calibration")
                result = self._calibrate_xccy_fallback(request.request, "xccy_joint")
            self._refresh_health_snapshot()
            return result._replace(native_solve_ms=(time.perf_counter() - started) * 1000.0)

    def validate_joint_xccy_admission(self, request: JointXccyGatewayRequest) -> object | None:
        with self._calibration_lock:
            if not hasattr(self._dal, "ValidateJointXccyAnalyticEligibility"):
                self._require_test_double_fallback(
                    "joint cross-currency analytic eligibility validation"
                )
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
                day_count=day_count,
                log_df_scheme=scheme,
                base=base,
            )
        raise ValueError(f"unsupported curve parameterization {dto.parameterization}")

    def build_curve_lab_archive(
        self,
        document: Mapping[str, Any],
        dependencies: Sequence[Mapping[str, Any]] = (),
    ) -> bytes:
        """Calibrate the declared mode and return exact native JSON bytes."""

        with self._calibration_lock:
            dependency_curves = self._curve_lab_dependency_curves(dependencies)
            curves = self._curve_lab_passive_curves(
                document,
                dependency_curves=dependency_curves,
            )
            extension = getattr(self._dal, "_dal", self._dal)
            root = (
                next(iter(curves.values()))
                if len(curves) == 1
                else extension._BagNew("curve-lab-set", curves)
            )
            payload = extension._StorableToJson(root)
            if not isinstance(payload, bytes):
                raise TypeError("native archive bridge must return bytes")
            return payload

    def _curve_lab_passive_curves(
        self,
        document: Mapping[str, Any],
        parameter_bumps: list[tuple[Mapping[str, Any], float]] | None = None,
        *,
        dependency_curves: Mapping[str, Any] | None = None,
        fixing_observations: Sequence[Mapping[str, Any]] = (),
    ) -> dict[str, Any]:
        dependencies = dict(dependency_curves or {})
        mode = str(document["mode"])
        if mode == "SINGLE":
            curves = self._curve_lab_single_curves(document, dependencies)
        elif mode == "MULTI_CURVE":
            curves = self._curve_lab_multi_curves(document, dependencies)
        elif mode == "STAGED_XCCY":
            curves = self._curve_lab_staged_xccy_curves(
                document,
                dependencies,
                fixing_observations,
            )
        elif mode == "JOINT_XCCY":
            curves = self._curve_lab_joint_xccy_curves(
                document,
                dependencies,
                fixing_observations,
            )
        else:
            raise ValueError(f"unsupported Curve Lab build mode {mode!r}")
        if parameter_bumps:
            curves = self._curve_lab_bumped_curves(
                curves,
                document["declarations"],
                parameter_bumps,
            )
        return curves

    def _curve_lab_single_curves(
        self,
        document: Mapping[str, Any],
        dependency_curves: Mapping[str, Any],
    ) -> dict[str, Any]:
        declarations = list(document["declarations"])
        if len(declarations) != 1:
            raise ValueError("SINGLE Curve Lab mode requires exactly one declaration")
        declaration = declarations[0]
        instruments = self._curve_lab_declaration_instruments(document, declaration)
        if hasattr(self._dal, "CurveCalibrationSpecBuilder_"):
            builder = self._curve_lab_curve_builder(
                document,
                declaration,
                instruments,
                dependency_curves,
            )
            native_result = self._dal.CalibrateSingleCurve(builder.Build())
            return {str(declaration["component_key"]): native_result.curve_}
        self._require_test_double_fallback("Curve Lab single-curve calibration")
        return self._curve_lab_fallback_curves(document, declarations)

    def _curve_lab_multi_curves(
        self,
        document: Mapping[str, Any],
        dependency_curves: Mapping[str, Any],
    ) -> dict[str, Any]:
        declarations = list(document["declarations"])
        if not hasattr(self._dal, "MultiCurveCalibrationSpec_"):
            raise RuntimeError("MULTI_CURVE native calibration is unavailable")
        currencies = {str(declaration["currency"]) for declaration in declarations}
        if len(currencies) != 1:
            raise ValueError("MULTI_CURVE declarations must share one currency")
        stages = [
            self._curve_lab_curve_builder(
                document,
                declaration,
                self._curve_lab_declaration_instruments(document, declaration),
                dependency_curves,
            ).Build()
            for declaration in declarations
        ]
        multi = self._dal.MultiCurveCalibrationSpec_()
        multi.name_ = self._dal.String_(f"curve-lab-{next(iter(currencies))}")
        multi.ccy_ = self._dal.String_(next(iter(currencies)))
        multi.liborBasis_ = self._dal.DayBasis_New(
            str(document.get("solver", {}).get("libor_basis", "ACT_365F"))
        )
        multi.stages_ = stages
        native_result = self._dal.CalibrateMultiCurveBundle(multi)
        curves = dict(dependency_curves)
        for declaration in declarations:
            key = str(declaration["component_key"])
            route = key.rsplit("/", 1)[-1]
            role = str(declaration["role"])
            if role == "DISCOUNT":
                curves[key] = _native_map_value(
                    native_result.discountCurves_,
                    self._dal.CollateralType_(route),
                )
            elif role == "PROJECTION":
                curves[key] = _native_map_value(
                    native_result.forwardCurves_,
                    self._dal.PeriodLength_New(route),
                )
            else:
                raise ValueError("MULTI_CURVE supports discount/projection declarations only")
        return {
            str(declaration["component_key"]): curves[str(declaration["component_key"])]
            for declaration in declarations
        }

    def _curve_lab_dependency_curves(
        self,
        dependencies: Sequence[Mapping[str, Any]],
    ) -> dict[str, Any]:
        """Reconstruct the immutable component set pinned by dependency versions."""

        extension = getattr(self._dal, "_dal", self._dal)
        curves: dict[str, Any] = {}
        for version in dependencies:
            payload = version.get("native_payload")
            expected_hash = version.get("native_payload_hash")
            if not isinstance(payload, bytes) or not isinstance(expected_hash, str):
                raise ValueError("dependency archive context is unavailable")
            if hashlib.sha256(payload).hexdigest() != expected_hash:
                raise ValueError("dependency archive hash mismatch")
            preflight = preflight_archive(payload)
            expected_kind = str(version.get("root_kind"))
            observed_kind = (
                "CURVE_SET" if preflight.root_type in {"Bag", "Bag_v1"} else "DISCOUNT_CURVE"
            )
            if observed_kind != expected_kind:
                raise ValueError("dependency archive root kind mismatch")
            root = extension._StorableFromJson(preflight.payload)
            if observed_kind == "CURVE_SET":
                rebuilt = {str(key): value for key, value in extension._BagContents(root).items()}
            else:
                verification = version.get("verification")
                document = (
                    verification.get("document") if isinstance(verification, Mapping) else None
                )
                declarations = (
                    document.get("declarations") if isinstance(document, Mapping) else None
                )
                if not isinstance(declarations, list) or len(declarations) != 1:
                    raise ValueError("single-curve dependency component identity is unavailable")
                component_key = declarations[0].get("component_key")
                if not isinstance(component_key, str):
                    raise ValueError("single-curve dependency component identity is unavailable")
                rebuilt = {component_key: root}
            duplicate_keys = curves.keys() & rebuilt.keys()
            if duplicate_keys:
                duplicate = sorted(duplicate_keys)[0]
                raise ValueError(f"duplicate Curve Lab dependency component {duplicate!r}")
            curves.update(rebuilt)
        return curves

    def _curve_lab_archive_curves(
        self,
        payload: bytes,
        root_kind: str,
        document: Mapping[str, Any],
        expected_hash: str | None = None,
    ) -> dict[str, Any]:
        """Restore the selected immutable archive without invoking calibration."""

        if expected_hash is not None and hashlib.sha256(payload).hexdigest() != expected_hash:
            raise ValueError("selected Curve Lab version archive hash mismatch")
        preflight = preflight_archive(payload)
        observed_kind = (
            "CURVE_SET" if preflight.root_type in {"Bag", "Bag_v1"} else "DISCOUNT_CURVE"
        )
        if observed_kind != root_kind:
            raise ValueError("selected Curve Lab version root kind mismatch")
        extension = getattr(self._dal, "_dal", self._dal)
        root = extension._StorableFromJson(preflight.payload)
        if root_kind == "CURVE_SET":
            return {str(key): value for key, value in extension._BagContents(root).items()}
        declarations = list(document.get("declarations", ()))
        if len(declarations) != 1:
            raise ValueError("single-curve runtime component identity is unavailable")
        return {str(declarations[0]["component_key"]): root}

    def curve_lab_archive_parameter_axis(
        self,
        document: Mapping[str, Any],
        payload: bytes,
    ) -> list[dict[str, Any]]:
        """Project the free-parameter axis from the calibrated native archive."""

        root_kind = "DISCOUNT_CURVE" if document["mode"] == "SINGLE" else "CURVE_SET"
        with self._calibration_lock:
            curves = self._curve_lab_archive_curves(payload, root_kind, document)
            return self._curve_lab_parameter_axis_from_curves(document, curves)

    def curve_lab_archive_curve_views(
        self,
        document: Mapping[str, Any],
        payload: bytes,
        parameter_axis: Sequence[Mapping[str, Any]],
    ) -> list[dict[str, Any]]:
        """Project numerical curve views from the calibrated native archive."""

        root_kind = "DISCOUNT_CURVE" if document["mode"] == "SINGLE" else "CURVE_SET"
        with self._calibration_lock:
            curves = self._curve_lab_archive_curves(payload, root_kind, document)
            return self._curve_lab_curve_views_from_curves(
                document,
                curves,
                parameter_axis,
            )

    def _curve_lab_curve_views_from_curves(
        self,
        document: Mapping[str, Any],
        curves: Mapping[str, Any],
        parameter_axis: Sequence[Mapping[str, Any]],
    ) -> list[dict[str, Any]]:
        as_of = date.fromisoformat(str(document["as_of_date"]))
        result: list[dict[str, Any]] = []
        for axis in parameter_axis:
            node = date.fromisoformat(str(axis["node_date"]))
            curve = curves[str(axis["component_key"])]
            discount_factor = self._curve_lab_discount_factor(curve, as_of, node)
            elapsed_days = (node - as_of).days
            zero_rate = (
                None if elapsed_days == 0 else -math.log(discount_factor) * 365.0 / elapsed_days
            )
            left_side = axis.get("side") == "LEFT"
            forward_start = node - timedelta(days=1) if left_side else node
            forward_end = node if left_side else node + timedelta(days=1)
            one_day_discount = self._curve_lab_discount_factor(
                curve,
                forward_start,
                forward_end,
            )
            result.append(
                {
                    "parameter_id": str(axis["parameter_id"]),
                    "component_key": str(axis["component_key"]),
                    "node_date": str(axis["node_date"]),
                    "side": axis.get("side"),
                    "discount_factor": discount_factor,
                    "zero_rate": zero_rate,
                    "one_day_forward_rate": -math.log(one_day_discount) * 365.0,
                }
            )
        return result

    def _curve_lab_discount_factor(
        self,
        curve: Any,
        start: date,
        end: date,
    ) -> float:
        if isinstance(curve, Mapping):
            return self._curve_lab_mapping_discount_factor(curve, start, end)
        value = float(curve(self._native_date(start), self._native_date(end)))
        if not math.isfinite(value) or value <= 0.0:
            raise ValueError("Curve Lab numerical view requires positive finite discount factors")
        return value

    @classmethod
    def _curve_lab_mapping_discount_factor(
        cls,
        curve: Mapping[str, Any],
        start: date,
        end: date,
    ) -> float:
        if end < start:
            return 1.0 / cls._curve_lab_mapping_discount_factor(curve, end, start)
        dates = [
            date.fromisoformat(str(item)) for item in curve.get("dates", curve.get("knotDates", ()))
        ]
        values = [float(item) for item in curve.get("values", curve.get("rightVals", ()))]
        if not dates or len(dates) != len(values):
            raise ValueError("Curve Lab test curve requires matching PWC dates and values")
        integral = 0.0
        cursor = start
        while cursor < end:
            right_index = max(
                (index for index, knot in enumerate(dates) if knot <= cursor),
                default=0,
            )
            next_knot = min((knot for knot in dates if knot > cursor), default=end)
            segment_end = min(end, next_knot)
            integral += (segment_end - cursor).days * values[right_index]
            cursor = segment_end
        value = math.exp(-integral / 365.0)
        base = curve.get("base")
        if isinstance(base, Mapping):
            value *= cls._curve_lab_mapping_discount_factor(base, start, end)
        if not math.isfinite(value) or value <= 0.0:
            raise ValueError("Curve Lab numerical view requires positive finite discount factors")
        return value

    def _curve_lab_parameter_axis_from_curves(
        self,
        document: Mapping[str, Any],
        curves: Mapping[str, Any],
    ) -> list[dict[str, Any]]:
        declarations = self._curve_lab_resolved_declaration_order(document)
        result: list[dict[str, Any]] = []
        stage_offsets: dict[str, int] = {}
        for declaration_index, declaration in enumerate(declarations):
            key = str(declaration["component_key"])
            curve = curves[key]
            representation = str(declaration["parameterization"])
            stage_id = (
                "stage-0"
                if document["mode"] in {"SINGLE", "MULTI_CURVE", "JOINT_XCCY"}
                else f"stage-{declaration_index}"
            )
            coordinates: list[tuple[object, str | None]] = []
            if representation == "PIECEWISE_CONSTANT_FWD":
                coordinates = [
                    (item, "RIGHT")
                    for item in self._curve_lab_curve_member(
                        curve,
                        "knot_dates",
                        "dates",
                    )
                ]
            elif representation == "PIECEWISE_LINEAR_FWD":
                coordinates = [
                    (item, side)
                    for item in self._curve_lab_curve_member(
                        curve,
                        "knot_dates",
                        "dates",
                    )
                    for side in ("LEFT", "RIGHT")
                ]
            elif representation == "ZERO_RATE":
                coordinates = [
                    (item, None)
                    for item in self._curve_lab_curve_member(
                        curve,
                        "node_dates",
                        "dates",
                    )
                ]
            elif representation == "LOG_DISCOUNT":
                if isinstance(curve, Mapping):
                    dates = list(curve.get("node_dates", curve.get("dates", ())))
                    free_dates = dates
                else:
                    dates = list(curve.node_dates)
                    free_dates = dates[1:]
                coordinates = [(item, None) for item in free_dates]
            else:  # pragma: no cover - closed request schema prevents this
                raise ValueError(f"unsupported Curve Lab parameterization {representation!r}")
            for local_index, (node, side) in enumerate(coordinates):
                node_date = repr(node) if hasattr(node, "_d") else str(node)
                side_token = side or "SINGLE"
                stage_local = stage_offsets.get(stage_id, 0)
                stage_offsets[stage_id] = stage_local + 1
                result.append(
                    {
                        "global_parameter_index": len(result),
                        "parameter_id": (f"{key}:{representation}:{node_date}:{side_token}"),
                        "component_key": key,
                        "stage_id": stage_id,
                        "stage_local_parameter_index": stage_local,
                        "component_local_parameter_index": local_index,
                        "coordinate_kind": representation,
                        "node_date": node_date,
                        "side": side,
                        "native_parameter_unit": (
                            "LOG_DISCOUNT_FACTOR"
                            if representation == "LOG_DISCOUNT"
                            else "DECIMAL_RATE"
                        ),
                        "display_label": (
                            f"{declaration['currency']} "
                            f"{key.rsplit('/', 1)[-1]} {node_date} {side_token}"
                        ),
                    }
                )
        return result

    @staticmethod
    def _curve_lab_curve_member(
        curve: Any,
        attribute: str,
        fallback_key: str,
    ) -> Sequence[Any]:
        if isinstance(curve, Mapping):
            value = curve.get(attribute, curve.get(fallback_key))
        else:
            value = getattr(curve, attribute)
        if not isinstance(value, Sequence):
            raise ValueError(f"native curve does not expose ordered {attribute} coordinates")
        return value

    @staticmethod
    def _curve_lab_resolved_declaration_order(
        document: Mapping[str, Any],
    ) -> list[Mapping[str, Any]]:
        return resolved_declaration_order(document)

    def _curve_lab_staged_xccy_curves(
        self,
        document: Mapping[str, Any],
        dependency_curves: Mapping[str, Any],
        fixing_observations: Sequence[Mapping[str, Any]] = (),
    ) -> dict[str, Any]:
        declarations = list(document["declarations"])
        basis_declaration = self._curve_lab_basis_declaration(declarations)
        xccy_instruments = self._curve_lab_declaration_instruments(
            document,
            basis_declaration,
        )
        if not xccy_instruments:
            raise ValueError("STAGED_XCCY requires an included XCCY instrument")
        domestic, foreign = self._curve_lab_xccy_pair(xccy_instruments[0])
        curves = self._curve_lab_local_currency_curves(
            document,
            declarations,
            dependency_curves,
        )
        builder = self._dal.CrossCurrencyCalibrationSpecBuilder_()
        today = self._native_date(date.fromisoformat(str(document["as_of_date"])))
        builder.today_ = today
        builder.valuation_time = self._dal.DateTime_(today, 0, 0)
        builder.collateral_currency = self._dal.Ccy_(domestic)
        builder.fixings = self._dal.MarketFixingSnapshot_New(
            self._curve_lab_fixing_values(fixing_observations)
        )
        builder.basis_pair = self._dal.CurrencyPair_New(domestic, foreign)
        builder.domestic_curve_block = self._curve_lab_native_curve_block(
            domestic,
            declarations,
            curves,
        )
        builder.foreign_curve_block = self._curve_lab_native_curve_block(
            foreign,
            declarations,
            curves,
        )
        terms = xccy_instruments[0]["terms"]
        builder.fx_spot = float(terms["fx_spot"])
        builder.fx_forward_collateral = self._dal.CollateralType_(
            str(terms.get("fx_forward_collateral", "OIS"))
        )
        builder.instruments = [self._curve_lab_xccy_instrument(item) for item in xccy_instruments]
        builder.knot_dates = [
            self._native_date(date.fromisoformat(str(item["maturity_date"])))
            for item in sorted(
                xccy_instruments,
                key=lambda item: (
                    str(item["maturity_date"]),
                    str(item["instrument_id"]),
                ),
            )
        ]
        self._curve_lab_configure_solver(builder, document)
        native_result = self._dal.CalibrateXccyMarket(builder.Build())
        curves[str(basis_declaration["component_key"])] = native_result.basis_curve
        return {
            str(declaration["component_key"]): curves[str(declaration["component_key"])]
            for declaration in declarations
        }

    def _curve_lab_joint_xccy_curves(
        self,
        document: Mapping[str, Any],
        dependency_curves: Mapping[str, Any],
        fixing_observations: Sequence[Mapping[str, Any]] = (),
    ) -> dict[str, Any]:
        if dependency_curves:
            raise ValueError("JOINT_XCCY cannot layer jointly calibrated curves over dependencies")
        declarations = list(document["declarations"])
        basis_declaration = self._curve_lab_basis_declaration(declarations)
        xccy_instruments = self._curve_lab_declaration_instruments(
            document,
            basis_declaration,
        )
        if not xccy_instruments:
            raise ValueError("JOINT_XCCY requires an included XCCY instrument")
        domestic, foreign = self._curve_lab_xccy_pair(xccy_instruments[0])
        basis = self._dal.XccyBasisCurveDeclaration_()
        basis.curve_name = str(basis_declaration["component_key"])
        basis.instruments = [self._curve_lab_xccy_instrument(item) for item in xccy_instruments]
        basis.knot_dates = [
            self._native_date(date.fromisoformat(str(item["maturity_date"])))
            for item in sorted(
                xccy_instruments,
                key=lambda item: (
                    str(item["maturity_date"]),
                    str(item["instrument_id"]),
                ),
            )
        ]
        basis.parameterization = getattr(
            self._dal.CurveParameterization,
            str(basis_declaration["parameterization"]),
        )
        basis.log_df_scheme = getattr(
            self._dal.LogDfScheme,
            str(basis_declaration.get("log_df_scheme", "LOG_LINEAR")),
        )
        solver = document.get("solver", {})
        basis.smoothing_weight = float(solver.get("smoothing_weight", 1.0))
        basis.initial_guess_per_node = [float(solver.get("initial_guess", 0.01))] * len(
            basis.knot_dates
        )

        today = self._native_date(date.fromisoformat(str(document["as_of_date"])))
        builder = self._dal.JointXccyCalibrationSpecBuilder_()
        builder.valuation_time = self._dal.DateTime_(today, 0, 0)
        builder.pair = self._dal.CurrencyPair_New(domestic, foreign)
        builder.collateral_currency = self._dal.Ccy_(domestic)
        builder.fx_spot = float(xccy_instruments[0]["terms"]["fx_spot"])
        builder.domestic = self._curve_lab_joint_currency_spec(
            document,
            domestic,
            declarations,
        )
        builder.foreign = self._curve_lab_joint_currency_spec(
            document,
            foreign,
            declarations,
        )
        builder.basis = basis
        builder.fixings = self._dal.MarketFixingSnapshot_New(
            self._curve_lab_fixing_values(fixing_observations)
        )
        self._curve_lab_configure_solver(builder.solver_options, document)
        native_result = self._dal.CalibrateJointXccyMarket(builder.Build())
        curves = self._curve_lab_curves_from_blocks(
            declarations,
            {
                domestic: native_result.domestic_curve_block,
                foreign: native_result.foreign_curve_block,
            },
        )
        curves[str(basis_declaration["component_key"])] = native_result.basis_curve
        return curves

    def _curve_lab_local_currency_curves(
        self,
        document: Mapping[str, Any],
        declarations: list[Mapping[str, Any]],
        dependency_curves: Mapping[str, Any],
    ) -> dict[str, Any]:
        curves = dict(dependency_curves)
        local_declarations = [
            declaration for declaration in declarations if declaration["role"] != "BASIS"
        ]
        for currency in dict.fromkeys(
            str(declaration["currency"]) for declaration in local_declarations
        ):
            currency_declarations = [
                declaration
                for declaration in local_declarations
                if str(declaration["currency"]) == currency
            ]
            local_document = {
                **document,
                "mode": ("SINGLE" if len(currency_declarations) == 1 else "MULTI_CURVE"),
                "declarations": currency_declarations,
            }
            curves.update(
                self._curve_lab_passive_curves(
                    local_document,
                    dependency_curves=curves,
                )
            )
        return curves

    def _curve_lab_native_curve_block(
        self,
        currency: str,
        declarations: list[Mapping[str, Any]],
        curves: Mapping[str, Any],
    ) -> Any:
        discounts = {
            self._dal.CollateralType_(str(declaration["component_key"]).rsplit("/", 1)[-1]): curves[
                str(declaration["component_key"])
            ]
            for declaration in declarations
            if declaration["role"] == "DISCOUNT" and str(declaration["currency"]) == currency
        }
        forwards = {
            self._dal.PeriodLength_New(
                str(declaration["component_key"]).rsplit("/", 1)[-1]
            ): curves[str(declaration["component_key"])]
            for declaration in declarations
            if declaration["role"] == "PROJECTION" and str(declaration["currency"]) == currency
        }
        if not discounts:
            raise ValueError(f"XCCY build is missing a discount declaration for {currency}")
        currency_declaration = next(
            declaration
            for declaration in declarations
            if str(declaration["currency"]) == currency and declaration["role"] != "BASIS"
        )
        return self._dal.CurveBlock_New(
            f"curve-lab-{currency}",
            currency,
            discounts,
            forwards,
            self._dal.DayBasis_New(str(currency_declaration.get("libor_basis", "ACT_365F"))),
        )

    def _curve_lab_joint_currency_spec(
        self,
        document: Mapping[str, Any],
        currency: str,
        declarations: list[Mapping[str, Any]],
    ) -> Any:
        native_declarations = []
        for declaration in declarations:
            if declaration["role"] == "BASIS" or str(declaration["currency"]) != currency:
                continue
            instruments = self._curve_lab_declaration_instruments(
                document,
                declaration,
            )
            if not instruments:
                raise ValueError(
                    f"Curve Lab declaration {declaration['component_key']!r} has no instruments"
                )
            native = self._dal.JointCurveDeclaration_()
            native.curve_name = str(declaration["component_key"])
            native.instruments = [
                self._curve_lab_calibration_instrument(
                    item,
                    use_projection_curve_default=(declaration["role"] == "PROJECTION"),
                )
                for item in instruments
            ]
            native.knot_dates = [
                self._native_date(date.fromisoformat(str(item["maturity_date"])))
                for item in sorted(
                    instruments,
                    key=lambda item: (
                        str(item["maturity_date"]),
                        str(item["instrument_id"]),
                    ),
                )
            ]
            route = str(declaration["component_key"]).rsplit("/", 1)[-1]
            native.target_collateral = self._dal.CollateralType_(
                route if declaration["role"] == "DISCOUNT" else "OIS"
            )
            if declaration["role"] == "PROJECTION":
                native.target_tenor = self._dal.PeriodLength_New(route)
            native.calibrate_discount_curve = declaration["role"] == "DISCOUNT"
            native.parameterization = getattr(
                self._dal.CurveParameterization,
                str(declaration["parameterization"]),
            )
            native.log_df_scheme = getattr(
                self._dal.LogDfScheme,
                str(declaration.get("log_df_scheme", "LOG_LINEAR")),
            )
            solver = document.get("solver", {})
            native.smoothing_weight = float(solver.get("smoothing_weight", 1.0))
            native.initial_guess_per_node = [float(solver.get("initial_guess", 0.05))] * len(
                native.knot_dates
            )
            native_declarations.append(native)
        if not native_declarations:
            raise ValueError(f"JOINT_XCCY has no local declarations for {currency}")
        result = self._dal.JointCurrencyCurveSpec_()
        result.ccy = self._dal.Ccy_(currency)
        result.libor_basis = self._dal.DayBasis_New(
            str(document.get("solver", {}).get("libor_basis", "ACT_365F"))
        )
        result.curves = native_declarations
        return result

    def _curve_lab_curves_from_blocks(
        self,
        declarations: list[Mapping[str, Any]],
        blocks: Mapping[str, Any],
    ) -> dict[str, Any]:
        curves: dict[str, Any] = {}
        for declaration in declarations:
            if declaration["role"] == "BASIS":
                continue
            block = blocks[str(declaration["currency"])]
            route = str(declaration["component_key"]).rsplit("/", 1)[-1]
            if declaration["role"] == "DISCOUNT":
                curve = _native_map_value(
                    block.discount_curves,
                    self._dal.CollateralType_(route),
                )
            else:
                curve = _native_map_value(
                    block.forward_curves,
                    self._dal.PeriodLength_New(route),
                )
            curves[str(declaration["component_key"])] = curve
        return curves

    @staticmethod
    def _curve_lab_basis_declaration(
        declarations: list[Mapping[str, Any]],
    ) -> Mapping[str, Any]:
        basis = [declaration for declaration in declarations if declaration["role"] == "BASIS"]
        if len(basis) != 1:
            raise ValueError("XCCY modes require exactly one basis declaration")
        return basis[0]

    @staticmethod
    def _curve_lab_xccy_pair(
        instrument: Mapping[str, Any],
    ) -> tuple[str, str]:
        token = str(instrument["currency_or_pair"]).replace("/", "-")
        parts = token.split("-")
        if len(parts) != 2 or not all(parts):
            raise ValueError("XCCY currency_or_pair must contain two currencies")
        return parts[0], parts[1]

    def _curve_lab_xccy_instrument(
        self,
        instrument: Mapping[str, Any],
    ) -> Any:
        if instrument["instrument_type"] != "XCCY":
            raise ValueError("XCCY basis declarations may only contain XCCY instruments")
        terms = instrument["terms"]
        domestic, foreign = self._curve_lab_xccy_pair(instrument)
        return self._dal.CrossCurrencySwap_New(
            self._native_date(date.fromisoformat(str(instrument["trade_date"]))),
            self._native_date(date.fromisoformat(str(instrument["start_date"]))),
            self._native_date(date.fromisoformat(str(instrument["maturity_date"]))),
            float(instrument["normalized_quote"]),
            self._dal.CurrencyPair_New(domestic, foreign),
            float(terms["domestic_notional"]),
            float(terms["foreign_notional"]),
            self._curve_lab_rate_leg(terms, "domestic"),
            self._curve_lab_rate_index(terms, "domestic_"),
            self._curve_lab_rate_leg(terms, "foreign"),
            self._curve_lab_rate_index(terms, "foreign_"),
        )

    def _curve_lab_configure_solver(
        self,
        target: Any,
        document: Mapping[str, Any],
    ) -> None:
        solver = document.get("solver", {})
        target.smoothing_weight = float(solver.get("smoothing_weight", 1.0))
        target.tolerance = float(solver.get("tolerance", 1.0e-8))
        target.fit_tolerance = float(solver.get("fit_tolerance", 1.0e-6))
        target.initial_guess = float(solver.get("initial_guess", 0.01))
        target.max_evaluations = int(solver.get("max_evaluations", 200))
        target.max_restarts = int(solver.get("max_restarts", 20))
        target.solve_mode = getattr(
            self._dal.CurveSolveMode,
            str(solver.get("solve_mode", "EXACT")),
        )

    def _curve_lab_curve_builder(
        self,
        document: Mapping[str, Any],
        declaration: Mapping[str, Any],
        instruments: list[Mapping[str, Any]],
        dependency_curves: Mapping[str, Any],
    ) -> Any:
        if not instruments:
            raise ValueError(
                f"Curve Lab declaration {declaration['component_key']!r} has no instruments"
            )
        builder = self._dal.CurveCalibrationSpecBuilder_()
        today = self._native_date(date.fromisoformat(str(document["as_of_date"])))
        builder.today_ = today
        builder.ccy_ = self._dal.String_(str(declaration["currency"]))
        builder.curveName_ = self._dal.String_(str(declaration["component_key"]))
        builder.instruments_ = [
            self._curve_lab_calibration_instrument(
                item,
                use_projection_curve_default=(declaration["role"] == "PROJECTION"),
            )
            for item in instruments
        ]
        knot_dates = [
            self._native_date(date.fromisoformat(str(item["maturity_date"])))
            for item in sorted(
                instruments,
                key=lambda item: (
                    str(item["maturity_date"]),
                    str(item["instrument_id"]),
                ),
            )
        ]
        if declaration["parameterization"] == "PIECEWISE_CONSTANT_FWD":
            knot_dates.insert(0, today)
        builder.knotDates_ = knot_dates
        route = str(declaration["component_key"]).rsplit("/", 1)[-1]
        builder.targetCollateral_ = self._dal.CollateralType_(
            route if declaration["role"] == "DISCOUNT" else "OIS"
        )
        if declaration["role"] == "PROJECTION":
            builder.targetTenor_ = self._dal.PeriodLength_New(route)
        builder.calibrateDiscountCurve_ = declaration["role"] == "DISCOUNT"
        builder.liborBasis_ = self._dal.DayBasis_New(
            str(document.get("solver", {}).get("libor_basis", "ACT_365F"))
        )
        builder.parameterization_ = getattr(
            self._dal.CurveParameterization,
            str(declaration["parameterization"]),
        )
        builder.knotPolicy_ = self._dal.CurveKnotPolicy.INPUT
        builder.logDfScheme_ = getattr(
            self._dal.LogDfScheme,
            str(declaration.get("log_df_scheme", "LOG_LINEAR")),
        )
        solver = document.get("solver", {})
        builder.smoothingWeight_ = float(solver.get("smoothing_weight", 1.0))
        builder.tolerance_ = float(solver.get("tolerance", 1.0e-8))
        builder.fitTolerance_ = float(solver.get("fit_tolerance", 1.0e-6))
        builder.maxEvaluations_ = int(solver.get("max_evaluations", 200))
        builder.maxRestarts_ = int(solver.get("max_restarts", 20))
        builder.initialGuess_ = float(solver.get("initial_guess", 0.05))
        builder.solveMode_ = getattr(
            self._dal.CurveSolveMode,
            str(solver.get("solve_mode", "EXACT")),
        )
        builder.discountCurves_ = {
            self._dal.CollateralType_(key.rsplit("/", 1)[-1]): curve
            for key, curve in dependency_curves.items()
            if "/discount/" in key and f"/{declaration['currency']}/" in key
        }
        builder.forwardCurves_ = {
            self._dal.PeriodLength_New(key.rsplit("/", 1)[-1]): curve
            for key, curve in dependency_curves.items()
            if "/projection/" in key and f"/{declaration['currency']}/" in key
        }
        return builder

    def _curve_lab_fallback_curves(
        self,
        document: Mapping[str, Any],
        declarations: list[Mapping[str, Any]],
    ) -> dict[str, Any]:
        self._require_test_double_fallback("Curve Lab passive curve construction")
        curves: dict[str, Any] = {}
        for declaration in declarations:
            instruments = self._curve_lab_declaration_instruments(
                document,
                declaration,
            )
            ordered = sorted(
                instruments,
                key=lambda item: (
                    str(item["maturity_date"]),
                    str(item["instrument_id"]),
                ),
            )
            dates, values = self._curve_lab_dates_and_values(ordered)
            key = str(declaration["component_key"])
            curves[key] = self._dal.DiscountPWC_New(
                key,
                str(declaration["currency"]),
                dates,
                values,
            )
        return curves

    def _curve_lab_declaration_instruments(
        self,
        document: Mapping[str, Any],
        declaration: Mapping[str, Any],
    ) -> list[Mapping[str, Any]]:
        default_component = str(document["declarations"][0]["component_key"])
        component_key = str(declaration["component_key"])
        return [
            item
            for item in document["instruments"]
            if item.get("included", True)
            and str(item.get("terms", {}).get("component_key", default_component)) == component_key
        ]

    def _curve_lab_dates_and_values(
        self,
        ordered: list[Mapping[str, Any]],
    ) -> tuple[list[Any], list[float]]:
        if not ordered:
            raise ValueError("Curve Lab build requires an included instrument")
        dates: list[Any] = []
        values: list[float] = []
        seen_dates: set[str] = set()
        for item in ordered:
            maturity = str(item["maturity_date"])
            if maturity in seen_dates:
                values[-1] = float(item["normalized_quote"])
                continue
            seen_dates.add(maturity)
            dates.append(self._native_date(date.fromisoformat(maturity)))
            values.append(float(item["normalized_quote"]))
        return dates, values

    def _curve_lab_rate_index(
        self,
        terms: Mapping[str, Any],
        prefix: str = "",
        *,
        use_projection_curve_default: bool = False,
    ) -> Any:
        return self._dal.RateIndexConvention_New(
            self._dal.PeriodLength_New(
                str(
                    terms.get(
                        f"{prefix}forecast_tenor",
                        terms.get("forecast_tenor", "3M"),
                    )
                )
            ),
            self._dal.DayBasis_New(
                str(
                    terms.get(
                        f"{prefix}day_basis",
                        terms.get("day_basis", "ACT_365F"),
                    )
                )
            ),
            self._dal.CollateralType_(
                str(
                    terms.get(
                        f"{prefix}collateral",
                        terms.get("collateral", "OIS"),
                    )
                )
            ),
            bool(
                terms.get(
                    f"{prefix}use_projection_curve",
                    terms.get(
                        "use_projection_curve",
                        use_projection_curve_default,
                    ),
                )
            ),
        )

    def _curve_lab_rate_leg(
        self,
        terms: Mapping[str, Any],
        prefix: str,
    ) -> Any:
        return self._dal.RateLegConvention_New(
            self._dal.PeriodLength_New(str(terms.get(f"{prefix}_payment_frequency", "12M"))),
            self._dal.DayBasis_New(str(terms.get(f"{prefix}_day_basis", "ACT_365F"))),
        )

    def _curve_lab_bumped_curves(
        self,
        curves: Mapping[str, Any],
        declarations: list[Mapping[str, Any]],
        parameter_bumps: list[tuple[Mapping[str, Any], float]],
    ) -> dict[str, Any]:
        """Rebuild calibrated curves after exact native-parameter shifts."""

        result = dict(curves)
        declarations_by_key = {str(item["component_key"]): item for item in declarations}
        for axis, bump in parameter_bumps:
            key = str(axis["component_key"])
            declaration = declarations_by_key[key]
            curve = result[key]
            representation = str(axis["coordinate_kind"])
            local_index = int(axis["component_local_parameter_index"])
            name = str(declaration["component_key"])
            currency = str(declaration["currency"])
            if representation == "PIECEWISE_CONSTANT_FWD":
                values = list(curve.right_forwards)
                values[local_index] += bump
                result[key] = self._dal.DiscountPWC_New(
                    name,
                    currency,
                    list(curve.knot_dates),
                    values,
                    curve.base,
                )
            elif representation == "PIECEWISE_LINEAR_FWD":
                left = list(curve.left_forwards)
                right = list(curve.right_forwards)
                index, side = divmod(local_index, 2)
                (left if side == 0 else right)[index] += bump
                result[key] = self._dal.DiscountPWLF_New(
                    name,
                    currency,
                    list(curve.knot_dates),
                    left,
                    right,
                    curve.base,
                )
            elif representation == "ZERO_RATE":
                values = list(curve.zero_rates)
                values[local_index] += bump
                result[key] = self._dal.DiscountZeroRate_New(
                    name,
                    currency,
                    curve.anchor_date,
                    list(curve.node_dates),
                    values,
                    day_count=self._dal.DayBasis_New(curve.day_count),
                    log_df_scheme=curve.log_df_scheme,
                    base=curve.base,
                )
            elif representation == "LOG_DISCOUNT":
                values = list(curve.log_discount_factors)
                values[local_index + 1] += bump
                result[key] = self._dal.DiscountLogDF_New(
                    name,
                    currency,
                    list(curve.node_dates),
                    values,
                    day_count=self._dal.DayBasis_New(curve.day_count),
                    log_df_scheme=curve.log_df_scheme,
                    base=curve.base,
                )
            else:
                raise ValueError(f"unsupported Curve Lab parameter coordinate {representation!r}")
        return result

    def curve_lab_parameter_values(
        self,
        document: Mapping[str, Any],
        parameter_axis: list[Mapping[str, Any]],
        dependencies: Sequence[Mapping[str, Any]] = (),
    ) -> list[str]:
        """Return calibrated parameter values in the persisted global axis."""

        with self._calibration_lock:
            dependency_curves = self._curve_lab_dependency_curves(dependencies)
            curves = self._curve_lab_passive_curves(
                document,
                dependency_curves=dependency_curves,
            )
            result: list[str] = []
            for axis in parameter_axis:
                curve = curves[str(axis["component_key"])]
                representation = str(axis["coordinate_kind"])
                local_index = int(axis["component_local_parameter_index"])
                if representation == "PIECEWISE_CONSTANT_FWD":
                    value = curve.right_forwards[local_index]
                elif representation == "PIECEWISE_LINEAR_FWD":
                    index, side = divmod(local_index, 2)
                    values = curve.left_forwards if side == 0 else curve.right_forwards
                    value = values[index]
                elif representation == "ZERO_RATE":
                    value = curve.zero_rates[local_index]
                elif representation == "LOG_DISCOUNT":
                    value = curve.log_discount_factors[local_index + 1]
                else:
                    raise ValueError(
                        f"unsupported Curve Lab parameter coordinate {representation!r}"
                    )
                result.append(str(value))
            return result

    def _curve_lab_calibration_instrument(
        self,
        instrument: Mapping[str, Any],
        *,
        use_projection_curve_default: bool = False,
    ) -> Any:
        terms = instrument["terms"]
        trade_date = self._native_date(date.fromisoformat(str(instrument["trade_date"])))
        start = self._native_date(date.fromisoformat(str(instrument["start_date"])))
        maturity = self._native_date(date.fromisoformat(str(instrument["maturity_date"])))
        market_rate = float(instrument["normalized_quote"])

        family = str(instrument["instrument_type"])
        if family == "DEPOSIT":
            return self._dal.Deposit_New(
                trade_date,
                start,
                maturity,
                market_rate,
                self._curve_lab_rate_index(
                    terms,
                    use_projection_curve_default=(use_projection_curve_default),
                ),
            )
        if family == "FRA":
            return self._dal.FRA_New(
                trade_date,
                start,
                maturity,
                market_rate,
                self._curve_lab_rate_index(
                    terms,
                    use_projection_curve_default=(use_projection_curve_default),
                ),
            )
        if family == "FUTURE":
            return self._dal.Future_New(
                trade_date,
                start,
                maturity,
                market_rate,
                self._curve_lab_rate_index(
                    terms,
                    use_projection_curve_default=(use_projection_curve_default),
                ),
                float(terms.get("convexity_adjustment", 0)),
            )
        if family == "OIS":
            return self._dal.OISSwap_New(
                trade_date,
                start,
                maturity,
                market_rate,
                self._curve_lab_rate_leg(terms, "fixed"),
                self._curve_lab_rate_index(
                    terms,
                    "float_",
                    use_projection_curve_default=(use_projection_curve_default),
                ),
                self._curve_lab_rate_leg(terms, "float"),
            )
        if family == "IRS":
            return self._dal.Swap_New(
                trade_date,
                start,
                maturity,
                market_rate,
                self._curve_lab_rate_leg(terms, "fixed"),
                self._curve_lab_rate_index(
                    terms,
                    "float_",
                    use_projection_curve_default=(use_projection_curve_default),
                ),
                self._curve_lab_rate_leg(terms, "float"),
            )
        if family == "BASIS_SWAP":
            return self._dal.BasisSwap_New(
                trade_date,
                start,
                maturity,
                market_rate,
                self._curve_lab_rate_index(
                    terms,
                    "spread_",
                    use_projection_curve_default=(use_projection_curve_default),
                ),
                self._curve_lab_rate_leg(terms, "spread"),
                self._curve_lab_rate_index(
                    terms,
                    "reference_",
                    use_projection_curve_default=(use_projection_curve_default),
                ),
                self._curve_lab_rate_leg(terms, "reference"),
            )
        if family == "XCCY":
            raise ValueError("XCCY calibration requires a staged or joint XCCY build mode")
        raise ValueError(f"unsupported Curve Lab calibration family {family!r}")

    def import_curve_lab_archive(self, payload: bytes) -> tuple[bytes, str]:
        """Round-trip one preflighted archive through the native reader."""

        extension = getattr(self._dal, "_dal", self._dal)
        with self._calibration_lock:
            root = extension._StorableFromJson(payload)
            canonical = extension._StorableToJson(root)
            native_type = str(getattr(root, "type", ""))
        if not isinstance(canonical, bytes):
            raise TypeError("native archive bridge must return bytes")
        root_kind = "CURVE_SET" if native_type == "Bag" else "DISCOUNT_CURVE"
        return canonical, root_kind

    def _curve_lab_native_trade_definitions(
        self,
        trades: Sequence[Mapping[str, Any]],
        default_component_key: str,
    ) -> list[Any]:
        """Build the immutable native trade definitions used by plan and price."""

        def native_index(terms: Mapping[str, Any], prefix: str = "") -> Any:
            def field(name: str, default: Any) -> Any:
                return terms.get(f"{prefix}{name}", terms.get(name, default))

            return self._dal.RateIndexConvention_New(
                self._dal.PeriodLength_New(str(field("forecast_tenor", "3M"))),
                self._dal.DayBasis_New(str(field("day_basis", "ACT_365F"))),
                self._dal.CollateralType_(str(field("collateral", "OIS"))),
                bool(field("use_projection_curve", False)),
            )

        def native_leg(terms: Mapping[str, Any], prefix: str) -> Any:
            return self._dal.RateLegConvention_New(
                self._dal.PeriodLength_New(str(terms.get(f"{prefix}_payment_frequency", "12M"))),
                self._dal.DayBasis_New(str(terms.get(f"{prefix}_day_basis", "ACT_365F"))),
            )

        def fixing(terms: Mapping[str, Any], prefix: str = "") -> Any:
            result = self._dal.FixingIdentity_()
            result.index_name = str(terms.get(f"{prefix}index_name", terms.get("index_name", "")))
            result.fixing_hour = int(
                terms.get(f"{prefix}fixing_hour", terms.get("fixing_hour", 11))
            )
            result.fixing_minute = int(
                terms.get(
                    f"{prefix}fixing_minute",
                    terms.get("fixing_minute", 0),
                )
            )
            return result

        def xccy_config(trade: Mapping[str, Any], terms: Mapping[str, Any]) -> Any:
            pair_token = str(trade["currency_or_pair"]).replace("/", "-")
            domestic, foreign = pair_token.split("-", 1)
            spread_on_foreign = str(terms.get("spread_leg", "FOREIGN")) == "FOREIGN"
            convention = self._dal.CrossCurrencyConvention_()
            convention.initial_notional_exchange = bool(
                terms.get("initial_notional_exchange", True)
            )
            convention.final_notional_exchange = bool(terms.get("final_notional_exchange", True))
            convention.spread_on_foreign_leg = spread_on_foreign
            convention.domestic_index = native_index(terms, "domestic_")
            convention.domestic_leg = native_leg(terms, "domestic")
            convention.foreign_index = native_index(terms, "foreign_")
            convention.foreign_leg = native_leg(terms, "foreign")
            builder = self._dal.CrossCurrencySwapConfigBuilder_()
            builder.pair = self._dal.CurrencyPair_New(domestic, foreign)
            builder.domestic_notional = float(terms["domestic_notional"])
            builder.foreign_notional = float(terms["foreign_notional"])
            builder.convention = convention
            builder.notional_mode = self._dal.XccyNotionalMode.FIXED
            builder.domestic_rate_fixing = fixing(terms, "domestic_")
            builder.foreign_rate_fixing = fixing(terms, "foreign_")
            return builder.Build()

        result: list[Any] = []
        for trade in trades:
            family = str(trade["instrument_type"])
            terms = trade["terms"]
            discount_key = str(terms.get("discount_component_key", default_component_key))
            forecast_key = str(terms.get("forecast_component_key", discount_key))
            if family == "DEPOSIT":
                native_terms = self._dal.DepositTradeTerms_(
                    notional=float(terms["notional"]),
                    contract_rate=float(terms["contract_rate"]),
                    lend=str(terms["side"]) == "LEND",
                    index=native_index(terms),
                    discount_component_key=discount_key,
                )
            elif family == "FRA":
                native_terms = self._dal.FraTradeTerms_(
                    notional=float(terms["notional"]),
                    contract_rate=float(terms["contract_rate"]),
                    receive_floating=str(terms["side"]) == "RECEIVE_FLOATING",
                    settle_at_start=(
                        str(terms.get("settlement_style", "AT_START_DISCOUNTED"))
                        == "AT_START_DISCOUNTED"
                    ),
                    index=native_index(terms),
                    fixing_identity=fixing(terms),
                    forecast_component_key=forecast_key,
                    discount_component_key=discount_key,
                )
            elif family == "FUTURE":
                native_terms = self._dal.FutureTradeTerms_(
                    contract_count=float(terms["contract_count"]),
                    long_position=str(terms["side"]) == "LONG",
                    reference_price=float(terms["reference_price"]),
                    contract_value_per_price_point=float(terms["contract_value_per_price_point"]),
                    convexity_adjustment=float(terms.get("convexity_adjustment", 0)),
                    index=native_index(terms),
                    fixing_identity=fixing(terms),
                    forecast_component_key=forecast_key,
                )
            elif family in {"OIS", "IRS"}:
                fixed_float = self._dal.FixedFloatTradeTerms_(
                    notional=float(terms["notional"]),
                    contract_rate=float(terms["contract_rate"]),
                    pay_fixed=str(terms["side"]) == "PAY_FIXED",
                    fixed_leg=native_leg(terms, "fixed"),
                    float_leg=native_leg(terms, "float"),
                    float_index=native_index(terms, "float_"),
                    fixing_identity=fixing(terms, "float_"),
                    forecast_component_key=forecast_key,
                    discount_component_key=discount_key,
                )
                native_terms = (
                    self._dal.OisTradeTerms_(value=fixed_float)
                    if family == "OIS"
                    else self._dal.IrsTradeTerms_(value=fixed_float)
                )
            elif family == "BASIS_SWAP":
                native_terms = self._dal.BasisTradeTerms_(
                    notional=float(terms["notional"]),
                    contract_spread=float(terms["contract_spread"]),
                    receive_reference_pay_spread=(
                        str(terms["side"]) == "RECEIVE_REFERENCE_PAY_SPREAD"
                    ),
                    spread_leg=native_leg(terms, "spread"),
                    reference_leg=native_leg(terms, "reference"),
                    spread_index=native_index(terms, "spread_"),
                    reference_index=native_index(terms, "reference_"),
                    spread_fixing_identity=fixing(terms, "spread_"),
                    reference_fixing_identity=fixing(terms, "reference_"),
                    spread_forecast_component_key=str(
                        terms.get("spread_forecast_component_key", forecast_key)
                    ),
                    reference_forecast_component_key=str(
                        terms.get("reference_forecast_component_key", forecast_key)
                    ),
                    discount_component_key=discount_key,
                )
            elif family == "XCCY":
                native_terms = self._dal.XccyTradeTerms_(
                    position_count=float(terms["position_count"]),
                    contract_spread=float(terms["contract_spread"]),
                    spread_on_foreign_leg=(str(terms.get("spread_leg", "FOREIGN")) == "FOREIGN"),
                    receive_non_spread_pay_spread=(
                        str(terms["side"]) == "RECEIVE_NON_SPREAD_PAY_SPREAD"
                    ),
                    config=xccy_config(trade, terms),
                )
            else:  # pragma: no cover - closed request schema prevents this
                raise ValueError(f"unsupported Curve Lab pricing family {family!r}")
            result.append(
                self._dal.RateTradeDefinition_(
                    instrument_id=str(trade["trade_id"]),
                    instrument_type=getattr(self._dal.RateInstrumentType, family),
                    trade_date=self._native_date(date.fromisoformat(str(trade["trade_date"]))),
                    start_date=self._native_date(date.fromisoformat(str(trade["start_date"]))),
                    maturity_date=self._native_date(
                        date.fromisoformat(str(trade["maturity_date"]))
                    ),
                    currency=str(trade["currency_or_pair"]).split("-")[0],
                    terms=native_terms,
                )
            )
        return result

    def curve_lab_required_historical_fixings(
        self,
        trades: Sequence[Mapping[str, Any]],
        evaluation_time: str,
        default_component_key: str,
    ) -> list[dict[str, Any]]:
        """Resolve exact historical fixing keys from native cashflow plans."""

        extension = getattr(self._dal, "_dal", self._dal)
        preflight = getattr(extension, "_RequiredHistoricalRateTradeFixings", None)
        valuation = canonical_utc_datetime(evaluation_time)
        if preflight is None:
            self._require_test_double_fallback("Curve Lab historical fixing planning")
            return self._curve_lab_required_historical_fixings_fallback(
                trades,
                valuation,
            )
        with self._calibration_lock:
            native_trades = self._curve_lab_native_trade_definitions(
                trades,
                default_component_key,
            )
            rows = preflight(native_trades, self._native_datetime(valuation))
        return [
            {
                "trade_index": int(trade_index),
                "index_name": str(index_name),
                "fixing_time": datetime.fromisoformat(repr(fixing_time)).isoformat(),
                "kind": ("FX" if str(index_name).startswith("FX[") else "RATE"),
                "units": (
                    "DOMESTIC_PER_FOREIGN" if str(index_name).startswith("FX[") else "DECIMAL_RATE"
                ),
            }
            for trade_index, index_name, fixing_time in rows
        ]

    @staticmethod
    def _curve_lab_required_historical_fixings_fallback(
        trades: Sequence[Mapping[str, Any]],
        valuation: datetime,
    ) -> list[dict[str, Any]]:
        """Compatibility fallback for an older local extension during upgrades."""

        result: list[dict[str, Any]] = []
        for trade_index, trade in enumerate(trades):
            family = str(trade["instrument_type"])
            if family not in {"FRA", "FUTURE"}:
                continue
            terms = trade["terms"]
            fixing_time = datetime.combine(
                date.fromisoformat(str(trade["start_date"])),
                datetime.min.time(),
                tzinfo=valuation.tzinfo,
            ).replace(
                hour=int(terms.get("fixing_hour", 11)),
                minute=int(terms.get("fixing_minute", 0)),
            )
            payment_date = date.fromisoformat(
                str(
                    trade["start_date"]
                    if family == "FRA"
                    and str(terms.get("settlement_style", "AT_START_DISCOUNTED"))
                    == "AT_START_DISCOUNTED"
                    else trade["maturity_date"]
                )
            )
            if fixing_time >= valuation or payment_date < valuation.date():
                continue
            result.append(
                {
                    "trade_index": trade_index,
                    "index_name": str(terms.get("index_name", "")),
                    "fixing_time": fixing_time.isoformat(),
                    "kind": "RATE",
                    "units": "DECIMAL_RATE",
                }
            )
        return result

    def price_curve_lab_trades(
        self,
        document: Mapping[str, Any],
        trades: list[dict[str, Any]],
        evaluation_time: str,
        base_currency: str,
        *,
        curve_version: Mapping[str, Any] | None = None,
        dependencies: Sequence[Mapping[str, Any]] = (),
        parameter_bumps: list[tuple[Mapping[str, Any], float]] | None = None,
        fixing_observations: Sequence[Mapping[str, Any]] | None = None,
        parameter_axis: Sequence[Mapping[str, Any]] = (),
        include_node_sensitivities: bool = False,
        check_deadline: Callable[[], None] | None = None,
    ) -> list[dict[str, Any]]:
        """Adapt normalized Curve Lab trades to the native pricing kernel."""

        with self._calibration_lock:
            if curve_version is None:
                dependency_curves = self._curve_lab_dependency_curves(dependencies)
                curves = self._curve_lab_passive_curves(
                    document,
                    parameter_bumps,
                    dependency_curves=dependency_curves,
                    fixing_observations=fixing_observations or (),
                )
            else:
                payload = curve_version.get("native_payload")
                expected_hash = curve_version.get("native_payload_hash")
                if not isinstance(payload, bytes) or not isinstance(expected_hash, str):
                    raise ValueError("selected Curve Lab version archive is unavailable")
                selected_curves = self._curve_lab_archive_curves(
                    payload,
                    str(curve_version["root_kind"]),
                    document,
                    expected_hash,
                )
                curves = self._curve_lab_dependency_curves(dependencies)
                duplicate = curves.keys() & selected_curves.keys()
                if duplicate:
                    raise ValueError(
                        f"selected version duplicates dependency component {sorted(duplicate)[0]!r}"
                    )
                curves.update(selected_curves)
                if parameter_bumps:
                    curves = self._curve_lab_bumped_curves(
                        curves,
                        list(document["declarations"]),
                        parameter_bumps,
                    )
            default_key = next(iter(curves))
            valuation = canonical_utc_datetime(evaluation_time)
            native_time = self._native_datetime(valuation)
            fixing_snapshot = self._dal.MarketFixingSnapshot_New(
                self._curve_lab_fixing_values(fixing_observations or ())
            )
            xccy_trade = next(
                (trade for trade in trades if trade["instrument_type"] == "XCCY"),
                None,
            )
            native_xccy_market = None
            if xccy_trade is not None:
                pair_token = str(xccy_trade["currency_or_pair"]).replace("/", "-")
                domestic, foreign = pair_token.split("-", 1)

                def curve_block(currency: str) -> Any:
                    discounts: dict[Any, Any] = {}
                    forwards: dict[Any, Any] = {}
                    for declaration in document["declarations"]:
                        if str(declaration["currency"]) != currency:
                            continue
                        key = str(declaration["component_key"])
                        if declaration["role"] == "DISCOUNT":
                            collateral = key.rsplit("/", 1)[-1]
                            discounts[self._dal.CollateralType_(collateral)] = curves[key]
                        elif declaration["role"] == "PROJECTION":
                            tenor = key.rsplit("/", 1)[-1]
                            forwards[self._dal.PeriodLength_New(tenor)] = curves[key]
                    if not discounts:
                        raise ValueError(
                            f"XCCY pricing is missing a discount declaration for {currency}"
                        )
                    return self._dal.CurveBlock_New(
                        f"curve-lab-{currency}",
                        currency,
                        discounts,
                        forwards,
                        self._dal.DayBasis_New("ACT_365F"),
                    )

                basis_curve = next(
                    (
                        curves[str(declaration["component_key"])]
                        for declaration in document["declarations"]
                        if declaration["role"] == "BASIS"
                    ),
                    None,
                )
                native_xccy_market = self._dal.CrossCurrencyMarket_New(
                    domestic_block=curve_block(domestic),
                    foreign_block=curve_block(foreign),
                    fx_spot=float(xccy_trade["terms"]["fx_spot"]),
                    valuation_time=native_time,
                    collateral_currency=domestic,
                    fixings=fixing_snapshot,
                    basis_curve=basis_curve,
                )
            native_market = self._dal.RatePricingMarket_(
                valuation_time=native_time,
                result_currency=base_currency,
                curve_components=curves,
                xccy_market=native_xccy_market,
                fixings=fixing_snapshot,
            )
            native_trades = self._curve_lab_native_trade_definitions(
                trades,
                default_key,
            )
            native_positions = list(range(len(trades)))
            result: list[dict[str, Any] | None] = [None] * len(trades)

            if check_deadline is not None:
                check_deadline()
            priced = self._dal.PriceRateTrades(
                trades=native_trades,
                market=native_market,
            )
            if check_deadline is not None:
                check_deadline()
            aad_rows: list[dict[str, Any] | None] = [None] * len(native_trades)
            if include_node_sensitivities and hasattr(
                self._dal,
                "RateTradeNodeSensitivities",
            ):
                axes_by_component: dict[str, list[Mapping[str, Any]]] = {}
                for axis in parameter_axis:
                    axes_by_component.setdefault(
                        str(axis["component_key"]),
                        [],
                    ).append(axis)
                for native_position, native_trade in enumerate(native_trades):
                    gradient_by_id: dict[str, str] = {}
                    eligible = False
                    reasons: list[str] = []
                    for component_key, axes in axes_by_component.items():
                        if check_deadline is not None:
                            check_deadline()
                        sensitivity = self._dal.RateTradeNodeSensitivities(
                            trade=native_trade,
                            market=native_market,
                            component_key=component_key,
                        )
                        if check_deadline is not None:
                            check_deadline()
                        if bool(sensitivity.eligible):
                            if len(sensitivity.gradient) != len(axes):
                                raise ValueError(
                                    "native AAD gradient does not match the persisted parameter axis"
                                )
                            eligible = True
                            gradient_by_id.update(
                                {
                                    str(axis["parameter_id"]): str(value)
                                    for axis, value in zip(
                                        axes,
                                        sensitivity.gradient,
                                        strict=True,
                                    )
                                }
                            )
                        elif str(sensitivity.reason) != "TRADE_DOES_NOT_DEPEND_ON_COMPONENT":
                            reasons.append(str(sensitivity.reason))
                    aad_rows[native_position] = {
                        "eligible": eligible,
                        "gradient": [
                            gradient_by_id.get(str(axis["parameter_id"]), "0")
                            for axis in parameter_axis
                        ],
                        "reason": ",".join(reasons),
                    }
            for native_position, (position, row) in enumerate(
                zip(native_positions, priced, strict=True)
            ):
                aad = aad_rows[native_position]
                result[position] = {
                    "trade_id": trades[position]["trade_id"],
                    "instrument_type": trades[position]["instrument_type"],
                    "succeeded": bool(row.succeeded),
                    "pv": str(row.pv),
                    "currency": str(row.currency),
                    "required_historical_fixings": list(row.required_historical_fixings),
                    "missing_historical_fixings": list(row.missing_historical_fixings),
                    "dependency_component_keys": list(row.dependency_component_keys),
                    "error": str(row.error),
                    "aad_node_gradient": (
                        aad["gradient"] if aad is not None and aad["eligible"] else None
                    ),
                    "aad_ineligibility_reason": (
                        aad["reason"] if aad is not None and not aad["eligible"] else None
                    ),
                }
            return [row for row in result if row is not None]

    def price_curve_lab_parameter_bump(
        self,
        document: Mapping[str, Any],
        trades: list[dict[str, Any]],
        evaluation_time: str,
        base_currency: str,
        parameter_axis: Mapping[str, Any],
        bump: float,
        curve_version: Mapping[str, Any] | None = None,
        dependencies: Sequence[Mapping[str, Any]] = (),
        fixing_observations: Sequence[Mapping[str, Any]] | None = None,
        check_deadline: Callable[[], None] | None = None,
    ) -> list[dict[str, Any]]:
        """Reprice after one calibrated native parameter is shifted."""

        return self.price_curve_lab_trades(
            document,
            trades,
            evaluation_time,
            base_currency,
            curve_version=curve_version,
            dependencies=dependencies,
            parameter_bumps=[(parameter_axis, bump)],
            fixing_observations=fixing_observations,
            check_deadline=check_deadline,
        )

    def _curve_lab_fixing_values(
        self,
        observations: Sequence[Mapping[str, Any]],
    ) -> dict[str, dict[Any, float]]:
        values: dict[str, dict[Any, float]] = {}
        for observation in observations:
            timestamp = canonical_utc_datetime(str(observation["fixing_time"]))
            values.setdefault(str(observation["index_name"]), {})[
                self._native_datetime(timestamp)
            ] = float(observation["value"])
        return values

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
        self._require_test_double_fallback("single-curve knot planning")
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
        self._require_test_double_fallback("single-curve calibration")
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
            self._require_test_double_fallback("single-curve spec construction")
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
        self._require_test_double_fallback("cross-currency calibration")
        if kind == "xccy_joint":
            return _fallback_joint_result(request)
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

    def _require_test_double_fallback(self, capability: str) -> None:
        raise NativeDalCapabilityError(
            f"Native DAL capability is required for {capability}; "
            "synthetic production fallbacks are disabled"
        )


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
    actual_scheme = _native_curve_log_df_scheme(parameterization, curve)
    if actual_scheme != declaration.log_df_scheme:
        raise RuntimeError("native curve log-DF scheme does not match the admitted declaration")
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
        "log_df_scheme": actual_scheme,
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


def _native_curve_log_df_scheme(
    parameterization: str,
    curve: object,
) -> str | None:
    if parameterization not in {"ZERO_RATE", "LOG_DISCOUNT"}:
        return None
    try:
        scheme = curve.log_df_scheme
    except AttributeError as exc:
        raise RuntimeError(
            "native zero/log-discount curve omitted its log-DF scheme getter"
        ) from exc
    return _enum_name(scheme)


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
    if available:
        _validate_native_matrix_shape(values, row_axis, column_axis)
    return MatrixDTO(
        availability=_native_matrix_availability(available, requested),
        shape=(len(row_axis), len(column_axis)),
        row_axis=row_axis,
        column_axis=column_axis,
        scaling=scaling,
        residual_tolerance=residual_tolerance,
        values=values,
    )


def _validate_native_matrix_shape(
    values: list[list[float]],
    row_axis: list[str],
    column_axis: list[str],
) -> None:
    if not row_axis or not column_axis:
        raise RuntimeError("native available matrix must have positive dimensions")
    if len(values) != len(row_axis) or any(len(row) != len(column_axis) for row in values):
        raise RuntimeError("native matrix shape does not match its response axes")


def _native_matrix_availability(available: bool, requested: bool) -> str:
    if available:
        return "available"
    if requested:
        return "not_available_for_mode"
    return "not_requested"


def _native_single_curve_payload(
    request: object, plan: ResolvedSingleKnotPlanDTO, curve: object
) -> dict[str, object]:
    parameterization = request.declaration.parameterization
    node_dates, parameters, day_count = _native_curve_state(parameterization, curve)
    actual_scheme = _native_curve_log_df_scheme(parameterization, curve)
    if actual_scheme != request.declaration.log_df_scheme:
        raise RuntimeError("native curve log-DF scheme does not match the admitted declaration")
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
        "log_df_scheme": actual_scheme,
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


def _fallback_joint_groups(
    request: object,
) -> list[tuple[str, str, object, list[object]]]:
    groups: list[tuple[str, str, object, list[object]]] = []
    for group_name, group in (
        ("domestic", request.domestic),
        ("foreign", request.foreign),
    ):
        groups.extend(
            (
                f"{group_name}:{index}",
                group.currency,
                declaration,
                list(declaration.instruments),
            )
            for index, declaration in enumerate(group.declarations)
        )
    groups.append(
        (
            "basis",
            request.pair.domestic,
            request.basis,
            list(request.basis.instruments),
        )
    )
    return groups


def _fallback_joint_curve_payload(
    request: object,
    currency: str,
    declaration: object,
    role: str,
) -> dict[str, object]:
    parameterization = declaration.parameterization
    node_dates = list(declaration.knot_dates)
    values = list(declaration.initial_guess_per_node)
    if parameterization == "PIECEWISE_CONSTANT_FWD":
        parameters: dict[str, object] = {"right_forwards": values}
    elif parameterization == "PIECEWISE_LINEAR_FWD":
        parameters = _fallback_linear_parameters(values, len(node_dates))
    elif parameterization == "ZERO_RATE":
        parameters = {"zero_rates": values}
    else:
        node_dates = [request.valuation_time.date(), *node_dates]
        parameters = {"log_discount_factors": [0.0, *values]}
    return {
        "name": declaration.curve_name,
        "currency": currency,
        "role": role,
        "target": {
            "collateral": getattr(
                declaration,
                "target_collateral",
                request.collateral_currency,
            ),
            "tenor": getattr(declaration, "target_tenor", None),
        },
        "parameterization": parameterization,
        "anchor_date": request.valuation_time.date(),
        "day_count": "ACT_365F",
        "log_df_scheme": declaration.log_df_scheme,
        "node_dates": node_dates,
        "parameters": parameters,
        "base_curve_id": None,
    }


def _fallback_joint_result(request: object) -> GatewayCalibrationResult:
    groups = _fallback_joint_groups(request)
    curves = tuple(
        _fallback_joint_curve_payload(
            request,
            currency,
            declaration,
            (
                "basis"
                if group_name == "basis"
                else "discount"
                if declaration.calibrate_discount_curve
                else "forward"
            ),
        )
        for group_name, currency, declaration, _instruments in groups
    )
    parameter_ranges: list[NamedRangeDTO] = []
    residual_ranges: list[NamedRangeDTO] = []
    diagnostics: list[InstrumentDiagnosticDTO] = []
    parameter_offset = 0
    residual_offset = 0
    for group_name, _currency, declaration, instruments in groups:
        parameter_size = len(declaration.initial_guess_per_node)
        parameter_ranges.append(
            NamedRangeDTO(
                name=group_name,
                offset=parameter_offset,
                size=parameter_size,
            )
        )
        residual_ranges.append(
            NamedRangeDTO(
                name=group_name,
                offset=residual_offset,
                size=len(instruments),
            )
        )
        diagnostics.extend(
            InstrumentDiagnosticDTO(
                instrument_id=f"{residual_offset + index + 1:032x}",
                group=group_name,
                calibration_index=residual_offset + index,
                market_rate=instrument.market_rate,
                model_rate=instrument.market_rate,
                residual=0.0,
            )
            for index, instrument in enumerate(instruments)
        )
        parameter_offset += parameter_size
        residual_offset += len(instruments)
    parameter_axis = _joint_parameter_axis(request, parameter_ranges)
    residual_axis = [f"residual:{diagnostic.instrument_id}" for diagnostic in diagnostics]
    jacobian, inverse = _fallback_matrices(
        request,
        parameter_axis,
        residual_axis,
    )
    return GatewayCalibrationResult(
        actual_jacobian_mode=request.options.jacobian_mode,
        actual_execution_identity=None,
        curves=curves,
        instrument_diagnostics=tuple(diagnostics),
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
        fx_forwards=_fallback_fx_forwards(request, xccy=True),
        named_ranges=NamedRangesDTO(
            parameters=parameter_ranges,
            residuals=residual_ranges,
        ),
        jacobian=jacobian,
        effective_inverse=inverse,
        native_solve_ms=0.0,
    )


def _fallback_diagnostics(request: object, *, xccy: bool) -> tuple[InstrumentDiagnosticDTO, ...]:
    group = "basis" if xccy else "single"
    instruments = list(_fallback_instruments(request))
    if not xccy:
        instruments.sort(
            key=lambda instrument: (
                instrument.maturity,
                instrument.start,
                _native_instrument_name(instrument.kind),
            )
        )
    return tuple(
        InstrumentDiagnosticDTO(
            instrument_id=f"{index + 1:032x}",
            group=group,
            calibration_index=index,
            market_rate=instrument.market_rate,
            model_rate=instrument.market_rate,
            residual=0.0,
        )
        for index, instrument in enumerate(instruments)
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
    if actual is None:
        parameter_count = sum(len(values) for values in curves[0]["parameters"].values())
        parameter_axis = [f"parameter:{index}" for index in range(parameter_count)]
    else:
        parameter_axis = _single_parameter_axis(request, actual)
        parameter_count = len(parameter_axis)
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


def _native_map_value(mapping: Mapping[Any, Any], requested_key: Any) -> Any:
    """Read enum-keyed pybind maps even when wrapper equality is identity-based."""

    try:
        return mapping[requested_key]
    except KeyError:
        requested = str(requested_key)
        for native_key, value in mapping.items():
            if str(native_key) == requested:
                return value
        raise


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
