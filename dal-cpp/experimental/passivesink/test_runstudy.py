#!/usr/bin/env python3
"""Guard evidence rejection and paired reduction, including optimized Python."""

import json
from pathlib import Path
import tempfile
import unittest
from runstudy import (append_measurement, fingerprint, memory_statistics, parse_measurement, production_measurement,
                      require, summarize, summarize_memory, validate_reused_evidence)


class SinkStudyEvidenceTest(unittest.TestCase):
    def test_bit_fingerprint_distinguishes_signed_zero_and_adjacent_doubles(self):
        self.assertNotEqual(fingerprint([0.0]), fingerprint([-0.0]))
        self.assertNotEqual(fingerprint([1.0]), fingerprint([1.0000000000000002]))
        self.assertNotEqual(fingerprint({"risk_names": ["a"]}), fingerprint({"risk_names": ["b"]}))
        with self.assertRaises(RuntimeError):
            fingerprint([float("nan")])
        with self.assertRaises(RuntimeError):
            require(False, "bad command must fail even under -O")

    def test_incomplete_processes_and_rounds_reject(self):
        with self.assertRaises(RuntimeError):
            summarize({"case": {"base": [[100] * 10, [100] * 9], "head": [[90] * 10, [90] * 10]}})
        with self.assertRaises(RuntimeError):
            summarize({"case": {"base": [[100] * 10], "head": [[90] * 10]}})

    def test_verdict_requires_both_rounds_and_preserves_minima(self):
        base = [[100] + [150] * 9, [100] + [150] * 9]
        for rounds, verdict in [([90, 95], "improvement"), ([105, 110], "regression"), ([90, 110], "no-change")]:
            result = summarize({"case": {"base": base, "head": [[value] + [200] * 9 for value in rounds]}})[0]
            self.assertEqual(result["verdict"], verdict)
            self.assertEqual(result["base_min_ms"], 0.0001)

    def test_wrong_workload_or_missing_results_reject(self):
        request = {"record": "request", "paths_per_output": 32, "threads": 1, "wall_ns": 100}
        result = {"record": "result", "pv": 2.0}
        self.assertEqual(production_measurement([request, result], "case", 32, 1)[0], {"wall_ns": 100})
        for size, threads in [(31, 1), (32, 4)]:
            with self.assertRaises(RuntimeError):
                production_measurement([request, result], "case", size, threads)
        with self.assertRaises(RuntimeError):
            production_measurement([request], "case", 32, 1)

    def test_cross_side_bit_mismatch_and_zero_duration_reject(self):
        samples, fingerprints = {}, {}
        append_measurement(samples, fingerprints, "case", "base", 0, {"wall_ns": 100}, [1.0])
        with self.assertRaises(RuntimeError):
            append_measurement(samples, fingerprints, "case", "head", 0, {"wall_ns": 100}, [1.0000000000000002])
        with self.assertRaises(RuntimeError):
            append_measurement(samples, fingerprints, "case", "head", 0, {"wall_ns": 0}, [1.0])

    def test_memory_reduction_uses_round_medians_and_keeps_range(self):
        base = [[10] + [100] * 9, [20] + [200] * 9]
        head = [[5] + [80] * 9, [10] + [160] * 9]
        result = summarize_memory({"case": {"base": base, "head": head}})[0]
        self.assertEqual(result["base"]["median_peak_rss_kib"], 100)
        self.assertEqual(result["base"]["min_peak_rss_kib"], 10)
        self.assertEqual(result["base"]["max_peak_rss_kib"], 200)
        self.assertEqual(result["base"]["round_median_peak_rss_kib"], [100, 200])
        for delta in result["round_median_percent"]:
            self.assertAlmostEqual(delta, -20)

    def test_incomplete_or_invalid_memory_evidence_rejects(self):
        for rounds in [[[100] * 10], [[100] * 10, [100] * 9], [[100] * 10, [0] * 10],
                       [[100] * 10, [float("nan")] * 10], [[100] * 10, [float("inf")] * 10]]:
            with self.assertRaises(RuntimeError):
                memory_statistics(rounds)

    def test_wrong_production_mode_cannot_enter_measurement(self):
        request = {"record": "request", "scenario": "gsr-swap-3f", "mode": "aad", "engine": "compiled", "measurement": "cold",
                   "outputs": 1, "profiling_build": False, "threads": 4, "paths_per_output": 8192, "wall_ns": 100}
        job = ("case", "production", "gsr-swap-3f", 8192, "compiled", 4)
        for key, wrong in [("scenario", "short"), ("mode", "double"), ("engine", "tree"), ("measurement", "warm"),
                           ("outputs", 4), ("profiling_build", True)]:
            rows = [json.dumps({**request, key: wrong}), json.dumps({"record": "result", "pv": 1.0})]
            with self.assertRaises(RuntimeError):
                parse_measurement("\n".join(rows), job, "head", Path("."))

    def test_changed_binary_cannot_reuse_prior_correctness(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary, report = root / "binary", root / "report.json"
            binary.write_bytes(b"changed")
            report.write_text(json.dumps({"timed_executable_sha256": {"base": {"production": "wrong"}}}))
            with self.assertRaises(RuntimeError):
                validate_reused_evidence({"base": {"production": binary}}, report)


if __name__ == "__main__":
    unittest.main()
