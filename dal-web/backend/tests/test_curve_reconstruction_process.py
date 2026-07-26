"""API-05 native curve reconstruction must cross a real process boundary."""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

import pytest


def _invoke(
    mode: str,
    database: Path,
    ids: dict[str, dict[str, str]] | None = None,
) -> dict:
    script = Path(__file__).with_name("curve_reconstruction_subprocess.py")
    command = [
        sys.executable,
        str(script),
        "--mode",
        mode,
        "--db",
        str(database),
    ]
    if ids is not None:
        command.extend(("--ids-json", json.dumps(ids, sort_keys=True)))
    result = subprocess.run(
        command,
        check=False,
        capture_output=True,
        env=os.environ.copy(),
        text=True,
        timeout=60,
    )
    assert result.returncode == 0, (  # nosec B101
        f"{mode} subprocess failed with {result.returncode}\n"
        f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
    )
    return json.loads(result.stdout.splitlines()[-1])


@pytest.mark.native
def test_api_05_two_fresh_subprocesses_rebuild_all_curve_representations(tmp_path):
    """FIX-CB1-PERSISTENCE — writer/reader use only production HTTP payloads."""
    database = tmp_path / "curve-reconstruction.db"

    writer = _invoke("write", database)
    reader = _invoke(
        "read",
        database,
        {
            "run_ids": writer["run_ids"],
            "curve_ids": writer["curve_ids"],
        },
    )

    assert writer["pid_mode"] == "write"  # nosec B101
    assert reader["pid_mode"] == "read"  # nosec B101
    assert reader["curve_ids"] == writer["curve_ids"]  # nosec B101
    assert {"base", "pwc", "pwlf", "zero", "log"} <= set(reader["values"])  # nosec B101
    assert writer["http_reads"] == {"runs": 8, "curves": 10}  # nosec B101
    assert reader["http_reads"] == {"runs": 8, "curves": 10}  # nosec B101
    assert reader["canonical_runs"] == writer["canonical_runs"]  # nosec B101
    assert reader["canonical_plans"] == writer["canonical_plans"]  # nosec B101
    assert reader["canonical_plan_hashes"] == writer["canonical_plan_hashes"]  # nosec B101
    assert reader["actual_schemes"] == writer["actual_schemes"]  # nosec B101
    assert reader["actual_schemes"]["zero"] == "MIXED"  # nosec B101
    assert reader["actual_schemes"]["log"] == "MIXED"  # nosec B101
    assert reader["actual_schemes"]["staged_basis"] is None  # nosec B101
    assert reader["actual_schemes"]["joint_basis"] == "MIXED"  # nosec B101
    assert reader["planner_calls"] == 0  # nosec B101
    assert reader["inspector_calls"] == 0  # nosec B101
    assert reader["repair_writes"] == 0  # nosec B101
    for name, expected in writer["values"].items():
        assert reader["values"][name] == pytest.approx(expected, rel=0.0, abs=1e-14)  # nosec B101
