"""Curve Lab fixing admission must match the real compiled DAL timestamp projection."""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

import pytest


@pytest.mark.native
def test_compiled_required_fixing_matches_utc_snapshot(tmp_path) -> None:
    repository = Path(__file__).parents[3]
    script = Path(__file__).with_name("curve_lab_compiled_fixing_admission_subprocess.py")
    environment = os.environ.copy()
    native_package = repository / "build" / "Release-linux" / "dal-python"
    environment["PYTHONPATH"] = os.pathsep.join(
        value
        for value in (
            str(native_package),
            environment.get("PYTHONPATH"),
        )
        if value
    )
    completed = subprocess.run(
        [
            sys.executable,
            str(script),
            "--db",
            str(tmp_path / "compiled-fixing-admission.db"),
        ],
        check=False,
        capture_output=True,
        env=environment,
        text=True,
        timeout=60,
    )
    assert completed.returncode == 0, (  # nosec B101
        f"compiled fixing subprocess failed with {completed.returncode}\n"
        f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
    )
    result = json.loads(completed.stdout.splitlines()[-1])
    assert [row["status_code"] for row in result] == [202, 202, 202], result  # nosec B101
    assert [row["terminal"]["state"] for row in result] == [  # nosec B101
        "SUCCEEDED",
        "SUCCEEDED",
        "SUCCEEDED",
    ], result
