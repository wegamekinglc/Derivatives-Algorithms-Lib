"""Closed Curve Lab V2 wire contracts and their normative quote registry."""

from __future__ import annotations

import json
from dataclasses import dataclass
from datetime import date, datetime
from typing import Annotated, Literal

from pydantic import BaseModel, ConfigDict, Field, WithJsonSchema, model_validator
from pydantic_core import PydanticCustomError

CurveLabV1SuccessFamily = Literal[
    "DEPOSIT",
    "FRA",
    "FUTURE",
    "OIS",
    "IRS",
    "BASIS_SWAP",
    "XCCY",
]
QuoteCoordinateKind = Literal["RATE", "PRICE", "SPREAD"]
CanonicalRawUnit = Literal["DECIMAL", "PRICE_POINTS"]
QuoteInputConventionV1 = Literal["DECIMAL", "PERCENT", "PRICE_POINTS"]
QuoteDisplayConventionV1 = Literal["DECIMAL", "PERCENT", "PRICE_POINTS"]
CanonicalQuoteDecimalV1 = Annotated[
    str,
    Field(
        max_length=512,
        pattern=r"^-?[0-9]+(?:\.[0-9]+)?$",
        description="Canonical plain base-10 financial value; never a JSON number.",
    ),
]


@dataclass(frozen=True, slots=True)
class CurveLabSuccessRegistryEntry:
    instrument_type: CurveLabV1SuccessFamily
    quote_coordinate_kind: QuoteCoordinateKind
    canonical_raw_unit: CanonicalRawUnit
    exact_risk_raw_bump: str
    normalized_risk_bump: str


CURVE_LAB_V1_SUCCESS_REGISTRY: tuple[CurveLabSuccessRegistryEntry, ...] = (
    CurveLabSuccessRegistryEntry("DEPOSIT", "RATE", "DECIMAL", "0.0001", "0.0001"),
    CurveLabSuccessRegistryEntry("FRA", "RATE", "DECIMAL", "0.0001", "0.0001"),
    CurveLabSuccessRegistryEntry("FUTURE", "PRICE", "PRICE_POINTS", "-0.01", "0.0001"),
    CurveLabSuccessRegistryEntry("OIS", "RATE", "DECIMAL", "0.0001", "0.0001"),
    CurveLabSuccessRegistryEntry("IRS", "RATE", "DECIMAL", "0.0001", "0.0001"),
    CurveLabSuccessRegistryEntry("BASIS_SWAP", "SPREAD", "DECIMAL", "0.0001", "0.0001"),
    CurveLabSuccessRegistryEntry("XCCY", "SPREAD", "DECIMAL", "0.0001", "0.0001"),
)
CURVE_LAB_V1_SUCCESS_FAMILIES: tuple[CurveLabV1SuccessFamily, ...] = tuple(
    row.instrument_type for row in CURVE_LAB_V1_SUCCESS_REGISTRY
)


class CurveLabWireModel(BaseModel):
    model_config = ConfigDict(extra="forbid", validate_default=True)


class CurveLabQuoteCanonicalizationRequest(CurveLabWireModel):
    instrument_type: Annotated[
        str,
        WithJsonSchema(
            {
                "type": "string",
                "enum": list(CURVE_LAB_V1_SUCCESS_FAMILIES),
                "title": "CurveLabV1SuccessFamily",
            }
        ),
    ]
    input_lexeme: str
    input_convention: Annotated[
        str,
        WithJsonSchema(
            {
                "type": "string",
                "enum": ["DECIMAL", "PERCENT", "PRICE_POINTS"],
                "title": "QuoteInputConventionV1",
            }
        ),
    ]

    @model_validator(mode="before")
    @classmethod
    def _forbid_axis_overrides(cls, value: object) -> object:
        derived = (
            "quote_coordinate_kind",
            "canonical_raw_unit",
            "raw_quote",
            "normalized_quote",
            "exact_risk_raw_bump",
            "normalized_risk_bump",
        )
        if isinstance(value, dict):
            for field in derived:
                if field in value:
                    raise PydanticCustomError(
                        "quote_axis_override_forbidden",
                        "Curve Lab quote-axis member '{field}' is server-derived.",
                        {"field": field},
                    )
        return value


class CurveLabQuoteCanonicalizationResponse(CurveLabWireModel):
    model_config = ConfigDict(extra="forbid", validate_default=True, frozen=True)

    instrument_type: CurveLabV1SuccessFamily
    quote_coordinate_kind: QuoteCoordinateKind
    canonical_raw_unit: CanonicalRawUnit
    raw_quote: CanonicalQuoteDecimalV1
    normalized_quote: CanonicalQuoteDecimalV1
    normalized_unit: Literal["DECIMAL_RATE"] = "DECIMAL_RATE"
    exact_risk_raw_bump: CanonicalQuoteDecimalV1
    normalized_risk_bump: CanonicalQuoteDecimalV1

    def financial_bytes(self) -> bytes:
        return json.dumps(
            self.model_dump(mode="json"),
            ensure_ascii=True,
            separators=(",", ":"),
        ).encode("ascii")


class CurveLabCapabilitiesResponse(CurveLabWireModel):
    schema_version: Literal[1] = 1
    success_families: tuple[CurveLabV1SuccessFamily, ...]
    registry: tuple[CurveLabRegistryEntryDTO, ...]
    quote_coordinates: tuple[QuoteCoordinateKind, ...] = ("RATE", "PRICE", "SPREAD")
    input_conventions: tuple[QuoteInputConventionV1, ...] = (
        "DECIMAL",
        "PERCENT",
        "PRICE_POINTS",
    )
    max_quote_bytes: Literal[512] = 512
    risk_limits: dict[str, int] = Field(
        default_factory=lambda: {
            "trades": 1_000,
            "parameters": 500,
            "quotes": 500,
            "price_evaluations": 100_000,
            "calibration_solves": 1_002,
            "aad_recordings": 1_000,
            "estimated_wall_millis": 900_000,
        }
    )
    risk_cost_coefficients: dict[str, int] = Field(
        default_factory=lambda: {
            "context_build_millis": 1,
            "price_evaluation_millis": 1,
            "calibration_solve_millis": 10,
            "aad_recording_overhead_millis": 1,
        }
    )


class CurveLabRegistryEntryDTO(CurveLabWireModel):
    instrument_type: CurveLabV1SuccessFamily
    quote_coordinate_kind: QuoteCoordinateKind
    canonical_raw_unit: CanonicalRawUnit
    exact_risk_raw_bump: CanonicalQuoteDecimalV1
    normalized_risk_bump: CanonicalQuoteDecimalV1


class CurveLabErrorDetail(CurveLabWireModel):
    code: str
    message: str
    field: str
    value: str | int | None = None
    resource_id: str | None = None
    details: dict[str, object] = Field(default_factory=dict)


class CurveLabErrorResponse(CurveLabWireModel):
    detail: CurveLabErrorDetail


CurveLabModeV2 = Literal[
    "SINGLE",
    "MULTI_CURVE",
    "STAGED_XCCY",
    "JOINT_XCCY",
]
CurveRoleV2 = Literal["DISCOUNT", "PROJECTION", "BASIS"]


class CurveDeclarationInputV2(CurveLabWireModel):
    component_key: Annotated[str, Field(min_length=1, max_length=256)]
    role: CurveRoleV2
    currency: Annotated[str, Field(min_length=3, max_length=16)]
    parameterization: Annotated[str, Field(min_length=1, max_length=64)]


_DERIVED_INSTRUMENT_FIELDS = (
    "quote_coordinate_kind",
    "canonical_raw_unit",
    "normalized_quote",
    "exact_risk_raw_bump",
    "normalized_risk_bump",
)


class InstrumentDefinitionInputV2(CurveLabWireModel):
    instrument_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")] | None = None
    source_instrument_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")] | None = None
    instrument_type: str
    trade_date: date
    start_date: date
    maturity_date: date
    currency_or_pair: Annotated[str, Field(min_length=3, max_length=32)]
    raw_quote: str
    source: Annotated[str, Field(min_length=1, max_length=128)]
    observed_at: datetime
    included: bool = True
    terms: dict[str, object]

    @model_validator(mode="before")
    @classmethod
    def _forbid_non_durable_quote_members(cls, value: object) -> object:
        if isinstance(value, dict):
            if "input_convention" in value:
                raise PydanticCustomError(
                    "quote_axis_override_forbidden",
                    "Durable instruments accept canonical raw_quote only.",
                    {"field": "input_convention"},
                )
            for field in _DERIVED_INSTRUMENT_FIELDS:
                if field in value:
                    raise PydanticCustomError(
                        "quote_axis_override_forbidden",
                        "Curve Lab quote-axis member '{field}' is server-derived.",
                        {"field": field},
                    )
        return value


class StoredInstrumentDefinitionV2(CurveLabWireModel):
    model_config = ConfigDict(extra="forbid", validate_default=True, frozen=True)

    instrument_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    source_instrument_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")] | None = None
    instrument_type: CurveLabV1SuccessFamily
    trade_date: date
    start_date: date
    maturity_date: date
    currency_or_pair: str
    quote_coordinate_kind: QuoteCoordinateKind
    canonical_raw_unit: CanonicalRawUnit
    raw_quote: CanonicalQuoteDecimalV1
    normalized_quote: CanonicalQuoteDecimalV1
    exact_risk_raw_bump: CanonicalQuoteDecimalV1
    normalized_risk_bump: CanonicalQuoteDecimalV1
    source: str
    observed_at: datetime
    included: bool
    terms: dict[str, object]


class CurveDraftDocumentInputV2(CurveLabWireModel):
    schema_version: Literal[2] = 2
    mode: CurveLabModeV2
    as_of_date: date
    market_snapshot_id: Annotated[str, Field(min_length=1, max_length=256)]
    declarations: tuple[CurveDeclarationInputV2, ...]
    instruments: tuple[InstrumentDefinitionInputV2, ...]
    dependency_version_ids: tuple[
        Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")], ...
    ] = ()
    solver: dict[str, object]


class CurveDraftDocumentV2(CurveLabWireModel):
    model_config = ConfigDict(extra="forbid", validate_default=True, frozen=True)

    schema_version: Literal[2] = 2
    mode: CurveLabModeV2
    as_of_date: date
    market_snapshot_id: str
    declarations: tuple[CurveDeclarationInputV2, ...]
    instruments: tuple[StoredInstrumentDefinitionV2, ...]
    dependency_version_ids: tuple[str, ...]
    solver: dict[str, object]


class CurveDraftResponse(CurveLabWireModel):
    id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    schema_version: Literal[2] = 2
    revision: Annotated[int, Field(ge=1)]
    fingerprint: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")]
    state: Literal["READY_TO_BUILD", "MODIFIED"]
    document: CurveDraftDocumentV2
    created_at: datetime
    updated_at: datetime


class QuoteAxisEntryV2(CurveLabWireModel):
    model_config = ConfigDict(extra="forbid", validate_default=True, frozen=True)

    global_quote_index: Annotated[int, Field(ge=0)]
    quote_id: str
    instrument_id: str
    component_key: str
    stage_id: str
    group_id: str
    stage_local_quote_index: Annotated[int, Field(ge=0)]
    quote_coordinate_kind: QuoteCoordinateKind
    canonical_raw_unit: CanonicalRawUnit
    raw_quote: CanonicalQuoteDecimalV1
    normalized_quote: CanonicalQuoteDecimalV1
    normalized_unit: Literal["DECIMAL_RATE"] = "DECIMAL_RATE"
    exact_risk_raw_bump: CanonicalQuoteDecimalV1
    normalized_risk_bump: CanonicalQuoteDecimalV1
    display_label: str


class ParameterAxisEntryV2(CurveLabWireModel):
    model_config = ConfigDict(extra="forbid", validate_default=True, frozen=True)

    global_parameter_index: Annotated[int, Field(ge=0)]
    parameter_id: str
    component_key: str
    stage_id: str
    stage_local_parameter_index: Annotated[int, Field(ge=0)]
    component_local_parameter_index: Annotated[int, Field(ge=0)]
    coordinate_kind: Literal[
        "PIECEWISE_CONSTANT_FWD",
        "PIECEWISE_LINEAR_FWD",
        "ZERO_RATE",
        "LOG_DISCOUNT",
    ]
    node_date: date
    side: Literal["LEFT", "RIGHT"] | None
    native_parameter_unit: str
    display_label: str


class CurveLabDependencyManifestEntryV2(CurveLabWireModel):
    version_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    content_hash: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")]
    root_kind: Literal["DISCOUNT_CURVE", "CURVE_SET"]


class CurveBuildRunResponse(CurveLabWireModel):
    id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    draft_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    draft_revision: Annotated[int, Field(ge=1)]
    draft_fingerprint: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")]
    state: Literal[
        "QUEUED",
        "DECLARING",
        "RESOLVING_DEPENDENCIES",
        "NORMALIZING",
        "SOLVING",
        "DIAGNOSTICS",
        "SERIALIZING",
        "SUCCEEDED",
        "FAILED",
        "TIMED_OUT",
    ]
    stale: bool
    request: CurveDraftDocumentV2
    resolved_plan: dict[str, object]
    quote_axis: tuple[QuoteAxisEntryV2, ...]
    parameter_axis: tuple[ParameterAxisEntryV2, ...]
    dependency_manifest: tuple[CurveLabDependencyManifestEntryV2, ...]
    diagnostics: dict[str, object] | None
    native_payload_hash: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")] | None = None
    error: CurveLabErrorDetail | None = None
    created_at: datetime
    finished_at: datetime | None = None


class CurveVersionCreateRequest(CurveLabWireModel):
    draft_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    draft_revision: Annotated[int, Field(ge=1)]
    draft_fingerprint: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")]
    build_run_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    name: Annotated[str, Field(min_length=1, max_length=256)]
    version_note: Annotated[str, Field(max_length=4096)] | None = None
    tags: tuple[Annotated[str, Field(min_length=1, max_length=128)], ...] = ()
    idempotency_key: Annotated[str, Field(min_length=1, max_length=256)]


class CurveVersionResponse(CurveLabWireModel):
    id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    source_kind: Literal["BUILD", "IMPORT"]
    build_run_id: str | None
    import_job_id: str | None
    name: str
    version_note: str | None
    tags: tuple[str, ...]
    native_payload_length: Annotated[int, Field(ge=1)]
    native_payload_hash: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")]
    archive_numeric_format: Literal["JSON_MAX_DIGITS10_V1"]
    root_kind: Literal["DISCOUNT_CURVE", "CURVE_SET"]
    build_validation_state: Literal["VERIFIED", "IMPORT_RECONSTRUCTED"]
    visibility_state: Literal["VISIBLE", "ARCHIVED"]
    created_at: datetime


class CurveImportJobResponse(CurveLabWireModel):
    id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    request_hash: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")]
    compressed_payload_length: int
    expanded_payload_length: int
    state: Literal["SUCCEEDED", "FAILED"]
    phase: str
    resulting_version_id: str | None = None
    error: CurveLabErrorDetail | None = None
    created_at: datetime
    finished_at: datetime | None = None


RiskMeasureV2 = Literal["PV", "DV01", "KEY_RATE_DV01"]
SensitivityLayerV2 = Literal[
    "TRADE_TO_NODE",
    "CALIBRATION_JACOBIAN",
    "COMPOSED_QUOTE_DIAGNOSTIC",
]


class RateTradeDefinitionInputV2(CurveLabWireModel):
    trade_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    instrument_type: CurveLabV1SuccessFamily
    trade_date: date
    start_date: date
    maturity_date: date
    currency_or_pair: Annotated[str, Field(min_length=3, max_length=32)]
    terms: dict[str, object]


class RiskTargetV2(CurveLabWireModel):
    trades: tuple[RateTradeDefinitionInputV2, ...]


class RiskRunOptionsV2(CurveLabWireModel):
    aad_fallback: Literal["ALLOW", "FORBID"] = "ALLOW"
    jacobian_replay_fallback: Literal["ALLOW", "FORBID"] = "ALLOW"


class RiskRunRequestV2(CurveLabWireModel):
    curve_version_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    target: RiskTargetV2
    measures: tuple[RiskMeasureV2, ...]
    sensitivity_layers: tuple[SensitivityLayerV2, ...] = ()
    fixing_snapshot_id: Annotated[str, Field(min_length=1, max_length=256)]
    evaluation_time: datetime
    base_currency: Annotated[str, Field(min_length=3, max_length=16)]
    options: RiskRunOptionsV2 = Field(default_factory=RiskRunOptionsV2)

    @model_validator(mode="after")
    def _validate_sets(self) -> RiskRunRequestV2:
        if not self.measures:
            raise ValueError("measures must be a non-empty set")
        if len(set(self.measures)) != len(self.measures):
            raise ValueError("measures must not contain duplicates")
        if len(set(self.sensitivity_layers)) != len(self.sensitivity_layers):
            raise ValueError("sensitivity_layers must not contain duplicates")
        return self


class RiskWorkEstimateV2(CurveLabWireModel):
    T: Annotated[int, Field(ge=0)]
    T_aad: Annotated[int, Field(ge=0)]
    P: Annotated[int, Field(ge=0)]
    Q: Annotated[int, Field(ge=0)]
    I_node: Literal[0, 1]
    N_param: Annotated[int, Field(ge=0)]
    N_aad: Annotated[int, Field(ge=0)]
    N_quote: Annotated[int, Field(ge=0)]
    N_jac: Annotated[int, Field(ge=0)]
    parameter_bump_price_evaluations: Annotated[int, Field(ge=0)]
    aad_price_evaluations: Annotated[int, Field(ge=0)]
    quote_bump_price_evaluations: Annotated[int, Field(ge=0)]
    contexts: Annotated[int, Field(ge=0)]
    price_evaluations: Annotated[int, Field(ge=0)]
    calibration_solves: Annotated[int, Field(ge=0)]
    aad_recordings: Annotated[int, Field(ge=0)]
    estimated_wall_millis: Annotated[int, Field(ge=0)]
    overflow: bool = False


class FixingKeyV1(CurveLabWireModel):
    index_name: str
    fixing_time: datetime


class PricingTradeSuccessV1(CurveLabWireModel):
    trade_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    instrument_type: CurveLabV1SuccessFamily
    status: Literal["SUCCEEDED"]
    pv: CanonicalQuoteDecimalV1
    currency: Annotated[str, Field(min_length=3, max_length=16)]
    normalized_plan_hash: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")]
    required_historical_fixing_keys: tuple[FixingKeyV1, ...]
    dependency_component_keys: tuple[str, ...]


class PricingTradeFailureV1(CurveLabWireModel):
    trade_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    instrument_type: CurveLabV1SuccessFamily
    status: Literal["FAILED"]
    error: CurveLabErrorDetail
    required_historical_fixing_keys: tuple[FixingKeyV1, ...]
    missing_historical_fixing_keys: tuple[FixingKeyV1, ...]
    dependency_component_keys: tuple[str, ...]


PricingTradeResultV1 = Annotated[
    PricingTradeSuccessV1 | PricingTradeFailureV1,
    Field(discriminator="status"),
]


class RiskValueV2(CurveLabWireModel):
    trade_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    value: CanonicalQuoteDecimalV1


class QuoteBumpResultV2(CurveLabWireModel):
    bump_id: str
    kind: Literal["PARALLEL", "KEY_RATE"]
    quote_id: str | None
    status: Literal["SUCCEEDED", "FAILED"]
    raw_bump: CanonicalQuoteDecimalV1 | None
    normalized_bump: CanonicalQuoteDecimalV1 | None
    calibration_status: Literal["SUCCEEDED", "FAILED"]
    pricing_status: Literal["SUCCEEDED", "FAILED"]
    error: CurveLabErrorDetail | None


class SensitivityMatrixReferenceV2(CurveLabWireModel):
    matrix_id: str
    availability: Literal[
        "AVAILABLE",
        "NOT_REQUESTED",
        "NOT_AVAILABLE_FOR_MODE",
        "FAILED",
    ]
    method: str


class RiskRunResultV2(CurveLabWireModel):
    pricing: tuple[PricingTradeResultV1, ...]
    dv01: tuple[RiskValueV2, ...] | None = None
    quote_bumps: tuple[QuoteBumpResultV2, ...] | None = None
    key_rate_sum: tuple[RiskValueV2, ...] | None = None
    nonlinear_reconciliation: tuple[RiskValueV2, ...] | None = None
    sensitivity_matrices: tuple[SensitivityMatrixReferenceV2, ...] | None = None


class RiskRunResponseV2(CurveLabWireModel):
    id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    curve_version_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    calibration_run_id: str | None
    import_job_id: str | None
    source_kind: Literal["BUILD_VERSION", "IMPORT_VERSION"]
    request: RiskRunRequestV2
    target_fingerprint: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")]
    quote_axis: tuple[QuoteAxisEntryV2, ...] | None
    parameter_axis: tuple[ParameterAxisEntryV2, ...]
    estimated_work: RiskWorkEstimateV2
    state: Literal["QUEUED", "RUNNING", "SUCCEEDED", "FAILED", "TIMED_OUT"]
    result: RiskRunResultV2 | None
    error: CurveLabErrorDetail | None
    created_at: datetime
    finished_at: datetime | None


class MatrixResultV2(CurveLabWireModel):
    matrix_id: str
    mathematical_name: str
    orientation: str
    row_axis_ref: str
    column_axis_ref: str
    rows: Annotated[int, Field(ge=0)]
    columns: Annotated[int, Field(ge=0)]
    availability: Literal[
        "AVAILABLE",
        "NOT_REQUESTED",
        "NOT_AVAILABLE_FOR_MODE",
        "FAILED",
    ]
    availability_reason_code: str | None = None
    availability_reason: str | None = None
    method: str
    bump_target: str | None = None
    bump_size: str | None = None
    input_unit: str
    output_unit: str
    values: tuple[tuple[str, ...], ...] | None = None
    failure: CurveLabErrorDetail | None = None
