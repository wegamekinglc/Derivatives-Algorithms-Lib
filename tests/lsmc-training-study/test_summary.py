"""Paired pricing uncertainty is distinct from observed training dispersion."""

import copy
import math
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dal-cpp/benchmarks/script_mc_perf"))
from lsmc_study_data import Job  # noqa: E402
from test_data import native_record  # noqa: E402
import lsmc_study_summary as study  # noqa: E402


def observations():
    result = []
    for budget in (2, 8):
        for training_index, training in enumerate((17, 41)):
            for pricing_index, pricing in enumerate((1009, 2027)):
                for mode in ("frozen", "retrained"):
                    job = Job("weekly_put", budget, training, pricing, mode)
                    record = native_record(job, 8)
                    delta = (1 + 4 * training_index + 2 * pricing_index) if budget == 2 else 0
                    record["pv"] = 100 + 100 * pricing_index + delta
                    record["risks"][0] = delta
                    record["hard_pv"] = record["pv"] - 0.1
                    record["policy"] = [{"coefficients": [training], "exercise_rate": pricing_index / 2}]
                    result.append((job, record))
    return result


class SummaryTest(unittest.TestCase):
    def test_differences_are_paired_before_se_and_training_dispersion(self):
        summary = study.summarize(observations())
        comparison = next(row for row in summary["comparisons"] if row["mode"] == "frozen" and row["training"] == 2)
        pv = comparison["coordinates"]["PV"]
        self.assertEqual([row["difference"] for row in pv["by_training_seed"]], [2, 6])
        self.assertEqual([row["pricing_se"] for row in pv["by_training_seed"]], [1, 1])
        self.assertEqual(pv["observed_training_means"]["mean_difference"], 4)
        self.assertEqual(pv["observed_training_means"]["sd_difference"], math.sqrt(8))
        self.assertEqual(pv["observed_training_means"]["min_difference"], 2)
        self.assertEqual(pv["observed_training_means"]["max_difference"], 6)
        self.assertEqual(set(comparison["coordinates"]), {"PV", "d_spot", "d_vol", "d_rate", "d_div", "d_K"})
        self.assertIsNone(comparison["coordinates"]["d_spot"]["by_training_seed"][0]["relative_difference"])

    def test_policy_fingerprint_excludes_pricing_exercise_rates(self):
        summary = study.summarize(observations())
        self.assertEqual(len(summary["policy_groups"]), 2 * 2 * 2)
        self.assertTrue(all(row["pricing_groups"] == 2 for row in summary["policy_groups"]))
        broken = observations()
        broken[2][1]["policy"][0]["coefficients"] = [999]
        with self.assertRaisesRegex(ValueError, "policy"):
            study.summarize(broken)

    def test_modes_must_share_prices_and_all_records_share_risk_axes(self):
        broken = observations()
        broken[1][1]["pv"] += 1
        with self.assertRaisesRegex(ValueError, "mode"):
            study.summarize(broken)
        broken = observations()
        broken[0][1]["risk_names"] = list(reversed(broken[0][1]["risk_names"]))
        with self.assertRaisesRegex(ValueError, "risk"):
            study.summarize(broken)

    def test_missing_or_duplicate_pairs_cannot_generate_a_summary(self):
        rows = observations()
        for broken in (rows[:-1], rows + [copy.deepcopy(rows[0])]):
            with self.assertRaises(ValueError):
                study.summarize(broken)

    def test_hard_and_fuzzy_diagnostics_are_separate_from_greeks(self):
        summary = study.summarize(observations())
        for comparison in summary["comparisons"]:
            self.assertEqual(set(comparison["diagnostics"]), {"hard_PV", "fuzzy_minus_hard"})
            difference = comparison["diagnostics"]["fuzzy_minus_hard"]["observed_training_means"]["mean_difference"]
            self.assertAlmostEqual(difference, 0, places=12)
        self.assertEqual(summary["default_training_budget"], "unchanged: pricing path count")
        self.assertFalse(summary["accuracy_acceptance_defined"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
