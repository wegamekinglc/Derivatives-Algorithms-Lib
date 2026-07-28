"""Bounded, closed-grammar preflight for untrusted native JSON archives."""

from __future__ import annotations

import json
import math
import re
import zlib
from collections.abc import Iterable
from dataclasses import dataclass
from typing import Any


@dataclass(frozen=True, slots=True)
class ArchiveLimits:
    wire_bytes: int = 10 * 1024 * 1024
    expanded_bytes: int = 50 * 1024 * 1024
    max_depth: int = 64
    max_values: int = 500_000
    max_objects: int = 10_000
    max_references: int = 50_000
    max_string_bytes: int = 1024 * 1024
    max_number_bytes: int = 1024
    max_numeric_array: int = 250_000


@dataclass(frozen=True, slots=True)
class ArchivePreflightResult:
    payload: bytes
    root_type: str
    wire_length: int
    expanded_length: int


class ArchivePreflightError(ValueError):
    def __init__(
        self,
        code: str,
        message: str,
        field: str = "payload",
        value: object = None,
        *,
        wire_length: int | None = None,
        expanded_length: int | None = None,
        **details: object,
    ) -> None:
        super().__init__(message)
        self.code = code
        self.message = message
        self.field = field
        self.value = value
        self.wire_length = wire_length
        self.expanded_length = expanded_length
        self.details = details


class _DuplicateKeyError(ValueError):
    def __init__(self, key: str) -> None:
        super().__init__(key)
        self.key = key


class _NonFiniteNumberError(ValueError):
    pass


_BAG_TYPES = frozenset({"Bag", "Bag_v1"})
_DISCOUNT_TYPES = frozenset(
    {
        "DiscountPWC",
        "DiscountPWC_v1",
        "DiscountPWLF",
        "DiscountPWLF_v1",
        "DiscountZeroRate",
        "DiscountZeroRate_v1",
        "DiscountLogDF",
        "DiscountLogDF_v1",
        "DiscountLogDF_v2",
    }
)
_INTERPOLATOR_TYPES = frozenset({"Interp1Linear_v1", "Cubic1", "LogLinear1"})
_ROOT_TYPES = _BAG_TYPES | _DISCOUNT_TYPES
_TYPE_FIELDS = {
    "Bag": {"$tag", "~type", "name", "keys"},
    "Bag_v1": {"$tag", "~type", "name", "keys"},
    "DiscountPWC": {"$tag", "~type", "name", "currency", "base", "dates", "values"},
    "DiscountPWC_v1": {
        "$tag",
        "~type",
        "name",
        "ccy",
        "base",
        "knotDates",
        "rightVals",
    },
    "DiscountPWLF": {
        "$tag",
        "~type",
        "name",
        "ccy",
        "base",
        "knotDates",
        "leftVals",
        "rightVals",
    },
    "DiscountPWLF_v1": {
        "$tag",
        "~type",
        "name",
        "ccy",
        "base",
        "knotDates",
        "leftVals",
        "rightVals",
    },
    "DiscountZeroRate": {
        "$tag",
        "~type",
        "name",
        "ccy",
        "base",
        "anchorDate",
        "nodeDates",
        "zeroRates",
        "dayCount",
        "scheme",
    },
    "DiscountZeroRate_v1": {
        "$tag",
        "~type",
        "name",
        "ccy",
        "base",
        "anchorDate",
        "nodeDates",
        "zeroRates",
        "dayCount",
        "scheme",
    },
    "DiscountLogDF": {
        "$tag",
        "~type",
        "name",
        "ccy",
        "base",
        "nodeDates",
        "logDF",
        "dayCount",
        "interp",
        "scheme",
    },
    "DiscountLogDF_v1": {
        "$tag",
        "~type",
        "name",
        "ccy",
        "base",
        "nodeDates",
        "logDF",
        "dayCount",
        "interp",
    },
    "DiscountLogDF_v2": {
        "$tag",
        "~type",
        "name",
        "ccy",
        "base",
        "nodeDates",
        "logDF",
        "dayCount",
        "scheme",
    },
    "Interp1Linear_v1": {"$tag", "~type", "x", "f"},
    "Cubic1": {"$tag", "~type", "x", "f", "fpp"},
    "LogLinear1": {"$tag", "~type", "x", "f"},
}
_REQUIRED_TYPE_FIELDS = {
    "Bag": {"~type", "name", "keys"},
    "Bag_v1": {"~type", "name", "keys"},
    "DiscountPWC": {"~type", "name", "currency", "dates", "values"},
    "DiscountPWC_v1": {"~type", "name", "ccy", "knotDates", "rightVals"},
    "DiscountPWLF": {
        "~type",
        "name",
        "ccy",
        "knotDates",
        "leftVals",
        "rightVals",
    },
    "DiscountPWLF_v1": {
        "~type",
        "name",
        "ccy",
        "knotDates",
        "leftVals",
        "rightVals",
    },
    "DiscountZeroRate": {
        "~type",
        "name",
        "ccy",
        "anchorDate",
        "nodeDates",
        "zeroRates",
        "dayCount",
        "scheme",
    },
    "DiscountZeroRate_v1": {
        "~type",
        "name",
        "ccy",
        "anchorDate",
        "nodeDates",
        "zeroRates",
        "dayCount",
        "scheme",
    },
    "DiscountLogDF": {
        "~type",
        "name",
        "ccy",
        "nodeDates",
        "logDF",
        "dayCount",
    },
    "DiscountLogDF_v1": {
        "~type",
        "name",
        "ccy",
        "nodeDates",
        "logDF",
        "dayCount",
        "interp",
    },
    "DiscountLogDF_v2": {
        "~type",
        "name",
        "ccy",
        "nodeDates",
        "logDF",
        "dayCount",
        "scheme",
    },
    "Interp1Linear_v1": {"~type", "x", "f"},
    "Cubic1": {"~type", "x", "f", "fpp"},
    "LogLinear1": {"~type", "x", "f"},
}
_CONTENTS_FIELD = re.compile(r"^contents[0-9]+$")


def preflight_archive(
    wire_payload: bytes,
    *,
    content_encoding: str | None = None,
    limits: ArchiveLimits = ArchiveLimits(),
) -> ArchivePreflightResult:
    """Expand and validate an archive without invoking native code."""

    payload: bytes | None = None
    try:
        if len(wire_payload) > limits.wire_bytes:
            raise ArchivePreflightError(
                "ARCHIVE_WIRE_PAYLOAD_TOO_LARGE",
                "Archive wire payload exceeds the 10 MiB limit.",
                value=len(wire_payload),
                expanded_length=0,
                limit=limits.wire_bytes,
            )
        payload = _expand(wire_payload, content_encoding, limits)
        if not payload:
            raise ArchivePreflightError(
                "ARCHIVE_PAYLOAD_EMPTY",
                "Archive payload must not be empty.",
            )
        if b"\x00" in payload:
            raise ArchivePreflightError(
                "ARCHIVE_PAYLOAD_NUL",
                "Archive payload must not contain NUL bytes.",
            )
        try:
            text = payload.decode("utf-8")
        except UnicodeDecodeError as exc:
            raise ArchivePreflightError(
                "ARCHIVE_PAYLOAD_INVALID_UTF8",
                "Archive payload must be valid UTF-8.",
                value=exc.start,
            ) from exc
        parsed = _decode_one_json_value(text, limits)
        _validate_structure(parsed, limits)
        root_type = _validate_grammar(parsed)
        _validate_reference_graph(parsed)
    except ArchivePreflightError as exc:
        if exc.wire_length is None:
            exc.wire_length = len(wire_payload)
        if exc.expanded_length is None:
            exc.expanded_length = len(payload) if payload is not None else 0
        raise
    assert payload is not None
    return ArchivePreflightResult(
        payload=payload,
        root_type=root_type,
        wire_length=len(wire_payload),
        expanded_length=len(payload),
    )


def _expand(
    wire_payload: bytes,
    content_encoding: str | None,
    limits: ArchiveLimits,
) -> bytes:
    encoding = (content_encoding or "identity").strip().lower()
    if encoding in {"", "identity"}:
        if len(wire_payload) > limits.expanded_bytes:
            raise ArchivePreflightError(
                "ARCHIVE_PAYLOAD_TOO_LARGE",
                "Expanded archive payload exceeds the 50 MiB limit.",
                value=len(wire_payload),
                limit=limits.expanded_bytes,
            )
        return wire_payload
    if encoding != "gzip":
        raise ArchivePreflightError(
            "ARCHIVE_CONTENT_ENCODING_UNSUPPORTED",
            "Archive Content-Encoding must be identity or gzip.",
            field="Content-Encoding",
            value=content_encoding,
        )
    decompressor = zlib.decompressobj(16 + zlib.MAX_WBITS)
    try:
        expanded = decompressor.decompress(
            wire_payload,
            limits.expanded_bytes + 1,
        )
        if len(expanded) <= limits.expanded_bytes:
            expanded += decompressor.flush(limits.expanded_bytes + 1 - len(expanded))
    except zlib.error as exc:
        raise ArchivePreflightError(
            "ARCHIVE_GZIP_MALFORMED",
            "Archive gzip stream is malformed.",
        ) from exc
    if len(expanded) > limits.expanded_bytes or decompressor.unconsumed_tail:
        raise ArchivePreflightError(
            "ARCHIVE_PAYLOAD_TOO_LARGE",
            "Expanded archive payload exceeds the 50 MiB limit.",
            value=len(expanded),
            expanded_length=len(expanded),
            limit=limits.expanded_bytes,
        )
    if not decompressor.eof:
        raise ArchivePreflightError(
            "ARCHIVE_GZIP_MALFORMED",
            "Archive gzip stream is incomplete.",
        )
    if decompressor.unused_data:
        raise ArchivePreflightError(
            "ARCHIVE_GZIP_TRAILING_BYTES",
            "Archive gzip stream contains trailing bytes.",
        )
    return expanded


def _decode_one_json_value(text: str, limits: ArchiveLimits) -> object:
    def pairs_hook(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise _DuplicateKeyError(key)
            result[key] = value
        return result

    decoder = json.JSONDecoder(
        object_pairs_hook=pairs_hook,
        parse_constant=lambda value: _raise_nonfinite(value),
        parse_float=lambda value: _parse_json_float(value, limits),
        parse_int=lambda value: _parse_json_int(value, limits),
    )
    start = len(text) - len(text.lstrip())
    try:
        parsed, end = decoder.raw_decode(text, start)
    except _DuplicateKeyError as exc:
        raise ArchivePreflightError(
            "ARCHIVE_JSON_DUPLICATE_KEY",
            "Archive JSON object contains a duplicate key.",
            value=exc.key,
        ) from exc
    except _NonFiniteNumberError as exc:
        raise ArchivePreflightError(
            "ARCHIVE_JSON_NONFINITE_NUMBER",
            "Archive JSON numbers must be finite.",
            value=str(exc),
        ) from exc
    except RecursionError as exc:
        raise ArchivePreflightError(
            "ARCHIVE_JSON_DEPTH_EXCEEDED",
            "Archive JSON nesting exceeds the depth limit.",
        ) from exc
    except json.JSONDecodeError as exc:
        raise ArchivePreflightError(
            "ARCHIVE_JSON_MALFORMED",
            "Archive payload is not one valid JSON value.",
            value=_utf8_offset(text, exc.pos),
        ) from exc
    if text[end:].strip():
        raise ArchivePreflightError(
            "ARCHIVE_JSON_TRAILING_BYTES",
            "Archive JSON contains non-whitespace trailing bytes.",
            value=_utf8_offset(text, end),
        )
    return parsed


def _utf8_offset(text: str, character_offset: int) -> int:
    return len(text[:character_offset].encode("utf-8"))


def _raise_nonfinite(value: str) -> None:
    raise _NonFiniteNumberError(value)


def _parse_json_float(value: str, limits: ArchiveLimits) -> float:
    _validate_number_length(value, limits)
    parsed = float(value)
    if not math.isfinite(parsed):
        raise ArchivePreflightError(
            "ARCHIVE_JSON_NONFINITE_NUMBER",
            "Archive JSON numbers must be finite.",
            value=value,
        )
    return parsed


def _parse_json_int(value: str, limits: ArchiveLimits) -> int:
    _validate_number_length(value, limits)
    return int(value)


def _validate_number_length(value: str, limits: ArchiveLimits) -> None:
    if len(value) > limits.max_number_bytes:
        raise ArchivePreflightError(
            "ARCHIVE_NUMBER_TOO_LARGE",
            "Archive numeric token exceeds the byte limit.",
            value=len(value),
            limit=limits.max_number_bytes,
        )


def _validate_structure(root: object, limits: ArchiveLimits) -> None:
    values = 0
    objects = 0
    references = 0
    stack: list[tuple[object, int, str]] = [(root, 1, "")]
    while stack:
        value, depth, path = stack.pop()
        values += 1
        if values > limits.max_values:
            raise ArchivePreflightError(
                "ARCHIVE_JSON_VALUE_LIMIT_EXCEEDED",
                "Archive JSON exceeds the total value limit.",
                value=values,
                limit=limits.max_values,
            )
        if depth > limits.max_depth:
            raise ArchivePreflightError(
                "ARCHIVE_JSON_DEPTH_EXCEEDED",
                "Archive JSON nesting exceeds the depth limit.",
                value=depth,
                limit=limits.max_depth,
            )
        if isinstance(value, str):
            size = _validated_string_size(value, path or "/")
            if size > limits.max_string_bytes:
                raise ArchivePreflightError(
                    "ARCHIVE_STRING_TOO_LARGE",
                    "Archive string exceeds the per-string byte limit.",
                    value=size,
                    limit=limits.max_string_bytes,
                )
        elif isinstance(value, dict):
            if set(value) == {"$tag"}:
                references += 1
                if references > limits.max_references:
                    raise ArchivePreflightError(
                        "ARCHIVE_REFERENCE_LIMIT_EXCEEDED",
                        "Archive exceeds the reference limit.",
                        value=references,
                        limit=limits.max_references,
                    )
            else:
                objects += 1
                if objects > limits.max_objects:
                    raise ArchivePreflightError(
                        "ARCHIVE_OBJECT_LIMIT_EXCEEDED",
                        "Archive exceeds the object limit.",
                        value=objects,
                        limit=limits.max_objects,
                    )
            for key in value:
                key_path = f"{path}/{_json_pointer_token(key)}"
                size = _validated_string_size(key, key_path)
                if size > limits.max_string_bytes:
                    raise ArchivePreflightError(
                        "ARCHIVE_STRING_TOO_LARGE",
                        "Archive string exceeds the per-string byte limit.",
                        value=size,
                        limit=limits.max_string_bytes,
                    )
            stack.extend(
                (
                    item,
                    depth + 1,
                    f"{path}/{_json_pointer_token(key)}",
                )
                for key, item in value.items()
            )
        elif isinstance(value, list):
            if len(value) > limits.max_numeric_array and all(
                isinstance(item, (int, float)) and not isinstance(item, bool) for item in value
            ):
                raise ArchivePreflightError(
                    "ARCHIVE_NUMERIC_ARRAY_TOO_LARGE",
                    "Archive numeric array exceeds the element limit.",
                    value=len(value),
                    limit=limits.max_numeric_array,
                )
            stack.extend((item, depth + 1, f"{path}/{index}") for index, item in enumerate(value))


def _validated_string_size(value: str, path: str) -> int:
    if "\x00" in value:
        raise ArchivePreflightError(
            "ARCHIVE_STRING_NUL",
            "Decoded archive strings must not contain U+0000.",
            field=path,
            value="\\u0000",
        )
    surrogate = next(
        (ord(character) for character in value if 0xD800 <= ord(character) <= 0xDFFF),
        None,
    )
    if surrogate is not None:
        raise ArchivePreflightError(
            "ARCHIVE_STRING_INVALID_UNICODE",
            "Decoded archive strings must contain only Unicode scalar values.",
            field=path,
            value=f"U+{surrogate:04X}",
        )
    return len(value.encode("utf-8"))


def _json_pointer_token(value: str) -> str:
    return value.replace("~", "~0").replace("/", "~1")


def _validate_grammar(root: object) -> str:
    if not isinstance(root, dict):
        raise ArchivePreflightError(
            "ARCHIVE_ROOT_OBJECT_REQUIRED",
            "Archive root must be a JSON object.",
        )
    root_type = root.get("~type")
    if root_type not in _ROOT_TYPES:
        raise ArchivePreflightError(
            "IMPORT_ROOT_TYPE_FORBIDDEN",
            "Native archive root type is outside the Curve Lab allowlist.",
            field="~type",
            value=root_type,
            allowed=sorted(_ROOT_TYPES),
        )
    definitions = _definition_type_index(root)
    handle_references: list[tuple[str, frozenset[str], str]] = []
    _validate_native_definition(
        root,
        allowed_types=_ROOT_TYPES,
        position="root",
        handle_references=handle_references,
    )
    for tag, allowed_types, field in handle_references:
        target_type = definitions.get(tag)
        if target_type is not None and target_type not in allowed_types:
            raise ArchivePreflightError(
                "ARCHIVE_TYPE_POSITION_FORBIDDEN",
                "Archive handle resolves to a native type forbidden in this position.",
                field=field,
                value=target_type,
                allowed=sorted(allowed_types),
            )
    return str(root_type)


def _definition_type_index(root: object) -> dict[str, str]:
    definitions: dict[str, str] = {}
    stack = [root]
    while stack:
        value = stack.pop()
        if isinstance(value, dict):
            if "$tag" in value and "~type" in value:
                definitions.setdefault(
                    _tag_token(value["$tag"]),
                    str(value["~type"]),
                )
            stack.extend(value.values())
        elif isinstance(value, list):
            stack.extend(value)
    return definitions


def _validate_native_definition(
    value: object,
    *,
    allowed_types: frozenset[str],
    position: str,
    handle_references: list[tuple[str, frozenset[str], str]],
) -> None:
    if isinstance(value, dict) and set(value) == {"$tag"}:
        handle_references.append((_tag_token(value["$tag"]), allowed_types, position))
        return
    if not isinstance(value, dict):
        _field_type_error(position, "native definition or tag reference", value)
    native_type_value = value.get("~type")
    if native_type_value is None:
        raise ArchivePreflightError(
            "ARCHIVE_OBJECT_GRAMMAR_FORBIDDEN",
            "Archive objects must be native definitions or tag references.",
            field=position,
        )
    if not isinstance(native_type_value, str):
        _field_type_error("~type", "string", native_type_value)
    native_type = native_type_value
    allowed_fields = _TYPE_FIELDS.get(native_type)
    if allowed_fields is None:
        raise ArchivePreflightError(
            "ARCHIVE_TYPE_FORBIDDEN",
            "Archive contains an unsupported native type.",
            field="~type",
            value=native_type,
        )
    if native_type not in allowed_types:
        raise ArchivePreflightError(
            "ARCHIVE_TYPE_POSITION_FORBIDDEN",
            "Archive native type is forbidden in this position.",
            field=position,
            value=native_type,
            allowed=sorted(allowed_types),
        )
    for field in value:
        if field not in allowed_fields and not (
            native_type in _BAG_TYPES and _CONTENTS_FIELD.fullmatch(field)
        ):
            raise ArchivePreflightError(
                "ARCHIVE_FIELD_FORBIDDEN",
                "Archive native object contains an unsupported field.",
                field=field,
                value=native_type,
            )
    missing = _REQUIRED_TYPE_FIELDS.get(native_type, set()) - value.keys()
    if missing:
        raise ArchivePreflightError(
            "ARCHIVE_FIELD_REQUIRED",
            "Archive native object is missing a required field.",
            field=sorted(missing)[0],
            value=native_type,
        )
    if "$tag" in value:
        _tag_token(value["$tag"])
    if native_type in _BAG_TYPES:
        _validate_bag(value, native_type, handle_references)
    elif native_type in _DISCOUNT_TYPES:
        _validate_discount_curve(value, native_type, handle_references)
    else:
        _validate_interpolator(value, native_type)


def _validate_bag(
    value: dict[str, Any],
    native_type: str,
    handle_references: list[tuple[str, frozenset[str], str]],
) -> None:
    _require_string(value, "name")
    keys = _require_array(value, "keys")
    if any(not isinstance(item, str) for item in keys):
        _field_type_error("keys", "array of strings", keys)
    empty_key = next((item for item in keys if not item), None)
    if empty_key is not None:
        raise ArchivePreflightError(
            "ARCHIVE_BAG_KEY_INVALID",
            "Archive Bag keys must be non-empty strings.",
            field="keys",
            value=empty_key,
        )
    if len(set(keys)) != len(keys):
        duplicate = next(key for index, key in enumerate(keys) if key in keys[:index])
        raise ArchivePreflightError(
            "ARCHIVE_BAG_KEY_DUPLICATE",
            "Archive Bag keys must be unique.",
            field="keys",
            value=duplicate,
        )
    contents_fields = sorted(
        (field for field in value if _CONTENTS_FIELD.fullmatch(field)),
        key=lambda field: int(field.removeprefix("contents")),
    )
    expected = [f"contents{index}" for index in range(len(keys))]
    if contents_fields != expected:
        raise ArchivePreflightError(
            "ARCHIVE_BAG_CARDINALITY_INVALID",
            "Archive Bag keys and contents fields must be contiguous and equal in count.",
            field="keys",
            value={
                "keys": len(keys),
                "contents": len(contents_fields),
            },
        )
    for field in contents_fields:
        _validate_native_definition(
            value[field],
            allowed_types=_DISCOUNT_TYPES,
            position=field,
            handle_references=handle_references,
        )
    if native_type not in _BAG_TYPES:  # pragma: no cover - caller invariant
        raise AssertionError(native_type)


def _validate_discount_curve(
    value: dict[str, Any],
    native_type: str,
    handle_references: list[tuple[str, frozenset[str], str]],
) -> None:
    _require_string(value, "name")
    for field in (
        "ccy",
        "currency",
        "anchorDate",
        "dayCount",
        "scheme",
    ):
        if field in value:
            _require_string(value, field)
    array_pairs: tuple[tuple[str, str], ...]
    if native_type == "DiscountPWC":
        array_pairs = (("dates", "string"), ("values", "number"))
    elif native_type == "DiscountPWC_v1":
        array_pairs = (("knotDates", "string"), ("rightVals", "number"))
    elif native_type in {"DiscountPWLF", "DiscountPWLF_v1"}:
        array_pairs = (
            ("knotDates", "string"),
            ("leftVals", "number"),
            ("rightVals", "number"),
        )
    elif native_type in {"DiscountZeroRate", "DiscountZeroRate_v1"}:
        array_pairs = (("nodeDates", "string"), ("zeroRates", "number"))
    else:
        array_pairs = (("nodeDates", "string"), ("logDF", "number"))
    lengths: list[int] = []
    for field, item_type in array_pairs:
        items = _require_array(value, field)
        predicate = (lambda item: isinstance(item, str)) if item_type == "string" else _is_number
        if any(not predicate(item) for item in items):
            _field_type_error(field, f"array of {item_type}s", items)
        lengths.append(len(items))
    if len(set(lengths)) > 1:
        raise ArchivePreflightError(
            "ARCHIVE_FIELD_CARDINALITY_INVALID",
            "Archive curve coordinate arrays must have equal lengths.",
            field=array_pairs[-1][0],
            value=lengths,
        )
    if "base" in value and value["base"] is not None:
        _validate_native_definition(
            value["base"],
            allowed_types=_DISCOUNT_TYPES,
            position="base",
            handle_references=handle_references,
        )
    if "interp" in value:
        _validate_native_definition(
            value["interp"],
            allowed_types=_INTERPOLATOR_TYPES,
            position="interp",
            handle_references=handle_references,
        )


def _validate_interpolator(value: dict[str, Any], native_type: str) -> None:
    fields = ("x", "f", "fpp") if native_type == "Cubic1" else ("x", "f")
    lengths: list[int] = []
    for field in fields:
        items = _require_array(value, field)
        if any(not _is_number(item) for item in items):
            _field_type_error(field, "array of numbers", items)
        lengths.append(len(items))
    if len(set(lengths)) > 1:
        raise ArchivePreflightError(
            "ARCHIVE_FIELD_CARDINALITY_INVALID",
            "Archive interpolator arrays must have equal lengths.",
            field=fields[-1],
            value=lengths,
        )


def _require_string(value: dict[str, Any], field: str) -> str:
    item = value[field]
    if not isinstance(item, str):
        _field_type_error(field, "string", item)
    return item


def _require_array(value: dict[str, Any], field: str) -> list[Any]:
    item = value[field]
    if not isinstance(item, list):
        _field_type_error(field, "array", item)
    return item


def _is_number(value: object) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _field_type_error(field: str, expected: str, value: object) -> None:
    raise ArchivePreflightError(
        "ARCHIVE_FIELD_TYPE_INVALID",
        "Archive native field has an invalid value type.",
        field=field,
        value=type(value).__name__,
        expected=expected,
    )


def _tag_token(value: object) -> str:
    if isinstance(value, bool) or not isinstance(value, (str, int)):
        raise ArchivePreflightError(
            "ARCHIVE_TAG_INVALID",
            "Archive tags must be strings or integers.",
            field="$tag",
            value=value,
        )
    return str(value)


def _validate_reference_graph(root: object) -> None:
    definitions: dict[str, dict[str, Any]] = {}
    references: list[str] = []
    stack: list[object] = [root]
    while stack:
        value = stack.pop()
        if isinstance(value, dict):
            if set(value) == {"$tag"}:
                references.append(_tag_token(value["$tag"]))
                continue
            if "$tag" in value and "~type" in value:
                tag = _tag_token(value["$tag"])
                if tag in definitions:
                    raise ArchivePreflightError(
                        "ARCHIVE_TAG_DUPLICATE",
                        "Archive contains duplicate tag definitions.",
                        field="$tag",
                        value=tag,
                    )
                definitions[tag] = value
            stack.extend(value.values())
        elif isinstance(value, list):
            stack.extend(value)
    dangling = next((tag for tag in references if tag not in definitions), None)
    if dangling is not None:
        raise ArchivePreflightError(
            "ARCHIVE_REFERENCE_DANGLING",
            "Archive contains a reference to an undefined tag.",
            field="$tag",
            value=dangling,
        )
    graph = {tag: set(_definition_edges(value, tag)) for tag, value in definitions.items()}
    state: dict[str, int] = {}
    for start in graph:
        if state.get(start, 0) != 0:
            continue
        state[start] = 1
        stack: list[tuple[str, Any]] = [(start, iter(graph.get(start, ())))]
        while stack:
            tag, targets = stack[-1]
            try:
                target = next(targets)
            except StopIteration:
                state[tag] = 2
                stack.pop()
                continue
            target_state = state.get(target, 0)
            if target_state == 1:
                raise ArchivePreflightError(
                    "ARCHIVE_REFERENCE_CYCLE",
                    "Archive tag graph contains a cycle.",
                    field="$tag",
                    value=target,
                )
            if target_state == 0:
                state[target] = 1
                stack.append((target, iter(graph.get(target, ()))))


def _definition_edges(value: object, owner: str) -> Iterable[str]:
    stack: list[object] = list(value.values()) if isinstance(value, dict) else []
    while stack:
        nested = stack.pop()
        if isinstance(nested, dict):
            if set(nested) == {"$tag"}:
                yield _tag_token(nested["$tag"])
                continue
            if "$tag" in nested and "~type" in nested:
                target = _tag_token(nested["$tag"])
                if target != owner:
                    yield target
                    continue
            stack.extend(nested.values())
        elif isinstance(nested, list):
            stack.extend(nested)
