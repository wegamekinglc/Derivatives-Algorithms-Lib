"""Correctness and evidence contracts for the opt-in performance suite."""

import json
from pathlib import Path
import re
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))

from dal_benchmarks.harness import Case, Workload, measure, select_cases
from dal_benchmarks.cases import CPP_COVERAGE, build_cases


def test_measure_keeps_setup_and_validation_outside_timing():
    events = []

    def prepare():
        events.append("prepare")
        return Workload(
            lambda: events.append("run") or 7, lambda value: events.append("validate")
        )

    ticks = iter((10, 20, 30, 60))

    def clock():
        events.append("clock")
        return next(ticks)

    case = Case("example", "rng_perf", {"paths": 10}, prepare)
    result = measure(case, samples=2, warmups=1, clock=clock)
    assert result["samples_ns"] == [10, 30]
    assert result["min_ns"] == 10
    assert result["median_ns"] == 20
    assert events == [
        "prepare",
        "run",
        "validate",
        "run",
        "validate",
        "clock",
        "run",
        "clock",
        "validate",
        "clock",
        "run",
        "clock",
        "validate",
    ]


def test_invalid_results_cannot_be_reported_as_timings():
    def reject(value):
        raise ValueError("incorrect output")

    case = Case("bad", "rng_perf", {}, lambda: Workload(lambda: 0, reject))
    with pytest.raises(ValueError, match="incorrect output"):
        measure(case, samples=1, warmups=0)


def test_invalid_measured_sample_cannot_follow_a_passing_preflight():
    outputs = iter([7, 0])

    def validate(value):
        if value != 7:
            raise ValueError("measured output changed")

    case = Case(
        "bad_sample", "rng_perf", {}, lambda: Workload(lambda: next(outputs), validate)
    )
    with pytest.raises(ValueError, match="measured output changed"):
        measure(case, samples=1, warmups=0)


def test_script_frontend_runs_inside_timing(monkeypatch):
    import dal
    from dal_benchmarks.simulation import prepare_script

    events = []
    original = dal.Product_DebugJson

    def dump(product):
        events.append("parse")
        return original(product)

    def clock():
        events.append("clock")
        return len(events)

    monkeypatch.setattr(dal, "Product_DebugJson", dump)
    case = Case("frontend", "script_perf", {}, prepare_script)
    measure(case, samples=1, warmups=0, clock=clock)
    assert events[-3:] == ["clock", "parse", "clock"]


def test_selection_rejects_empty_or_unknown_groups():
    case = Case("rng.normal", "rng_perf", {}, lambda: None)
    assert select_cases([case], ["rng_perf"], "normal") == [case]
    with pytest.raises(ValueError, match="Unknown"):
        select_cases([case], ["typo"], "")
    with pytest.raises(ValueError, match="No benchmark"):
        select_cases([case], [], "missing")


def test_cpp_target_inventory_has_no_unexplained_gaps():
    root = Path(__file__).resolve().parents[2]
    cmake = (root / "dal-cpp/benchmarks/CMakeLists.txt").read_text()
    targets = set(re.findall(r"\b\w+_perf\b", cmake))
    assert set(CPP_COVERAGE) == targets
    cases = build_cases(smoke=True)
    assert len({case.name for case in cases}) == len(cases)
    covered = {case.cpp_target for case in cases}
    assert covered == {
        name for name, entry in CPP_COVERAGE.items() if entry["status"] == "partial"
    }
    assert all(entry["detail"] for entry in CPP_COVERAGE.values())
    json.dumps([case.description() for case in cases], allow_nan=False)


@pytest.mark.parametrize("case", build_cases(smoke=True), ids=lambda case: case.name)
def test_smoke_workload_validates_through_python_api(case):
    # This verifies real native results without introducing timing thresholds in CI.
    result = measure(case, samples=1, warmups=0)
    assert result["min_ns"] > 0


def test_runner_retains_failures_and_restores_evaluation_date(tmp_path, monkeypatch):
    import dal
    from dal_benchmarks import runner

    original = dal.EvaluationDate_Get()

    def prepare():
        dal.EvaluationDate_Set(dal.Date_(2030, 1, 1))
        raise ValueError("invalid fixture")

    monkeypatch.setattr(
        runner, "build_cases", lambda smoke: [Case("bad", "rng_perf", {}, prepare)]
    )
    monkeypatch.setattr(runner, "environment", lambda: {})
    assert runner.main(["--smoke", "--output-dir", str(tmp_path)]) == 1
    report = json.loads((tmp_path / "results.json").read_text())
    assert report["status"] == "failed"
    assert "invalid fixture" in report["results"][0]["error"]
    assert "samples_ns" not in report["results"][0]
    assert dal.EvaluationDate_Get() == original


@pytest.mark.parametrize(
    "arguments",
    [
        ["--samples", "0"],
        ["--warmups", "-1"],
        ["--filter", "does-not-exist"],
        ["--group", "typo"],
    ],
)
def test_runner_rejects_invalid_requests(arguments):
    from dal_benchmarks.runner import main

    with pytest.raises(SystemExit) as caught:
        main(arguments)
    assert caught.value.code == 2


def test_report_round_trip_preserves_raw_samples_and_binary_provenance(tmp_path):
    from dal_benchmarks.runner import main

    assert (
        main(
            [
                "--smoke",
                "--filter",
                "sobol_uniform",
                "--samples",
                "2",
                "--output-dir",
                str(tmp_path),
            ]
        )
        == 0
    )
    report = json.loads((tmp_path / "results.json").read_text())
    assert report["status"] == "passed"
    assert len(report["results"]) == 1
    assert len(report["results"][0]["samples_ns"]) == 2
    assert report["results"][0]["workload"]["paths"] == 1024
    assert Path(report["environment"]["native_module"]).is_file()
    assert len(report["environment"]["native_sha256"]) == 64
    assert set(report["coverage"]) == set(CPP_COVERAGE)


def test_full_profile_preserves_native_path_and_portfolio_sizes():
    cases = {case.name: case.workload for case in build_cases()}
    assert cases["rng.sobol_normal_fast"]["paths"] == 100_000
    assert cases["mc.vanilla.double.tree"]["paths"] == 200_000
    assert cases["mc.barrier.aad.compiled"]["paths"] == 10_000
    assert cases["calibration.PWC.BUMPED.diagnostics"]["swaps"] == 23
    assert cases["nodes.batch"]["trades"] == 120
    assert cases["nodes.xccy"]["trades"] == 24
    for width in (5, 10, 16):
        assert cases[f"quotes.generic.n{width}.t1000"]["trades"] == 1000
        assert cases[f"quotes.generic.n{width}.t1000"]["quotes"] == width
