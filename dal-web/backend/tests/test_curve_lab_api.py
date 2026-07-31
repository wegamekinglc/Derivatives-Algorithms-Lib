"""HTTP contract for the stateless Curve Lab quote authoring adapter."""

from __future__ import annotations

import inspect
import json
from pathlib import Path

import pytest


def test_curve_lab_endpoints_are_async() -> None:
    from app.routers.curve_lab import router

    assert router.routes
    assert all(inspect.iscoroutinefunction(route.endpoint) for route in router.routes)


def test_curve_lab_styles_follow_the_industrial_terminal_contract() -> None:
    stylesheet = (Path(__file__).parents[2] / "frontend" / "src" / "styles.css").read_text(
        encoding="utf-8"
    )
    shell_rule = stylesheet.split(".curve-lab-v2 {", 1)[1].split("}", 1)[0]
    active_tab_rule = stylesheet.split("button.curve-lab-flow-tab.active {", 1)[1].split("}", 1)[0]

    assert "gradient" not in shell_rule
    assert "shadow" not in active_tab_rule


def test_capabilities_publish_the_exact_registry(client) -> None:
    response = client.get("/api/curve-lab/capabilities")

    assert response.status_code == 200
    body = response.json()
    assert body["success_families"] == [
        "DEPOSIT",
        "FRA",
        "FUTURE",
        "OIS",
        "IRS",
        "BASIS_SWAP",
        "XCCY",
    ]
    assert [row["quote_coordinate_kind"] for row in body["registry"]] == [
        "RATE",
        "RATE",
        "PRICE",
        "RATE",
        "RATE",
        "SPREAD",
        "SPREAD",
    ]
    assert body["max_quote_bytes"] == 512


@pytest.mark.parametrize("family", ["DEPOSIT", "FRA", "OIS", "IRS", "BASIS_SWAP", "XCCY"])
def test_percent_and_decimal_authoring_return_identical_durable_json(client, family: str) -> None:
    percent = client.post(
        "/api/curve-lab/quote-canonicalizations",
        json={
            "instrument_type": family,
            "input_lexeme": "4",
            "input_convention": "PERCENT",
        },
    )
    decimal = client.post(
        "/api/curve-lab/quote-canonicalizations",
        json={
            "instrument_type": family,
            "input_lexeme": "0.04",
            "input_convention": "DECIMAL",
        },
    )

    assert percent.status_code == 200
    assert percent.content == decimal.content
    assert percent.json()["raw_quote"] == "0.04"
    assert percent.json()["exact_risk_raw_bump"] == "0.0001"
    assert percent.json()["normalized_risk_bump"] == "0.0001"
    assert b"PERCENT" not in percent.content


def test_future_authoring_retains_price_and_derives_normalized_rate(client) -> None:
    response = client.post(
        "/api/curve-lab/quote-canonicalizations",
        json={
            "instrument_type": "FUTURE",
            "input_lexeme": "95.8225",
            "input_convention": "PRICE_POINTS",
        },
    )

    assert response.status_code == 200
    assert response.json() == {
        "instrument_type": "FUTURE",
        "quote_coordinate_kind": "PRICE",
        "canonical_raw_unit": "PRICE_POINTS",
        "raw_quote": "95.8225",
        "normalized_quote": "0.041775",
        "normalized_unit": "DECIMAL_RATE",
        "exact_risk_raw_bump": "-0.01",
        "normalized_risk_bump": "0.0001",
    }


@pytest.mark.parametrize(
    ("family", "raw_quote", "convention", "scale", "expected"),
    [
        ("IRS", "0.04", "PERCENT", 0, "4"),
        ("IRS", "0.04", "PERCENT", 1, "4.0"),
        ("IRS", "0.04", "PERCENT", 6, "4.000000"),
        ("IRS", "0.04", "PERCENT", 12, "4.000000000000"),
        ("IRS", "0.0405", "PERCENT", 1, "4.0"),
        ("IRS", "0.0415", "PERCENT", 1, "4.2"),
        ("IRS", "-0.0405", "PERCENT", 1, "-4.0"),
        ("IRS", "-0.0415", "PERCENT", 1, "-4.2"),
        ("IRS", "0", "DECIMAL", 12, "0.000000000000"),
        ("FUTURE", "95.8225", "PRICE_POINTS", 12, "95.822500000000"),
    ],
)
def test_quote_rendering_endpoint_returns_exact_presentation_strings(
    client,
    family: str,
    raw_quote: str,
    convention: str,
    scale: int,
    expected: str,
) -> None:
    response = client.post(
        "/api/curve-lab/quote-renderings",
        json={
            "instrument_type": family,
            "canonical_raw_quote": raw_quote,
            "display_convention": convention,
            "display_scale": scale,
        },
    )

    assert response.status_code == 200
    assert response.json() == {"rendered_quote": expected}
    assert isinstance(response.json()["rendered_quote"], str)


@pytest.mark.parametrize(
    ("payload", "code", "field"),
    [
        (
            {
                "instrument_type": "FUTURE",
                "canonical_raw_quote": "95.8225",
                "display_convention": "PERCENT",
                "display_scale": 4,
            },
            "QUOTE_DISPLAY_CONVENTION_MISMATCH",
            "display_convention",
        ),
        (
            {
                "instrument_type": "IRS",
                "canonical_raw_quote": "0.04",
                "display_convention": "PERCENT",
                "display_scale": 13,
            },
            "QUOTE_DISPLAY_SCALE_INVALID",
            "display_scale",
        ),
        (
            {
                "instrument_type": "IRS",
                "canonical_raw_quote": "0.0400",
                "display_convention": "PERCENT",
                "display_scale": 4,
            },
            "QUOTE_PERSISTED_BYTES_NOT_CANONICAL",
            "raw_quote",
        ),
    ],
)
def test_quote_rendering_errors_use_the_curve_lab_envelope(
    client,
    payload: dict[str, object],
    code: str,
    field: str,
) -> None:
    response = client.post("/api/curve-lab/quote-renderings", json=payload)

    assert response.status_code == 422
    assert response.json()["detail"]["code"] == code
    assert response.json()["detail"]["field"] == field


def test_quote_renderer_has_no_store_gateway_queue_or_audit_dependency() -> None:
    from app.routers import curve_lab

    source_names = set(curve_lab.render_authoring_quote.__code__.co_names)
    assert source_names.isdisjoint(
        {"get_store", "get_gateway", "create_task", "add", "commit", "audit"}
    )


def test_quote_rendering_normalizes_signed_zero_at_the_authoring_boundary(client) -> None:
    canonical = client.post(
        "/api/curve-lab/quote-canonicalizations",
        json={
            "instrument_type": "IRS",
            "input_lexeme": "-0",
            "input_convention": "DECIMAL",
        },
    )
    assert canonical.status_code == 200
    assert canonical.json()["raw_quote"] == "0"

    rendered = client.post(
        "/api/curve-lab/quote-renderings",
        json={
            "instrument_type": "IRS",
            "canonical_raw_quote": canonical.json()["raw_quote"],
            "display_convention": "PERCENT",
            "display_scale": 12,
        },
    )
    assert rendered.status_code == 200
    assert rendered.json() == {"rendered_quote": "0.000000000000"}


@pytest.mark.parametrize(
    "field",
    [
        "quote_coordinate_kind",
        "canonical_raw_unit",
        "raw_quote",
        "normalized_quote",
        "exact_risk_raw_bump",
        "normalized_risk_bump",
    ],
)
def test_authoring_rejects_axis_overrides_before_handler_side_effects(
    client, monkeypatch, field: str
) -> None:
    import app.routers.curve_lab as router

    called = False

    def should_not_run(*_args, **_kwargs):
        nonlocal called
        called = True
        raise AssertionError("canonicalization handler ran after an override")

    monkeypatch.setattr(router, "canonicalize_quote", should_not_run)
    payload = {
        "instrument_type": "IRS",
        "input_lexeme": "4",
        "input_convention": "PERCENT",
        field: "caller-owned",
    }

    response = client.post("/api/curve-lab/quote-canonicalizations", json=payload)

    assert response.status_code == 422
    assert response.json()["detail"]["code"] == "QUOTE_AXIS_OVERRIDE_FORBIDDEN"
    assert response.json()["detail"]["field"] == field
    assert called is False


@pytest.mark.parametrize(
    ("payload", "code", "field"),
    [
        (
            {
                "instrument_type": "SWAPTION",
                "input_lexeme": "4",
                "input_convention": "PERCENT",
            },
            "UNSUPPORTED_PRODUCT",
            "instrument_type",
        ),
        (
            {
                "instrument_type": "IRS",
                "input_lexeme": "4",
                "input_convention": "BASIS_POINTS",
            },
            "QUOTE_CONVENTION_UNKNOWN",
            "input_convention",
        ),
        (
            {
                "instrument_type": "FUTURE",
                "input_lexeme": "not-a-number",
                "input_convention": "PERCENT",
            },
            "QUOTE_INPUT_CONVENTION_MISMATCH",
            "input_convention",
        ),
    ],
)
def test_authoring_errors_use_the_curve_lab_envelope(
    client, payload: dict[str, str], code: str, field: str
) -> None:
    response = client.post("/api/curve-lab/quote-canonicalizations", json=payload)

    assert response.status_code == 422
    assert response.json()["detail"]["code"] == code
    assert response.json()["detail"]["field"] == field


def test_authoring_adapter_has_no_store_gateway_or_queue_dependency() -> None:
    from app.routers import curve_lab

    source_names = set(curve_lab.canonicalize_authoring_quote.__code__.co_names)
    assert source_names.isdisjoint({"get_store", "get_gateway", "create_task", "add"})


def test_openapi_quote_contract_is_closed_and_string_valued(client) -> None:
    document = client.app.openapi()
    schemas = document["components"]["schemas"]
    request = schemas["CurveLabQuoteCanonicalizationRequest"]
    response = schemas["CurveLabQuoteCanonicalizationResponse"]
    rendering_request = schemas["CurveLabQuoteRenderingRequest"]
    rendering_response = schemas["CurveLabQuoteRenderingResponse"]

    assert request["additionalProperties"] is False
    assert response["additionalProperties"] is False
    assert rendering_request["additionalProperties"] is False
    assert rendering_response["additionalProperties"] is False
    assert response["properties"]["raw_quote"]["type"] == "string"
    assert response["properties"]["normalized_quote"]["type"] == "string"
    assert rendering_request["properties"]["canonical_raw_quote"]["type"] == "string"
    assert rendering_response["properties"]["rendered_quote"]["type"] == "string"
    assert "/api/curve-lab/quote-renderings" in document["paths"]
    family_schema = request["properties"]["instrument_type"]
    if "$ref" in family_schema:
        family_schema = schemas[family_schema["$ref"].rsplit("/", 1)[-1]]
    assert family_schema["enum"] == [
        "DEPOSIT",
        "FRA",
        "FUTURE",
        "OIS",
        "IRS",
        "BASIS_SWAP",
        "XCCY",
    ]

    encoded = json.dumps(document, sort_keys=True)
    assert '"raw_quote": {"type": "number"}' not in encoded


def test_capability_defaults_are_the_single_risk_admission_source(client) -> None:
    import app.services.curve_risk as curve_risk
    from app.schemas.curve_lab import (
        CURVE_LAB_RISK_COST_COEFFICIENTS,
        CURVE_LAB_RISK_LIMITS,
    )

    capabilities = client.get("/api/curve-lab/capabilities")

    assert capabilities.status_code == 200
    assert capabilities.json()["risk_limits"] == dict(CURVE_LAB_RISK_LIMITS)
    assert capabilities.json()["risk_cost_coefficients"] == dict(CURVE_LAB_RISK_COST_COEFFICIENTS)
    assert not hasattr(curve_risk, "_LIMITS")
    assert not hasattr(curve_risk, "_COSTS")


@pytest.mark.parametrize(
    ("value", "bump", "expected"),
    [
        ("0.04", "0.0001", "0.0401"),
        ("95.82", "-0.01", "95.81"),
        ("0", "-0.01", "-0.01"),
        ("-0.0001", "0.0001", "0"),
        ("0.0000000000000001", "0.0000000000000001", "0.0000000000000002"),
    ],
)
def test_exact_decimal_bump_preserves_canonical_financial_bytes(
    value: str,
    bump: str,
    expected: str,
) -> None:
    from app.services.quote_canonicalization import apply_exact_decimal_bump

    assert apply_exact_decimal_bump(value, bump) == expected


@pytest.mark.parametrize("value", ["01", "NaN", "1e-4", "+0.04"])
def test_exact_decimal_bump_rejects_noncanonical_or_malformed_values(value: str) -> None:
    from app.services.quote_canonicalization import (
        QuoteCanonicalizationError,
        apply_exact_decimal_bump,
    )

    with pytest.raises(QuoteCanonicalizationError):
        apply_exact_decimal_bump(value, "0.0001")


def test_exact_decimal_bump_rejects_a_move_erased_by_binary64() -> None:
    from app.services.quote_canonicalization import (
        QuoteCanonicalizationError,
        apply_exact_decimal_bump,
    )

    with pytest.raises(QuoteCanonicalizationError) as raised:
        apply_exact_decimal_bump(
            "12345678901234567890.00000000000000000001",
            "0.00000000000000000009",
        )

    assert raised.value.code == "RISK_BUMP_NOT_REPRESENTABLE"
    assert raised.value.field == "raw_quote"
    assert raised.value.value == "12345678901234567890.00000000000000000001"


def test_curve_lab_canonical_json_preserves_exact_ascii_bytes_and_hash() -> None:
    import hashlib

    from app.services.canonical_json import canonical_json_bytes, canonical_json_hash

    value = {"z": "é", "a": [1, True, None]}
    expected = b'{"a":[1,true,null],"z":"\\u00e9"}'

    assert canonical_json_bytes(value) == expected
    assert canonical_json_hash(value) == hashlib.sha256(expected).hexdigest()


@pytest.mark.parametrize("value", [float("nan"), float("inf"), float("-inf")])
def test_curve_lab_canonical_json_rejects_non_finite_numbers(value: float) -> None:
    from app.services.canonical_json import canonical_json_bytes, canonical_json_hash

    with pytest.raises(ValueError):
        canonical_json_bytes({"value": value})
    with pytest.raises(ValueError):
        canonical_json_hash({"value": value})
