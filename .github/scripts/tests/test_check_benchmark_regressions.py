"""Tests for the paired DAL benchmark regression gate."""

import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock


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
            exc = subprocess.TimeoutExpired(
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


if __name__ == "__main__":
    unittest.main()
