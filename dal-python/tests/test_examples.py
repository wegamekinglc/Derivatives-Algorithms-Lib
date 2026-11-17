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
        ("012.fix_settings.py", {}, "PV=260.0000"),
        ("hybrid_script.py", {}, "d_spot:EQ[B]="),
        (
            "013.exercise_bermudan.py",
            {"DAL_EXAMPLE_NPATHS": "4096"},
            "Early-exercise premium",
        ),
        ("014.gsr_swap_swaption.py", {}, "Static YieldCurve PV"),
        ("017.native_aad_risk.py", {"DAL_NUM_THREADS": "1"}, "Native AAD risk examples passed."),
        ("018.quote_curvature.py", {"DAL_NUM_THREADS": "1"}, "Native quote curvature examples passed."),
        ("020.compounded_ois_quote_risk.py", {"DAL_NUM_THREADS": "1"}, "ScriptCurveQuoteRiskLiveQuoteUnsupported"),
        ("021.nanoql_multi_curve.py", {"DAL_NUM_THREADS": "1"}, "Calibration and pricing time:"),
        ("022.market_iv_local_vol.py", {"DAL_NUM_THREADS": "1"}, "Market-grid checks passed"),
    ],
)
def test_example_runs_through_its_guards(name, env, marker):
    # Exit 0 means every guard assertion inside the example held.
    child_env = {**os.environ, **env}
    child_env["PYTHONPATH"] = _PACKAGE_ROOT + (
        os.pathsep + child_env["PYTHONPATH"] if child_env.get("PYTHONPATH") else ""
    )
    result = subprocess.run(
        [sys.executable, "-S", str(EXAMPLES / name)],
        capture_output=True,
        text=True,
        env=child_env,
        timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert marker in result.stdout


@pytest.mark.parametrize("optimized", [False, True])
@pytest.mark.parametrize("invalid_audit", [False, True])
def test_compounded_ois_example_checks_survive_optimization(optimized, invalid_audit):
    child_env = {**os.environ, "DAL_NUM_THREADS": "1", "PYTHONPATH": os.pathsep.join((_PACKAGE_ROOT, str(EXAMPLES)))}
    program = "import dal, runpy, sys\n"
    if invalid_audit:
        program += "dal.CompoundedOis_Get_ObservationSchedule = lambda prepared: []\n"
    program += "runpy.run_path(sys.argv[1], run_name='__main__')\n"
    flags = ["-O"] if optimized else []
    result = subprocess.run(
        [sys.executable, "-S", *flags, "-c", program, str(EXAMPLES / "020.compounded_ois_quote_risk.py")],
        capture_output=True,
        text=True,
        env=child_env,
        timeout=60,
    )
    if invalid_audit:
        assert result.returncode == 1
        assert "RuntimeError: Expected three observations in a 22-column prepared audit" in result.stderr
    else:
        assert result.returncode == 0, result.stderr
        for mode in ("None", "Lookback", "ObservationShift", "Lockout"):
            assert f"Observation mode: {mode}\n" in result.stdout
        assert "ScriptCurveQuoteRiskLiveQuoteUnsupported" in result.stdout
