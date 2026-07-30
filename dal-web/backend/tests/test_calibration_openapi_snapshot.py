"""Committed OpenAPI contract for the DAL Web backend."""

from __future__ import annotations

import json
from pathlib import Path


def test_openapi_snapshot_has_no_drift(client) -> None:
    snapshot = Path(__file__).parents[1] / "openapi" / "dal-web.openapi.json"
    committed = json.loads(snapshot.read_text(encoding="utf-8"))
    assert committed == client.app.openapi()


def test_rate_trade_day_basis_schema_order_is_stable(client) -> None:
    schema = client.app.openapi()["components"]["schemas"]["RateTradeTermsInputV2"]
    day_basis = schema["properties"]["day_basis"]["anyOf"][0]
    assert day_basis["enum"] == ["ACT_365F", "ACT_360", "30_360"]
