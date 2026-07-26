#!/usr/bin/env python3
"""Run the marked Release/native Web calibration performance gates."""

from __future__ import annotations

import argparse
import gc
import json
import math
import os
import platform
import statistics
import subprocess
import sys
import tempfile
import time
import tracemalloc
from pathlib import Path

import pydantic
from fastapi.testclient import TestClient

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

from generate_web_calibration_perf_reports import (  # noqa: E402
    _canonical_hash,
    _cpu_model,
    _fixture,
)

PERF_30_TRIALS = 3
PERF_100_TRIALS = 7
HEALTH_SAMPLES = 5
HEALTH_LIMIT_MS = 200.0
SERIALIZATION_RATIO_LIMIT = 0.15


def _environment(fixture_id: str, payload: object) -> dict[str, object]:
    return {
        "fixture_id": fixture_id,
        "fixture_sha256": _canonical_hash(payload),
        "dal_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"],
            cwd=ROOT,
            text=True,
        ).strip(),
        "build_type": "Release",
        "aad_backend": os.environ.get("DAL_AAD_BACKEND", "AADET"),
        "cpu_model": _cpu_model(),
        "logical_cpu_count": os.cpu_count() or 0,
        "os": platform.platform(),
        "python_version": platform.python_version(),
        "pydantic_version": pydantic.__version__,
        "clock": "monotonic_high_resolution",
        "gc_disabled_during_trials": True,
        "warmups": 1,
    }


def _reset_application(database: Path) -> TestClient:
    os.environ.pop("DAL_WEB_STORE", None)
    os.environ["DAL_WEB_DB_URL"] = f"sqlite:///{database}"
    from app.main import create_app
    from app.services import dal_gateway, store

    dal_gateway._gateway_box[0] = None
    store._store_box[0] = None
    return TestClient(create_app())


def _wait_for_terminal(
    client: TestClient,
    location: str,
    *,
    health_samples: int = 0,
) -> tuple[dict[str, object], list[dict[str, object]]]:
    health: list[dict[str, object]] = []
    saw_solving = False
    for _ in range(2_000):
        polled = client.get(location)
        polled.raise_for_status()
        body = polled.json()
        if body["status"] != "running":
            if health_samples and not saw_solving:
                raise RuntimeError(
                    "native calibration completed before a solving-phase health sample"
                )
            return body, health
        if body["phase"] == "solving" and not saw_solving:
            saw_solving = True
            for sample in range(health_samples):
                started = time.perf_counter()
                response = client.get("/api/health")
                latency_ms = (time.perf_counter() - started) * 1_000.0
                response.raise_for_status()
                health.append(
                    {
                        "sample": sample,
                        "latency_ms": latency_ms,
                        "status": response.json()["status"],
                        "calibration_phase": "solving",
                    }
                )
        time.sleep(0.001)
    raise RuntimeError(f"calibration did not terminalize: {location}")


def _solve(
    client: TestClient,
    payload: dict[str, object],
    *,
    health_samples: int = 0,
) -> tuple[dict[str, object], list[dict[str, object]]]:
    submitted = client.post("/api/calibrations/single", json=payload)
    submitted.raise_for_status()
    return _wait_for_terminal(
        client,
        submitted.headers["location"],
        health_samples=health_samples,
    )


def _run_perf_30(client: TestClient) -> dict[str, object]:
    payload = _fixture(30, matrices=False)
    warmup, _ = _solve(client, payload)
    if warmup["status"] != "completed":
        raise RuntimeError(f"FIX-PERF-30 warmup failed: {warmup}")

    observations: list[dict[str, object]] = []
    native: list[float] = []
    trial_maxima: list[float] = []
    for trial in range(PERF_30_TRIALS):
        terminal, health = _solve(
            client,
            payload,
            health_samples=HEALTH_SAMPLES,
        )
        if terminal["status"] != "completed":
            raise RuntimeError(f"FIX-PERF-30 trial failed: {terminal}")
        value = float(terminal["timings"]["native_solve_ms"])
        native.append(value)
        for observation in health:
            observation["trial"] = trial
        observations.extend(health)
        trial_maxima.append(max(item["latency_ms"] for item in health))

    median_of_maxima = statistics.median(trial_maxima)
    if median_of_maxima > HEALTH_LIMIT_MS:
        raise RuntimeError(
            f"PERF-01 failed: median-of-maxima {median_of_maxima:.6f} ms "
            f"> {HEALTH_LIMIT_MS:.6f} ms"
        )
    return _environment("FIX-PERF-30", payload) | {
        "native_solve_ms": native,
        "serialization_ms": [],
        "matrix_materialization_ms": [],
        "peak_python_allocation_bytes": 0,
        "health_observations": observations,
        "trial_max_ms": trial_maxima,
        "median_trial_max_ms": median_of_maxima,
        "health_limit_ms": HEALTH_LIMIT_MS,
        "gate_status": "passed",
    }


def _run_perf_100(client: TestClient) -> dict[str, object]:
    from app.schemas.calibrations import CalibrationTimingsDTO
    from app.services import calibrations, dal_gateway

    payload = _fixture(100, matrices=True)
    warmup, _ = _solve(client, payload)
    if warmup["status"] != "completed":
        raise RuntimeError(f"FIX-PERF-100X100 warmup failed: {warmup}")

    materialization_total = [0.0]
    materialization_peak = [0]
    original_matrix_dto = dal_gateway._native_matrix_dto

    def measured_matrix_dto(*args, **kwargs):
        available = bool(kwargs.get("available"))
        if not available:
            return original_matrix_dto(*args, **kwargs)
        tracemalloc.start()
        started = time.perf_counter()
        try:
            return original_matrix_dto(*args, **kwargs)
        finally:
            materialization_total[0] += (time.perf_counter() - started) * 1_000.0
            _, peak = tracemalloc.get_traced_memory()
            materialization_peak[0] = max(materialization_peak[0], peak)
            tracemalloc.stop()

    dal_gateway._native_matrix_dto = measured_matrix_dto
    native: list[float] = []
    serialization: list[float] = []
    materialization: list[float] = []
    gc_was_enabled = gc.isenabled()
    gc.disable()
    try:
        for _ in range(PERF_100_TRIALS):
            materialization_total[0] = 0.0
            terminal, _ = _solve(client, payload)
            if terminal["status"] != "completed":
                raise RuntimeError(f"FIX-PERF-100X100 trial failed: {terminal}")
            native.append(float(terminal["timings"]["native_solve_ms"]))
            materialization.append(materialization_total[0])
            immutable = calibrations.RUN_RESPONSE_ADAPTER.validate_python(terminal)
            candidate = immutable.model_copy(
                update={
                    "timings": CalibrationTimingsDTO(
                        native_solve_ms=None,
                        serialization_ms=None,
                    )
                }
            )
            started = time.perf_counter()
            calibrations.RUN_RESPONSE_ADAPTER.dump_json(candidate)
            serialization.append((time.perf_counter() - started) * 1_000.0)
    finally:
        dal_gateway._native_matrix_dto = original_matrix_dto
        if gc_was_enabled:
            gc.enable()

    median_native = statistics.median(native)
    median_serialization = statistics.median(serialization)
    ratio = median_serialization / median_native
    if ratio > SERIALIZATION_RATIO_LIMIT:
        raise RuntimeError(
            f"PERF-02 failed: serialization/native ratio {ratio:.9f} "
            f"> {SERIALIZATION_RATIO_LIMIT:.9f}"
        )
    return _environment("FIX-PERF-100X100", payload) | {
        "native_solve_ms": native,
        "serialization_ms": serialization,
        "matrix_materialization_ms": materialization,
        "peak_python_allocation_bytes": materialization_peak[0],
        "matrix_shapes": {
            "jacobian": [100, 100],
            "effective_inverse": [100, 100],
        },
        "median_native_solve_ms": median_native,
        "median_serialization_ms": median_serialization,
        "serialization_ratio": ratio,
        "serialization_ratio_limit": SERIALIZATION_RATIO_LIMIT,
        "gate_status": "passed",
    }


def _finite_samples(report: dict[str, object], field: str, count: int) -> None:
    values = report.get(field)
    if not isinstance(values, list) or len(values) != count:
        raise ValueError(f"{field} must contain exactly {count} samples")
    if any(
        not isinstance(value, (int, float))
        or not math.isfinite(value)
        or value < 0.0
        for value in values
    ):
        raise ValueError(f"{field} contains an invalid timing")


def validate_reports(report_dir: Path) -> None:
    perf_30 = json.loads((report_dir / "fix-perf-30.json").read_text())
    perf_100 = json.loads((report_dir / "fix-perf-100x100.json").read_text())
    if (
        perf_30.get("gate_status") != "passed"
        or perf_100.get("gate_status") != "passed"
    ):
        raise ValueError("both marked-runner reports must pass")
    _finite_samples(perf_30, "native_solve_ms", PERF_30_TRIALS)
    _finite_samples(perf_30, "trial_max_ms", PERF_30_TRIALS)
    health = perf_30.get("health_observations")
    if not isinstance(health, list) or len(health) != PERF_30_TRIALS * HEALTH_SAMPLES:
        raise ValueError("FIX-PERF-30 must contain the raw 3x5 health matrix")
    if statistics.median(perf_30["trial_max_ms"]) > HEALTH_LIMIT_MS:
        raise ValueError("PERF-01 threshold failed")
    for field in (
        "native_solve_ms",
        "serialization_ms",
        "matrix_materialization_ms",
    ):
        _finite_samples(perf_100, field, PERF_100_TRIALS)
    ratio = statistics.median(perf_100["serialization_ms"]) / statistics.median(
        perf_100["native_solve_ms"]
    )
    if ratio > SERIALIZATION_RATIO_LIMIT:
        raise ValueError("PERF-02 threshold failed")
    if not isinstance(perf_100.get("peak_python_allocation_bytes"), int):
        raise ValueError("peak_python_allocation_bytes must be an integer")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--validate-dir", type=Path)
    args = parser.parse_args()
    if args.validate_dir is not None:
        validate_reports(args.validate_dir)
        print("marked Release/native performance reports validated")
        return 0
    if os.environ.get("DAL_WEB_PERFORMANCE_RUNNER") != "1":
        raise SystemExit(
            "refusing measured reports: DAL_WEB_PERFORMANCE_RUNNER=1 is required"
        )
    output_dir = args.output_dir
    if output_dir is None:
        raise SystemExit("--output-dir is required for measured reports")
    output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="dal-web-performance-") as directory:
        with _reset_application(Path(directory) / "performance.db") as client:
            reports = {
                "fix-perf-30.json": _run_perf_30(client),
                "fix-perf-100x100.json": _run_perf_100(client),
            }
    for name, report in reports.items():
        (output_dir / name).write_text(
            json.dumps(report, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
    validate_reports(output_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
