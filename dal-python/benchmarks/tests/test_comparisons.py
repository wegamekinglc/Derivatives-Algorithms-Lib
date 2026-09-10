"""Financial parity and fail-closed checks for the optional comparison suite."""

import copy
import json
import math
from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from dal_comparisons.scenarios import cases, expected, validate
from dal_comparisons.suite import prepare
from dal_comparisons import compare
from dal_comparisons.evidence import SCHEMA, check_report, source_hashes
from dal_comparisons.scenarios import CONVENTIONS


@pytest.mark.parametrize("backend", ["dal", "quantlib", "rateslib"])
@pytest.mark.parametrize("case", cases(smoke=True), ids=lambda case: case["name"])
def test_backends_match_independent_cashflows(backend, case):
    work = prepare(backend, case)
    first = work.run()
    work.validate(first)
    work.validate(work.run())


def test_payer_receiver_and_central_difference_signs():
    case = {"name": "oracle", "operation": "pv", "size": 2}
    values = expected(case)
    assert values[0] > 0
    assert values[1] < 0
    case["operation"] = "dv01"
    values = expected(case)
    assert values[0] > 0
    assert values[1] < 0


@pytest.mark.parametrize("bad", [[], [float("nan")], [float("inf")], [1e9]])
def test_invalid_financial_outputs_fail(bad):
    with pytest.raises(ValueError):
        validate(bad, [12.0])


def test_changed_cashflow_is_rejected():
    case = cases(smoke=True)[1]
    values = expected(case)
    values[-1] += 0.01
    with pytest.raises(ValueError, match="mismatch"):
        validate(values, expected(case))


def test_smoke_preserves_case_inventory_but_reduces_sizes():
    full, smoke = cases(), cases(smoke=True)
    assert [case["name"] for case in full] == [case["name"] for case in smoke]
    assert all(a["size"] > b["size"] for a, b in zip(full, smoke))
    assert all(math.isfinite(value) for case in smoke for value in expected(case))


def worker_report(backend="dal", duration=100):
    return {
        "schema": SCHEMA,
        "backend": backend,
        "status": "passed",
        "smoke": True,
        "source_hashes": source_hashes(),
        "conventions": copy.deepcopy(CONVENTIONS),
        "versions": {"version": "fixed"},
        "backend_files": {"library": backend},
        "environment": {
            "native_sha256": "native",
            "cmake": {},
            "python": "3.13",
            "thread_environment": {},
        },
        "results": [
            dict(
                name=case["name"],
                workload=case,
                status="passed",
                samples_ns=[duration],
                values=expected(case),
            )
            for case in cases(smoke=True)
        ],
    }


@pytest.mark.parametrize(
    "mutate",
    [
        lambda report: report.update(backend="wrong"),
        lambda report: report.update(status="running"),
        lambda report: report.update(smoke=False),
        lambda report: report["source_hashes"].clear(),
        lambda report: report["conventions"].update(day_count="ACT/360"),
        lambda report: report["results"].pop(),
        lambda report: report["results"].append(report["results"][0]),
        lambda report: report["results"][0]["workload"].update(size=1),
        lambda report: report["results"][0].update(status="failed"),
        lambda report: report["results"][0].update(samples_ns=[True]),
        lambda report: report["results"][0].update(samples_ns=[float("nan")]),
        lambda report: report["results"][0].update(samples_ns=[0]),
        lambda report: report["results"][0].update(samples_ns=[100, 100]),
        lambda report: report["results"][0].update(values=[1e9] * 4),
    ],
)
def test_invalid_worker_evidence_fails(mutate):
    report = worker_report()
    mutate(report)
    with pytest.raises(ValueError):
        check_report(report, "dal", True, source_hashes())


def test_full_mode_rejects_smoke_sized_sample_count():
    with pytest.raises(ValueError, match="10 samples"):
        compare.arguments(
            ["--dal-package", ".", "--output-dir", "unused", "--samples", "1"]
        )


def smoke_args(tmp_path):
    return compare.arguments(
        [
            "--dal-package",
            str(tmp_path),
            "--output-dir",
            str(tmp_path / "reports"),
            "--smoke",
        ]
    )


@pytest.mark.parametrize(
    "failure",
    [
        FileNotFoundError("missing dependency"),
        compare.subprocess.TimeoutExpired("worker", 1),
        compare.subprocess.CalledProcessError(1, "worker"),
    ],
)
def test_failed_execution_returns_nonzero_and_retains_failure(
    tmp_path, monkeypatch, failure
):
    def fail(*_args):
        raise failure

    monkeypatch.setattr(compare, "invoke", fail)
    args = smoke_args(tmp_path)
    assert compare.run(args) == 1
    report = json.loads((args.output_dir / "results.json").read_text())
    assert report["status"] == "failed"
    assert type(failure).__name__ in report["error"]
    assert (args.output_dir / "summary.md").is_file()


def test_worker_cannot_reuse_old_output_or_succeed_without_report(
    tmp_path, monkeypatch
):
    monkeypatch.setattr(compare.subprocess, "run", lambda *_args, **_kwargs: None)
    args = smoke_args(tmp_path)
    directory = tmp_path / "worker"
    with pytest.raises(FileNotFoundError):
        compare.invoke(args, "dal", directory, source_hashes())
    (directory / "worker.json").write_text(json.dumps(worker_report()))
    with pytest.raises(FileExistsError):
        compare.invoke(args, "dal", directory, source_hashes())


def test_rotates_processes_and_reports_minimum_without_relative_speed_gate(
    tmp_path, monkeypatch
):
    calls = []

    def invoke(_args, backend, _directory, _hashes):
        calls.append(backend)
        # DAL deliberately slower: relative speed is evidence, not an invented gate.
        duration = {"dal": 200, "quantlib": 100, "rateslib": 50}[backend]
        return worker_report(backend, duration)

    monkeypatch.setattr(compare, "invoke", invoke)
    args = smoke_args(tmp_path)
    args.samples, args.rounds = 3, 2
    assert compare.run(args) == 0
    assert calls[:9] == [
        "dal",
        "quantlib",
        "rateslib",
        "quantlib",
        "rateslib",
        "dal",
        "rateslib",
        "dal",
        "quantlib",
    ]
    assert calls[9:12] == ["rateslib", "quantlib", "dal"]
    report = json.loads((args.output_dir / "results.json").read_text())
    row = report["rounds"][0]["cases"][0]
    assert row["third_party_over_dal"] == {"quantlib": 0.5, "rateslib": 0.25}


def test_provenance_drift_fails_between_samples(tmp_path, monkeypatch):
    calls = []

    def invoke(_args, backend, _directory, _hashes):
        result = worker_report(backend)
        if backend in calls:
            result["versions"] = {"version": "changed"}
        calls.append(backend)
        return result

    monkeypatch.setattr(compare, "invoke", invoke)
    args = smoke_args(tmp_path)
    args.samples = 2
    assert compare.run(args) == 1
    assert "provenance changed" in (args.output_dir / "results.json").read_text()
