"""Exact base-10 quote normalization at the Curve Lab authoring boundary."""

from __future__ import annotations

import math
import re
from dataclasses import dataclass
from decimal import ROUND_HALF_EVEN, Decimal, localcontext

from pydantic import ValidationError

from app.schemas.curve_lab import (
    CURVE_LAB_V1_SUCCESS_REGISTRY,
    CurveLabQuoteCanonicalizationResponse,
    CurveLabSuccessRegistryEntry,
)

MAX_QUOTE_BYTES = 512
_DECIMAL_PATTERN = re.compile(r"^-?[0-9]+(?:\.[0-9]+)?$", re.ASCII)
_KNOWN_CONVENTIONS = frozenset(("DECIMAL", "PERCENT", "PRICE_POINTS"))
_REGISTRY = {row.instrument_type: row for row in CURVE_LAB_V1_SUCCESS_REGISTRY}


class QuoteCanonicalizationError(ValueError):
    """Stable, field-addressed Curve Lab boundary failure."""

    def __init__(
        self,
        code: str,
        message: str,
        field: str,
        value: object,
        **details: object,
    ) -> None:
        super().__init__(message)
        self.code = code
        self.message = message
        self.field = field
        self.value = _safe_value(value)
        self.details = details

    def as_detail(self) -> dict[str, object]:
        return {
            "code": self.code,
            "message": self.message,
            "field": self.field,
            "value": self.value,
            "resource_id": None,
            "details": self.details,
        }


@dataclass(frozen=True, slots=True)
class _ExactDecimal:
    coefficient: int
    scale: int

    @classmethod
    def parse(cls, lexeme: str, field: str = "raw_quote") -> _ExactDecimal:
        if not isinstance(lexeme, str):
            raise _error(
                "QUOTE_DECIMAL_INVALID",
                "Quote input must be an ASCII plain-decimal string.",
                field,
                lexeme,
                constraint="ascii_plain_decimal",
            )
        try:
            encoded = lexeme.encode("ascii")
        except UnicodeEncodeError as exc:
            raise _error(
                "QUOTE_DECIMAL_INVALID",
                "Quote input must contain ASCII decimal characters only.",
                field,
                lexeme,
                constraint="ascii_plain_decimal",
            ) from exc
        if len(encoded) > MAX_QUOTE_BYTES:
            raise _error(
                "QUOTE_DECIMAL_RANGE",
                "Quote input exceeds the 512-byte limit.",
                field,
                lexeme,
                input_length=len(encoded),
                limit=MAX_QUOTE_BYTES,
            )
        if not _DECIMAL_PATTERN.fullmatch(lexeme):
            raise _error(
                "QUOTE_DECIMAL_INVALID",
                "Quote input must use plain base-10 notation.",
                field,
                lexeme,
                constraint=r"^-?[0-9]+(\.[0-9]+)?$",
            )
        negative = lexeme.startswith("-")
        unsigned = lexeme[1:] if negative else lexeme
        integer, separator, fraction = unsigned.partition(".")
        digits = integer + fraction
        coefficient = int(digits)
        if negative:
            coefficient = -coefficient
        return cls(coefficient, len(fraction) if separator else 0)

    def divided_by_100(self) -> _ExactDecimal:
        return _ExactDecimal(self.coefficient, self.scale + 2)

    def one_minus_divided_by_100(self) -> _ExactDecimal:
        target_scale = self.scale + 2
        return _ExactDecimal(
            10**target_scale - self.coefficient,
            target_scale,
        )

    def plus(self, other: _ExactDecimal) -> _ExactDecimal:
        scale = max(self.scale, other.scale)
        left = self.coefficient * 10 ** (scale - self.scale)
        right = other.coefficient * 10 ** (scale - other.scale)
        return _ExactDecimal(left + right, scale)

    def canonical(self, field: str = "raw_quote") -> str:
        coefficient = self.coefficient
        if coefficient == 0:
            return "0"
        negative = coefficient < 0
        digits = str(abs(coefficient))
        if self.scale == 0:
            result = digits
        elif len(digits) <= self.scale:
            result = "0." + "0" * (self.scale - len(digits)) + digits
        else:
            result = digits[: -self.scale] + "." + digits[-self.scale :]
        if "." in result:
            result = result.rstrip("0").rstrip(".")
        result = result.lstrip("0") if not result.startswith("0.") else result
        if result.startswith("."):
            result = "0" + result
        if not result:
            result = "0"
        if negative:
            result = "-" + result
        if len(result.encode("ascii")) > MAX_QUOTE_BYTES:
            raise _error(
                "QUOTE_DECIMAL_RANGE",
                "Canonical quote exceeds the 512-byte limit.",
                field,
                result,
                output_length=len(result),
                limit=MAX_QUOTE_BYTES,
            )
        return result


def _safe_value(value: object) -> str | int | None:
    if value is None or isinstance(value, int):
        return value
    rendered = str(value)
    return rendered[:64] + ("…" if len(rendered) > 64 else "")


def _error(
    code: str,
    message: str,
    field: str,
    value: object,
    **details: object,
) -> QuoteCanonicalizationError:
    return QuoteCanonicalizationError(code, message, field, value, **details)


def _registry_row(instrument_type: str) -> CurveLabSuccessRegistryEntry:
    try:
        return _REGISTRY[instrument_type]  # type: ignore[index]
    except (KeyError, TypeError) as exc:
        raise _error(
            "UNSUPPORTED_PRODUCT",
            "Instrument family is outside the Curve Lab V1 success registry.",
            "instrument_type",
            instrument_type,
            supported=list(_REGISTRY),
        ) from exc


def _validate_convention(
    row: CurveLabSuccessRegistryEntry,
    convention: str,
    *,
    display: bool = False,
) -> None:
    field = "display_convention" if display else "input_convention"
    if convention not in _KNOWN_CONVENTIONS:
        code = "QUOTE_CONVENTION_UNKNOWN"
        raise _error(
            code,
            "Quote convention is outside the closed convention enum.",
            field,
            convention,
            coordinate=row.quote_coordinate_kind,
        )
    permitted = (
        frozenset(("PRICE_POINTS",))
        if row.quote_coordinate_kind == "PRICE"
        else frozenset(("DECIMAL", "PERCENT"))
    )
    if convention not in permitted:
        code = "QUOTE_DISPLAY_CONVENTION_MISMATCH" if display else "QUOTE_INPUT_CONVENTION_MISMATCH"
        raise _error(
            code,
            "Quote convention is incompatible with the family coordinate.",
            field,
            convention,
            coordinate=row.quote_coordinate_kind,
            permitted=sorted(permitted),
        )


def _native_value(value: _ExactDecimal, field: str) -> float:
    canonical = value.canonical(field)
    converted = float(canonical)
    if not math.isfinite(converted) or (value.coefficient != 0 and converted == 0.0):
        raise _error(
            "QUOTE_NATIVE_RANGE",
            "Quote cannot be represented as a finite nonzero binary64 value.",
            field,
            canonical,
            constraint="finite_binary64_without_nonzero_underflow",
        )
    return converted


def _validate_risk_bump(
    base: _ExactDecimal,
    bump_lexeme: str,
    field: str,
) -> None:
    bump = _ExactDecimal.parse(bump_lexeme, field)
    base_native = _native_value(base, field)
    bumped_native = _native_value(base.plus(bump), field)
    if base_native == bumped_native:
        raise _error(
            "RISK_BUMP_NOT_REPRESENTABLE",
            "The fixed risk bump is erased by binary64 conversion.",
            field,
            base.canonical(field),
            bump=bump_lexeme,
        )


def canonicalize_quote(
    instrument_type: str,
    input_lexeme: str,
    input_convention: str,
) -> CurveLabQuoteCanonicalizationResponse:
    """Canonicalize one authoring lexeme without persistence or native dispatch."""

    row = _registry_row(instrument_type)
    _validate_convention(row, input_convention)
    entered = _ExactDecimal.parse(input_lexeme)
    raw = entered.divided_by_100() if input_convention == "PERCENT" else entered
    normalized = raw.one_minus_divided_by_100() if row.quote_coordinate_kind == "PRICE" else raw
    raw_quote = raw.canonical()
    normalized_quote = normalized.canonical("normalized_quote")
    _native_value(raw, "raw_quote")
    _native_value(normalized, "normalized_quote")
    _validate_risk_bump(raw, row.exact_risk_raw_bump, "raw_quote")
    _validate_risk_bump(
        normalized,
        row.normalized_risk_bump,
        "normalized_quote",
    )
    return CurveLabQuoteCanonicalizationResponse(
        instrument_type=row.instrument_type,
        quote_coordinate_kind=row.quote_coordinate_kind,
        canonical_raw_unit=row.canonical_raw_unit,
        raw_quote=raw_quote,
        normalized_quote=normalized_quote,
        exact_risk_raw_bump=row.exact_risk_raw_bump,
        normalized_risk_bump=row.normalized_risk_bump,
    )


def apply_exact_decimal_bump(canonical_value: str, canonical_bump: str) -> str:
    """Apply a canonical decimal bump and prove the native move is distinct."""

    base = _ExactDecimal.parse(canonical_value)
    if base.canonical() != canonical_value:
        raise _error(
            "QUOTE_PERSISTED_BYTES_NOT_CANONICAL",
            "Stored quote bytes are not canonical.",
            "raw_quote",
            canonical_value,
            expected=base.canonical(),
        )
    bump = _ExactDecimal.parse(canonical_bump, "risk_bump")
    moved = base.plus(bump)
    _validate_risk_bump(base, bump.canonical("risk_bump"), "raw_quote")
    return moved.canonical()


def replay_canonical_quote(
    payload: bytes,
) -> CurveLabQuoteCanonicalizationResponse:
    """Reconstruct and revalidate persisted quote evidence after a restart."""

    try:
        stored = CurveLabQuoteCanonicalizationResponse.model_validate_json(payload)
    except (ValidationError, ValueError) as exc:
        raise _error(
            "QUOTE_PERSISTED_BYTES_NOT_CANONICAL",
            "Persisted quote evidence does not satisfy the closed schema.",
            "payload",
            payload[:64],
            constraint="CurveLabQuoteCanonicalizationResponse",
        ) from exc
    expected = canonicalize_quote(
        stored.instrument_type,
        stored.raw_quote,
        stored.canonical_raw_unit,
    )
    if stored.financial_bytes() != expected.financial_bytes():
        raise _error(
            "QUOTE_PERSISTED_BYTES_NOT_CANONICAL",
            "Persisted quote evidence differs from registry-derived canonical bytes.",
            "raw_quote",
            stored.raw_quote,
            expected=expected.raw_quote,
        )
    return expected


def render_quote(
    instrument_type: str,
    canonical_raw_quote: str,
    display_convention: str,
    display_scale: int,
) -> str:
    """Project canonical bytes into a rounded display string only."""

    row = _registry_row(instrument_type)
    _validate_convention(row, display_convention, display=True)
    if (
        isinstance(display_scale, bool)
        or not isinstance(display_scale, int)
        or not 0 <= display_scale <= 12
    ):
        raise _error(
            "QUOTE_DISPLAY_SCALE_INVALID",
            "Display scale must be an integer from 0 through 12.",
            "display_scale",
            display_scale,
            minimum=0,
            maximum=12,
        )
    exact = _ExactDecimal.parse(canonical_raw_quote)
    canonical = exact.canonical()
    if canonical != canonical_raw_quote:
        raise _error(
            "QUOTE_PERSISTED_BYTES_NOT_CANONICAL",
            "Stored quote bytes are not canonical.",
            "raw_quote",
            canonical_raw_quote,
            expected=canonical,
        )
    with localcontext() as context:
        context.prec = max(600, len(canonical_raw_quote) + display_scale + 32)
        value = Decimal(canonical)
        if display_convention == "PERCENT":
            value *= Decimal(100)
        quantum = Decimal(1).scaleb(-display_scale)
        rounded = value.quantize(quantum, rounding=ROUND_HALF_EVEN)
    if rounded.is_zero():
        rounded = abs(rounded)
    return format(rounded, f".{display_scale}f")
