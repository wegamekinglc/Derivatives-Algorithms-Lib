"""Independent paired-width uncertainty and evidence completeness checks."""

import copy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dal-cpp/benchmarks/script_mc_perf"))
from lsmc_storage_summary import summarize_storage  # noqa: E402
from lsmc_study_data import Job  # noqa: E402
from test_data import native_record  # noqa: E402


def observations():
    native, reduced = [], []
    for budget in (512, 32768):
        for seed, scale in ((17, 1), (41, 2)):
            for group, sign in ((1009, -1), (2027, 1)):
                for mode in ("frozen", "retrained"):
                    job = Job("weekly_put", budget, seed, group, mode)
                    record = native_record(job, 32768)
                    record["pv"] = float(group)
                    other = copy.deepcopy(record)
                    other["pv"] += scale * sign
                    other["hard_pv"] += 2.0
                    other["risks"][0] += 2 * scale + 3 + scale * sign
                    native.append((job, record))
                    reduced.append((job, other))
    return reduced, native


class StorageSummaryTest(unittest.TestCase):
    def test_widths_pair_at_the_same_budget_before_conditional_error(self):
        summary = summarize_storage(*observations())
        self.assertEqual(summary["samples_per_width"], 16)
        self.assertEqual(len(summary["comparisons"]), 4)
        for comparison in summary["comparisons"]:
            pv = comparison["coordinates"]["PV"]["observed_training_means"]
            self.assertEqual(pv["mean_difference"], 0.0)
            self.assertEqual(pv["sd_difference"], 0.0)
            self.assertEqual(pv["max_conditional_pricing_se"], 2.0)
            spot = comparison["coordinates"]["d_spot"]["observed_training_means"]
            self.assertAlmostEqual(spot["mean_difference"], 6.0)
            self.assertAlmostEqual(spot["sd_difference"], 2**0.5)
            self.assertEqual(len(comparison["coordinates"]), 8)
            self.assertEqual(comparison["coordinates"]["hard_PV"]["observed_training_means"]["mean_difference"], 2.0)

    def test_missing_duplicate_and_mismatched_risk_axes_are_rejected(self):
        reduced, native = observations()
        for rows in (reduced[:-1], reduced + [reduced[0]]):
            with self.subTest(size=len(rows)), self.assertRaises(ValueError):
                summarize_storage(rows, native)
        broken = copy.deepcopy(reduced)
        broken[0][1]["risk_names"][0] = "wrong"
        with self.assertRaises(ValueError):
            summarize_storage(broken, native)


if __name__ == "__main__":
    unittest.main(verbosity=2)
