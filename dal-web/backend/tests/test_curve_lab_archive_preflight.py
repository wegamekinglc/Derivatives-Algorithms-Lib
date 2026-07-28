"""Hostile archive boundaries must fail before the native JSON reader."""

from __future__ import annotations

import asyncio
import gzip
import json

import pytest


def _curve(**overrides: object) -> dict[str, object]:
    return {
        "$tag": "1",
        "~type": "DiscountPWC_v1",
        "name": "curve",
        "ccy": "USD",
        "knotDates": ["2027-01-15"],
        "rightVals": [0.04],
        **overrides,
    }


@pytest.mark.parametrize(
    ("payload", "code"),
    [
        (b"", "ARCHIVE_PAYLOAD_EMPTY"),
        (b"\xff", "ARCHIVE_PAYLOAD_INVALID_UTF8"),
        (b'{"~type":"Bag"}\x00', "ARCHIVE_PAYLOAD_NUL"),
        (b'{"~type":', "ARCHIVE_JSON_MALFORMED"),
        (b'{"~type":"Bag"} trailing', "ARCHIVE_JSON_TRAILING_BYTES"),
        (
            b'{"~type":"Bag","~type":"DiscountPWC_v1"}',
            "ARCHIVE_JSON_DUPLICATE_KEY",
        ),
        (b'{"~type":"Swaption"}', "IMPORT_ROOT_TYPE_FORBIDDEN"),
        (
            b'{"~type":"DiscountPWC_v1","mystery":1}',
            "ARCHIVE_FIELD_FORBIDDEN",
        ),
        (
            b'{"~type":"DiscountPWC_v1","name":"missing curve data"}',
            "ARCHIVE_FIELD_REQUIRED",
        ),
        (
            b'{"~type":"Bag","name":"bag","keys":["bad"],"contents0":{"not":"native"}}',
            "ARCHIVE_OBJECT_GRAMMAR_FORBIDDEN",
        ),
        (
            b'{"~type":"Bag","keys":[1e999]}',
            "ARCHIVE_JSON_NONFINITE_NUMBER",
        ),
        (
            (b'{"~type":"Bag","keys":[' + b"1" * 1025 + b"]}"),
            "ARCHIVE_NUMBER_TOO_LARGE",
        ),
        (
            json.dumps(_curve(rightVals="not-an-array")).encode(),
            "ARCHIVE_FIELD_TYPE_INVALID",
        ),
        (
            json.dumps(_curve(ccy={"$tag": "1"})).encode(),
            "ARCHIVE_FIELD_TYPE_INVALID",
        ),
        (
            json.dumps(
                {
                    "~type": "Bag",
                    "name": "outer",
                    "keys": ["nested"],
                    "contents0": {
                        "~type": "Bag",
                        "name": "inner",
                        "keys": [],
                    },
                }
            ).encode(),
            "ARCHIVE_TYPE_POSITION_FORBIDDEN",
        ),
        (
            json.dumps(
                _curve(
                    base={
                        "~type": "Bag",
                        "name": "not-a-curve",
                        "keys": [],
                    }
                )
            ).encode(),
            "ARCHIVE_TYPE_POSITION_FORBIDDEN",
        ),
        (
            json.dumps(
                {
                    "~type": "Bag",
                    "name": "empty-key",
                    "keys": [""],
                    "contents0": _curve(),
                }
            ).encode(),
            "ARCHIVE_BAG_KEY_INVALID",
        ),
        (
            json.dumps(
                {
                    "~type": "Bag",
                    "name": "duplicate-key",
                    "keys": ["same", "same"],
                    "contents0": _curve(),
                    "contents1": _curve(
                        **{
                            "$tag": "2",
                            "name": "second",
                        }
                    ),
                }
            ).encode(),
            "ARCHIVE_BAG_KEY_DUPLICATE",
        ),
        (
            json.dumps(_curve(name="\x00")).encode(),
            "ARCHIVE_STRING_NUL",
        ),
        (
            b'{"$tag":"1","~type":"DiscountPWC_v1","name":"\\ud800",'
            b'"ccy":"USD","knotDates":["2027-01-15"],"rightVals":[0.04]}',
            "ARCHIVE_STRING_INVALID_UNICODE",
        ),
        (
            b'{"$tag":"1","~type":"DiscountPWC_v1","name":"\\udc00",'
            b'"ccy":"USD","knotDates":["2027-01-15"],"rightVals":[0.04]}',
            "ARCHIVE_STRING_INVALID_UNICODE",
        ),
    ],
)
def test_preflight_classifies_lexical_and_grammar_failures(
    payload: bytes,
    code: str,
) -> None:
    from app.services.archive_preflight import ArchivePreflightError, preflight_archive

    with pytest.raises(ArchivePreflightError) as captured:
        preflight_archive(payload)

    assert captured.value.code == code


def test_preflight_accepts_valid_supplementary_unicode_scalar() -> None:
    from app.services.archive_preflight import preflight_archive

    payload = (
        b'{"$tag":"1","~type":"DiscountPWC_v1","name":"curve-\\ud83d\\ude00",'
        b'"ccy":"USD","knotDates":["2027-01-15"],"rightVals":[0.04]}'
    )

    accepted = preflight_archive(payload)

    assert accepted.payload == payload
    assert accepted.root_type == "DiscountPWC_v1"


def test_preflight_enforces_wire_and_expanded_caps() -> None:
    from app.services.archive_preflight import (
        ArchiveLimits,
        ArchivePreflightError,
        preflight_archive,
    )

    limits = ArchiveLimits(wire_bytes=32, expanded_bytes=64)
    with pytest.raises(ArchivePreflightError) as wire:
        preflight_archive(b" " * 33, limits=limits)
    with pytest.raises(ArchivePreflightError) as expanded:
        preflight_archive(
            gzip.compress(b" " * 65),
            content_encoding="gzip",
            limits=limits,
        )

    assert wire.value.code == "ARCHIVE_WIRE_PAYLOAD_TOO_LARGE"
    assert expanded.value.code == "ARCHIVE_PAYLOAD_TOO_LARGE"


@pytest.mark.parametrize(
    ("value", "limits", "code"),
    [
        (
            {"~type": "Bag", "contents0": {"a": {"b": {"c": 1}}}},
            {"max_depth": 3},
            "ARCHIVE_JSON_DEPTH_EXCEEDED",
        ),
        (
            {"~type": "Bag", "keys": ["a", "b", "c"]},
            {"max_values": 4},
            "ARCHIVE_JSON_VALUE_LIMIT_EXCEEDED",
        ),
        (
            _curve(name="x" * 17),
            {"max_string_bytes": 16},
            "ARCHIVE_STRING_TOO_LARGE",
        ),
        (
            _curve(rightVals=[0.01, 0.02, 0.03]),
            {"max_numeric_array": 2},
            "ARCHIVE_NUMERIC_ARRAY_TOO_LARGE",
        ),
        (
            {"~type": "Bag", "contents0": {}, "contents1": {}},
            {"max_objects": 2},
            "ARCHIVE_OBJECT_LIMIT_EXCEEDED",
        ),
        (
            {"~type": "Bag", "contents0": {"$tag": "1"}, "contents1": {"$tag": "2"}},
            {"max_references": 1},
            "ARCHIVE_REFERENCE_LIMIT_EXCEEDED",
        ),
    ],
)
def test_preflight_enforces_structural_limits(
    value: object,
    limits: dict[str, int],
    code: str,
) -> None:
    from app.services.archive_preflight import (
        ArchiveLimits,
        ArchivePreflightError,
        preflight_archive,
    )

    with pytest.raises(ArchivePreflightError) as captured:
        preflight_archive(
            json.dumps(value).encode(),
            limits=ArchiveLimits(**limits),
        )

    assert captured.value.code == code


@pytest.mark.parametrize(
    ("value", "code"),
    [
        (
            {
                "~type": "Bag",
                "name": "bag",
                "keys": ["curve", "duplicate"],
                "contents0": _curve(),
                "contents1": _curve(name="duplicate"),
            },
            "ARCHIVE_TAG_DUPLICATE",
        ),
        (
            _curve(base={"$tag": "missing"}),
            "ARCHIVE_REFERENCE_DANGLING",
        ),
        (
            _curve(
                **{
                    "$tag": "a",
                    "base": _curve(
                        **{
                            "$tag": "b",
                            "name": "base",
                            "base": {"$tag": "a"},
                        }
                    ),
                }
            ),
            "ARCHIVE_REFERENCE_CYCLE",
        ),
    ],
)
def test_preflight_validates_tag_reference_graph(
    value: object,
    code: str,
) -> None:
    from app.services.archive_preflight import ArchivePreflightError, preflight_archive

    with pytest.raises(ArchivePreflightError) as captured:
        preflight_archive(json.dumps(value).encode())

    assert captured.value.code == code


def test_preflight_returns_exact_expanded_bytes_for_native_reader() -> None:
    from app.services.archive_preflight import preflight_archive

    expanded = json.dumps(_curve(name="curve-" + "x" * 200), separators=(",", ":")).encode()
    result = preflight_archive(gzip.compress(expanded), content_encoding="GZip")

    assert result.payload == expanded
    assert result.wire_length < result.expanded_length
    assert result.root_type == "DiscountPWC_v1"


def test_preflight_accepts_a_curve_handle_only_in_an_allowed_base_position() -> None:
    from app.services.archive_preflight import preflight_archive

    payload = json.dumps(
        _curve(
            base=_curve(
                **{
                    "$tag": "base",
                    "name": "base-curve",
                }
            )
        )
    ).encode()

    assert preflight_archive(payload).root_type == "DiscountPWC_v1"


def test_bounded_router_reader_stops_after_limit_plus_one_bytes() -> None:
    from app.routers.curve_lab import _read_bounded_request_body

    class ChunkedRequest:
        consumed = 0

        async def stream(self):
            for chunk in (b"abc", b"def", b"must-not-be-consumed"):
                self.consumed += 1
                yield chunk

    request = ChunkedRequest()

    payload = asyncio.run(_read_bounded_request_body(request, wire_bytes=4))

    assert payload == b"abcde"
    assert request.consumed == 2
