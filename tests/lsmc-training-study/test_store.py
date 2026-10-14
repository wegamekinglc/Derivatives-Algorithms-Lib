"""Immutable manifest and resumable raw-evidence controls."""

from dataclasses import asdict
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dal-cpp/benchmarks/script_mc_perf"))
import lsmc_study_data as data  # noqa: E402
import lsmc_study_store as store  # noqa: E402


class EvidenceStoreTest(unittest.TestCase):
    def test_manifest_cannot_merge_a_different_image_or_job_matrix(self):
        job = data.Job("weekly_put", 512, 17, 1009, "frozen")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            store.prepare_dataset(root, {"image": "first"}, (job,))
            store.prepare_dataset(root, {"image": "first"}, (job,))
            with self.assertRaises(ValueError):
                store.prepare_dataset(root, {"image": "second"}, (job,))
            with self.assertRaises(ValueError):
                store.prepare_dataset(root, {"image": "first"}, ())

    def test_successful_raw_outputs_are_verified_before_resume(self):
        job = data.Job("weekly_put", 512, 17, 1009, "frozen")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            store.prepare_dataset(root, {"image": "first"}, (job,))
            store.save_attempt(root, job, ["native"], "original stdout", "stderr", 0, 1.0)
            loaded = store.load_attempt(root, job)
            self.assertEqual(loaded["stdout"], "original stdout")
            self.assertEqual(loaded["job"], asdict(job))
            (root / "samples" / (job.key + ".stdout")).write_text("changed")
            with self.assertRaises(ValueError):
                store.load_attempt(root, job)

    def test_failure_or_partial_evidence_is_preserved_and_not_silently_replaced(self):
        job = data.Job("weekly_put", 512, 17, 1009, "frozen")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            store.prepare_dataset(root, {"image": "first"}, (job,))
            store.save_attempt(root, job, ["native"], "failed output", "error", 2, 1.0)
            with self.assertRaises(ValueError):
                store.load_attempt(root, job)
            with self.assertRaises(FileExistsError):
                store.save_attempt(root, job, ["native"], "replacement", "", 0, 1.0)
            store.archive_attempt(root, job)
            self.assertTrue(any((root / "failed-attempts").rglob("*.stdout")))
            self.assertIsNone(store.load_attempt(root, job))

    def test_incomplete_dataset_cannot_be_reported_complete(self):
        job = data.Job("weekly_put", 512, 17, 1009, "frozen")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            store.prepare_dataset(root, {"image": "first"}, (job,))
            with self.assertRaises(ValueError):
                store.require_complete(root, (job,))


if __name__ == "__main__":
    unittest.main(verbosity=2)
