"""Smoke tests: the numbered examples run end-to-end through their own guards."""

import os
from pathlib import Path
import subprocess
import sys

import dal
import pytest

EXAMPLES = Path(__file__).resolve().parents[1] / "examples"

# The examples are plain scripts, not package modules: the child interpreter
# needs the same dal package the test process imported (a build-tree package
# on CI, an installed one locally).
_PACKAGE_ROOT = str(Path(dal.__file__).resolve().parent.parent)


@pytest.mark.parametrize(
    "name,env,marker",
    [
        ("010.generic_joint_quote_risk.py", {}, "Quote key"),
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
    child_env = {**os.environ, **env}
    child_env["PYTHONPATH"] = _PACKAGE_ROOT + (
        os.pathsep + child_env["PYTHONPATH"] if child_env.get("PYTHONPATH") else ""
    )
    result = subprocess.run(
        [sys.executable, str(EXAMPLES / name)],
        capture_output=True,
        text=True,
        env=child_env,
        timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert marker in result.stdout
