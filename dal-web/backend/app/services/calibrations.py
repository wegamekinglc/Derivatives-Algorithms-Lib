"""Pure calibration admission, evidence, matrix, and serialization helpers."""

from __future__ import annotations

import asyncio
import hashlib
import json
import logging
import math
import time
from collections.abc import Coroutine, Mapping, Sequence
from dataclasses import dataclass
from datetime import UTC, date, datetime
from typing import TYPE_CHECKING, Any, Literal, NamedTuple
from uuid import uuid4

from pydantic import TypeAdapter

from app.schemas.calibrations import (
    ApiErrorDTO,
    CalibrationRunResponse,
    CalibrationTimingsDTO,
    CompletedCalibrationRunResponse,
    CurveReconstructionDTO,
    ExecutionSingleKnotIdentityDTO,
    FailedCalibrationRunResponse,
    JointBasisDeclarationDTO,
    JointCurveDeclarationDTO,
    JointXccyCalibrationRequest,
    MatrixDTO,
    NormalizedCalibrationOptionsDTO,
    NormalizedCalibrationSolverDTO,
    ParameterDeltaDTO,
    QuoteBumpPreviewDTO,
    QuoteBumpQueryDTO,
    ResolvedSingleKnotPlanDTO,
    RunningCalibrationRunResponse,
    SingleCalibrationRequest,
    StagedXccyCalibrationRequest,
)
from app.services.calibration_store import (
    CalibrationInstrumentRecord,
    CalibrationRunRecord,
    CurveDefinitionRecord,
)

if TYPE_CHECKING:
    from app.services.dal_gateway import DalGateway, GatewayCalibrationResult
    from app.services.store import StoreProtocol

logger = logging.getLogger(__name__)

type JsonScalar = bool | int | float | str | None


@dataclass(frozen=True, slots=True)
class FrozenJsonArray:
    items: tuple[FrozenJsonValue, ...]


@dataclass(frozen=True, slots=True)
class FrozenJsonObject:
    entries: tuple[tuple[str, FrozenJsonValue], ...]


type FrozenJsonValue = JsonScalar | FrozenJsonArray | FrozenJsonObject
type FrozenErrorLocation = tuple[str | int, ...] | None


@dataclass(frozen=True, slots=True)
class FrozenIntegrityErrorEvidence:
    code: str
    message: str
    location: FrozenErrorLocation
    context: FrozenJsonObject
    canonical_error_utf8: bytes


def _freeze_json(value: object) -> FrozenJsonValue:
    if value is None or isinstance(value, (bool, str)):
        return value
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        if not math.isfinite(value):
            raise ValueError("integrity error context contains a non-finite float")
        return value
    if isinstance(value, Mapping):
        if any(not isinstance(key, str) for key in value):
            raise TypeError("integrity error context object keys must be strings")
        entries: list[tuple[str, FrozenJsonValue]] = []
        for key in sorted(value):
            entries.append((key, _freeze_json(value[key])))
        return FrozenJsonObject(tuple(entries))
    if isinstance(value, (list, tuple)):
        return FrozenJsonArray(tuple(_freeze_json(item) for item in value))
    raise TypeError(f"integrity error context contains unsupported {type(value).__name__}")


def _thaw_json(value: FrozenJsonValue) -> object:
    if isinstance(value, FrozenJsonArray):
        return [_thaw_json(item) for item in value.items]
    if isinstance(value, FrozenJsonObject):
        return {key: _thaw_json(item) for key, item in value.entries}
    return value


def canonical_json_bytes(value: object) -> bytes:
    return json.dumps(
        value,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    ).encode("utf-8")


def canonical_model_hash(value: object) -> str:
    if hasattr(value, "model_dump"):
        value = value.model_dump(mode="json")
    return hashlib.sha256(canonical_json_bytes(value)).hexdigest()


def freeze_integrity_error_evidence(
    code: str,
    message: str,
    location: Sequence[str | int] | None,
    context: Mapping[str, object],
) -> FrozenIntegrityErrorEvidence:
    if not 1 <= len(code) <= 128:
        raise ValueError("integrity error code must contain 1..128 characters")
    frozen_location: FrozenErrorLocation
    if location is None:
        frozen_location = None
    else:
        location_items: list[str | int] = []
        for item in location:
            if isinstance(item, bool) or not isinstance(item, (str, int)):
                raise TypeError("integrity error locations contain only str or int")
            location_items.append(item)
        frozen_location = tuple(location_items)
    frozen_context = _freeze_json(context)
    if not isinstance(frozen_context, FrozenJsonObject):
        raise TypeError("integrity error context must be an object")
    semantic = {
        "code": code,
        "message": message,
        "location": list(frozen_location) if frozen_location is not None else None,
        "context": _thaw_json(frozen_context),
    }
    return FrozenIntegrityErrorEvidence(
        code=code,
        message=message,
        location=frozen_location,
        context=frozen_context,
        canonical_error_utf8=canonical_json_bytes(semantic),
    )


def to_api_error_dto(error: FrozenIntegrityErrorEvidence) -> ApiErrorDTO:
    return ApiErrorDTO(
        code=error.code,
        message=error.message,
        location=list(error.location) if error.location is not None else None,
        context=_thaw_json(error.context),
    )


class _IntegrityError(Exception):
    __slots__ = ("_error",)

    def __init__(self, error: FrozenIntegrityErrorEvidence) -> None:
        super().__init__(error.message)
        self._error = error

    @property
    def error(self) -> FrozenIntegrityErrorEvidence:
        return self._error


class PersistedKnotPlanIntegrityError(_IntegrityError):
    pass


class PersistedExpectedExecutionIdentityIntegrityError(_IntegrityError):
    pass


class SingleGatewayPreLockRequest(NamedTuple):
    request: object
    referenced_curves: Mapping[str, CurveReconstructionDTO]


class VerifiedSingleWorkerAdmissionEvidence(NamedTuple):
    resolved_knot_plan: ResolvedSingleKnotPlanDTO
    resolved_knot_plan_hash: str
    expected_execution_identity: ExecutionSingleKnotIdentityDTO
    expected_execution_identity_hash: str


class VerifiedSingleGatewayRequest(NamedTuple):
    pre_lock_request: SingleGatewayPreLockRequest
    evidence: VerifiedSingleWorkerAdmissionEvidence


JointAdmissionGroup = Literal["domestic", "foreign", "basis"]


@dataclass(frozen=True)
class JointAdmissionDeclarationCount:
    group: JointAdmissionGroup
    declaration_input_index: int | None
    parameterization: str
    storage_nodes: int
    free_parameter_count: int
    cumulative_before: int
    cumulative_after: int
    parameter_offset: int


@dataclass(frozen=True)
class JointAdmissionCountPlan:
    declarations: tuple[JointAdmissionDeclarationCount, ...]
    total_free_parameters: int
    first_overflowing_declaration: JointAdmissionDeclarationCount | None


def _declaration_counts(parameterization: str, submitted_nodes: int) -> tuple[int, int]:
    anchor_added = parameterization in {"ZERO_RATE", "LOG_DISCOUNT"}
    storage_nodes = submitted_nodes + int(anchor_added)
    if parameterization == "PIECEWISE_LINEAR_FWD":
        free_parameters = 2 * submitted_nodes
    elif parameterization == "LOG_DISCOUNT":
        free_parameters = submitted_nodes
    else:
        free_parameters = submitted_nodes
    return storage_nodes, free_parameters


def build_joint_admission_count_plan(
    *,
    domestic: Sequence[JointCurveDeclarationDTO],
    foreign: Sequence[JointCurveDeclarationDTO],
    basis: JointBasisDeclarationDTO,
    limit: int = 200,
) -> JointAdmissionCountPlan:
    pending: list[tuple[JointAdmissionGroup, int | None, str, int]] = []
    pending.extend(
        ("domestic", index, declaration.parameterization, len(declaration.knot_dates))
        for index, declaration in enumerate(domestic)
    )
    pending.extend(
        ("foreign", index, declaration.parameterization, len(declaration.knot_dates))
        for index, declaration in enumerate(foreign)
    )
    pending.append(("basis", None, basis.parameterization, len(basis.knot_dates)))

    declarations: list[JointAdmissionDeclarationCount] = []
    cumulative = 0
    first_overflow: JointAdmissionDeclarationCount | None = None
    for group, input_index, parameterization, submitted_nodes in pending:
        storage_nodes, free_parameters = _declaration_counts(parameterization, submitted_nodes)
        item = JointAdmissionDeclarationCount(
            group=group,
            declaration_input_index=input_index,
            parameterization=parameterization,
            storage_nodes=storage_nodes,
            free_parameter_count=free_parameters,
            cumulative_before=cumulative,
            cumulative_after=cumulative + free_parameters,
            parameter_offset=cumulative,
        )
        declarations.append(item)
        cumulative = item.cumulative_after
        if first_overflow is None and cumulative > limit:
            first_overflow = item
    return JointAdmissionCountPlan(tuple(declarations), cumulative, first_overflow)


def _check_joint_declaration_bounds(
    request: JointXccyCalibrationRequest,
    plan: JointAdmissionCountPlan,
) -> None:
    declarations = [
        *request.domestic.declarations,
        *request.foreign.declarations,
        request.basis,
    ]
    for declaration, counts in zip(declarations, plan.declarations, strict=True):
        prefix: list[str | int] = (
            ["body", "basis"]
            if counts.group == "basis"
            else [
                "body",
                counts.group,
                "declarations",
                counts.declaration_input_index,
            ]
        )
        if counts.storage_nodes > 100:
            offending_index = 99
            _raise(
                "CURVE_STORAGE_NODE_LIMIT_EXCEEDED",
                f"{'.'.join(str(item) for item in prefix[1:])}.knot_dates "
                f"produces {counts.storage_nodes} storage nodes; maximum is 100",
                [*prefix, "knot_dates", offending_index],
                {
                    "policy": "INPUT",
                    "parameterization": counts.parameterization,
                    "resolved_declared_nodes": len(declaration.knot_dates),
                    "storage_nodes": counts.storage_nodes,
                    "max_storage_nodes": 100,
                    "anchor_added": True,
                    "candidate_ordinal": offending_index,
                    "candidate_date": declaration.knot_dates[
                        offending_index
                    ].isoformat(),
                    "candidate_origin": {
                        "kind": "INPUT",
                        "input_knot_index": offending_index,
                    },
                    "origins": [
                        {
                            "kind": "INPUT",
                            "input_knot_index": offending_index,
                        }
                    ],
                },
            )
        scheme = declaration.log_df_scheme
        minimum = {
            "LOG_LINEAR": 2,
            "LOG_CUBIC_NATURAL": 3,
            "MIXED": 4,
        }.get(scheme or "", 1)
        if counts.storage_nodes < minimum:
            _raise(
                "CURVE_SCHEME_NODE_COUNT_INVALID",
                f"{'.'.join(str(item) for item in prefix[1:])}.log_df_scheme "
                f"scheme {scheme} requires at least {minimum} storage nodes; "
                f"got {counts.storage_nodes}",
                [*prefix, "log_df_scheme"],
                {
                    "scheme": scheme,
                    "storage_nodes": counts.storage_nodes,
                    "minimum_storage_nodes": minimum,
                },
            )


def calculate_quote_bump_preview(
    effective_inverse: MatrixDTO,
    residual_index: int,
    quote_bump: float,
) -> QuoteBumpPreviewDTO:
    if effective_inverse.availability != "available" or effective_inverse.values is None:
        raise ValueError("effective inverse matrix is not available")
    if effective_inverse.scaling != "solver_scaled":
        raise ValueError("effective inverse matrix must use solver_scaled units")
    tolerance = effective_inverse.residual_tolerance
    if tolerance is None or tolerance <= 0.0 or not math.isfinite(tolerance):
        raise ValueError("effective inverse residual tolerance must be positive and finite")
    if not math.isfinite(quote_bump) or quote_bump == 0.0:
        raise ValueError("quote bump must be non-zero and finite")
    rows, columns = effective_inverse.shape
    if residual_index < 0 or residual_index >= columns:
        raise IndexError("quote bump residual index is outside the inverse matrix")
    if rows != len(effective_inverse.row_axis):
        raise ValueError("effective inverse parameter axis does not match shape")
    residual_axis = effective_inverse.column_axis[residual_index]
    prefix = "residual:"
    instrument_id = residual_axis[len(prefix) :] if residual_axis.startswith(prefix) else ""
    deltas = [
        ParameterDeltaDTO(
            axis=effective_inverse.row_axis[row],
            value=effective_inverse.values[row][residual_index] * quote_bump / tolerance,
        )
        for row in range(rows)
    ]
    return QuoteBumpPreviewDTO(
        residual_index=residual_index,
        instrument_id=instrument_id,
        quote_bump=quote_bump,
        residual_tolerance=tolerance,
        delta_parameters=deltas,
        formula="delta_x = effective_inverse * delta_quote / residual_tolerance",
    )


def first_canonical_difference(expected: object, actual: object) -> dict | None:
    if hasattr(expected, "model_dump"):
        expected = expected.model_dump(mode="json")
    if hasattr(actual, "model_dump"):
        actual = actual.model_dump(mode="json")

    def visit(left: object, right: object, path: tuple[str | int, ...]) -> dict | None:
        if isinstance(left, dict) and isinstance(right, dict):
            for key in sorted(set(left) | set(right)):
                if key not in left or key not in right:
                    return {
                        "path": list((*path, key)),
                        "expected": left.get(key),
                        "actual": right.get(key),
                    }
                difference = visit(left[key], right[key], (*path, key))
                if difference is not None:
                    return difference
            return None
        if isinstance(left, list) and isinstance(right, list):
            for index in range(max(len(left), len(right))):
                if index >= len(left) or index >= len(right):
                    return {
                        "path": list((*path, index)),
                        "expected": left[index] if index < len(left) else None,
                        "actual": right[index] if index < len(right) else None,
                    }
                difference = visit(left[index], right[index], (*path, index))
                if difference is not None:
                    return difference
            return None
        if left != right:
            return {"path": list(path), "expected": left, "actual": right}
        return None

    return visit(expected, actual, ())


RUN_RESPONSE_ADAPTER = TypeAdapter(CalibrationRunResponse)
CURVE_RESPONSE_ADAPTER = TypeAdapter(CurveReconstructionDTO)


class CalibrationHttpError(Exception):
    __slots__ = ("status_code", "error")

    def __init__(
        self,
        status_code: int,
        code: str,
        message: str,
        location: list[str | int] | None,
        context: dict[str, object],
    ) -> None:
        super().__init__(message)
        self.status_code = status_code
        self.error = ApiErrorDTO(
            code=code,
            message=message,
            location=location,
            context=context,
        )


_BACKGROUND_TASKS: set[asyncio.Task[None]] = set()


def _schedule_calibration(coro: Coroutine[Any, Any, None]) -> None:
    task = asyncio.create_task(coro)
    _BACKGROUND_TASKS.add(task)
    task.add_done_callback(_BACKGROUND_TASKS.discard)


def _normalized_solver(request: object) -> NormalizedCalibrationSolverDTO:
    return NormalizedCalibrationSolverDTO.model_validate(
        request.solver.model_dump(mode="json")
    )


def _normalized_options(request: object) -> NormalizedCalibrationOptionsDTO:
    return NormalizedCalibrationOptionsDTO.model_validate(
        request.options.model_dump(mode="json")
    )


def _project_expected_identity(
    request: SingleCalibrationRequest, plan: ResolvedSingleKnotPlanDTO
) -> ExecutionSingleKnotIdentityDTO:
    return ExecutionSingleKnotIdentityDTO(
        identity_version=1,
        execution_policy="INPUT",
        today=request.today,
        parameterization=request.declaration.parameterization,
        log_df_scheme=request.declaration.log_df_scheme,
        resolved_declared_dates=tuple(node.date for node in plan.resolved_declared_nodes),
        storage_dates=tuple(node.date for node in plan.storage_nodes),
        free_parameters=plan.free_parameters,
        counts={
            "resolved_declared_nodes": plan.counts.resolved_declared_nodes,
            "storage_nodes": plan.counts.storage_nodes,
            "free_parameters": plan.counts.free_parameters,
        },
    )


def _raise(
    code: str,
    message: str,
    location: list[str | int] | None,
    context: dict[str, object],
    status_code: int = 422,
) -> None:
    raise CalibrationHttpError(status_code, code, message, location, context)


def _validate_single_policy(request: SingleCalibrationRequest) -> None:
    declaration = request.declaration
    if declaration.knot_policy == "INSTRUMENTS" and declaration.knot_dates:
        count = len(declaration.knot_dates)
        _raise(
            "KNOT_POLICY_INPUT_NOT_ALLOWED",
            "declaration.knot_dates must be empty when knot_policy is "
            f"INSTRUMENTS; got {count} dates",
            ["body", "declaration", "knot_dates", 0],
            {"policy": "INSTRUMENTS", "submitted_count": count},
        )
    if (
        declaration.knot_policy == "INSTRUMENTS"
        and declaration.parameterization == "LOG_DISCOUNT"
    ):
        _raise(
            "KNOT_POLICY_INCOMPATIBLE",
            "declaration.knot_policy INSTRUMENTS is incompatible with "
            "LOG_DISCOUNT because the required anchor "
            f"{request.today.isoformat()} is not derivable",
            ["body", "declaration", "knot_policy"],
            {
                "policy": "INSTRUMENTS",
                "parameterization": "LOG_DISCOUNT",
                "reason_code": "LOG_DISCOUNT_ANCHOR_NOT_DERIVABLE",
                "required_anchor": request.today.isoformat(),
            },
        )
    if declaration.knot_policy != "INSTRUMENTS" and not declaration.knot_dates:
        _raise(
            "VALIDATION_ERROR",
            "INPUT and AUGMENTED require submitted knot dates",
            ["body", "declaration", "knot_dates"],
            {"type": "value_error"},
        )


def _check_plan_bounds(
    request: SingleCalibrationRequest, plan: object
) -> None:
    if plan.counts.storage_nodes > 100:
        offending = next(
            (
                candidate
                for candidate in plan.candidate_trace
                if candidate.disposition == "ADDED"
                and candidate.resolved_index is not None
                and candidate.resolved_index + 1 + int(plan.anchor_added) > 100
            ),
            plan.candidate_trace[-1],
        )
        origin = offending.origin
        if origin.kind == "INPUT":
            location = [
                "body",
                "declaration",
                "knot_dates",
                origin.input_knot_index,
            ]
            field = "declaration.knot_dates"
        elif origin.kind == "INSTRUMENT_START":
            location = [
                "body",
                "instruments",
                origin.instrument_input_index,
                "start",
            ]
            field = "instruments.start"
        else:
            location = [
                "body",
                "instruments",
                origin.instrument_input_index,
                "maturity",
            ]
            field = "instruments.maturity"
        _raise(
            "CURVE_STORAGE_NODE_LIMIT_EXCEEDED",
            f"{field} produces {plan.counts.storage_nodes} storage nodes; maximum is 100",
            location,
            {
                "policy": plan.requested_policy,
                "parameterization": request.declaration.parameterization,
                "resolved_declared_nodes": plan.counts.resolved_declared_nodes,
                "storage_nodes": plan.counts.storage_nodes,
                "max_storage_nodes": 100,
                "anchor_added": plan.anchor_added,
                "candidate_ordinal": offending.ordinal,
                "candidate_date": offending.date.isoformat(),
                "candidate_origin": offending.origin.model_dump(mode="json"),
                "origins": [
                    origin.model_dump(mode="json")
                    for node in plan.resolved_declared_nodes
                    if node.date == offending.date
                    for origin in node.origins
                ],
            },
        )
    if plan.counts.storage_nodes == 0:
        latest_index = max(
            range(len(request.instruments)),
            key=lambda index: request.instruments[index].maturity,
        )
        _raise(
            "VALIDATION_ERROR",
            "curve knot resolution produced no future storage nodes",
            ["body", "instruments", latest_index, "maturity"],
            {
                "type": "value_error",
                "policy": plan.requested_policy,
                "latest_instrument_end": request.instruments[
                    latest_index
                ].maturity.isoformat(),
            },
        )
    if request.declaration.parameterization not in {
        "ZERO_RATE",
        "LOG_DISCOUNT",
    }:
        return
    minimum = {
        "LOG_LINEAR": 2,
        "LOG_CUBIC_NATURAL": 3,
        "MIXED": 4,
    }.get(request.declaration.log_df_scheme or "", 1)
    if plan.counts.storage_nodes < minimum:
        scheme = request.declaration.log_df_scheme
        _raise(
            "CURVE_SCHEME_NODE_COUNT_INVALID",
            "declaration.log_df_scheme scheme "
            f"{scheme} requires at least {minimum} storage nodes; "
            f"got {plan.counts.storage_nodes}",
            ["body", "declaration", "log_df_scheme"],
            {
                "scheme": scheme,
                "storage_nodes": plan.counts.storage_nodes,
                "minimum_storage_nodes": minimum,
            },
        )


def _check_single_maturity_coverage(
    request: SingleCalibrationRequest,
    plan: ResolvedSingleKnotPlanDTO,
    latest_instrument_end: date,
) -> None:
    final_node = plan.storage_nodes[-1].date
    if final_node >= latest_instrument_end:
        return
    knot_index = len(request.declaration.knot_dates) - 1
    _raise(
        "VALIDATION_ERROR",
        "declaration.knot_dates must cover the final instrument maturity "
        f"{latest_instrument_end.isoformat()}; got {final_node.isoformat()}",
        ["body", "declaration", "knot_dates", knot_index],
        {
            "type": "value_error",
            "latest_instrument_end": latest_instrument_end.isoformat(),
            "final_storage_node": final_node.isoformat(),
        },
    )


def _resolve_initial_guess(
    values: Sequence[float], scalar: float, free_count: int, location: list[str | int]
) -> list[float]:
    if values and len(values) != free_count:
        _raise(
            "INITIAL_GUESS_SHAPE_MISMATCH",
            f"initial_guess_per_node contains {len(values)} raw values; "
            f"expected {free_count} in free-parameter order",
            location,
            {
                "actual_count": len(values),
                "free_parameter_count": free_count,
                "raw_order": "free-parameter",
            },
        )
    return list(values) if values else [scalar] * free_count


def _day_count_fraction(anchor: date, node: date, day_basis: str) -> float:
    if day_basis == "ACT_365F":
        return (node - anchor).days / 365.0
    if day_basis == "ACT_360":
        return (node - anchor).days / 360.0
    if day_basis == "ACT_ACT":
        current = anchor
        result = 0.0
        while current < node:
            next_year = date(current.year + 1, 1, 1)
            end = min(node, next_year)
            year_days = (next_year - date(current.year, 1, 1)).days
            result += (end - current).days / year_days
            current = end
        return result
    if day_basis == "BOND":
        y1, m1, d1 = anchor.year, anchor.month, anchor.day
        y2, m2, d2 = node.year, node.month, node.day
        next_month = date(y1 + int(m1 == 12), (m1 % 12) + 1, 1)
        anchor_is_month_end = anchor == next_month - date.resolution
        node_next_month = date(y2 + int(m2 == 12), (m2 % 12) + 1, 1)
        node_is_month_end = node == node_next_month - date.resolution
        if m1 == 2 and anchor_is_month_end:
            if m2 == 2 and node_is_month_end:
                d2 = 30
            d1 = 30
        if d1 > 30:
            d2 = min(d1, d2)
        return (360 * (y2 - y1) + 30 * (m2 - m1) + d2 - d1) / 360.0
    raise ValueError(f"unsupported dated scalar day basis {day_basis}")


def _resolve_declaration_initial_guess(
    *,
    values: Sequence[float],
    scalar: float,
    free_count: int,
    parameterization: str,
    anchor: date,
    knot_dates: Sequence[date],
    day_basis: str,
    location: list[str | int],
) -> list[float]:
    explicit = _resolve_initial_guess(values, scalar, free_count, location)
    if values or parameterization != "LOG_DISCOUNT":
        return explicit
    if len(knot_dates) != free_count:
        raise RuntimeError(
            "LOG_DISCOUNT seed dates do not match the free-parameter count"
        )
    try:
        return [
            -scalar * _day_count_fraction(anchor, node, day_basis)
            for node in knot_dates
        ]
    except ValueError:
        _raise(
            "UNSUPPORTED_CONVENTION",
            f"day basis {day_basis} cannot resolve LOG_DISCOUNT scalar seeds",
            location,
            {
                "convention_kind": "day_basis",
                "value": day_basis[:128],
            },
        )


_SUPPORTED_DAY_BASES = {
    "30_360",
    "ACT/365F",
    "ACT_360",
    "ACT_365F",
    "ACT_365L",
    "ACT_ACT",
    "Act_365F",
    "BOND",
}
_SUPPORTED_BUSINESS_DAY_CONVENTIONS = {
    "Following",
    "ModifiedFollowing",
    "Preceding",
    "Unadjusted",
}


def _format_convention_field(path: Sequence[str | int]) -> str:
    result = ""
    for item in path:
        if isinstance(item, int):
            result += f"[{item}]"
        else:
            result += ("." if result else "") + item
    return result


def _validate_supported_conventions(
    value: object, path: list[str | int] | None = None
) -> None:
    current_path = path or []
    fields = getattr(type(value), "model_fields", None)
    if fields is not None:
        for field_name in fields:
            field_value = getattr(value, field_name)
            field_path = [*current_path, field_name]
            if field_name in {"day_basis", "libor_basis"}:
                supported = _SUPPORTED_DAY_BASES
                kind = "day_basis"
            elif field_name in {
                "business_day_convention",
                "payment_convention",
                "fixing_convention",
            }:
                supported = _SUPPORTED_BUSINESS_DAY_CONVENTIONS
                kind = "business_day_convention"
            else:
                _validate_supported_conventions(field_value, field_path)
                continue
            if field_value not in supported:
                sanitized = str(field_value)[:128]
                field = _format_convention_field(field_path)
                _raise(
                    "UNSUPPORTED_CONVENTION",
                    f"{field} value '{sanitized}' is not a supported {kind}",
                    ["body", *field_path],
                    {
                        "convention_kind": kind,
                        "value": sanitized,
                    },
                )
        return
    if isinstance(value, Mapping):
        for key, item in value.items():
            _validate_supported_conventions(item, [*current_path, str(key)])
    elif isinstance(value, Sequence) and not isinstance(value, (str, bytes)):
        for index, item in enumerate(value):
            _validate_supported_conventions(item, [*current_path, index])


def _native_instrument_label(kind: str) -> str:
    return {
        "DEPOSIT": "Deposit",
        "FRA": "FRA",
        "FUTURE": "Future",
        "SWAP": "Swap",
        "OIS_SWAP": "OISSwap",
        "BASIS_SWAP": "BasisSwap",
        "XCCY_SWAP": "CrossCurrencySwap",
    }[kind]


def _check_ambiguous_instrument_order(
    instruments: Sequence[object],
    location_prefix: list[str | int],
    native_names: Sequence[str] | None = None,
) -> None:
    seen: dict[tuple[object, object, str], int] = {}
    for index, instrument in enumerate(instruments):
        native_name = (
            native_names[index]
            if native_names is not None
            else _native_instrument_label(instrument.kind)
        )
        key = (instrument.maturity, instrument.start, native_name)
        first = seen.get(key)
        if first is not None:
            _raise(
                "AMBIGUOUS_INSTRUMENT_ORDER",
                f"instruments {first} and {index} have the same calibration ordering key",
                [*location_prefix, index],
                {
                    "first_input_index": first,
                    "second_input_index": index,
                    "first_label": instruments[first].label,
                    "second_label": instrument.label,
                    "maturity": instrument.maturity.isoformat(),
                    "start": instrument.start.isoformat(),
                    "native_name": native_name,
                },
            )
        seen[key] = index


def _check_single_analytic_eligibility(
    request: SingleCalibrationRequest,
    report: object | None,
    native_names_by_input: tuple[str, ...],
) -> None:
    if (
        request.options.jacobian_mode != "ANALYTIC"
        or report is None
        or report.eligible
    ):
        return
    canonical_to_input = sorted(
        range(len(request.instruments)),
        key=lambda index: (
            request.instruments[index].maturity,
            request.instruments[index].start,
            native_names_by_input[index],
        ),
    )
    candidates: list[tuple[int, object]] = []
    for issue in report.issues:
        calibration_index = int(issue.instrument_index)
        input_index = (
            canonical_to_input[calibration_index]
            if 0 <= calibration_index < len(canonical_to_input)
            else -1
        )
        candidates.append((input_index, issue))
    input_index, issue = min(
        candidates,
        key=lambda item: (
            item[0] if item[0] >= 0 else len(request.instruments),
            int(item[1].reset_index),
        ),
    )
    reason = str(getattr(issue.reason, "name", issue.reason)).rsplit(".", 1)[-1]
    if reason == "DISCOUNT_TARGET_REQUIRED":
        location: list[str | int] = [
            "body",
            "declaration",
            "calibrate_discount_curve",
        ]
    else:
        field: list[str | int]
        instrument = request.instruments[input_index]
        if reason == "TRADE_DATE_MISMATCH":
            field = ["trade_date"]
        elif reason == "TEMPLATED_RATE_UNAVAILABLE":
            field = ["kind"]
        elif reason in {"PROJECTION_NOT_ALLOWED", "PROJECTION_REQUIRED"}:
            index_field = {
                "DEPOSIT": "index",
                "FRA": "index",
                "FUTURE": "index",
                "SWAP": "float_index",
                "OIS_SWAP": "overnight_index",
                "BASIS_SWAP": "spread_index",
            }[instrument.kind]
            field = [index_field, "use_projection_curve"]
        else:
            field = ["kind"]
        location = ["body", "instruments", input_index, *field]
    context: dict[str, object] = {
        "reason_code": reason,
        "group": "single",
        "instrument_index": input_index,
        "input_index": input_index,
        "calibration_index": int(issue.instrument_index),
        "native_message": issue.native_message,
    }
    if reason == "TRADE_DATE_MISMATCH":
        context.update(
            {
                "expected": request.today.isoformat(),
                "actual": request.instruments[input_index].trade_date.isoformat(),
            }
        )
        message = (
            "ANALYTIC requires the instrument trade date to equal "
            f"{request.today.isoformat()}"
        )
    else:
        message = f"ANALYTIC is ineligible: {reason}"
    _raise("ANALYTIC_INELIGIBLE", message, location, context)


def _rate_index_field(instrument: object) -> str:
    return {
        "DEPOSIT": "index",
        "FRA": "index",
        "FUTURE": "index",
        "SWAP": "float_index",
        "OIS_SWAP": "overnight_index",
        "BASIS_SWAP": "spread_index",
    }[instrument.kind]


def _missing_xccy_route(
    request: object, reason: str, instrument_index: int
) -> tuple[str, str]:
    instrument = request.basis.instruments[instrument_index]
    for leg in ("domestic", "foreign"):
        index = getattr(instrument.config.convention, f"{leg}_index")
        if hasattr(request, "domestic_curve_block"):
            block = getattr(request, f"{leg}_curve_block")
            discount_slots = set(block.discount_curve_ids)
            forward_slots = {
                _native_tenor_key(value) for value in block.forward_curve_ids
            }
        else:
            group = getattr(request, leg)
            discount_slots = {
                declaration.target_collateral
                for declaration in group.declarations
                if declaration.calibrate_discount_curve
            }
            forward_slots = {
                _native_tenor_key(declaration.target_tenor)
                for declaration in group.declarations
                if not declaration.calibrate_discount_curve
            }
        if reason == "DISCOUNT_ROUTE_MISSING" and index.collateral not in discount_slots:
            return leg, "collateral"
        if (
            reason == "PROJECTION_ROUTE_MISSING"
            and index.use_projection_curve
            and _native_tenor_key(index.forecast_tenor) not in forward_slots
        ):
            return leg, "forecast_tenor"
    return "domestic", (
        "collateral" if reason == "DISCOUNT_ROUTE_MISSING" else "forecast_tenor"
    )


def _native_tenor_key(value: object) -> str:
    text = str(value)
    return text[1:] if text.startswith("P") else text


def _check_xccy_analytic_eligibility(
    request: object, report: object | None, kind: str
) -> None:
    if (
        request.options.jacobian_mode != "ANALYTIC"
        or report is None
        or report.eligible
    ):
        return
    group_rank = {"domestic": 0, "foreign": 1, "staged": 2, "basis": 2}
    issue = min(
        report.issues,
        key=lambda item: (
            group_rank.get(item.group, 3),
            item.declaration_index if item.declaration_index >= 0 else 10_000,
            item.instrument_index if item.instrument_index >= 0 else -1,
            item.reset_index if item.reset_index >= 0 else -1,
        ),
    )
    reason = str(getattr(issue.reason, "name", issue.reason)).rsplit(".", 1)[-1]
    group = str(issue.group)
    declaration_index = int(issue.declaration_index)
    instrument_index = int(issue.instrument_index)
    reset_index = int(issue.reset_index)

    if reason == "LIBOR_BASIS_UNSUPPORTED":
        location = (
            ["body", f"{group}_curve_block", "libor_basis"]
            if kind == "xccy_staged"
            else ["body", group, "libor_basis"]
        )
    elif kind == "xccy_joint" and group in {"domestic", "foreign"}:
        declaration = getattr(request, group).declarations[declaration_index]
        instrument = declaration.instruments[instrument_index]
        suffix = (
            ["trade_date"]
            if reason == "TRADE_DATE_MISMATCH"
            else [_rate_index_field(instrument), "use_projection_curve"]
            if reason in {"PROJECTION_NOT_ALLOWED", "PROJECTION_REQUIRED"}
            else ["kind"]
        )
        location = [
            "body",
            group,
            "declarations",
            declaration_index,
            "instruments",
            instrument_index,
            *suffix,
        ]
    else:
        prefix: list[str | int] = ["body", "basis", "instruments", instrument_index]
        if reason == "PAIR_CURRENCY_MISMATCH":
            suffix = ["config", "pair"]
        elif reason in {"COUPON_PLAN_EMPTY", "CASHFLOW_PLAN_UNSUPPORTED"}:
            suffix = ["config"]
        elif reason == "NOTIONAL_MODE_UNSUPPORTED":
            suffix = ["config", "notional_mode"]
        elif reason == "RESET_MAPPING_INVALID":
            suffix = ["config", "fx_reset"]
        elif reason in {"DISCOUNT_ROUTE_MISSING", "PROJECTION_ROUTE_MISSING"}:
            leg, field = _missing_xccy_route(request, reason, instrument_index)
            suffix = ["config", "convention", f"{leg}_index", field]
        else:
            suffix = ["config"]
        location = [*prefix, *suffix]
    _raise(
        "ANALYTIC_INELIGIBLE",
        f"ANALYTIC is ineligible: {reason}",
        location,
        {
            "reason_code": reason,
            "group": group,
            "declaration_index": (
                declaration_index if declaration_index >= 0 else None
            ),
            "instrument_index": (
                instrument_index if instrument_index >= 0 else None
            ),
            "input_index": instrument_index if instrument_index >= 0 else None,
            "calibration_index": instrument_index if instrument_index >= 0 else None,
            "reset_index": reset_index if reset_index >= 0 else None,
            "native_message": issue.native_message,
        },
    )


def _check_system_and_matrices(
    *,
    residual_count: int,
    parameter_count: int,
    request: object,
) -> None:
    if request.solver.solve_mode == "EXACT" and residual_count > parameter_count:
        _raise(
            "EXACT_SYSTEM_OVERDETERMINED",
            "solver.solve_mode EXACT requires residual_count <= parameter_count; "
            f"got {residual_count} > {parameter_count}",
            ["body", "solver", "solve_mode"],
            {
                "residual_count": residual_count,
                "parameter_count": parameter_count,
            },
        )
    checks = (
        (
            request.options.include_jacobian,
            "jacobian",
            (residual_count, parameter_count),
            "include_jacobian",
        ),
        (
            request.options.include_effective_inverse,
            "effective_inverse",
            (parameter_count, residual_count),
            "include_effective_inverse",
        ),
    )
    for enabled, matrix, shape, field in checks:
        if enabled and max(shape) > 100:
            _raise(
                "MATRIX_DIMENSION_EXCEEDED",
                f"{matrix} expected shape [{shape[0]}, {shape[1]}] exceeds "
                "the 100 x 100 materialization limit",
                ["body", "options", field],
                {
                    "matrix": matrix,
                    "expected_shape": list(shape),
                    "max_materialized_rows": 100,
                    "max_materialized_columns": 100,
                    "max_metadata_dimension": 200,
                },
            )


def _estimate_success_response_bytes(
    request: object, residual_count: int, parameter_count: int
) -> int:
    base = len(canonical_json_bytes(request.model_dump(mode="json"))) * 12 + 16_384
    floats = 0
    if request.options.include_jacobian:
        floats += residual_count * parameter_count
    if request.options.include_effective_inverse:
        floats += residual_count * parameter_count
    return base + floats * 25 + (residual_count + parameter_count) * 256


def _check_response_estimate(
    request: object, residual_count: int, parameter_count: int
) -> None:
    estimate = _estimate_success_response_bytes(
        request, residual_count, parameter_count
    )
    if estimate > 1 << 20:
        preview_reserved = bool(
            request.solver.solve_mode == "EXACT"
            and request.options.include_effective_inverse
        )
        _raise(
            "RESPONSE_LIMIT_EXCEEDED",
            f"estimated response is {estimate} bytes; limit is {1 << 20} bytes",
            ["body", "options"],
            {
                "estimated_bytes": estimate,
                "limit_bytes": 1 << 20,
                "preview_reserved": preview_reserved,
            },
        )


def _referenced_curve_ids(request: object) -> set[str]:
    ids: set[str] = set()
    declaration = getattr(request, "declaration", None)
    if declaration is not None:
        if declaration.base_curve_id:
            ids.add(declaration.base_curve_id)
        ids.update(declaration.discount_curve_ids.values())
        ids.update(declaration.forward_curve_ids.values())
    for block_name in ("domestic_curve_block", "foreign_curve_block"):
        block = getattr(request, block_name, None)
        if block is not None:
            ids.update(block.discount_curve_ids.values())
            ids.update(block.forward_curve_ids.values())
    return ids


def _load_referenced_curves(
    store: StoreProtocol, request: object
) -> dict[str, CurveReconstructionDTO]:
    result: dict[str, CurveReconstructionDTO] = {}
    for curve_id in _referenced_curve_ids(request):
        try:
            result[curve_id] = get_curve_response(store, curve_id)
        except CalibrationHttpError:
            _raise(
                "REFERENCE_MISMATCH",
                f"referenced curve {curve_id} does not exist",
                ["body", "declaration", "base_curve_id"],
                {"curve_id": curve_id, "constraint": "exists"},
            )
    expectations: list[tuple[str, list[str | int], str, set[str]]] = []
    declaration = getattr(request, "declaration", None)
    if declaration is not None:
        if declaration.base_curve_id:
            expectations.append(
                (
                    declaration.base_curve_id,
                    ["body", "declaration", "base_curve_id"],
                    request.currency,
                    set(),
                )
            )
        expectations.extend(
            (
                curve_id,
                ["body", "declaration", "discount_curve_ids", slot],
                request.currency,
                {"discount"},
            )
            for slot, curve_id in declaration.discount_curve_ids.items()
        )
        expectations.extend(
            (
                curve_id,
                ["body", "declaration", "forward_curve_ids", slot],
                request.currency,
                {"forward"},
            )
            for slot, curve_id in declaration.forward_curve_ids.items()
        )
    for block_name in ("domestic_curve_block", "foreign_curve_block"):
        block = getattr(request, block_name, None)
        if block is None:
            continue
        expectations.extend(
            (
                curve_id,
                ["body", block_name, "discount_curve_ids", slot],
                block.currency,
                {"discount"},
            )
            for slot, curve_id in block.discount_curve_ids.items()
        )
        expectations.extend(
            (
                curve_id,
                ["body", block_name, "forward_curve_ids", slot],
                block.currency,
                {"forward"},
            )
            for slot, curve_id in block.forward_curve_ids.items()
        )
    for curve_id, location, currency, roles in expectations:
        curve = result[curve_id]
        if curve.currency != currency:
            _raise(
                "REFERENCE_MISMATCH",
                f"curve {curve_id} has currency {curve.currency}; expected {currency}",
                location,
                {
                    "curve_id": curve_id,
                    "constraint": "currency",
                    "expected": currency,
                    "actual": curve.currency,
                },
            )
        if roles and curve.role not in roles:
            _raise(
                "REFERENCE_MISMATCH",
                f"curve {curve_id} has role {curve.role}; expected "
                f"{' or '.join(sorted(roles))}",
                location,
                {
                    "curve_id": curve_id,
                    "constraint": "role",
                    "expected": sorted(roles),
                    "actual": curve.role,
                },
            )
    return result


async def submit_single_calibration(
    store: StoreProtocol,
    gateway: DalGateway,
    request: SingleCalibrationRequest,
) -> RunningCalibrationRunResponse:
    from app.services.dal_gateway import (
        SingleGatewayAdmissionRequest,
        SingleGatewayPreLockRequest,
    )

    _validate_supported_conventions(request)
    _validate_single_policy(request)
    referenced = _load_referenced_curves(store, request)
    inspected: list[object] = []

    def on_plan(plan: object) -> None:
        _check_plan_bounds(request, plan)
        inspected.append(plan)

    admission = await asyncio.to_thread(
        gateway.plan_single_admission,
        SingleGatewayAdmissionRequest(request, referenced),
        on_plan,
    )
    if len(inspected) != 1 or admission.resolved_knot_plan is not inspected[0]:
        raise RuntimeError("gateway returned a different single-knot plan")
    plan = admission.resolved_knot_plan.to_bounded_dto()
    _check_single_maturity_coverage(
        request, plan, admission.latest_instrument_end
    )
    expected = _project_expected_identity(request, plan)
    _resolve_initial_guess(
        request.declaration.initial_guess_per_node,
        request.solver.initial_guess,
        plan.counts.free_parameters,
        ["body", "declaration", "initial_guess_per_node"],
    )
    resolved_initial_guess = list(
        admission.resolved_initial_guess_per_node
    )
    if len(resolved_initial_guess) != plan.counts.free_parameters:
        raise RuntimeError(
            "gateway resolved an initial-guess vector with the wrong shape"
        )
    _check_ambiguous_instrument_order(
        request.instruments,
        ["body", "instruments"],
        admission.native_names_by_input,
    )
    _check_single_analytic_eligibility(
        request,
        admission.analytic_eligibility,
        admission.native_names_by_input,
    )
    _check_system_and_matrices(
        residual_count=len(request.instruments),
        parameter_count=plan.counts.free_parameters,
        request=request,
    )
    _check_response_estimate(
        request, len(request.instruments), plan.counts.free_parameters
    )
    run_id = uuid4().hex
    plan_hash = canonical_model_hash(plan)
    expected_hash = canonical_model_hash(expected)
    normalized_request = request.model_dump(mode="json")
    submitted_initial_guess = normalized_request["declaration"].pop(
        "initial_guess_per_node"
    )
    normalized_request["declaration"]["submitted_initial_guess_per_node"] = (
        submitted_initial_guess
    )
    normalized_request["declaration"]["resolved_initial_guess_per_node"] = (
        resolved_initial_guess
    )
    normalized_declaration = request.declaration.model_copy(
        update={"initial_guess_per_node": resolved_initial_guess}
    )
    normalized_worker_request = request.model_copy(
        update={"declaration": normalized_declaration}
    )
    record = CalibrationRunRecord(
        id=run_id,
        schema_version=1,
        kind="single",
        name=request.name,
        status="running",
        phase="queued",
        request_payload=normalized_request,
        solver_payload=_normalized_solver(request).model_dump(mode="json"),
        options_payload=_normalized_options(request).model_dump(mode="json"),
        resolved_knot_plan=plan.model_dump(mode="json"),
        resolved_knot_plan_hash=plan_hash,
        expected_execution_identity=expected.model_dump(mode="json"),
        expected_execution_identity_hash=expected_hash,
        actual_jacobian_mode=None,
        actual_execution_identity=None,
        actual_execution_identity_hash=None,
        result_payload=None,
        error_payload=None,
        backend=gateway.backend_name,
        is_native=gateway.is_native,
        created_at=datetime.now(UTC),
        started_at=None,
        finished_at=None,
        native_solve_ms=None,
        serialization_ms=None,
    )
    order = sorted(
        range(len(request.instruments)),
        key=lambda index: (
            request.instruments[index].maturity,
            request.instruments[index].start,
            admission.native_names_by_input[index],
        ),
    )
    instruments = tuple(
        CalibrationInstrumentRecord(
            id=uuid4().hex,
            run_id=run_id,
            group_name="single:curve",
            input_index=input_index,
            calibration_index=calibration_index,
            kind=request.instruments[input_index].kind,
            label=request.instruments[input_index].label,
            native_name=admission.native_names_by_input[input_index],
            payload=request.instruments[input_index].model_dump(mode="json"),
        )
        for calibration_index, input_index in enumerate(order)
    )
    store.add_calibration_admission(record, instruments)
    response = _run_response(store, record, QuoteBumpQueryDTO())
    _schedule_calibration(
        _run_single_worker(
            store,
            gateway,
            run_id,
            SingleGatewayPreLockRequest(normalized_worker_request, referenced),
            instruments,
        )
    )
    return response


async def submit_staged_xccy_calibration(
    store: StoreProtocol,
    gateway: DalGateway,
    request: StagedXccyCalibrationRequest,
) -> RunningCalibrationRunResponse:
    _validate_supported_conventions(request)
    parameter_count = len(request.basis.knot_dates)
    return await _submit_xccy(
        store, gateway, request, "xccy_staged", parameter_count, None
    )


async def submit_joint_xccy_calibration(
    store: StoreProtocol,
    gateway: DalGateway,
    request: JointXccyCalibrationRequest,
) -> RunningCalibrationRunResponse:
    _validate_supported_conventions(request)
    count_plan = build_joint_admission_count_plan(
        domestic=request.domestic.declarations,
        foreign=request.foreign.declarations,
        basis=request.basis,
    )
    _check_joint_declaration_bounds(request, count_plan)
    if count_plan.first_overflowing_declaration is not None:
        item = count_plan.first_overflowing_declaration
        location = (
            ["body", "basis", "parameterization"]
            if item.group == "basis"
            else [
                "body",
                item.group,
                "declarations",
                item.declaration_input_index,
                "parameterization",
            ]
        )
        _raise(
            "JOINT_FREE_PARAMETER_LIMIT_EXCEEDED",
            f"joint calibration has {count_plan.total_free_parameters} free "
            "parameters; maximum is 200",
            location,
            {
                "total_free_parameters": count_plan.total_free_parameters,
                "max_total_free_parameters": 200,
                "cumulative_before": item.cumulative_before,
                "cumulative_after": item.cumulative_after,
                "offending_group": item.group,
                "offending_declaration_index": item.declaration_input_index,
                "offending_parameterization": item.parameterization,
                "offending_storage_nodes": item.storage_nodes,
            },
        )
    return await _submit_xccy(
        store,
        gateway,
        request,
        "xccy_joint",
        count_plan.total_free_parameters,
        count_plan,
    )


async def _submit_xccy(
    store: StoreProtocol,
    gateway: DalGateway,
    request: object,
    kind: Literal["xccy_staged", "xccy_joint"],
    parameter_count: int,
    count_plan: JointAdmissionCountPlan | None,
) -> RunningCalibrationRunResponse:
    from app.services.dal_gateway import (
        JointXccyGatewayRequest,
        StagedXccyGatewayRequest,
    )

    referenced = _load_referenced_curves(store, request)
    normalized_request = request.model_dump(mode="json")
    normalized_gateway_request = request
    if kind == "xccy_staged":
        submitted: list[tuple[str, int, object]] = [
            ("basis", index, instrument)
            for index, instrument in enumerate(request.basis.instruments)
        ]
        resolved = _resolve_declaration_initial_guess(
            values=request.basis.initial_guess_per_node,
            scalar=request.solver.initial_guess,
            free_count=parameter_count,
            parameterization="PIECEWISE_CONSTANT_FWD",
            anchor=request.valuation_time.date(),
            knot_dates=request.basis.knot_dates,
            day_basis="ACT_365F",
            location=["body", "basis", "initial_guess_per_node"],
        )
        submitted_seed = normalized_request["basis"].pop(
            "initial_guess_per_node"
        )
        normalized_request["basis"]["submitted_initial_guess_per_node"] = (
            submitted_seed
        )
        normalized_request["basis"]["resolved_initial_guess_per_node"] = resolved
        normalized_gateway_request = request.model_copy(
            update={
                "basis": request.basis.model_copy(
                    update={"initial_guess_per_node": resolved}
                )
            }
        )
        _check_ambiguous_instrument_order(
            request.basis.instruments,
            ["body", "basis", "instruments"],
        )
    else:
        if count_plan is None:
            raise TypeError("joint admission requires its immutable count plan")
        submitted = []
        for group_name, group in (
            ("domestic", request.domestic),
            ("foreign", request.foreign),
        ):
            for declaration_index, declaration in enumerate(group.declarations):
                submitted.extend(
                    (
                        f"{group_name}:{declaration_index}",
                        instrument_index,
                        instrument,
                    )
                    for instrument_index, instrument in enumerate(
                        declaration.instruments
                    )
                )
        submitted.extend(
            ("basis", index, instrument)
            for index, instrument in enumerate(request.basis.instruments)
        )
        request_declarations = [
            *request.domestic.declarations,
            *request.foreign.declarations,
            request.basis,
        ]
        wire_declarations = [
            *normalized_request["domestic"]["declarations"],
            *normalized_request["foreign"]["declarations"],
            normalized_request["basis"],
        ]
        normalized_declarations: list[object] = []
        for declaration, wire, counts in zip(
            request_declarations,
            wire_declarations,
            count_plan.declarations,
            strict=True,
        ):
            location = (
                    ["body", "basis", "initial_guess_per_node"]
                    if counts.group == "basis"
                    else [
                        "body",
                        counts.group,
                        "declarations",
                        counts.declaration_input_index,
                        "initial_guess_per_node",
                    ]
                )
            day_basis = (
                "ACT_365F"
                if counts.group == "basis"
                else getattr(request, counts.group).libor_basis
            )
            resolved = _resolve_declaration_initial_guess(
                values=declaration.initial_guess_per_node,
                scalar=request.solver.initial_guess,
                free_count=counts.free_parameter_count,
                parameterization=counts.parameterization,
                anchor=request.valuation_time.date(),
                knot_dates=declaration.knot_dates,
                day_basis=day_basis,
                location=location,
            )
            submitted_seed = wire.pop("initial_guess_per_node")
            wire["submitted_initial_guess_per_node"] = submitted_seed
            wire["resolved_initial_guess_per_node"] = resolved
            normalized_declarations.append(
                declaration.model_copy(
                    update={"initial_guess_per_node": resolved}
                )
            )
        domestic_count = len(request.domestic.declarations)
        foreign_count = len(request.foreign.declarations)
        normalized_gateway_request = request.model_copy(
            update={
                "domestic": request.domestic.model_copy(
                    update={
                        "declarations": normalized_declarations[
                            :domestic_count
                        ]
                    }
                ),
                "foreign": request.foreign.model_copy(
                    update={
                        "declarations": normalized_declarations[
                            domestic_count : domestic_count + foreign_count
                        ]
                    }
                ),
                "basis": normalized_declarations[-1],
            }
        )
        for group_name, group in (
            ("domestic", request.domestic),
            ("foreign", request.foreign),
        ):
            for declaration_index, declaration in enumerate(group.declarations):
                _check_ambiguous_instrument_order(
                    declaration.instruments,
                    [
                        "body",
                        group_name,
                        "declarations",
                        declaration_index,
                        "instruments",
                    ],
                )
        _check_ambiguous_instrument_order(
            request.basis.instruments,
            ["body", "basis", "instruments"],
        )
    gateway_request = (
        StagedXccyGatewayRequest(normalized_gateway_request, referenced)
        if kind == "xccy_staged"
        else JointXccyGatewayRequest(normalized_gateway_request)
    )
    if request.options.jacobian_mode == "ANALYTIC":
        validator = (
            gateway.validate_staged_xccy_admission
            if kind == "xccy_staged"
            else gateway.validate_joint_xccy_admission
        )
        report = await asyncio.to_thread(validator, gateway_request)
        _check_xccy_analytic_eligibility(request, report, kind)
    _check_system_and_matrices(
        residual_count=len(submitted),
        parameter_count=parameter_count,
        request=request,
    )
    _check_response_estimate(request, len(submitted), parameter_count)
    run_id = uuid4().hex
    record = CalibrationRunRecord(
        id=run_id,
        schema_version=1,
        kind=kind,
        name=request.name,
        status="running",
        phase="queued",
        request_payload=normalized_request,
        solver_payload=_normalized_solver(request).model_dump(mode="json"),
        options_payload=_normalized_options(request).model_dump(mode="json"),
        resolved_knot_plan=None,
        resolved_knot_plan_hash=None,
        expected_execution_identity=None,
        expected_execution_identity_hash=None,
        actual_jacobian_mode=None,
        actual_execution_identity=None,
        actual_execution_identity_hash=None,
        result_payload=None,
        error_payload=None,
        backend=gateway.backend_name,
        is_native=gateway.is_native,
        created_at=datetime.now(UTC),
        started_at=None,
        finished_at=None,
        native_solve_ms=None,
        serialization_ms=None,
    )
    instruments = tuple(
        CalibrationInstrumentRecord(
            id=uuid4().hex,
            run_id=run_id,
            group_name=group,
            input_index=input_index,
            calibration_index=calibration_index,
            kind=instrument.kind,
            label=instrument.label,
            native_name=instrument.kind,
            payload=instrument.model_dump(mode="json"),
        )
        for calibration_index, (group, input_index, instrument) in enumerate(submitted)
    )
    store.add_calibration_admission(record, instruments)
    response = _run_response(store, record, QuoteBumpQueryDTO())
    _schedule_calibration(
        _run_xccy_worker(
            store, gateway, run_id, gateway_request, instruments, kind
        )
    )
    return response


def _verify_single_evidence(
    store: StoreProtocol,
    calibration_id: str,
    _pre_lock_request: SingleGatewayPreLockRequest,
) -> VerifiedSingleWorkerAdmissionEvidence:
    raw = store.load_single_worker_admission_evidence(calibration_id)
    actual_plan_hash = canonical_model_hash(raw.resolved_knot_plan_raw)
    if actual_plan_hash != raw.resolved_knot_plan_hash:
        evidence = freeze_integrity_error_evidence(
            "PERSISTED_KNOT_PLAN_HASH_MISMATCH",
            "persisted single-knot plan failed canonical hash verification",
            None,
            {
                "stored_plan_hash": raw.resolved_knot_plan_hash,
                "actual_plan_hash": actual_plan_hash,
                "first_difference": None,
            },
        )
        raise PersistedKnotPlanIntegrityError(evidence)
    actual_expected_hash = canonical_model_hash(
        raw.expected_execution_identity_raw
    )
    if actual_expected_hash != raw.expected_execution_identity_hash:
        evidence = freeze_integrity_error_evidence(
            "PERSISTED_EXPECTED_EXECUTION_IDENTITY_HASH_MISMATCH",
            "persisted expected single-knot execution identity failed "
            "canonical hash verification",
            None,
            {
                "integrity_domain": "expected_execution_identity",
                "stored_expected_execution_identity_hash": (
                    raw.expected_execution_identity_hash
                ),
                "actual_expected_execution_identity_hash": actual_expected_hash,
                "first_difference": None,
            },
        )
        raise PersistedExpectedExecutionIdentityIntegrityError(evidence)
    return VerifiedSingleWorkerAdmissionEvidence(
        resolved_knot_plan=ResolvedSingleKnotPlanDTO.model_validate(
            raw.resolved_knot_plan_raw
        ),
        resolved_knot_plan_hash=raw.resolved_knot_plan_hash,
        expected_execution_identity=ExecutionSingleKnotIdentityDTO.model_validate(
            raw.expected_execution_identity_raw
        ),
        expected_execution_identity_hash=raw.expected_execution_identity_hash,
    )


async def _run_single_worker(
    store: StoreProtocol,
    gateway: DalGateway,
    calibration_id: str,
    pre_lock_request: SingleGatewayPreLockRequest,
    instruments: tuple[CalibrationInstrumentRecord, ...],
) -> None:
    from app.services.dal_gateway import GatewayLifecycleTransitionError

    def on_lock(acquired_at: datetime) -> None:
        store.mark_calibration_solving(calibration_id, acquired_at)

    def verify(
        request: SingleGatewayPreLockRequest,
    ) -> VerifiedSingleWorkerAdmissionEvidence:
        return _verify_single_evidence(store, calibration_id, request)

    def on_identity(actual: ExecutionSingleKnotIdentityDTO) -> None:
        expected = ExecutionSingleKnotIdentityDTO.model_validate(
            store.get_calibration_run(calibration_id).expected_execution_identity
        )
        if canonical_model_hash(actual) != canonical_model_hash(expected):
            raise NativeExecutionIdentityMismatchError(
                expected,
                actual,
                comparison_stage="pre_solve_execution_identity",
            )

    try:
        result = await asyncio.to_thread(
            gateway.calibrate_single,
            pre_lock_request,
            on_lock,
            verify,
            on_identity,
        )
        expected = ExecutionSingleKnotIdentityDTO.model_validate(
            store.get_calibration_run(
                calibration_id
            ).expected_execution_identity
        )
        actual = result.actual_execution_identity
        if actual is None:
            raise RuntimeError(
                "single calibration omitted terminal execution identity"
            )
        if canonical_model_hash(actual) != canonical_model_hash(expected):
            raise NativeExecutionIdentityMismatchError(
                expected,
                actual,
                comparison_stage="post_solve_storage",
                actual_jacobian_mode=result.actual_jacobian_mode,
                native_solve_ms=result.native_solve_ms,
            )
        await _terminalize_success(
            store, calibration_id, result, instruments
        )
    except PersistedKnotPlanIntegrityError as exc:
        _terminalize_integrity_error(store, calibration_id, exc, "plan")
    except PersistedExpectedExecutionIdentityIntegrityError as exc:
        _terminalize_integrity_error(store, calibration_id, exc, "expected")
    except NativeExecutionIdentityMismatchError as exc:
        actual_hash = canonical_model_hash(exc.actual)
        error = {
            "code": "NATIVE_KNOT_PLAN_MISMATCH",
            "message": "worker single-knot execution identity does not match "
            "the admitted identity",
            "location": None,
            "context": {
                "comparison_stage": exc.comparison_stage,
                "expected_execution_identity_hash": canonical_model_hash(
                    exc.expected
                ),
                "actual_execution_identity_hash": actual_hash,
                "first_difference": first_canonical_difference(
                    exc.expected, exc.actual
                ),
            },
        }
        store.fail_calibration(
            calibration_id,
            error_payload=error,
            finished_at=datetime.now(UTC),
            actual_jacobian_mode=exc.actual_jacobian_mode,
            actual_execution_identity=exc.actual.model_dump(mode="json"),
            actual_execution_identity_hash=actual_hash,
            native_solve_ms=exc.native_solve_ms,
        )
    except GatewayLifecycleTransitionError as exc:
        _fail_lifecycle(store, calibration_id, exc.transition)
    except Exception as exc:  # noqa: BLE001 - background terminal envelope
        logger.exception("Single calibration %s failed", calibration_id)
        _fail_native(store, calibration_id, exc)


async def _run_xccy_worker(
    store: StoreProtocol,
    gateway: DalGateway,
    calibration_id: str,
    gateway_request: object,
    instruments: tuple[CalibrationInstrumentRecord, ...],
    kind: str,
) -> None:
    def on_lock(acquired_at: datetime) -> None:
        store.mark_calibration_solving(calibration_id, acquired_at)

    try:
        method = (
            gateway.calibrate_staged_xccy
            if kind == "xccy_staged"
            else gateway.calibrate_joint_xccy
        )
        result = await asyncio.to_thread(method, gateway_request, on_lock)
        await _terminalize_success(
            store, calibration_id, result, instruments
        )
    except Exception as exc:  # noqa: BLE001 - background terminal envelope
        logger.exception("XCCY calibration %s failed", calibration_id)
        _fail_native(store, calibration_id, exc)


def _terminalize_integrity_error(
    store: StoreProtocol,
    calibration_id: str,
    exception: _IntegrityError,
    domain: Literal["plan", "expected"],
) -> None:
    try:
        error = to_api_error_dto(exception.error)
        if canonical_json_bytes(error.model_dump(mode="json")) != (
            exception.error.canonical_error_utf8
        ):
            raise RuntimeError("integrity error projection changed canonical bytes")
        method = (
            store.fail_knot_plan_integrity
            if domain == "plan"
            else store.fail_expected_execution_identity_integrity
        )
        method(
            calibration_id,
            datetime.now(UTC),
            exception.error.canonical_error_utf8,
        )
    except Exception:  # noqa: BLE001 - mandatory per-domain lifecycle fallback
        logger.exception(
            "Persisted %s integrity terminalization failed for %s",
            domain,
            calibration_id,
        )
        transition = (
            "persist_knot_plan_integrity_failure"
            if domain == "plan"
            else "persist_expected_identity_integrity_failure"
        )
        _fail_lifecycle(store, calibration_id, transition)


class NativeExecutionIdentityMismatchError(Exception):
    def __init__(
        self,
        expected: ExecutionSingleKnotIdentityDTO,
        actual: ExecutionSingleKnotIdentityDTO,
        *,
        comparison_stage: Literal[
            "pre_solve_execution_identity", "post_solve_storage"
        ],
        actual_jacobian_mode: str | None = None,
        native_solve_ms: float | None = None,
    ) -> None:
        super().__init__("worker execution identity mismatch")
        self.expected = expected
        self.actual = actual
        self.comparison_stage = comparison_stage
        self.actual_jacobian_mode = actual_jacobian_mode
        self.native_solve_ms = native_solve_ms


def _fail_native(
    store: StoreProtocol, calibration_id: str, exception: Exception
) -> None:
    incident_id = uuid4().hex
    store.fail_calibration(
        calibration_id,
        error_payload={
            "code": "NATIVE_CALIBRATION_FAILED",
            "message": "Native calibration failed",
            "location": None,
            "context": {"incident_id": incident_id},
        },
        finished_at=datetime.now(UTC),
    )


def _fail_lifecycle(
    store: StoreProtocol, calibration_id: str, transition: str
) -> None:
    incident_id = uuid4().hex
    try:
        store.fail_calibration(
            calibration_id,
            error_payload={
                "code": "LIFECYCLE_TRANSITION_FAILED",
                "message": "Calibration lifecycle transition failed",
                "location": None,
                "context": {
                    "transition": transition,
                    "incident_id": incident_id,
                },
            },
            finished_at=datetime.now(UTC),
        )
    except Exception:  # noqa: BLE001 - row intentionally remains running/solving
        logger.exception(
            "Lifecycle fallback %s rolled back for calibration %s",
            transition,
            calibration_id,
        )


def _curve_record_and_dto(
    store: StoreProtocol,
    calibration_id: str,
    payload: Mapping[str, object],
    created_at: datetime,
) -> tuple[CurveDefinitionRecord, CurveReconstructionDTO]:
    curve_id = uuid4().hex
    base_id = payload.get("base_curve_id")
    base = get_curve_response(store, base_id) if isinstance(base_id, str) else None
    wire = {
        "dto_version": 1,
        "id": curve_id,
        "name": payload["name"],
        "currency": payload["currency"],
        "role": payload["role"],
        "target": payload["target"],
        "parameterization": payload["parameterization"],
        "anchor_date": payload["anchor_date"],
        "day_count": payload["day_count"],
        "log_df_scheme": payload["log_df_scheme"],
        "node_dates": payload["node_dates"],
        "parameters": payload["parameters"],
        "base_curve_id": base_id,
        "base": base,
        "source_run_id": calibration_id,
    }
    dto = CURVE_RESPONSE_ADAPTER.validate_python(wire)
    stored_payload = {
        key: dto.model_dump(mode="json")[key]
        for key in (
            "target",
            "parameterization",
            "anchor_date",
            "day_count",
            "log_df_scheme",
            "node_dates",
            "parameters",
        )
    }
    return (
        CurveDefinitionRecord(
            id=curve_id,
            dto_version=1,
            name=dto.name,
            currency=dto.currency,
            role=dto.role,
            source_run_id=calibration_id,
            base_curve_id=dto.base_curve_id,
            payload=stored_payload,
            created_at=created_at,
        ),
        dto,
    )


async def _terminalize_success(
    store: StoreProtocol,
    calibration_id: str,
    result: GatewayCalibrationResult,
    instruments: tuple[CalibrationInstrumentRecord, ...],
) -> None:
    store.update_calibration_phase(calibration_id, "serializing")
    created_at = datetime.now(UTC)
    projected = [
        _curve_record_and_dto(store, calibration_id, payload, created_at)
        for payload in result.curves
    ]
    curve_records = tuple(item[0] for item in projected)
    curves = [item[1] for item in projected]
    diagnostics = [
        diagnostic.model_copy(update={"instrument_id": instrument.id})
        for diagnostic, instrument in zip(
            result.instrument_diagnostics, instruments, strict=True
        )
    ]
    instrument_id_map = {
        diagnostic.instrument_id: instrument.id
        for diagnostic, instrument in zip(
            result.instrument_diagnostics, instruments, strict=True
        )
    }

    def persisted_axis(axis: list[str]) -> list[str]:
        return [
            (
                f"residual:{instrument_id_map[item.removeprefix('residual:')]}"
                if item.startswith("residual:")
                and item.removeprefix("residual:") in instrument_id_map
                else item
            )
            for item in axis
        ]

    jacobian = result.jacobian.model_copy(
        update={
            "row_axis": persisted_axis(result.jacobian.row_axis),
            "column_axis": persisted_axis(result.jacobian.column_axis),
        }
    )
    effective_inverse = result.effective_inverse.model_copy(
        update={
            "row_axis": persisted_axis(result.effective_inverse.row_axis),
            "column_axis": persisted_axis(result.effective_inverse.column_axis),
        }
    )
    run = store.get_calibration_run(calibration_id)
    actual = result.actual_execution_identity
    actual_hash = canonical_model_hash(actual) if actual is not None else None
    common = _common_response_fields(run)
    candidate = CompletedCalibrationRunResponse.model_validate(
        common
        | {
            "status": "completed",
            "phase": "finished",
            "finished_at": created_at,
            "actual_jacobian_mode": result.actual_jacobian_mode,
            "actual_execution_identity": actual,
            "actual_execution_identity_hash": actual_hash,
            "solver_diagnostics": result.solver_diagnostics,
            "curves": curves,
            "instrument_diagnostics": diagnostics,
            "fx_forwards": result.fx_forwards,
            "named_ranges": result.named_ranges,
            "jacobian": jacobian,
            "effective_inverse": effective_inverse,
            "quote_bump_preview": None,
            "error": None,
            "timings": CalibrationTimingsDTO(
                native_solve_ms=result.native_solve_ms, serialization_ms=None
            ),
        }
    )
    started = time.perf_counter()
    RUN_RESPONSE_ADAPTER.dump_json(candidate)
    serialization_ms = (time.perf_counter() - started) * 1000.0
    final = candidate.model_copy(
        update={
            "timings": candidate.timings.model_copy(
                update={"serialization_ms": serialization_ms}
            )
        }
    )
    final_bytes = RUN_RESPONSE_ADAPTER.dump_json(final)
    if len(final_bytes) > 1 << 20:
        store.fail_calibration(
            calibration_id,
            error_payload={
                "code": "RESPONSE_LIMIT_EXCEEDED",
                "message": "serialized calibration response exceeds the response limit",
                "location": None,
                "context": {
                    "actual_bytes": len(final_bytes),
                    "limit_bytes": 1 << 20,
                },
            },
            finished_at=created_at,
            actual_jacobian_mode=result.actual_jacobian_mode,
            actual_execution_identity=(
                actual.model_dump(mode="json") if actual is not None else None
            ),
            actual_execution_identity_hash=actual_hash,
            native_solve_ms=result.native_solve_ms,
            serialization_ms=serialization_ms,
        )
        return
    payload = {
        "curve_ids": [curve.id for curve in curves],
        "actual_jacobian_mode": result.actual_jacobian_mode,
        "solver_diagnostics": result.solver_diagnostics.model_dump(mode="json"),
        "instrument_diagnostics": [
            item.model_dump(mode="json") for item in diagnostics
        ],
        "fx_forwards": (
            result.fx_forwards.model_dump(mode="json")
            if result.fx_forwards is not None
            else None
        ),
        "named_ranges": result.named_ranges.model_dump(mode="json"),
        "jacobian": jacobian.model_dump(mode="json"),
        "effective_inverse": effective_inverse.model_dump(mode="json"),
    }
    store.update_calibration_phase(calibration_id, "persisting")
    store.complete_calibration(
        calibration_id,
        result_payload=payload,
        curves=curve_records,
        actual_jacobian_mode=result.actual_jacobian_mode,
        actual_execution_identity=(
            actual.model_dump(mode="json") if actual is not None else None
        ),
        actual_execution_identity_hash=actual_hash,
        native_solve_ms=result.native_solve_ms,
        serialization_ms=serialization_ms,
        finished_at=created_at,
    )


def _common_response_fields(run: CalibrationRunRecord) -> dict[str, object]:
    return {
        "id": run.id,
        "kind": run.kind,
        "name": run.name,
        "schema_version": run.schema_version,
        "created_at": run.created_at,
        "started_at": run.started_at,
        "finished_at": run.finished_at,
        "backend": run.backend,
        "is_native": run.is_native,
        "solver": NormalizedCalibrationSolverDTO.model_validate(run.solver_payload),
        "options": NormalizedCalibrationOptionsDTO.model_validate(
            run.options_payload
        ),
        "requested_jacobian_mode": run.options_payload["jacobian_mode"],
        "resolved_knot_plan": run.resolved_knot_plan,
        "resolved_knot_plan_hash": run.resolved_knot_plan_hash,
        "expected_execution_identity": run.expected_execution_identity,
        "expected_execution_identity_hash": run.expected_execution_identity_hash,
        "actual_execution_identity": run.actual_execution_identity,
        "actual_execution_identity_hash": run.actual_execution_identity_hash,
        "timings": CalibrationTimingsDTO(
            native_solve_ms=run.native_solve_ms,
            serialization_ms=run.serialization_ms,
        ),
    }


def _run_response(
    store: StoreProtocol,
    run: CalibrationRunRecord,
    quote_bump: QuoteBumpQueryDTO,
) -> CalibrationRunResponse:
    common = _common_response_fields(run)
    if quote_bump.quote_bump_index is not None and run.status != "completed":
        _raise(
            "RUN_NOT_COMPLETED",
            f"calibration {run.id} is {run.status}; quote bump requires completed",
            ["path", "calibration_id"],
            {
                "calibration_id": run.id,
                "status": run.status,
                "phase": run.phase,
            },
            status_code=409,
        )
    if run.status == "running":
        return RunningCalibrationRunResponse(
            **common,
            status="running",
            phase=run.phase,
            actual_jacobian_mode=None,
            solver_diagnostics=None,
            curves=[],
            instrument_diagnostics=[],
            fx_forwards=None,
            named_ranges=None,
            jacobian=None,
            effective_inverse=None,
            quote_bump_preview=None,
            error=None,
        )
    if run.status == "failed":
        return FailedCalibrationRunResponse(
            **common,
            status="failed",
            phase="finished",
            actual_jacobian_mode=(
                run.actual_jacobian_mode
            ),
            solver_diagnostics=None,
            curves=[],
            instrument_diagnostics=[],
            fx_forwards=None,
            named_ranges=None,
            jacobian=None,
            effective_inverse=None,
            quote_bump_preview=None,
            error=run.error_payload,
        )
    payload = run.result_payload or {}
    curves = [
        get_curve_response(store, curve_id)
        for curve_id in payload.get("curve_ids", [])
    ]
    inverse = MatrixDTO.model_validate(payload["effective_inverse"])
    preview = None
    if quote_bump.quote_bump_index is not None:
        if inverse.availability != "available":
            _raise(
                "MATRIX_NOT_AVAILABLE",
                "effective inverse matrix is not available",
                ["query", "quote_bump_index"],
                {"availability": inverse.availability},
                status_code=409,
            )
        preview = calculate_quote_bump_preview(
            inverse,
            quote_bump.quote_bump_index,
            quote_bump.quote_bump_size,
        )
    return CompletedCalibrationRunResponse(
        **common,
        status="completed",
        phase="finished",
        actual_jacobian_mode=run.actual_jacobian_mode,
        solver_diagnostics=payload["solver_diagnostics"],
        curves=curves,
        instrument_diagnostics=payload["instrument_diagnostics"],
        fx_forwards=payload["fx_forwards"],
        named_ranges=payload["named_ranges"],
        jacobian=payload["jacobian"],
        effective_inverse=inverse,
        quote_bump_preview=preview,
        error=None,
    )


def get_calibration_response(
    store: StoreProtocol,
    calibration_id: str,
    quote_bump: QuoteBumpQueryDTO,
) -> CalibrationRunResponse:
    try:
        run = store.get_calibration_run(calibration_id)
    except KeyError:
        _raise(
            "CALIBRATION_NOT_FOUND",
            f"calibration {calibration_id} was not found",
            ["path", "calibration_id"],
            {"calibration_id": calibration_id},
            status_code=404,
        )
    return _run_response(store, run, quote_bump)


def get_curve_response(
    store: StoreProtocol,
    curve_id: str,
    *,
    _seen: frozenset[str] = frozenset(),
    _depth: int = 0,
) -> CurveReconstructionDTO:
    if curve_id in _seen or _depth > 8:
        raise RuntimeError("persisted curve base chain is cyclic or too deep")
    try:
        record = store.get_curve_definition(curve_id)
    except KeyError:
        _raise(
            "CURVE_NOT_FOUND",
            f"curve {curve_id} was not found",
            ["path", "curve_id"],
            {"curve_id": curve_id},
            status_code=404,
        )
    if record.dto_version != 1:
        _raise(
            "UNSUPPORTED_CURVE_DTO_VERSION",
            f"curve {curve_id} uses unsupported DTO version {record.dto_version}",
            ["path", "curve_id"],
            {"actual_version": record.dto_version, "supported_versions": [1]},
            status_code=409,
        )
    base = (
        get_curve_response(
            store,
            record.base_curve_id,
            _seen=_seen | {curve_id},
            _depth=_depth + 1,
        )
        if record.base_curve_id is not None
        else None
    )
    return CURVE_RESPONSE_ADAPTER.validate_python(
        {
            "dto_version": record.dto_version,
            "id": record.id,
            "name": record.name,
            "currency": record.currency,
            "role": record.role,
            **record.payload,
            "base_curve_id": record.base_curve_id,
            "base": base,
            "source_run_id": record.source_run_id,
        }
    )
