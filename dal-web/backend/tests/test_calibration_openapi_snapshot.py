"""Committed OpenAPI contract for the DAL Web backend."""

from __future__ import annotations

import json
from pathlib import Path


def test_openapi_snapshot_has_no_drift(client) -> None:
    snapshot = Path(__file__).parents[1] / "openapi" / "dal-web.openapi.json"
    committed = json.loads(snapshot.read_text(encoding="utf-8"))
    assert committed == client.app.openapi()
