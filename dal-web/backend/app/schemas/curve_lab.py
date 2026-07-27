"""Closed Curve Lab V2 wire contracts and their normative quote registry."""

from __future__ import annotations

import json
from dataclasses import dataclass
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
