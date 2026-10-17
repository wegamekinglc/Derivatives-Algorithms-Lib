"""Study coverage and sample-admission contracts."""

import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dal-cpp/benchmarks/script_mc_perf"))
import lsmc_study_data as study  # noqa: E402


def native_record(job, anchor):
    return {
        "record": "lsmc_training_sample", "addressing": "controlled", "case": job.case,
        "engine": job.engine, "mode": job.mode, "training_paths": job.training,
        "pricing_paths": job.pricing, "validation_paths": job.validation,
        "validation_offset": anchor, "pricing_offset": anchor + job.validation,
        "replicates": job.replicates, "training_seed": job.training_seed, "pricing_seed": job.pricing_seed,
        "threads": job.threads, "normal_precision": job.precision, "brownian_bridge": job.bridge,
        "diagnostics": True, "pv": 10.0, "risk_names": ["spot", "vol", "rate", "div", "K"],
        "risks": [-0.4, 20.0, -30.0, 10.0, 0.5], "hard_pv": 9.99,
        "hard_replicate_means": [9.98, 10.0], "policy": [{"coefficients": [1.0], "exercise_rate": 0.25}],
    }


class MatrixTest(unittest.TestCase):
    def test_full_profiles_preserve_eight_training_and_pricing_seeds(self):
        accuracy = study.profile_jobs("accuracy")
        corner = study.profile_jobs("corner")
        self.assertEqual(len(accuracy), 11 * 4 * 8 * 8 * 2)
        self.assertEqual(len(corner), 3 * 8 * 8 * 2)
        self.assertEqual(len({job.key for job in accuracy}), len(accuracy))
        self.assertEqual({job.mode for job in accuracy}, {"frozen", "retrained"})
        self.assertEqual({job.training for job in corner}, {65536, 131072, 524288})

    def test_smoke_is_diagnostic_and_subset_selection_keeps_seed_counts(self):
        subset = study.profile_jobs("accuracy", cases=("weekly_put",))
        self.assertEqual(len(subset), 4 * 8 * 8 * 2)
        self.assertTrue(study.profile_definition("smoke")["diagnostic_only"])
        with self.assertRaises(ValueError):
            study.profile_jobs("accuracy", cases=("unknown",))

    def test_sample_identity_includes_pricing_seed_and_mode(self):
        left = study.Job("weekly_put", 512, 17, 1009, "frozen")
        right = study.Job("weekly_put", 512, 17, 2027, "frozen")
        retrained = study.Job("weekly_put", 512, 17, 1009, "retrained")
        self.assertNotEqual(left.key, right.key)
        self.assertNotEqual(left.key, retrained.key)


class AdmissionTest(unittest.TestCase):
    def setUp(self):
        self.job = study.Job("weekly_put", 512, 17, 1009, "frozen")
        self.record = native_record(self.job, 32768)

    def test_complete_native_record_and_every_greek_are_retained(self):
        parsed = study.parse_record(json.dumps(self.record) + "\n", self.job, 32768)
        self.assertEqual(parsed, self.record)
        self.assertEqual(study.risk_vector(parsed), {"PV": 10.0, "d_spot": -0.4, "d_vol": 20.0,
                                                   "d_rate": -30.0, "d_div": 10.0, "d_K": 0.5})

    def test_mismatched_settings_wrong_addresses_and_cropped_risks_are_rejected(self):
        for field, value in (("pricing_seed", 99), ("pricing_offset", 512), ("case", "daily_put"),
                             ("addressing", "stock"), ("risks", [1.0]), ("risk_names", ["spot"]), ("pv", float("inf"))):
            broken = dict(self.record, **{field: value})
            with self.subTest(field=field), self.assertRaises(ValueError):
                study.parse_record(json.dumps(broken), self.job, 32768)

    def test_missing_or_multiple_result_records_are_rejected(self):
        for output in ("startup only", json.dumps(self.record) + "\n" + json.dumps(self.record)):
            with self.subTest(output=output[:20]), self.assertRaises(ValueError):
                study.parse_record(output, self.job, 32768)


if __name__ == "__main__":
    unittest.main(verbosity=2)
