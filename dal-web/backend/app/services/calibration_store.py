"""Store-neutral calibration persistence records."""

from __future__ import annotations

from collections.abc import Mapping
from dataclasses import dataclass
from datetime import datetime
from typing import NamedTuple

from pydantic import JsonValue


@dataclass(frozen=True, slots=True)
class CalibrationRunRecord:
    id: str
    schema_version: int
    kind: str
    name: str
    status: str
    phase: str
    request_payload: dict
    solver_payload: dict
    options_payload: dict
    resolved_knot_plan: dict | None
    resolved_knot_plan_hash: str | None
    expected_execution_identity: dict | None
    expected_execution_identity_hash: str | None
    actual_jacobian_mode: str | None
    actual_execution_identity: dict | None
    actual_execution_identity_hash: str | None
    result_payload: dict | None
    error_payload: dict | None
    backend: str
    is_native: bool
    created_at: datetime
    started_at: datetime | None
    finished_at: datetime | None
    native_solve_ms: float | None
    serialization_ms: float | None


@dataclass(frozen=True, slots=True)
class CalibrationInstrumentRecord:
    id: str
    run_id: str
    group_name: str
    input_index: int
    calibration_index: int
    kind: str
    label: str
    native_name: str
    payload: dict


@dataclass(frozen=True, slots=True)
class CurveDefinitionRecord:
    id: str
    dto_version: int
    name: str
    currency: str
    role: str
    source_run_id: str
    base_curve_id: str | None
    payload: dict
    created_at: datetime


class RawSingleWorkerAdmissionEvidence(NamedTuple):
    resolved_knot_plan_raw: Mapping[str, JsonValue]
    resolved_knot_plan_hash: str
    expected_execution_identity_raw: Mapping[str, JsonValue]
    expected_execution_identity_hash: str
