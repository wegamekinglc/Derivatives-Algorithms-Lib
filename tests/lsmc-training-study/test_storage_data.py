"""Native storage-width admission and reference selection."""

import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dal-cpp/benchmarks/script_mc_perf"))

from lsmc_storage_data import parse_storage_record, storage_jobs  # noqa: E402
from test_data import native_record  # noqa: E402
from lsmc_study_data import Job  # noqa: E402


class StorageDataTest(unittest.TestCase):
    def test_width_allocation_and_release_metadata_are_required(self):
        job = Job("weekly_put", 512, 17, 1009, "frozen")
        record = native_record(job, 32768)
        record.update(storage_bits=32, peak_training_row_bytes=1024, live_training_row_bytes=0)
        self.assertEqual(parse_storage_record(json.dumps(record), job, 32768, 32), record)
        for mutation in ({"storage_bits": 64}, {"storage_bits": True}, {"peak_training_row_bytes": 0},
                         {"peak_training_row_bytes": True}, {"live_training_row_bytes": 1}):
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                parse_storage_record(json.dumps(dict(record, **mutation)), job, 32768, 32)
        del record["storage_bits"]
        with self.assertRaises(ValueError):
            parse_storage_record(json.dumps(record), job, 32768, 32)

    def test_financial_matrix_preserves_seeds_modes_and_explicit_budget_boundaries(self):
        jobs = storage_jobs("accuracy")
        self.assertEqual(len(jobs), 11 * 2 * 8 * 8 * 2)
        self.assertEqual({job.training for job in jobs}, {512, 32768})
        self.assertEqual({job.mode for job in jobs}, {"frozen", "retrained"})
        self.assertEqual(len({job.training_seed for job in jobs}), 8)
        self.assertEqual(len({job.pricing_seed for job in jobs}), 8)
        corner = storage_jobs("corner")
        self.assertEqual(len(corner), 2 * 8 * 8 * 2)
        self.assertEqual({job.training for job in corner}, {65536, 524288})


if __name__ == "__main__":
    unittest.main(verbosity=2)
