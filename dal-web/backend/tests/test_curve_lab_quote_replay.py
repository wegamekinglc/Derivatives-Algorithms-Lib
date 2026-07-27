"""Restart/replay regression for the approved percent-authoring caveat."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
from pathlib import Path

import pytest

from app.services.quote_canonicalization import (
    QuoteCanonicalizationError,
    canonicalize_quote,
    replay_canonical_quote,
)


@pytest.mark.parametrize(
    "family",
    ["DEPOSIT", "FRA", "OIS", "IRS", "BASIS_SWAP", "XCCY"],
)
def test_percent_and_decimal_quotes_replay_identically_with_one_bp_not_one_percent(
    family: str,
) -> None:
    percent = canonicalize_quote(family, "4", "PERCENT")
    decimal = canonicalize_quote(family, "0.04", "DECIMAL")
    payload = percent.financial_bytes()
    helper = Path(__file__).with_name("curve_lab_quote_replay_subprocess.py")

    completed = subprocess.run(
        [sys.executable, str(helper)],
        input=payload,
        capture_output=True,
        check=True,
    )
    replay = json.loads(completed.stdout)

    assert payload == decimal.financial_bytes()
    assert hashlib.sha256(payload).digest() == hashlib.sha256(decimal.financial_bytes()).digest()
    assert replay["financial"] == json.loads(decimal.financial_bytes())
    assert replay["raw_bumped"] == "0.0401"
    assert replay["normalized_bumped"] == "0.0401"
    assert replay["raw_bumped"] != "0.05"


def test_replay_rejects_noncanonical_persisted_bytes_before_dispatch() -> None:
    canonical = canonicalize_quote("IRS", "0.04", "DECIMAL")
    corrupted = json.loads(canonical.financial_bytes())
    corrupted["raw_quote"] = "0.040"
    payload = json.dumps(corrupted, separators=(",", ":")).encode()

    with pytest.raises(QuoteCanonicalizationError) as raised:
        replay_canonical_quote(payload)

    assert raised.value.code == "QUOTE_PERSISTED_BYTES_NOT_CANONICAL"
