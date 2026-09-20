"""Smoke tests: the numbered examples run end-to-end through their own guards."""

import os
from pathlib import Path
import subprocess
import sys

import pytest

EXAMPLES = Path(__file__).resolve().parents[1] / "examples"


@pytest.mark.parametrize(
    "name,env,marker",
    [
        ("010.generic_joint_quote_risk.py", {}, "quote_key,currency"),
        ("011.prepared_rate_pricing.py", {}, "dPV/dforward"),
        ("012.fix_settings.py", {}, "'PV': 260.0"),
        (
            "013.exercise_bermudan.py",
            {"DAL_EXAMPLE_NPATHS": "4096"},
            "Early-exercise premium",
        ),
    ],
)
def test_example_runs_through_its_guards(name, env, marker):
    # Exit 0 means every guard assertion inside the example held.
    result = subprocess.run(
        [sys.executable, str(EXAMPLES / name)],
        capture_output=True,
        text=True,
        env={**os.environ, **env},
        timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert marker in result.stdout
