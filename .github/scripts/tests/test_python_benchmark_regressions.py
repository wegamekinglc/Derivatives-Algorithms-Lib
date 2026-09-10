"""Fail-closed contracts for the Python-interface paired performance gate."""

import copy
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))
SPEC = importlib.util.spec_from_file_location(
    "python_gate", SCRIPTS / "check_python_benchmark_regressions.py"
)
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)


def report(duration=100):
    return {
        "schema": "dal.python-benchmarks/1",
        "profile": "full",
        "status": "passed",
        "samples": 1,
        "warmups": 2,
        "environment": {
            "suite_sha256": {"cases.py": "suite"},
            "native_sha256": "binary",
            "python": "3.13",
            "thread_environment": {"DAL_NUM_THREADS": "4"},
        },
        "results": [
            {
                "name": "case",
                "cpp_target": "rng_perf",
                "workload": {"profile": "full", "paths": 100000},
                "status": "passed",
                "samples_ns": [duration],
                "min_ns": duration,
            }
        ],
    }


class PythonBenchmarkRegressionTest(unittest.TestCase):
    def test_threshold_requires_two_independent_best_of_ten_failures(self):
        base = [report()] * 20
        head = [report(105)] * 20
        rows, failures = GATE.evaluate(base, head, 10, 2, 4)
        self.assertFalse(rows[0]["passed"])
        self.assertEqual(len(failures), 1)
        head[-1] = report(100)
        rows, failures = GATE.evaluate(base, head, 10, 2, 4)
        self.assertTrue(rows[0]["passed"])
        self.assertEqual(failures, [])

    def test_minimum_not_median_and_strict_threshold(self):
        base = [report()] * 20
        head = [report(100), report(150)] * 10
        self.assertEqual(GATE.evaluate(base, head, 10, 2, 4)[1], [])
        rows, failures = GATE.evaluate(base, [report(104)] * 20, 10, 2, 4)
        self.assertEqual(failures, [])
        self.assertEqual(rows[0]["round_delta_percent"], [4.0, 4.0])

    def test_rejects_missing_processes_cases_and_mismatched_workloads(self):
        with self.assertRaisesRegex(ValueError, "process samples"):
            GATE.evaluate([report()] * 19, [report()] * 20, 10, 2, 4)
        for change in (
            lambda r: r["results"].clear(),
            lambda r: r["results"][0]["workload"].update(paths=1),
            lambda r: r["results"][0].update(name="renamed"),
            lambda r: r["environment"]["suite_sha256"].update(other="changed"),
        ):
            head = [report() for _ in range(20)]
            change(head[-1])
            with self.assertRaises(ValueError):
                GATE.evaluate([report()] * 20, head, 10, 2, 4)

    def test_rejects_smoke_failed_nonfinite_duplicate_and_partial_rows(self):
        changes = [
            lambda r: r.update(profile="smoke"),
            lambda r: r.update(status="running"),
            lambda r: r["results"][0].update(status="failed"),
            lambda r: r["results"][0].update(samples_ns=[float("nan")]),
            lambda r: r["results"][0].update(samples_ns=[0]),
            lambda r: r["results"][0].update(samples_ns=[True]),
            lambda r: r["results"].append(copy.deepcopy(r["results"][0])),
            lambda r: r["results"][0].update(samples_ns=[100, 100]),
            lambda r: r["results"][0].update(min_ns=50),
        ]
        for change in changes:
            invalid = report()
            change(invalid)
            with self.subTest(change=change), self.assertRaises(ValueError):
                GATE.validate_report(invalid)

    def test_rejects_native_binary_and_thread_drift_between_samples(self):
        for key, value in (
            ("native_sha256", "changed"),
            ("thread_environment", {"DAL_NUM_THREADS": "8"}),
        ):
            head = [report() for _ in range(20)]
            head[-1]["environment"][key] = value
            with self.assertRaises(ValueError):
                GATE.evaluate([report()] * 20, head, 10, 2, 4)

    def test_baseline_inventory_rejects_removed_case_but_allows_new_cases(self):
        base = [{"name": "old"}]
        self.assertEqual(
            GATE.removed_cases(base, [{"name": "old"}, {"name": "new"}]), []
        )
        self.assertEqual(GATE.removed_cases(base, [{"name": "new"}]), ["old"])

    def test_collect_interleaves_processes_and_uses_head_suite_for_both_sides(self):
        calls = []

        def run(suite, package, output, inventory=False):
            calls.append((suite, package, output))
            return report()

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with mock.patch.object(GATE, "run_worker", side_effect=run):
                collected = GATE.collect(
                    root / "suite",
                    {"base": root / "base", "head": root / "head"},
                    4,
                    root,
                )
        self.assertEqual(
            [call[1].name for call in calls],
            ["base", "head", "head", "base", "base", "head", "head", "base"],
        )
        self.assertTrue(all(call[0].name == "suite" for call in calls))
        self.assertEqual(len(collected["base"]), 4)

    def test_worker_timeout_and_nonzero_exit_fail_and_retain_logs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            failure = mock.Mock(
                returncode=1, stdout="partial output", stderr="bad native call"
            )
            with mock.patch.object(GATE.subprocess, "run", return_value=failure):
                with self.assertRaisesRegex(RuntimeError, "exited"):
                    GATE.run_worker(root, root, root / "failed")
            self.assertIn("bad native call", (root / "failed/worker.log").read_text())
            with mock.patch.object(
                GATE.subprocess,
                "run",
                side_effect=GATE.subprocess.TimeoutExpired("worker", 600),
            ):
                with self.assertRaisesRegex(RuntimeError, "timed out"):
                    GATE.run_worker(root, root, root / "timeout")
            self.assertIn("timed out", (root / "timeout/worker.log").read_text())

    def test_worker_cannot_reuse_a_stale_inventory_after_empty_success(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "inventory.json").write_text('[{"name": "stale"}]')
            completed = mock.Mock(returncode=0, stdout="", stderr="")
            with (
                mock.patch.object(GATE.subprocess, "run", return_value=completed),
                self.assertRaises(FileNotFoundError),
            ):
                GATE.run_worker(root, root, root, inventory=True)

    def test_first_introduction_compares_all_cases_and_propagates_regression_exit(self):
        for head_duration, expected_exit in ((100, 0), (105, 1)):
            calls = []

            def worker(suite, package, output, inventory=False):
                calls.append((suite, package, inventory))
                if inventory:
                    row = report()["results"][0]
                    return [
                        {key: row[key] for key in ("name", "cpp_target", "workload")}
                    ]
                return report(
                    head_duration if package.parent.name == "head-build" else 100
                )

            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                argv = [
                    "--base-root",
                    str(root / "base-build"),
                    "--head-root",
                    str(root / "head-build"),
                    "--base-source",
                    str(root / "base-source"),
                    "--head-source",
                    str(root / "head-source"),
                    "--output-dir",
                    str(root / "results"),
                ]
                with (
                    mock.patch.object(GATE, "build_configuration", return_value={}),
                    mock.patch.object(GATE, "source_sha", return_value="commit"),
                    mock.patch.object(GATE, "run_worker", side_effect=worker),
                    contextlib.redirect_stdout(io.StringIO()),
                    contextlib.redirect_stderr(io.StringIO()),
                ):
                    self.assertEqual(GATE.main(argv), expected_exit)
                result = json.loads((root / "results/results.json").read_text())
                self.assertEqual(
                    result["status"], "passed" if expected_exit == 0 else "failed"
                )
                self.assertFalse(result["baseline_has_suite"])
                self.assertEqual(len(result["comparisons"]), 1)
                self.assertEqual(len(result["raw_samples_ns"]["base"]["case"]), 20)
                self.assertEqual(
                    len(calls), 41
                )  # one inventory plus 20 base/head pairs
                self.assertTrue(
                    all(
                        call[0] == root / "head-source/dal-python/benchmarks"
                        for call in calls
                    )
                )

    def test_invalid_cli_sampling_and_thresholds_are_rejected(self):
        required = [
            "--base-root",
            "base",
            "--head-root",
            "head",
            "--base-source",
            "source",
            "--output-dir",
            "output",
        ]
        for extra in (
            ["--samples", "9"],
            ["--confirmation-rounds", "1"],
            ["--threshold-percent", "nan"],
            ["--threshold-percent", "-1"],
        ):
            with (
                contextlib.redirect_stderr(io.StringIO()),
                self.assertRaises(SystemExit) as error,
            ):
                GATE.arguments(required + extra)
            self.assertEqual(error.exception.code, 2)

    def test_build_contract_rejects_debug_or_wrong_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "CMakeCache.txt"
            path.write_text(
                f"CMAKE_BUILD_TYPE:STRING=Release\nDAL_BUILD_PYTHON:BOOL=ON\nCMAKE_HOME_DIRECTORY:INTERNAL={root}\n"
            )
            GATE.build_configuration(root, root)
            with self.assertRaisesRegex(ValueError, "build/source mismatch"):
                GATE.build_configuration(root, root / "different")
            path.write_text(path.read_text().replace("Release", "Debug"))
            with self.assertRaisesRegex(ValueError, "Release"):
                GATE.build_configuration(root, root)

    def test_linux_required_benchmark_job_runs_python_gate_with_native_policy(self):
        workflow = (SCRIPTS.parent / "workflows/cmake-linux.yml").read_text()
        job = workflow.split("  benchmark:\n", 1)[1].split("  linux-gate:\n", 1)[0]
        self.assertIn("check_python_benchmark_regressions.py", job)
        self.assertEqual(job.count("-DDAL_BUILD_PYTHON=ON"), 2)
        self.assertEqual(job.count('-DPython3_EXECUTABLE="$(command -v python3)"'), 2)
        gate = job.split("- name: Compare base and head Python performance", 1)[
            1
        ].split("- name:", 1)[0]
        for flag in (
            "--samples 10",
            "--confirmation-rounds 2",
            "--threshold-percent 4",
            "--output-dir benchmark-results/python-paired",
        ):
            self.assertIn(flag, gate)
        self.assertNotIn("continue-on-error", gate)
        self.assertIn("- benchmark", workflow.split("  linux-gate:\n", 1)[1])
        self.assertIn(
            "if: always()", job.split("- name: Upload benchmark evidence", 1)[1]
        )


if __name__ == "__main__":
    unittest.main()
