#!/usr/bin/env python3
"""Guard evidence rejection and paired reduction, including optimized Python."""

import unittest
from runstudy import append_measurement, fingerprint, production_measurement, require, summarize


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


if __name__ == "__main__":
    unittest.main()
