"""Process failures and resumption retain verifiable raw evidence."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dal-cpp/benchmarks/script_mc_perf"))
from lsmc_study_data import Job  # noqa: E402
from lsmc_study_store import prepare_dataset, attempt_paths  # noqa: E402
from test_data import native_record  # noqa: E402
import lsmc_study_run as study  # noqa: E402


class RunTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.job = Job("weekly_put", 512, 17, 1009, "frozen")
        prepare_dataset(self.root, {"test": True}, (self.job,))
        self.output = json.dumps(native_record(self.job, 32768)) + "\n"

    def run_jobs(self, process, retry=False):
        return study.run_jobs(self.root, (self.job,), 32768, Path("sample"), 3, retry, process)

    def test_success_is_resumed_without_another_process(self):
        process = Mock(return_value=subprocess.CompletedProcess([], 0, self.output, "startup\n"))
        self.run_jobs(process)
        self.run_jobs(process)
        process.assert_called_once()
        self.assertEqual(attempt_paths(self.root, self.job)["stdout"].read_text(), self.output)

    def test_failed_process_requires_explicit_retry_and_keeps_first_attempt(self):
        process = Mock(return_value=subprocess.CompletedProcess([], 2, "", "native failure"))
        with self.assertRaises(ValueError):
            self.run_jobs(process)
        with self.assertRaises(ValueError):
            self.run_jobs(process)
        process.assert_called_once()
        process.return_value = subprocess.CompletedProcess([], 0, self.output, "")
        self.run_jobs(process, retry=True)
        archived = list((self.root / "failed-attempts").glob("*/attempt.stderr"))
        self.assertEqual(len(archived), 1)
        self.assertEqual(archived[0].read_text(), "native failure")

    def test_timeout_is_a_terminal_failed_attempt_with_partial_output(self):
        process = Mock(side_effect=subprocess.TimeoutExpired(["sample"], 3, b"partial", b"timeout detail"))
        with self.assertRaises(ValueError):
            self.run_jobs(process)
        paths = attempt_paths(self.root, self.job)
        self.assertEqual(paths["stdout"].read_text(), "partial")
        self.assertEqual(json.loads(paths["json"].read_text())["returncode"], 124)

    def test_wrong_native_settings_cannot_be_accepted_or_silently_retried(self):
        broken = native_record(self.job, 32768)
        broken["pricing_offset"] = 512
        process = Mock(return_value=subprocess.CompletedProcess([], 0, json.dumps(broken), ""))
        with self.assertRaises(ValueError):
            self.run_jobs(process)
        with self.assertRaises(ValueError):
            self.run_jobs(process)
        process.assert_called_once()


if __name__ == "__main__":
    unittest.main(verbosity=2)
