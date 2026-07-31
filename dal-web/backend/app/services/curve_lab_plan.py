"""Pure resolved component ordering for Curve Lab axes and native plans."""

from __future__ import annotations

from collections.abc import Mapping
from typing import Any


def _xccy_pair(document: Mapping[str, Any]) -> tuple[str, str]:
    declarations = list(document["declarations"])
    default_component = str(declarations[0]["component_key"])
    basis_keys = {
        str(declaration["component_key"])
        for declaration in declarations
        if declaration["role"] == "BASIS"
    }
    instruments = [
        item
        for item in document["instruments"]
        if item.get("included", True)
        and item["instrument_type"] == "XCCY"
        and str(item.get("terms", {}).get("component_key", default_component)) in basis_keys
    ]
    if not instruments:
        raise ValueError("XCCY component order requires an included basis instrument")
    token = str(instruments[0]["currency_or_pair"]).replace("/", "-")
    parts = token.split("-")
    if len(parts) != 2 or not all(parts):
        raise ValueError("XCCY currency_or_pair must contain two currencies")
    pair = parts[0], parts[1]
    if any(
        str(item["currency_or_pair"]).replace("/", "-") != "-".join(pair)
        for item in instruments[1:]
    ):
        raise ValueError("XCCY basis instruments must use one currency pair")
    return pair


def resolved_declaration_order(
    document: Mapping[str, Any],
) -> list[Mapping[str, Any]]:
    declarations = list(document["declarations"])
    if document["mode"] not in {"STAGED_XCCY", "JOINT_XCCY"}:
        return declarations
    domestic, foreign = _xccy_pair(document)
    indexed = list(enumerate(declarations))

    def group(item: tuple[int, Mapping[str, Any]]) -> tuple[int, int]:
        index, declaration = item
        if declaration["role"] == "BASIS":
            return 2, index
        currency = str(declaration["currency"])
        if currency == domestic:
            return 0, index
        if currency == foreign:
            return 1, index
        raise ValueError(f"XCCY declaration currency {currency!r} is outside {domestic}-{foreign}")

    return [declaration for _, declaration in sorted(indexed, key=group)]


def stage_id(document: Mapping[str, Any], declaration_index: int) -> str:
    if document["mode"] == "STAGED_XCCY":
        return f"stage-{declaration_index}"
    return "stage-0"
