"""Tests for the paired DAL benchmark regression gate."""

import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock
from subprocess import TimeoutExpired


SCRIPT = Path(__file__).resolve().parents[1] / "check_benchmark_regressions.py"
SPEC = importlib.util.spec_from_file_location("check_benchmark_regressions", SCRIPT)
BENCHMARKS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BENCHMARKS)


class BenchmarkRegressionTest(unittest.TestCase):
    @staticmethod
    def _create_benchmark_binary(build_root, benchmark="pde_perf", mode=0o755):
        binary = build_root / "dal-cpp" / "benchmarks" / benchmark / benchmark
        binary.parent.mkdir(parents=True)
        binary.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
        binary.chmod(mode)
        return binary

    def test_parse_benchmark_output_converts_minimum_to_nanoseconds(self):
        output = """Benchmark                  Median       Min       Max  Reps
Case in milliseconds       1.100 ms  900.000 us  1.300 ms    10
Case in nanoseconds      120.000 ns  100.000 ns  130.000 ns    10
"""

        parsed = BENCHMARKS.parse_benchmark_output(output)

        self.assertEqual(parsed["Case in milliseconds"], 900_000.0)
        self.assertEqual(parsed["Case in nanoseconds"], 100.0)

    def test_compare_benchmark_applies_regression_threshold(self):
        samples = {
            "base": {"case": [100.0] * 20},
            "head": {"case": [104.1] * 20},
        }

        rows, failures = BENCHMARKS.compare_benchmark("pde_perf", samples, 4.0, 10.0, 10, 2)

        self.assertAlmostEqual(rows[0]["delta_percent"], 4.1)
        self.assertEqual(len(failures), 1)

    def test_compare_benchmark_requires_confirmation_round(self):
        samples = {
            "base": {"case": [100.0] * 20},
            "head": {"case": [104.1] * 10 + [100.0] * 10},
        }

        rows, failures = BENCHMARKS.compare_benchmark("pde_perf", samples, 4.0, 10.0, 10, 2)

        self.assertAlmostEqual(rows[0]["round_delta_percent"][0], 4.1)
        self.assertEqual(rows[0]["round_delta_percent"][1], 0.0)
        self.assertEqual(failures, [])

    def test_compare_benchmark_reduces_each_round_on_minimum(self):
        # Project policy (ci-benchmark-noise-floor) gates on best-of-N (min) because
        # benchmark timings are right-skewed; verify the reducer is min, not median.
        # head min is 100.0 but head median is 102.05, so this distinguishes them.
        samples = {
            "base": {"case": [100.0] * 20},
            "head": {"case": [104.1, 100.0] * 10},
        }

        rows, failures = BENCHMARKS.compare_benchmark("pde_perf", samples, 4.0, 10.0, 10, 2)

        self.assertAlmostEqual(rows[0]["base_ns"], 100.0)
        self.assertAlmostEqual(rows[0]["head_ns"], 100.0)
        self.assertEqual(rows[0]["round_delta_percent"], [0.0, 0.0])
        self.assertEqual(failures, [])

    @staticmethod
    def _generic_joint_samples():
        samples = {}
        for width in (5, 10, 16):
            for trades in (100, 1000):
                suffix = f" ({trades} IRS x N={width})"
                samples["Quote risk generic joint" + suffix] = [110.0] * 20
                samples["Quote risk generic joint node reference" + suffix] = [100.0] * 20
        return samples

    def test_generic_joint_overhead_does_not_fail_on_one_noisy_process(self):
        head = self._generic_joint_samples()
        head["Quote risk generic joint (100 IRS x N=5)"][12] = 120.978

        rows, failures = BENCHMARKS.generic_joint_overhead_checks(head, 10, 2)

        self.assertEqual(len(rows), 6)
        self.assertEqual(failures, [])
        self.assertTrue(all(row["passed"] for row in rows))

    def test_generic_joint_overhead_rejects_sustained_excess_in_both_rounds(self):
        head = self._generic_joint_samples()
        case = "Quote risk generic joint (100 IRS x N=5)"
        head[case] = [121.0] * 20

        rows, failures = BENCHMARKS.generic_joint_overhead_checks(head, 10, 2)

        self.assertEqual(len(failures), 1)
        self.assertIn(case, failures[0])
        self.assertIn("20", failures[0])
        self.assertEqual(sum(not row["passed"] for row in rows), 1)

    def test_generic_joint_overhead_requires_both_rounds_and_accepts_boundary(self):
        head = self._generic_joint_samples()
        head["Quote risk generic joint (100 IRS x N=5)"] = [121.0] * 10 + [119.0] * 10
        head["Quote risk generic joint (1000 IRS x N=5)"] = [120.0] * 20

        _, failures = BENCHMARKS.generic_joint_overhead_checks(head, 10, 2)

        self.assertEqual(failures, [])

    def test_generic_joint_overhead_rejects_missing_reference_and_incomplete_samples(self):
        for damage in ("missing", "partial", "zero", "negative", "nan", "infinity"):
            with self.subTest(damage=damage):
                head = self._generic_joint_samples()
                reference = "Quote risk generic joint node reference (100 IRS x N=5)"
                if damage == "missing":
                    del head[reference]
                elif damage == "partial":
                    head[reference].pop()
                elif damage == "zero":
                    head[reference][0] = 0.0
                elif damage == "negative":
                    head[reference][0] = -1.0
                else:
                    head[reference][0] = float("nan" if damage == "nan" else "inf")

                _, failures = BENCHMARKS.generic_joint_overhead_checks(head, 10, 2)

                self.assertEqual(len(failures), 1)
                self.assertIn(reference, failures[0])

    def test_generic_joint_overhead_gates_new_cases_without_a_base_binary(self):
        head = self._generic_joint_samples()
        head["Quote risk generic joint (100 IRS x N=5)"] = [121.0] * 20

        _, failures = BENCHMARKS.compare_benchmark(
            "rate_risk_perf", {"base": {}, "head": head}, 4.0, 10.0, 10, 2
        )

        self.assertEqual(len(failures), 1)

    def test_generic_joint_overhead_report_identifies_same_head_reference(self):
        rows, _ = BENCHMARKS.generic_joint_overhead_checks(self._generic_joint_samples(), 10, 2)

        report = BENCHMARKS.markdown_report({}, [], 10, 2, 4.0, None, 10.0, rows)

        self.assertIn("Head generic joint overhead", report)
        self.assertIn("Head node reference", report)
        self.assertIn("20.00%", report)

    def test_generic_joint_overhead_failure_reaches_cli_status_and_saved_evidence(self):
        head = self._generic_joint_samples()
        head["Quote risk generic joint (100 IRS x N=5)"] = [121.0] * 20
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            args = mock.Mock(
                output_dir=root / "results", samples=10, confirmation_rounds=2,
                threshold_percent=4.0, precise_slowdown_limit=10.0,
                base_root=root / "base", head_root=root / "head",
                benchmarks=["rate_risk_perf"], summary_file=root / "step-summary.md",
            )
            with mock.patch.object(BENCHMARKS, "parse_args", return_value=args), \
                    mock.patch.object(BENCHMARKS, "collect_samples", return_value={
                        "rate_risk_perf": {"base": head, "head": head},
                    }), mock.patch("sys.stdout", new_callable=io.StringIO):
                status = BENCHMARKS.main()

            self.assertEqual(status, 1)
            results = json.loads((args.output_dir / "results.json").read_text(encoding="utf-8"))
            self.assertEqual(len(results["generic_joint_overhead"]), 6)
            self.assertEqual(len(results["failures"]), 1)
            self.assertEqual(results["samples"]["rate_risk_perf"]["head"], head)
            report = (args.output_dir / "summary.md").read_text(encoding="utf-8")
            self.assertIn("+21.00%, +21.00%", report)
            self.assertEqual(args.summary_file.read_text(encoding="utf-8"), report)

    def test_validate_sample_counts_rejects_partial_case(self):
        sides = {
            "base": {"case": [100.0] * 9},
            "head": {"case": [100.0] * 10},
        }

        with self.assertRaises(RuntimeError):
            BENCHMARKS.validate_sample_counts("pde_perf", sides, 10)

    def test_benchmark_binary_rejects_name_outside_allowlist(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_root = Path(temporary_directory)

            with self.assertRaisesRegex(ValueError, "unsupported benchmark"):
                BENCHMARKS.benchmark_binary(build_root, "../../malicious")

    def test_benchmark_binary_rejects_path_escape(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary_root = Path(temporary_directory)
            build_root = temporary_root / "build"
            outside_binary = temporary_root / "outside"
            outside_binary.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            outside_binary.chmod(0o755)
            expected_binary = build_root / "dal-cpp" / "benchmarks" / "pde_perf" / "pde_perf"
            expected_binary.parent.mkdir(parents=True)
            try:
                expected_binary.symlink_to(outside_binary)
            except OSError as error:
                self.skipTest(f"symbolic links are unavailable: {error}")

            with self.assertRaisesRegex(ValueError, "escapes build root"):
                BENCHMARKS.benchmark_binary(build_root, "pde_perf")

    @unittest.skipIf(os.name == "nt", "POSIX execute permission is not meaningful on Windows")
    def test_benchmark_binary_rejects_non_executable_file(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_root = Path(temporary_directory)
            self._create_benchmark_binary(build_root, mode=0o644)

            with self.assertRaisesRegex(PermissionError, "not executable"):
                BENCHMARKS.benchmark_binary(build_root, "pde_perf")

    def test_run_benchmark_uses_validated_path_without_a_shell(self):
        output = "Case 1.000 ms 900.000 us 1.100 ms 10\n"
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary_root = Path(temporary_directory)
            build_root = temporary_root / "build"
            binary = self._create_benchmark_binary(build_root)
            output_file = temporary_root / "output.txt"
            completed = mock.Mock(stdout=output, stderr="", returncode=0)

            with mock.patch.object(BENCHMARKS.subprocess, "run", return_value=completed) as run:
                values = BENCHMARKS.run_benchmark(build_root, "pde_perf", output_file)

            self.assertEqual(values, {"Case": 900_000.0})
            run.assert_called_once()
            self.assertEqual(run.call_args.args[0], [str(binary.resolve())])
            self.assertFalse(run.call_args.kwargs["shell"])
            self.assertEqual(run.call_args.kwargs["timeout"], BENCHMARKS.BENCHMARK_TIMEOUT_SECONDS)

    def test_run_benchmark_raises_and_writes_marker_on_timeout(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary_root = Path(temporary_directory)
            build_root = temporary_root / "build"
            self._create_benchmark_binary(build_root)
            output_file = temporary_root / "output.txt"
            # TimeoutExpired is an exception ctor raised via a mock, not a subprocess call.
            exc = TimeoutExpired(  # nosemgrep
                cmd=["x"], timeout=BENCHMARKS.BENCHMARK_TIMEOUT_SECONDS
            )

            with mock.patch.object(BENCHMARKS.subprocess, "run", side_effect=exc):
                with self.assertRaisesRegex(RuntimeError, "exceeded"):
                    BENCHMARKS.run_benchmark(build_root, "pde_perf", output_file)

            self.assertIn("timed out", output_file.read_text(encoding="utf-8"))

    def test_rng_allows_precise_case_rename_and_checks_relative_cost(self):
        samples = {
            "base": {
                BENCHMARKS.FAST_SOBOL_CASE: [100.0] * 20,
                BENCHMARKS.OLD_PRECISE_SOBOL_CASE: [100.0] * 20,
            },
            "head": {
                BENCHMARKS.FAST_SOBOL_CASE: [102.0] * 20,
                BENCHMARKS.PRECISE_SOBOL_CASE: [900.0] * 20,
            },
        }

        rows, failures = BENCHMARKS.compare_benchmark("rng_perf", samples, 4.0, 10.0, 10, 2)

        self.assertEqual(len(rows), 2)
        self.assertFalse(rows[0]["gated"])
        self.assertAlmostEqual(rows[0]["delta_percent"], 800.0)
        self.assertEqual(failures, [])

    def test_rng_rejects_excessive_precise_opt_in_cost(self):
        head = {
            BENCHMARKS.FAST_SOBOL_CASE: 100.0,
            BENCHMARKS.PRECISE_SOBOL_CASE: 1_001.0,
        }

        failures = BENCHMARKS.check_precise_sobol_ratio(head, 10.0)

        self.assertEqual(len(failures), 1)

    def test_benchmark_case_differences_tolerates_head_only_new_coverage(self):
        base = {"case": 100.0}
        head = {"case": 100.0, "new case": 200.0}

        _, failures, new_coverage = BENCHMARKS.benchmark_case_differences("tape_perf", base, head)

        self.assertEqual(failures, [])
        self.assertEqual(new_coverage, ["new case"])

    def test_benchmark_case_differences_rejects_base_only_case(self):
        base = {"case": 100.0, "dropped case": 200.0}
        head = {"case": 100.0}

        _, failures, _ = BENCHMARKS.benchmark_case_differences("tape_perf", base, head)

        self.assertEqual(len(failures), 1)
        self.assertIn("dropped case", failures[0])

    def test_benchmark_case_differences_accepts_identical_case_sets(self):
        base = {"case": 100.0}
        head = {"case": 101.0}

        _, failures, new_coverage = BENCHMARKS.benchmark_case_differences("tape_perf", base, head)

        self.assertEqual(failures, [])
        self.assertEqual(new_coverage, [])

    def test_benchmark_case_differences_reports_new_coverage_alongside_base_only_failure(self):
        base = {"case": 100.0, "dropped case": 200.0}
        head = {"case": 100.0, "new case": 300.0}

        _, failures, new_coverage = BENCHMARKS.benchmark_case_differences("tape_perf", base, head)

        self.assertEqual(len(failures), 1)
        self.assertEqual(new_coverage, ["new case"])

    def test_benchmark_case_differences_migration_is_not_reported_as_new_coverage(self):
        base = {BENCHMARKS.OLD_PRECISE_SOBOL_CASE: 100.0}
        head = {BENCHMARKS.PRECISE_SOBOL_CASE: 900.0}

        migration_permitted, failures, new_coverage = BENCHMARKS.benchmark_case_differences("rng_perf", base, head)

        self.assertTrue(migration_permitted)
        self.assertEqual(failures, [])
        self.assertEqual(new_coverage, [])

    def test_compare_benchmark_reports_head_only_case_as_ungated_info_row(self):
        samples = {
            "base": {"case": [100.0] * 20},
            "head": {"case": [100.0] * 20, "new case": [200.0] * 20},
        }

        rows, failures = BENCHMARKS.compare_benchmark("tape_perf", samples, 4.0, 10.0, 10, 2)

        self.assertEqual(failures, [])
        info_rows = [row for row in rows if not row["gated"]]
        self.assertEqual(len(info_rows), 1)
        self.assertIn("new coverage", info_rows[0]["case"])
        self.assertEqual(BENCHMARKS.result_label(info_rows[0]), "info")
        self.assertIsNone(info_rows[0]["base_ns"])
        self.assertEqual(info_rows[0]["head_ns"], 200.0)

    def test_markdown_report_renders_new_coverage_row_without_base_value(self):
        comparisons = {"tape_perf": [BENCHMARKS.new_coverage_row("new case", 200.0)]}

        report = BENCHMARKS.markdown_report(comparisons, [], 10, 2, 4.0, None, 10.0)

        self.assertIn("new case (new coverage)", report)
        self.assertIn("info", report)

    def test_compare_benchmark_marks_missing_base_binary_as_not_gated(self):
        samples = {
            "base": {},
            "head": {"case": [100.0] * 20},
        }

        rows, failures = BENCHMARKS.compare_benchmark("pde_perf", samples, 4.0, 10.0, 10, 2)

        self.assertEqual(failures, [])
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["kind"], "not_gated_no_base")
        self.assertIn("not gated: no base binary", rows[0]["case"])
        self.assertEqual(BENCHMARKS.result_label(rows[0]), "info")
        self.assertIsNone(rows[0]["base_ns"])
        self.assertEqual(rows[0]["head_ns"], 100.0)

    def test_markdown_report_names_ungated_benchmarks_without_a_base_binary(self):
        comparisons = {"pde_perf": [BENCHMARKS.not_gated_no_base_row("case", 100.0)]}

        report = BENCHMARKS.markdown_report(comparisons, [], 10, 2, 4.0, None, 10.0)
        ungated = BENCHMARKS.not_gated_no_base_benchmarks(comparisons)

        self.assertEqual(ungated, ["pde_perf"])
        self.assertIn("not gated: no base binary", report)
        self.assertIn("pde_perf", report.split("Rows marked (not gated: no base binary)")[1])


if __name__ == "__main__":
    unittest.main()
