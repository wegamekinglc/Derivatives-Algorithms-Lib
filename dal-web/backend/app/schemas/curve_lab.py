"""Closed Curve Lab V2 wire contracts and their normative quote registry."""

from __future__ import annotations

import json
from dataclasses import dataclass
from datetime import date, datetime
from types import MappingProxyType
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
FiniteFloat = Annotated[float, Field(allow_inf_nan=False)]
PositiveFiniteFloat = Annotated[float, Field(gt=0, allow_inf_nan=False)]
QuoteInputConventionInput = Annotated[
    str,
    WithJsonSchema(
        {
            "type": "string",
            "enum": ["DECIMAL", "PERCENT", "PRICE_POINTS"],
            "title": "QuoteInputConventionV1",
        }
    ),
]
QuoteDisplayConventionInput = Annotated[
    str,
    WithJsonSchema(
        {
            "type": "string",
            "enum": ["DECIMAL", "PERCENT", "PRICE_POINTS"],
            "title": "QuoteDisplayConventionV1",
        }
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
CURVE_LAB_RISK_LIMITS = MappingProxyType(
    {
        "trades": 1_000,
        "parameters": 500,
        "quotes": 500,
        "price_evaluations": 100_000,
        "calibration_solves": 1_002,
        "aad_recordings": 1_000,
        "estimated_wall_millis": 900_000,
    }
)
CURVE_LAB_RISK_COST_COEFFICIENTS = MappingProxyType(
    {
        "context_build_millis": 1,
        "price_evaluation_millis": 1,
        "calibration_solve_millis": 10,
        "aad_recording_overhead_millis": 1,
    }
)
CurveLabSuccessFamilyInput = Annotated[
    str,
    WithJsonSchema(
        {
            "type": "string",
            "enum": list(CURVE_LAB_V1_SUCCESS_FAMILIES),
            "title": "CurveLabV1SuccessFamily",
        }
    ),
]


class CurveLabWireModel(BaseModel):
    model_config = ConfigDict(
        extra="forbid",
        validate_default=True,
        allow_inf_nan=False,
    )


class CurveLabQuoteCanonicalizationRequest(CurveLabWireModel):
    instrument_type: CurveLabSuccessFamilyInput
    input_lexeme: str
    input_convention: QuoteInputConventionInput

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


class CurveLabQuoteRenderingRequest(CurveLabWireModel):
    instrument_type: CurveLabSuccessFamilyInput
    canonical_raw_quote: CanonicalQuoteDecimalV1
    display_convention: QuoteDisplayConventionInput
    display_scale: Annotated[int, Field(strict=True)]


class CurveLabQuoteRenderingResponse(CurveLabWireModel):
    model_config = ConfigDict(extra="forbid", validate_default=True, frozen=True)

    rendered_quote: str


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
    risk_limits: dict[str, int] = Field(default_factory=lambda: dict(CURVE_LAB_RISK_LIMITS))
    risk_cost_coefficients: dict[str, int] = Field(
        default_factory=lambda: dict(CURVE_LAB_RISK_COST_COEFFICIENTS)
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
CurveParameterizationV2 = Literal[
    "PIECEWISE_CONSTANT_FWD",
    "PIECEWISE_LINEAR_FWD",
    "ZERO_RATE",
    "LOG_DISCOUNT",
]
TenorV2 = Literal["1M", "3M", "6M", "12M"]
CollateralV2 = Literal["OIS", "GC", "NONE"]


class CurveDeclarationInputV2(CurveLabWireModel):
    component_key: Annotated[str, Field(min_length=1, max_length=256)]
    role: CurveRoleV2
    currency: Annotated[str, Field(pattern=r"^[A-Z]{3}$")]
    parameterization: CurveParameterizationV2
    log_df_scheme: Literal["LOG_LINEAR", "NATURAL_CUBIC", "LINEAR_CUBIC"] = "LOG_LINEAR"


class CurveLabSolverInputV2(CurveLabWireModel):
    solve_mode: Literal["EXACT", "APPROXIMATE"] = "EXACT"
    smoothing_weight: PositiveFiniteFloat = 1.0
    tolerance: PositiveFiniteFloat = 1.0e-8
    fit_tolerance: PositiveFiniteFloat = 1.0e-6
    max_evaluations: Annotated[int, Field(gt=0, le=1_000_000)] = 200
    max_restarts: Annotated[int, Field(ge=0, le=10_000)] = 20
    initial_guess: FiniteFloat = 0.05
    libor_basis: Literal["ACT_365F", "ACT_360", "30_360"] = "ACT_365F"
    parameterization: CurveParameterizationV2 | None = None


class CurveInstrumentTermsV2(CurveLabWireModel):
    """Closed superset projected through a family-specific allowlist below."""

    component_key: Annotated[str, Field(min_length=1, max_length=256)] | None = None
    index: Annotated[str, Field(min_length=1, max_length=128)] | None = None
    index_name: Annotated[str, Field(min_length=1, max_length=128)] | None = None
    forecast_tenor: TenorV2 | None = None
    day_basis: Literal["ACT_365F", "ACT_360", "30_360"] | None = None
    collateral: CollateralV2 | None = None
    use_projection_curve: bool | None = None
    convexity_adjustment: CanonicalQuoteDecimalV1 | FiniteFloat | None = None
    fixed_payment_frequency: TenorV2 | None = None
    fixed_day_basis: Literal["ACT_365F", "ACT_360", "30_360"] | None = None
    float_payment_frequency: TenorV2 | None = None
    float_day_basis: Literal["ACT_365F", "ACT_360", "30_360"] | None = None
    float_forecast_tenor: TenorV2 | None = None
    float_collateral: CollateralV2 | None = None
    float_use_projection_curve: bool | None = None
    spread_payment_frequency: TenorV2 | None = None
    spread_day_basis: Literal["ACT_365F", "ACT_360", "30_360"] | None = None
    spread_forecast_tenor: TenorV2 | None = None
    spread_collateral: CollateralV2 | None = None
    spread_use_projection_curve: bool | None = None
    reference_payment_frequency: TenorV2 | None = None
    reference_day_basis: Literal["ACT_365F", "ACT_360", "30_360"] | None = None
    reference_forecast_tenor: TenorV2 | None = None
    reference_collateral: CollateralV2 | None = None
    reference_use_projection_curve: bool | None = None
    domestic_notional: CanonicalQuoteDecimalV1 | FiniteFloat | None = None
    foreign_notional: CanonicalQuoteDecimalV1 | FiniteFloat | None = None
    domestic_payment_frequency: TenorV2 | None = None
    domestic_day_basis: Literal["ACT_365F", "ACT_360", "30_360"] | None = None
    domestic_forecast_tenor: TenorV2 | None = None
    domestic_collateral: CollateralV2 | None = None
    domestic_use_projection_curve: bool | None = None
    foreign_payment_frequency: TenorV2 | None = None
    foreign_day_basis: Literal["ACT_365F", "ACT_360", "30_360"] | None = None
    foreign_forecast_tenor: TenorV2 | None = None
    foreign_collateral: CollateralV2 | None = None
    foreign_use_projection_curve: bool | None = None
    fx_spot: PositiveFiniteFloat | None = None
    fx_forward_collateral: CollateralV2 | None = None


_CALIBRATION_TERM_KEYS: dict[str, frozenset[str]] = {
    "DEPOSIT": frozenset(
        {
            "component_key",
            "index",
            "index_name",
            "forecast_tenor",
            "day_basis",
            "collateral",
            "use_projection_curve",
        }
    ),
    "FRA": frozenset(
        {
            "component_key",
            "index",
            "index_name",
            "forecast_tenor",
            "day_basis",
            "collateral",
            "use_projection_curve",
        }
    ),
    "FUTURE": frozenset(
        {
            "component_key",
            "index",
            "index_name",
            "forecast_tenor",
            "day_basis",
            "collateral",
            "use_projection_curve",
            "convexity_adjustment",
        }
    ),
    "OIS": frozenset(
        {
            "component_key",
            "fixed_payment_frequency",
            "fixed_day_basis",
            "float_payment_frequency",
            "float_day_basis",
            "float_forecast_tenor",
            "float_collateral",
            "float_use_projection_curve",
            "index_name",
        }
    ),
    "IRS": frozenset(
        {
            "component_key",
            "fixed_payment_frequency",
            "fixed_day_basis",
            "float_payment_frequency",
            "float_day_basis",
            "float_forecast_tenor",
            "float_collateral",
            "float_use_projection_curve",
            "index_name",
        }
    ),
    "BASIS_SWAP": frozenset(
        {
            "component_key",
            "spread_payment_frequency",
            "spread_day_basis",
            "spread_forecast_tenor",
            "spread_collateral",
            "spread_use_projection_curve",
            "reference_payment_frequency",
            "reference_day_basis",
            "reference_forecast_tenor",
            "reference_collateral",
            "reference_use_projection_curve",
        }
    ),
    "XCCY": frozenset(
        {
            "component_key",
            "domestic_notional",
            "foreign_notional",
            "domestic_payment_frequency",
            "domestic_day_basis",
            "domestic_forecast_tenor",
            "domestic_collateral",
            "domestic_use_projection_curve",
            "foreign_payment_frequency",
            "foreign_day_basis",
            "foreign_forecast_tenor",
            "foreign_collateral",
            "foreign_use_projection_curve",
            "fx_spot",
            "fx_forward_collateral",
        }
    ),
}


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
    terms: CurveInstrumentTermsV2

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

    @model_validator(mode="after")
    def _validate_family_terms_and_dates(self) -> InstrumentDefinitionInputV2:
        allowed = _CALIBRATION_TERM_KEYS.get(self.instrument_type)
        supplied = self.terms.model_fields_set
        if allowed is not None and not supplied <= allowed:
            field = sorted(supplied - allowed)[0]
            raise ValueError(f"{self.instrument_type} calibration terms do not permit {field}")
        if not (self.trade_date <= self.start_date < self.maturity_date):
            raise ValueError(
                "instrument dates must satisfy trade_date <= start_date < maturity_date"
            )
        if self.instrument_type == "XCCY":
            required = {
                "component_key",
                "domestic_notional",
                "foreign_notional",
                "fx_spot",
            }
            if not required <= supplied:
                raise ValueError(
                    "XCCY calibration terms require component_key, notionals, and fx_spot"
                )
        return self


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
    dependency_version_ids: tuple[Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")], ...] = ()
    solver: CurveLabSolverInputV2

    @model_validator(mode="after")
    def _validate_topology(self) -> CurveDraftDocumentInputV2:
        declarations = self.declarations
        included = tuple(item for item in self.instruments if item.included)
        if not declarations or not included:
            raise PydanticCustomError(
                "draft_topology_invalid",
                "Curve draft requires declarations and included instruments.",
                {"field": "document"},
            )
        keys = tuple(item.component_key for item in declarations)
        if len(set(keys)) != len(keys):
            raise PydanticCustomError(
                "draft_topology_invalid",
                "Curve declaration component keys must be unique.",
                {"field": "declarations"},
            )
        roles = tuple(item.role for item in declarations)
        if self.mode == "SINGLE" and (len(declarations) != 1 or roles != ("DISCOUNT",)):
            raise PydanticCustomError(
                "draft_topology_invalid",
                "SINGLE mode requires exactly one discount declaration.",
                {"field": "mode"},
            )
        if self.mode == "MULTI_CURVE" and (
            len(declarations) < 2
            or "DISCOUNT" not in roles
            or "BASIS" in roles
            or len({item.currency for item in declarations}) != 1
        ):
            raise PydanticCustomError(
                "draft_topology_invalid",
                "MULTI_CURVE requires one-currency discount/projection declarations.",
                {"field": "declarations"},
            )
        if self.mode in {"STAGED_XCCY", "JOINT_XCCY"} and roles.count("BASIS") != 1:
            raise PydanticCustomError(
                "draft_topology_invalid",
                "XCCY modes require exactly one basis declaration.",
                {"field": "declarations"},
            )
        assigned = {
            (item.terms.component_key or declarations[0].component_key) for item in included
        }
        unknown = assigned - set(keys)
        if unknown:
            raise PydanticCustomError(
                "draft_topology_invalid",
                "Every included instrument must name a declared component.",
                {"field": "instruments"},
            )
        missing = set(keys) - assigned
        if missing:
            raise PydanticCustomError(
                "draft_topology_invalid",
                "Every declaration must own at least one included instrument.",
                {"field": "declarations"},
            )
        return self


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


class CurveViewPointV1(CurveLabWireModel):
    model_config = ConfigDict(extra="forbid", validate_default=True, frozen=True)

    parameter_id: str
    component_key: str
    node_date: date
    side: Literal["LEFT", "RIGHT"] | None
    discount_factor: PositiveFiniteFloat
    zero_rate: FiniteFloat | None
    one_day_forward_rate: FiniteFloat


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
    curve_views: tuple[CurveViewPointV1, ...] = ()
    dependency_manifest: tuple[CurveLabDependencyManifestEntryV2, ...]
    diagnostics: dict[str, object] | None
    native_payload_hash: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")] | None = None
    error: CurveLabErrorDetail | None = None
    created_at: datetime
    deadline_at: datetime
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
    state: Literal["QUEUED", "RUNNING", "SUCCEEDED", "FAILED", "TIMED_OUT"]
    phase: str
    resulting_version_id: str | None = None
    error: CurveLabErrorDetail | None = None
    created_at: datetime
    deadline_at: datetime
    finished_at: datetime | None = None


class CurveRuntimeComponentV1(CurveLabWireModel):
    component_key: Annotated[str, Field(min_length=1, max_length=512)]
    role: Literal["DISCOUNT", "PROJECTION", "BASIS"]
    currency: Annotated[str, Field(pattern=r"^[A-Z]{3}$")]
    parameterization: CurveParameterizationV2
    parameter_ids: tuple[str, ...]


class CurveRuntimeManifestV1(CurveLabWireModel):
    schema_version: Literal[1]
    mode: CurveLabModeV2
    as_of_date: date
    market_snapshot_id: Annotated[str, Field(min_length=1, max_length=256)]
    components: tuple[CurveRuntimeComponentV1, ...]

    @model_validator(mode="after")
    def _validate_components(self) -> CurveRuntimeManifestV1:
        if not self.components:
            raise ValueError("runtime manifest components must be non-empty")
        keys = [item.component_key for item in self.components]
        if len(set(keys)) != len(keys):
            raise ValueError("runtime manifest component keys must be unique")
        if self.mode == "SINGLE" and len(keys) != 1:
            raise ValueError("SINGLE runtime manifests require exactly one component")
        return self


RiskMeasureV2 = Literal["PV", "DV01", "KEY_RATE_DV01"]
SensitivityLayerV2 = Literal[
    "TRADE_TO_NODE",
    "CALIBRATION_JACOBIAN",
    "COMPOSED_QUOTE_DIAGNOSTIC",
]
RateTradeDayBasisV2 = Annotated[
    Literal["ACT_365F", "ACT_360", "30_360"],
    WithJsonSchema(
        {
            "type": "string",
            "enum": ["ACT_365F", "ACT_360", "30_360"],
        }
    ),
]


class RateTradeTermsInputV2(CurveLabWireModel):
    notional: CanonicalQuoteDecimalV1 | None = None
    contract_rate: CanonicalQuoteDecimalV1 | None = None
    contract_spread: CanonicalQuoteDecimalV1 | None = None
    contract_count: CanonicalQuoteDecimalV1 | None = None
    position_count: CanonicalQuoteDecimalV1 | None = None
    reference_price: CanonicalQuoteDecimalV1 | None = None
    contract_value_per_price_point: CanonicalQuoteDecimalV1 | None = None
    convexity_adjustment: CanonicalQuoteDecimalV1 | None = None
    domestic_notional: CanonicalQuoteDecimalV1 | None = None
    foreign_notional: CanonicalQuoteDecimalV1 | None = None
    fx_spot: CanonicalQuoteDecimalV1 | None = None
    side: (
        Literal[
            "LEND",
            "BORROW",
            "RECEIVE_FLOATING",
            "PAY_FLOATING",
            "LONG",
            "SHORT",
            "PAY_FIXED",
            "RECEIVE_FIXED",
            "RECEIVE_REFERENCE_PAY_SPREAD",
            "PAY_REFERENCE_RECEIVE_SPREAD",
            "RECEIVE_NON_SPREAD_PAY_SPREAD",
            "PAY_NON_SPREAD_RECEIVE_SPREAD",
        ]
        | None
    ) = None
    settlement_style: Literal["AT_START_DISCOUNTED", "AT_END"] | None = None
    spread_leg: Literal["DOMESTIC", "FOREIGN"] | None = None
    initial_notional_exchange: bool | None = None
    final_notional_exchange: bool | None = None
    discount_component_key: str | None = None
    forecast_component_key: str | None = None
    spread_forecast_component_key: str | None = None
    reference_forecast_component_key: str | None = None
    forecast_tenor: Annotated[str, Field(pattern=r"^[1-9][0-9]*[DWMY]$")] | None = None
    day_basis: RateTradeDayBasisV2 | None = None
    collateral: Annotated[str, Field(min_length=1, max_length=32)] | None = None
    use_projection_curve: bool | None = None
    index_name: Annotated[str, Field(min_length=1, max_length=256)] | None = None
    fixing_hour: Annotated[int, Field(ge=0, le=23)] | None = None
    fixing_minute: Annotated[int, Field(ge=0, le=59)] | None = None
    fixed_payment_frequency: str | None = None
    fixed_day_basis: str | None = None
    float_payment_frequency: str | None = None
    float_day_basis: str | None = None
    spread_payment_frequency: str | None = None
    spread_day_basis: str | None = None
    reference_payment_frequency: str | None = None
    reference_day_basis: str | None = None
    domestic_payment_frequency: str | None = None
    domestic_day_basis: str | None = None
    foreign_payment_frequency: str | None = None
    foreign_day_basis: str | None = None
    float_forecast_tenor: str | None = None
    float_collateral: str | None = None
    float_use_projection_curve: bool | None = None
    float_index_name: str | None = None
    float_fixing_hour: Annotated[int, Field(ge=0, le=23)] | None = None
    float_fixing_minute: Annotated[int, Field(ge=0, le=59)] | None = None
    spread_forecast_tenor: str | None = None
    spread_collateral: str | None = None
    spread_use_projection_curve: bool | None = None
    spread_index_name: str | None = None
    spread_fixing_hour: Annotated[int, Field(ge=0, le=23)] | None = None
    spread_fixing_minute: Annotated[int, Field(ge=0, le=59)] | None = None
    reference_forecast_tenor: str | None = None
    reference_collateral: str | None = None
    reference_use_projection_curve: bool | None = None
    reference_index_name: str | None = None
    reference_fixing_hour: Annotated[int, Field(ge=0, le=23)] | None = None
    reference_fixing_minute: Annotated[int, Field(ge=0, le=59)] | None = None
    domestic_forecast_tenor: str | None = None
    domestic_collateral: str | None = None
    domestic_use_projection_curve: bool | None = None
    domestic_index_name: str | None = None
    domestic_fixing_hour: Annotated[int, Field(ge=0, le=23)] | None = None
    domestic_fixing_minute: Annotated[int, Field(ge=0, le=59)] | None = None
    foreign_forecast_tenor: str | None = None
    foreign_collateral: str | None = None
    foreign_use_projection_curve: bool | None = None
    foreign_index_name: str | None = None
    foreign_fixing_hour: Annotated[int, Field(ge=0, le=23)] | None = None
    foreign_fixing_minute: Annotated[int, Field(ge=0, le=59)] | None = None


class RateTradeDefinitionInputV2(CurveLabWireModel):
    trade_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    instrument_type: CurveLabV1SuccessFamily
    trade_date: date
    start_date: date
    maturity_date: date
    currency_or_pair: Annotated[str, Field(min_length=3, max_length=32)]
    terms: RateTradeTermsInputV2

    @model_validator(mode="after")
    def _validate_trade(self) -> RateTradeDefinitionInputV2:
        if not (self.trade_date <= self.start_date < self.maturity_date):
            raise ValueError("trade dates must satisfy trade_date <= start_date < maturity_date")
        required = {
            "DEPOSIT": {"notional", "contract_rate", "side"},
            "FRA": {"notional", "contract_rate", "side"},
            "FUTURE": {
                "contract_count",
                "side",
                "reference_price",
                "contract_value_per_price_point",
            },
            "OIS": {"notional", "contract_rate", "side"},
            "IRS": {"notional", "contract_rate", "side"},
            "BASIS_SWAP": {"notional", "contract_spread", "side"},
            "XCCY": {
                "position_count",
                "contract_spread",
                "side",
                "domestic_notional",
                "foreign_notional",
                "fx_spot",
            },
        }[self.instrument_type]
        missing = sorted(required - self.terms.model_fields_set)
        if missing:
            raise ValueError(f"{self.instrument_type} trade terms require {', '.join(missing)}")
        valid_sides = {
            "DEPOSIT": {"LEND", "BORROW"},
            "FRA": {"RECEIVE_FLOATING", "PAY_FLOATING"},
            "FUTURE": {"LONG", "SHORT"},
            "OIS": {"PAY_FIXED", "RECEIVE_FIXED"},
            "IRS": {"PAY_FIXED", "RECEIVE_FIXED"},
            "BASIS_SWAP": {
                "RECEIVE_REFERENCE_PAY_SPREAD",
                "PAY_REFERENCE_RECEIVE_SPREAD",
            },
            "XCCY": {
                "RECEIVE_NON_SPREAD_PAY_SPREAD",
                "PAY_NON_SPREAD_RECEIVE_SPREAD",
            },
        }[self.instrument_type]
        if self.terms.side not in valid_sides:
            raise ValueError(
                f"{self.instrument_type} trade side must be one of {', '.join(sorted(valid_sides))}"
            )
        index_fields = {
            "forecast_tenor",
            "day_basis",
            "collateral",
            "use_projection_curve",
            "index_name",
            "fixing_hour",
            "fixing_minute",
            "discount_component_key",
            "forecast_component_key",
        }
        fixed_float_fields = {
            f"{prefix}_{suffix}"
            for prefix in ("fixed", "float")
            for suffix in (
                "payment_frequency",
                "day_basis",
                "forecast_tenor",
                "collateral",
                "use_projection_curve",
                "index_name",
                "fixing_hour",
                "fixing_minute",
            )
        }
        basis_fields = {
            f"{prefix}_{suffix}"
            for prefix in ("spread", "reference")
            for suffix in (
                "payment_frequency",
                "day_basis",
                "forecast_tenor",
                "collateral",
                "use_projection_curve",
                "index_name",
                "fixing_hour",
                "fixing_minute",
                "forecast_component_key",
            )
        }
        xccy_fields = {
            f"{prefix}_{suffix}"
            for prefix in ("domestic", "foreign")
            for suffix in (
                "payment_frequency",
                "day_basis",
                "forecast_tenor",
                "collateral",
                "use_projection_curve",
                "index_name",
                "fixing_hour",
                "fixing_minute",
            )
        }
        allowed = {
            "DEPOSIT": {"notional", "contract_rate", "side"} | index_fields,
            "FRA": {
                "notional",
                "contract_rate",
                "side",
                "settlement_style",
            }
            | index_fields,
            "FUTURE": {
                "contract_count",
                "side",
                "reference_price",
                "contract_value_per_price_point",
                "convexity_adjustment",
            }
            | index_fields,
            "OIS": {
                "notional",
                "contract_rate",
                "side",
                "discount_component_key",
                "forecast_component_key",
            }
            | fixed_float_fields,
            "IRS": {
                "notional",
                "contract_rate",
                "side",
                "discount_component_key",
                "forecast_component_key",
            }
            | fixed_float_fields,
            "BASIS_SWAP": {
                "notional",
                "contract_spread",
                "side",
                "discount_component_key",
                "forecast_component_key",
                "spread_forecast_component_key",
                "reference_forecast_component_key",
            }
            | basis_fields,
            "XCCY": {
                "position_count",
                "contract_spread",
                "side",
                "spread_leg",
                "domestic_notional",
                "foreign_notional",
                "fx_spot",
                "initial_notional_exchange",
                "final_notional_exchange",
            }
            | xccy_fields,
        }[self.instrument_type]
        irrelevant = sorted(self.terms.model_fields_set - allowed)
        if irrelevant:
            raise ValueError(
                f"{self.instrument_type} trade terms do not allow {', '.join(irrelevant)}"
            )
        positive = {
            "notional",
            "contract_count",
            "position_count",
            "contract_value_per_price_point",
            "domestic_notional",
            "foreign_notional",
            "fx_spot",
        }
        for field in positive & self.terms.model_fields_set:
            value = getattr(self.terms, field)
            if value is not None and float(value) <= 0:
                raise ValueError(f"{field} must be positive")
        return self


class RiskTargetV2(CurveLabWireModel):
    trades: tuple[RateTradeDefinitionInputV2, ...]


class RiskRunOptionsV2(CurveLabWireModel):
    aad_fallback: Literal["ALLOW", "FORBID"] = "ALLOW"
    jacobian_replay_fallback: Literal["ALLOW", "FORBID"] = "ALLOW"


class FixingObservationInputV1(CurveLabWireModel):
    index_name: Annotated[str, Field(min_length=1, max_length=256)]
    fixing_time: datetime
    kind: Literal["RATE", "FX"]
    units: Literal["DECIMAL_RATE", "DOMESTIC_PER_FOREIGN"]
    value: CanonicalQuoteDecimalV1

    @model_validator(mode="after")
    def _validate_value(self) -> FixingObservationInputV1:
        numeric = float(self.value)
        expected_units = {
            "RATE": "DECIMAL_RATE",
            "FX": "DOMESTIC_PER_FOREIGN",
        }[self.kind]
        if self.units != expected_units:
            raise ValueError(f"{self.kind} fixing values require {expected_units} units")
        if self.kind == "FX" and numeric <= 0.0:
            raise ValueError("FX fixing values must be positive")
        return self


class FixingSnapshotCreateV1(CurveLabWireModel):
    id: Annotated[str, Field(min_length=1, max_length=256)]
    observations: tuple[FixingObservationInputV1, ...]

    @model_validator(mode="after")
    def _validate_unique_keys(self) -> FixingSnapshotCreateV1:
        keys = [(item.index_name, item.fixing_time) for item in self.observations]
        if len(set(keys)) != len(keys):
            raise ValueError("fixing observations must have unique keys")
        return self


class FixingSnapshotResponseV1(FixingSnapshotCreateV1):
    content_hash: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")]
    created_at: datetime


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
    fixing_snapshot_hash: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")]
    target_fingerprint: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")]
    quote_axis: tuple[QuoteAxisEntryV2, ...] | None
    parameter_axis: tuple[ParameterAxisEntryV2, ...]
    estimated_work: RiskWorkEstimateV2
    state: Literal["QUEUED", "RUNNING", "SUCCEEDED", "FAILED", "TIMED_OUT"]
    result: RiskRunResultV2 | None
    error: CurveLabErrorDetail | None
    created_at: datetime
    deadline_at: datetime
    finished_at: datetime | None


class AadParityEvidenceV1(CurveLabWireModel):
    trade_id: Annotated[str, Field(pattern=r"^[0-9a-f]{32}$")]
    status: Literal["PASSED", "FAILED", "UNAVAILABLE"]
    absolute_tolerance: CanonicalQuoteDecimalV1
    relative_tolerance: CanonicalQuoteDecimalV1
    aad_values: tuple[CanonicalQuoteDecimalV1, ...] | None
    central_values: tuple[CanonicalQuoteDecimalV1, ...] | None
    max_abs_discrepancy: CanonicalQuoteDecimalV1 | None


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
    trade_methods: tuple[str, ...] | None = None
    aad_parity: tuple[AadParityEvidenceV1, ...] | None = None
    curve_version_id: str | None = None
    curve_version_hash: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")] | None = None
    fixing_snapshot_id: str | None = None
    fixing_snapshot_hash: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")] | None = None
    row_axis_hash: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")] | None = None
    column_axis_hash: Annotated[str, Field(pattern=r"^[0-9a-f]{64}$")] | None = None
    evaluation_time: datetime | None = None
    base_currency: str | None = None
    bump_target: str | None = None
    bump_size: str | None = None
    input_unit: str
    output_unit: str
    values: tuple[tuple[str, ...], ...] | None = None
    failure: CurveLabErrorDetail | None = None
