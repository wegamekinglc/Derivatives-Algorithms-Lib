"""Exact Curve Lab quote-boundary contract."""

from __future__ import annotations

import pytest

from app.schemas.curve_lab import (
    CURVE_LAB_V1_SUCCESS_FAMILIES,
    CURVE_LAB_V1_SUCCESS_REGISTRY,
)
from app.services.quote_canonicalization import (
    QuoteCanonicalizationError,
    canonicalize_quote,
    render_quote,
)

EXPECTED_FAMILIES = (
    "DEPOSIT",
    "FRA",
    "FUTURE",
    "OIS",
    "IRS",
    "BASIS_SWAP",
    "XCCY",
)


def test_success_registry_is_exact_ordered_and_owns_every_axis_value() -> None:
    assert CURVE_LAB_V1_SUCCESS_FAMILIES == EXPECTED_FAMILIES
    assert tuple(row.instrument_type for row in CURVE_LAB_V1_SUCCESS_REGISTRY) == (
        *EXPECTED_FAMILIES,
    )
    assert tuple(row.quote_coordinate_kind for row in CURVE_LAB_V1_SUCCESS_REGISTRY) == (
        "RATE",
        "RATE",
        "PRICE",
        "RATE",
        "RATE",
        "SPREAD",
        "SPREAD",
    )
    assert tuple(row.canonical_raw_unit for row in CURVE_LAB_V1_SUCCESS_REGISTRY) == (
        "DECIMAL",
        "DECIMAL",
        "PRICE_POINTS",
        "DECIMAL",
        "DECIMAL",
        "DECIMAL",
        "DECIMAL",
    )


@pytest.mark.parametrize(
    ("family", "lexeme", "convention", "raw", "normalized", "raw_bump"),
    [
        ("DEPOSIT", "4", "PERCENT", "0.04", "0.04", "0.0001"),
        ("FRA", "0.0400", "DECIMAL", "0.04", "0.04", "0.0001"),
        ("OIS", "-0.00", "PERCENT", "0", "0", "0.0001"),
        ("IRS", "-0004.000", "PERCENT", "-0.04", "-0.04", "0.0001"),
        ("BASIS_SWAP", "000.00100", "DECIMAL", "0.001", "0.001", "0.0001"),
        ("XCCY", "0", "DECIMAL", "0", "0", "0.0001"),
        ("FUTURE", "95.8225", "PRICE_POINTS", "95.8225", "0.041775", "-0.01"),
    ],
)
def test_quote_transform_is_exact_and_registry_derived(
    family: str,
    lexeme: str,
    convention: str,
    raw: str,
    normalized: str,
    raw_bump: str,
) -> None:
    result = canonicalize_quote(family, lexeme, convention)

    assert result.raw_quote == raw
    assert result.normalized_quote == normalized
    assert result.exact_risk_raw_bump == raw_bump
    assert result.normalized_risk_bump == "0.0001"
    assert result.canonical_raw_unit == ("PRICE_POINTS" if family == "FUTURE" else "DECIMAL")


@pytest.mark.parametrize(
    "family",
    ["DEPOSIT", "FRA", "OIS", "IRS", "BASIS_SWAP", "XCCY"],
)
@pytest.mark.parametrize(("percent", "decimal"), [("4", "0.04"), ("0", "0"), ("-4", "-0.04")])
def test_percent_and_decimal_inputs_have_identical_financial_bytes(
    family: str, percent: str, decimal: str
) -> None:
    from_percent = canonicalize_quote(family, percent, "PERCENT")
    from_decimal = canonicalize_quote(family, decimal, "DECIMAL")

    assert from_percent.financial_bytes() == from_decimal.financial_bytes()


@pytest.mark.parametrize(
    ("lexeme", "code"),
    [
        ("", "QUOTE_DECIMAL_INVALID"),
        (" 4", "QUOTE_DECIMAL_INVALID"),
        ("4 ", "QUOTE_DECIMAL_INVALID"),
        ("+4", "QUOTE_DECIMAL_INVALID"),
        ("4e-2", "QUOTE_DECIMAL_INVALID"),
        ("4,0", "QUOTE_DECIMAL_INVALID"),
        ("NaN", "QUOTE_DECIMAL_INVALID"),
        ("Infinity", "QUOTE_DECIMAL_INVALID"),
        ("é", "QUOTE_DECIMAL_INVALID"),
        ("1" * 513, "QUOTE_DECIMAL_RANGE"),
        ("9" * 400, "QUOTE_NATIVE_RANGE"),
        ("0." + "0" * 400 + "1", "QUOTE_NATIVE_RANGE"),
        ("10000000000000000", "RISK_BUMP_NOT_REPRESENTABLE"),
    ],
)
def test_decimal_failures_have_stable_codes(lexeme: str, code: str) -> None:
    with pytest.raises(QuoteCanonicalizationError) as raised:
        canonicalize_quote("DEPOSIT", lexeme, "DECIMAL")

    assert raised.value.code == code
    assert raised.value.field == "raw_quote"


def test_exact_512_byte_boundary_is_accepted_before_native_range_check() -> None:
    lexeme = "0." + "0" * 508 + "1"
    assert len(lexeme) == 511

    with pytest.raises(QuoteCanonicalizationError) as raised:
        canonicalize_quote("DEPOSIT", lexeme + "0", "DECIMAL")

    assert raised.value.code == "QUOTE_NATIVE_RANGE"


@pytest.mark.parametrize(
    ("family", "convention", "code"),
    [
        ("FUTURE", "DECIMAL", "QUOTE_INPUT_CONVENTION_MISMATCH"),
        ("FUTURE", "PERCENT", "QUOTE_INPUT_CONVENTION_MISMATCH"),
        ("DEPOSIT", "PRICE_POINTS", "QUOTE_INPUT_CONVENTION_MISMATCH"),
        ("DEPOSIT", "BASIS_POINTS", "QUOTE_CONVENTION_UNKNOWN"),
    ],
)
def test_convention_errors_precede_decimal_parsing(family: str, convention: str, code: str) -> None:
    with pytest.raises(QuoteCanonicalizationError) as raised:
        canonicalize_quote(family, "not-a-decimal", convention)

    assert raised.value.code == code
    assert raised.value.field == "input_convention"


@pytest.mark.parametrize(
    ("raw", "convention", "scale", "expected"),
    [
        ("0.04", "PERCENT", 1, "4.0"),
        ("0.0405", "PERCENT", 0, "4"),
        ("0.0415", "PERCENT", 0, "4"),
        ("-0.0405", "PERCENT", 0, "-4"),
        ("-0.0415", "PERCENT", 0, "-4"),
        ("-0", "DECIMAL", 12, "0.000000000000"),
        ("95.8225", "PRICE_POINTS", 12, "95.822500000000"),
    ],
)
def test_display_inverse_rounds_half_even_without_changing_financial_bytes(
    raw: str, convention: str, scale: int, expected: str
) -> None:
    family = "FUTURE" if convention == "PRICE_POINTS" else "IRS"
    before = canonicalize_quote(
        family,
        raw,
        "PRICE_POINTS" if family == "FUTURE" else "DECIMAL",
    )

    assert render_quote(family, before.raw_quote, convention, scale) == expected
    assert (
        before.financial_bytes()
        == canonicalize_quote(
            family,
            before.raw_quote,
            "PRICE_POINTS" if family == "FUTURE" else "DECIMAL",
        ).financial_bytes()
    )


@pytest.mark.parametrize(
    ("scale", "code"), [(-1, "QUOTE_DISPLAY_SCALE_INVALID"), (13, "QUOTE_DISPLAY_SCALE_INVALID")]
)
def test_display_scale_is_closed(scale: int, code: str) -> None:
    with pytest.raises(QuoteCanonicalizationError) as raised:
        render_quote("IRS", "0.04", "PERCENT", scale)

    assert raised.value.code == code
