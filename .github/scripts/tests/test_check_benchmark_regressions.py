"""Tests for the paired DAL benchmark regression gate."""

import importlib.util
from pathlib import Path
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "check_benchmark_regressions.py"
SPEC = importlib.util.spec_from_file_location("check_benchmark_regressions", SCRIPT)
BENCHMARKS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BENCHMARKS)


class BenchmarkRegressionTest(unittest.TestCase):
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

    def test_validate_sample_counts_rejects_partial_case(self):
        sides = {
            "base": {"case": [100.0] * 9},
            "head": {"case": [100.0] * 10},
        }

        with self.assertRaises(RuntimeError):
            BENCHMARKS.validate_sample_counts("pde_perf", sides, 10)

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
