"""Exercise the native budget-study protocol against independent price controls."""

import argparse
import json
import math
from pathlib import Path
import subprocess  # nosec B404: execute the explicitly selected local DAL study image
import sys
import unittest


def native_executable(value):
    binary = Path(value).resolve(strict=True)
    if binary.name not in ("script_mc_perf", "script_mc_perf.exe") or not binary.is_file():
        raise ValueError("expected the local script_mc_perf study executable")
    return binary


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=native_executable)
    parser.add_argument("--addressing", choices=("stock", "controlled"), default="stock")
    return parser.parse_args()


OPTIONS = None


class NativeSampleTest(unittest.TestCase):
    def sample(self, **options):
        settings = {
            "case": "positive_control",
            "training": "128",
            "pricing": "257",
            "training-seed": "17",
            "pricing-seed": "29",
            "replicates": "2",
            "engine": "compiled",
            "mode": "frozen",
            "threads": "1",
        }
        settings.update({name.replace("_", "-"): str(value) for name, value in options.items()})
        command = [str(OPTIONS.binary), "--lsmc-training-sample"]
        for name, value in settings.items():
            command.extend(("--" + name, value))
        # Fixed test options reach the selected local executable without a shell.
        result = subprocess.run(command, capture_output=True, text=True, timeout=90, shell=False)  # nosemgrep  # nosec B603
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        rows = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
        self.assertEqual(len(rows), 1)
        row = rows[0]
        self.assertEqual(row["record"], "lsmc_training_sample")
        self.assertEqual(len(row["risk_names"]), len(row["risks"]))
        self.assertEqual(len(set(row["risk_names"])), len(row["risk_names"]))
        self.assertTrue(all(math.isfinite(value) for value in [row["pv"], *row["risks"]]))
        return row

    def test_deterministic_positive_claim_has_complete_normalized_greeks(self):
        row = self.sample(vol=0)
        time = row["maturity_time"]
        discounted_spot = 100.0 * math.exp(-0.02 * time)
        discounted_strike = 100.0 * math.exp(-0.05 * time)
        self.assertAlmostEqual(row["pv"], discounted_spot + discounted_strike, delta=1e-10)
        self.assertEqual([name.lower() for name in row["risk_names"]], ["spot", "vol", "rate", "div", "k"])
        risks = dict(zip((name.lower() for name in row["risk_names"]), row["risks"]))
        for name, expected in {
            "spot": math.exp(-0.02 * time),
            "rate": -time * discounted_strike,
            "div": -time * discounted_spot,
            "k": math.exp(-0.05 * time),
        }.items():
            self.assertAlmostEqual(risks[name], expected, delta=1e-10)
        self.assertEqual(row["training_paths"], 128)
        self.assertEqual(row["pricing_paths"], 257)
        self.assertEqual(row["replicates"], 2)

    def test_unset_training_budget_matches_explicit_pricing_count(self):
        default = self.sample(training="default")
        explicit = self.sample(training=257)
        self.assertEqual(default["training_paths"], 257)
        self.assertEqual(default["pv"], explicit["pv"])
        self.assertEqual(default["risks"], explicit["risks"])
        self.assertEqual(default["pricing_offset"], explicit["pricing_offset"])

    def test_tree_compiled_and_policy_risk_modes_preserve_the_positive_claim(self):
        reference = self.sample()
        for engine in ("tree", "compiled"):
            for mode in ("frozen", "retrained"):
                with self.subTest(engine=engine, mode=mode):
                    row = self.sample(engine=engine, mode=mode)
                    self.assertAlmostEqual(row["pv"], reference["pv"], delta=1e-10)
                    self.assertEqual(row["risk_names"], reference["risk_names"])
                    for actual, expected in zip(row["risks"], reference["risks"]):
                        self.assertAlmostEqual(actual, expected, delta=1e-10)

    def test_budget_comparison_exposes_stock_or_common_pricing_points(self):
        low = self.sample(training=64)
        high = self.sample(training=512)
        if OPTIONS.addressing == "controlled":
            self.assertEqual(low["pv"], high["pv"])
            self.assertEqual(low["risk_names"], high["risk_names"])
            self.assertEqual(low["risks"], high["risks"])
            self.assertEqual(low["pricing_offset"], high["pricing_offset"])
        else:
            self.assertNotEqual(low["pv"], high["pv"])
            self.assertEqual(low["pricing_offset"], 64)
            self.assertEqual(high["pricing_offset"], 512)
        self.assertEqual(low["addressing"], OPTIONS.addressing)
        self.assertEqual(high["addressing"], OPTIONS.addressing)

    def test_policy_diagnostics_retain_effective_counts_and_replicates(self):
        row = self.sample(validation=16, diagnostics="true")
        self.assertEqual(row["validation_paths"], 16)
        self.assertEqual(row["pricing_offset"], row["validation_offset"] + 16)
        self.assertEqual(len(row["hard_replicate_means"]), 2)
        self.assertAlmostEqual(sum(row["hard_replicate_means"]) / 2, row["hard_pv"], delta=1e-10)
        self.assertAlmostEqual(row["hard_pv"], row["pv"], delta=1e-10)
        self.assertEqual(len(row["policy"]), 1)
        self.assertTrue(all(value == 0.0 for value in row["policy"][0]["coefficients"]))
        self.assertEqual(row["policy"][0]["eligible_paths"], 128)

    def test_path_dependent_cases_use_the_requested_state_features(self):
        for case, features in (("asian_two", 2), ("asian_three", 3)):
            with self.subTest(case=case):
                row = self.sample(case=case, training=64, pricing=32)
                self.assertEqual(row["features"], features)
                self.assertEqual(len(row["risks"]), 5)

    def test_mixed_original_fixture_keeps_its_call_maturity(self):
        mixed = self.sample(case="mixed_original", mode="hard", vol=0)
        put = self.sample(case="weekly_put", mode="hard", vol=0)
        time = mixed["maturity_time"]
        expected = 100.0 * math.exp(-0.02 * time) - 100.0 * math.exp(-0.05 * time)
        self.assertAlmostEqual(mixed["pv"], expected, delta=1e-10)
        self.assertEqual(put["pv"], 0.0)
        self.assertEqual(mixed["risks"], [])

    def test_invalid_options_fail_without_a_result(self):
        bad_options = (
            ["--pricing", "0"],
            ["--training", "0"],
            ["--training", "-1"],
            ["--training-seed", "2147483648"],
            ["--pricing-seed", "-1"],
            ["--replicates", "1"],
            ["--engine", "unknown"],
            ["--case", "unknown"],
            ["--case", "weekly_put", "--case", "weekly_call"],
            ["--vol", "nan"],
            ["--pricing", "2147483647", "--training", "128"],
        )
        for options in bad_options:
            with self.subTest(options=options):
                result = subprocess.run(  # nosemgrep  # nosec B603
                    [str(OPTIONS.binary), "--lsmc-training-sample", *options],  # nosemgrep
                    capture_output=True, text=True, timeout=30, shell=False,
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(any(line.startswith("{") for line in result.stdout.splitlines()))
                self.assertTrue(result.stderr.strip())


if __name__ == "__main__":
    OPTIONS = arguments()
    unittest.main(argv=[sys.argv[0]], verbosity=2)
