#!/usr/bin/env python3
"""Check selected Sobol profiling requests and complete-stream fingerprints."""

import argparse
import json
from pathlib import Path
import subprocess  # nosec B404: execute the explicitly selected DAL build-tree binary
import unittest


def native_executable(value):
    binary = Path(value).resolve(strict=True)
    if binary.name not in ("rng_perf", "rng_perf.exe") or not binary.is_file():
        raise ValueError("expected the rng_perf build-tree executable")
    if tuple(parent.name for parent in binary.parents[:3]) != ("rng_perf", "benchmarks", "dal-cpp"):
        raise ValueError("executable must be under a DAL build tree")
    return str(binary)


class SobolProfileTest(unittest.TestCase):
    executable = None

    def run_cli(self, arguments, success=True):
        # Fixed test arguments reach a validated local binary; no shell interprets them.
        result = subprocess.run(  # nosemgrep  # nosec B603
            [self.executable, "--sobol-profile", *arguments],  # nosemgrep
            capture_output=True, text=True, timeout=30, shell=False,
        )
        self.assertEqual(result.returncode == 0, success, (arguments, result.stdout, result.stderr))
        return [json.loads(line) for line in result.stdout.splitlines()] if success else []

    def check_request(self, rows, dimension, precision):
        self.assertEqual(len(rows), 2)
        for row in rows:
            self.assertEqual(row["dimensions"], dimension)
            self.assertEqual(row["paths"], 32)
            self.assertEqual(row["precision"], precision)
            self.assertGreater(row["wall_ns"], 0)
            self.assertEqual(row["fingerprint"], rows[0]["fingerprint"])
        return rows[0]["fingerprint"]

    def test_stream_fingerprints(self):
        for dimension in (1, 52, 366):
            fingerprints = []
            for precision in ("uniform", "fast", "precise"):
                arguments = [str(dimension), "32", precision, "2"]
                rows = self.run_cli(arguments)
                replay = self.run_cli(arguments)
                fingerprint = self.check_request(rows, dimension, precision)
                self.assertEqual(fingerprint, self.check_request(replay, dimension, precision))
                fingerprints.append(fingerprint)
            self.assertEqual(len(set(fingerprints)), 3)

    def test_invalid_requests(self):
        invalid = ([], ["0", "32", "fast", "1"], ["1", "0", "fast", "1"], ["1", "32", "fast", "0"],
                   ["-1", "32", "fast", "1"], ["1x", "32", "fast", "1"], ["1", "32", "unknown", "1"],
                   ["2147483648", "32", "fast", "1"], ["1", "18446744073709551616", "fast", "1"],
                   ["", "32", "fast", "1"], ["1", "32", "fast", "1", "extra"])
        for arguments in invalid:
            self.run_cli(arguments, False)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=native_executable)
    args = parser.parse_args()
    SobolProfileTest.executable = args.executable
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(SobolProfileTest)
    result = unittest.TextTestRunner().run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
