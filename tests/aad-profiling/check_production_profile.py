#!/usr/bin/env python3
"""Exercise production profiling CLI contracts without timing thresholds."""

import argparse
import json
import math
from pathlib import Path
import subprocess  # nosec B404: execute the explicitly selected DAL build-tree binary
import unittest


def native_executable(value):
    binary = Path(value).resolve(strict=True)
    if binary.name not in ("script_mc_perf", "script_mc_perf.exe") or not binary.is_file():
        raise ValueError("expected the script_mc_perf build-tree executable")
    if tuple(parent.name for parent in binary.parents[:3]) != ("script_mc_perf", "benchmarks", "dal-cpp"):
        raise ValueError("executable must be under a DAL build tree")
    return str(binary)


class ProductionProfileTest(unittest.TestCase):
    executable = None
    profiling = False

    def run_cli(self, arguments, success=True):
        # Only fixed test arguments reach the validated local binary; no shell interprets them.
        result = subprocess.run(  # nosemgrep  # nosec B603
            [self.executable, "--production-profile", *arguments],  # nosemgrep
            capture_output=True, text=True, timeout=120, shell=False,
        )
        self.assertEqual(result.returncode == 0, success, (arguments, result.stdout, result.stderr))
        return [json.loads(line) for line in result.stdout.splitlines()] if success else []

    def records(self, rows, kind):
        return [row for row in rows if row["record"] == kind]

    def check_finite(self, results):
        for row in results:
            self.assertTrue(math.isfinite(row["pv"]))
            for risk in row["risks"]:
                self.assertTrue(math.isfinite(risk))

    def check_short_request(self, rows):
        request = self.records(rows, "request")[0]
        self.assertEqual(request["outputs"], 4)
        self.assertEqual(request["channel_width"], 1)
        self.assertEqual(request["paths_per_output"], 32)
        self.assertEqual(request["output_strategy"], "sequential_single_output")
        results = self.records(rows, "result")
        self.assertEqual(len(results), 4)
        self.assertEqual(len({row["output"] for row in results}), 4)
        self.assertEqual(len({row["pv"] for row in results}), 4)
        self.check_finite(results)
        return results

    def check_matching_results(self, lhs, rhs):
        self.assertEqual(len(lhs), len(rhs))
        for left, right in zip(lhs, rhs):
            self.assertTrue(math.isclose(left["pv"], right["pv"], rel_tol=1e-10, abs_tol=1e-10))
            self.assertEqual(left["risk_names"], right["risk_names"])
            self.assertEqual(len(left["risks"]), len(right["risks"]))
            for x, y in zip(left["risks"], right["risks"]):
                self.assertTrue(math.isclose(x, y, rel_tol=1e-10, abs_tol=1e-10))

    def test_cold_warm(self):
        common = ["short", "32", "aad", "compiled"]
        cold = self.check_short_request(self.run_cli([*common, "cold", "4", "0", "0", "1"]))
        warm = self.check_short_request(self.run_cli([*common, "warm", "4", "0", "0", "1"]))
        self.check_matching_results(cold, warm)

    def test_phases(self):
        rows = self.run_cli(["short", "32", "aad", "compiled", "phases", "1", "0", "0", "1"], self.profiling)
        if self.profiling:
            self.check_scopes(rows)

    def check_scopes(self, rows):
        scopes = self.records(rows, "scope")
        self.assertTrue(scopes)
        for scope in scopes:
            self.assertTrue(scope["complete"])
        self.assertGreater(sum(row["tape_samples"] for row in scopes), 32)
        self.assertTrue(any(row["path_array_live_bytes"] > 0 for row in scopes))
        self.assertTrue(any(row["result_array_live_bytes"] > 0 for row in scopes))
        suffix = [row for row in self.records(rows, "phase") if row["phase"] == "REVERSE_SUFFIX"]
        self.assertEqual(sum(row["calls"] for row in suffix), 32)

    def test_representative_scenarios(self):
        scenarios = {"long": (0, 0), "local-vol": (2, 0), "lsmc-bs": (0, 64), "lsmc-local-vol": (2, 64)}
        for scenario, (grid, training) in scenarios.items():
            rows = self.run_cli([scenario, "8", "aad", "compiled", "cold", "1", str(grid), str(training), "1"])
            request = self.records(rows, "request")[0]
            result = self.records(rows, "result")[0]
            self.assertGreater(request["events"], 1)
            self.assertEqual(request["active_parameters"], len(result["risks"]))
            self.assertEqual(len(result["risk_names"]), len(result["risks"]))
            self.check_finite([result])

    def test_passive(self):
        rows = self.run_cli(["short", "8", "double", "tree", "warm", "1", "0", "0", "1"])
        request = self.records(rows, "request")[0]
        result = self.records(rows, "result")[0]
        self.assertEqual(request["channel_width"], 0)
        self.assertEqual(request["active_parameters"], 0)
        self.assertEqual(result["risk_names"], [])
        self.assertEqual(result["risks"], [])

    def test_sixty_four_outputs(self):
        for engine in ("tree", "compiled"):
            rows = self.run_cli(["short", "64", "aad", engine, "cold", "64", "0", "0", "1"])
            results = self.records(rows, "result")
            self.assertEqual(len(results), 64)
            self.assertEqual(len({row["output"] for row in results}), 64)
            for row in results:
                self.assertEqual(len(row["risks"]), 4)
            self.check_finite(results)

    def test_invalid_options(self):
        common = ["short", "32", "aad", "compiled"]
        invalid = ([], [*common, "cold", "3", "0", "0", "1"], [*common, "cold", "1", "2", "0", "1"],
                   ["short", "0", "aad", "compiled", "cold", "1", "0", "0", "1"])
        for arguments in invalid:
            self.run_cli(arguments, False)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable")
    parser.add_argument("--profiling", choices=("on", "off"), required=True)
    args = parser.parse_args()
    ProductionProfileTest.executable = native_executable(args.executable)
    ProductionProfileTest.profiling = args.profiling == "on"
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ProductionProfileTest)
    result = unittest.TextTestRunner().run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
