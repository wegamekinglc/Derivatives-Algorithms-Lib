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


CurveLabModeV2 = Literal["SINGLE", "MULTI_CURVE", "XCCY"]
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
