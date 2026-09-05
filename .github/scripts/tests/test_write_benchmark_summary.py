"""Tests for the aggregated benchmark step-summary table writer."""

import importlib.util
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "write_benchmark_summary.py"
SPEC = importlib.util.spec_from_file_location("write_benchmark_summary", SCRIPT)
SUMMARY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SUMMARY)


TIMED_OUTPUT = """\
starting DAL with: 4 threads.
Benchmark                                                                             Median               Min               Max      Reps
-------------------------------------------------------------------------------------------------------------------------------------------
CholeskyDecompose (200x200)                                                        420.821 us        395.497 us        602.254 us        20
Fast case                                                                              120 ns           100 ns           130 ns       1000
"""

PER_ITER_OUTPUT = """\
preprocess (stage 1)               392.175 us/iter
parse (stage 2)                    399.595 us/iter
"""

OBSERVATION_OUTPUT = """\
Quote risk aggregate (single curve)                                                  8.505 us          8.322 us          8.954 us        10
Quote risk aggregate (single curve) observations: passive=1 preparations=1 sweeps=1 tape_high_water=11
"""


class ParseRowsTest(unittest.TestCase):
    def test_parses_timed_rows_and_skips_noise(self):
        rows = SUMMARY.parse_rows(TIMED_OUTPUT)

        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[0]["case"], "CholeskyDecompose (200x200)")
        self.assertEqual(rows[0]["median"], "420.821 us")
        self.assertEqual(rows[0]["minimum"], "395.497 us")
        self.assertEqual(rows[0]["maximum"], "602.254 us")
        self.assertEqual(rows[0]["reps"], "20")
        self.assertEqual(rows[1]["median"], "120 ns")

    def test_parses_per_iter_rows(self):
        rows = SUMMARY.parse_rows(PER_ITER_OUTPUT)

        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[0]["case"], "preprocess (stage 1)")
        self.assertEqual(rows[0]["median"], "392.175 us/iter")
        self.assertEqual(rows[0]["reps"], "—")

    def test_skips_observation_lines(self):
        rows = SUMMARY.parse_rows(OBSERVATION_OUTPUT)

        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["case"], "Quote risk aggregate (single curve)")


class BuildReportTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.results_dir = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def _write(self, benchmark, output, status=None):
        (self.results_dir / f"{benchmark}.txt").write_text(output, encoding="utf-8")
        if status is not None:
            (self.results_dir / f"{benchmark}.status").write_text(f"{status}\n", encoding="utf-8")

    def test_single_table_combines_all_benchmarks(self):
        self._write("cholesky_perf", TIMED_OUTPUT)
        self._write("script_perf", PER_ITER_OUTPUT)

        report = SUMMARY.build_report(["cholesky_perf", "script_perf"], self.results_dir, "gcc-14, Release.")

        self.assertEqual(report.count("| Benchmark | Case | Median | Min | Max | Reps |"), 1)
        self.assertNotIn("### cholesky_perf", report)
        self.assertIn("| cholesky_perf | CholeskyDecompose (200x200) | 420.821 us |", report)
        self.assertIn("| script_perf | preprocess (stage 1) | 392.175 us/iter | — | — | — |", report)
        self.assertIn("All benchmarks exited cleanly.", report)

    def test_failed_benchmark_is_noted_but_rows_still_appear(self):
        self._write("curve_calibration_perf", TIMED_OUTPUT, status=134)

        report = SUMMARY.build_report(["curve_calibration_perf"], self.results_dir, "gcc-14, Release.")

        self.assertIn("| curve_calibration_perf | CholeskyDecompose (200x200) | 420.821 us |", report)
        self.assertIn("`curve_calibration_perf` (exit code 134)", report)
        self.assertNotIn("All benchmarks exited cleanly.", report)

    def test_missing_output_file_gets_placeholder_row(self):
        report = SUMMARY.build_report(["ghost_perf"], self.results_dir, "gcc-14, Release.")

        self.assertIn("| ghost_perf | (no output captured) |", report)

    def test_unparseable_output_gets_placeholder_row(self):
        self._write("krylov_perf", "no table here\n")

        report = SUMMARY.build_report(["krylov_perf"], self.results_dir, "gcc-14, Release.")

        self.assertIn("| krylov_perf | (no parseable result rows) |", report)


if __name__ == "__main__":
    unittest.main()
