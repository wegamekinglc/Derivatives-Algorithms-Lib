#!/usr/bin/env python3
"""Generate or validate non-gating Web calibration performance reports.

Only a repository-marked Release/native performance runner may publish passing
PERF-01/PERF-02 measurements. Other machines commit an explicit skipped report
with the exact fixture hash and environment instead of fabricating timings.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import sys
from datetime import date
from pathlib import Path

import pydantic

ROOT = Path(__file__).resolve().parents[2]
REPORT_DIR = ROOT / "dal-web" / "backend" / "performance"
BASE_COMMIT = "54768fdd0c1a551d997475ea9d40e069b146a097"
SKIP_REASON = (
    "not executed: this host is not a repository-marked Release/native "
    "performance runner"
)


def _month(year: int, month: int) -> date:
    return date(year + (month - 1) // 12, (month - 1) % 12 + 1, 2)


def _fixture(count: int, *, matrices: bool) -> dict[str, object]:
    knots = [_month(2026, month) for month in range(2, count + 2)]
    index = {
        "spot_lag": 0,
        "fixing_lag": 0,
        "use_projection_curve": False,
        "forecast_tenor": "P12M",
        "day_basis": "ACT_365F",
        "business_day_convention": "Following",
        "fixing_holidays": "",
        "accrual_holidays": "",
        "end_of_month": False,
        "collateral": "OIS",
    }
    return {
        "schema_version": 1,
        "name": f"perf-{count}",
        "today": "2026-01-02",
        "currency": "USD",
        "declaration": {
            "curve_name": f"perf-{count}",
            "target_collateral": "OIS",
            "target_tenor": None,
            "calibrate_discount_curve": True,
            "libor_basis": "ACT_365F",
            "parameterization": "PIECEWISE_CONSTANT_FWD",
            "log_df_scheme": None,
            "knot_policy": "INPUT",
            "knot_dates": [value.isoformat() for value in knots],
            "base_curve_id": None,
            "discount_curve_ids": {},
            "forward_curve_ids": {},
            "initial_guess_per_node": [0.02] * count,
        },
        "instruments": [
            {
                "kind": "DEPOSIT",
                "label": f"USD DEP {index_number + 1}",
                "trade_date": "2026-01-02",
                "start": "2026-01-02",
                "maturity": knot.isoformat(),
                "market_rate": 0.0200 + index_number * 0.0001,
                "index": index,
            }
            for index_number, knot in enumerate(knots)
        ],
        "solver": {
            "solve_mode": "EXACT",
            "smoothing_weight": 1.0,
            "tolerance": 1e-8,
            "fit_tolerance": 1e-6,
            "initial_guess": 0.02,
            "max_evaluations": 200,
            "max_restarts": 20,
        },
        "options": {
            "jacobian_mode": "ANALYTIC",
            "include_jacobian": matrices,
            "include_effective_inverse": matrices,
        },
    }


def _canonical_hash(value: object) -> str:
    encoded = json.dumps(
        value,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    ).encode()
    return hashlib.sha256(encoded).hexdigest()


def _cpu_model() -> str:
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.exists():
        for line in cpuinfo.read_text(encoding="utf-8").splitlines():
            if line.startswith("model name"):
                return line.split(":", 1)[1].strip()
    return platform.processor() or "unknown"


def _skipped_report(fixture_id: str, payload: object) -> dict[str, object]:
    return {
        "fixture_id": fixture_id,
        "fixture_sha256": _canonical_hash(payload),
        "dal_commit": BASE_COMMIT,
        "build_type": "Release",
        "aad_backend": "AADET",
        "cpu_model": _cpu_model(),
        "logical_cpu_count": os.cpu_count() or 0,
        "os": platform.platform(),
        "python_version": platform.python_version(),
        "pydantic_version": pydantic.__version__,
        "clock": "monotonic_high_resolution",
        "gc_disabled_during_trials": True,
        "warmups": 1,
        "native_solve_ms": [],
        "serialization_ms": [],
        "matrix_materialization_ms": [],
        "peak_python_allocation_bytes": 0,
        "gate_status": "skipped",
        "skip_reason": SKIP_REASON,
    }


def expected_reports() -> dict[Path, dict[str, object]]:
    perf_30 = _skipped_report("FIX-PERF-30", _fixture(30, matrices=False))
    perf_30["health_observations"] = []
    perf_30["trial_max_ms"] = []
    return {
        REPORT_DIR / "fix-perf-30.json": perf_30,
        REPORT_DIR / "fix-perf-100x100.json": _skipped_report(
            "FIX-PERF-100X100", _fixture(100, matrices=True)
        ),
    }


def generate() -> None:
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    for path, report in expected_reports().items():
        path.write_text(
            json.dumps(report, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
        print(f"wrote {path.relative_to(ROOT)}")


def check() -> None:
    if os.environ.get("DAL_WEB_PERFORMANCE_RUNNER") == "1":
        raise SystemExit(
            "skipped reports are forbidden on a marked performance runner; "
            "run the dedicated measured-report job"
        )
    expected = expected_reports()
    for path, generated in expected.items():
        stored = json.loads(path.read_text(encoding="utf-8"))
        stable_fields = (
            "fixture_id",
            "fixture_sha256",
            "dal_commit",
            "build_type",
            "gate_status",
            "skip_reason",
        )
        for field in stable_fields:
            if stored.get(field) != generated[field]:
                raise SystemExit(
                    f"{path.relative_to(ROOT)}: stale or invalid {field}"
                )
        for timing_field in (
            "native_solve_ms",
            "serialization_ms",
            "matrix_materialization_ms",
        ):
            if stored.get(timing_field) != []:
                raise SystemExit(
                    f"{path.relative_to(ROOT)}: skipped report contains "
                    f"fabricated {timing_field}"
                )
    print("web calibration performance reports: explicit non-runner skips valid")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--generate", action="store_true")
    args = parser.parse_args()
    generate() if args.generate else check()
    return 0


if __name__ == "__main__":
    sys.exit(main())
