"""API-05 native curve reconstruction must cross a real process boundary."""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

import pytest


def _invoke(mode: str, database: Path) -> dict:
    script = Path(__file__).with_name("curve_reconstruction_subprocess.py")
    result = subprocess.run(
        [sys.executable, str(script), "--mode", mode, "--db", str(database)],
        check=True,
        capture_output=True,
        env=os.environ.copy(),
        text=True,
        timeout=60,
    )
    return json.loads(result.stdout.splitlines()[-1])


@pytest.mark.native
def test_api_05_two_fresh_subprocesses_rebuild_all_curve_representations(tmp_path):
    """Writer A exits; reader B rebuilds recursive native curves from SQLite."""
    database = tmp_path / "curve-reconstruction.db"

    writer = _invoke("write", database)
    reader = _invoke("read", database)

    assert writer["pid_mode"] == "write"  # nosec B101
    assert reader["pid_mode"] == "read"  # nosec B101
    assert reader["curve_ids"] == writer["curve_ids"]  # nosec B101
    assert set(reader["values"]) == {"base", "pwc", "pwlf", "zero", "log"}  # nosec B101
    for name, expected in writer["values"].items():
        assert reader["values"][name] == pytest.approx(expected, rel=0.0, abs=1e-14)  # nosec B101
