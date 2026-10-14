"""Process failures and resumption retain verifiable raw evidence."""

import json
from pathlib import Path
import subprocess  # nosec B404: only construct a timeout exception; no test process is executed
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock
from functools import partial

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dal-cpp/benchmarks/script_mc_perf"))
from lsmc_study_data import Job  # noqa: E402
from lsmc_study_store import prepare_dataset, attempt_paths  # noqa: E402
from test_data import native_record  # noqa: E402
from lsmc_storage_data import parse_storage_record  # noqa: E402
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
        process = Mock(return_value=SimpleNamespace(returncode=0, stdout=self.output, stderr="startup\n"))
        self.run_jobs(process)
        self.run_jobs(process)
        process.assert_called_once()
        self.assertFalse(process.call_args.kwargs["shell"])
        self.assertEqual(attempt_paths(self.root, self.job)["stdout"].read_text(), self.output)

    def test_failed_process_requires_explicit_retry_and_keeps_first_attempt(self):
        process = Mock(return_value=SimpleNamespace(returncode=2, stdout="", stderr="native failure"))
        with self.assertRaises(ValueError):
            self.run_jobs(process)
        with self.assertRaises(ValueError):
            self.run_jobs(process)
        process.assert_called_once()
        process.return_value = SimpleNamespace(returncode=0, stdout=self.output, stderr="")
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

    def test_launch_error_is_retained_and_requires_explicit_retry(self):
        process = Mock(side_effect=OSError("executable permission denied"))
        with self.assertRaises(ValueError):
            self.run_jobs(process)
        paths = attempt_paths(self.root, self.job)
        self.assertEqual(json.loads(paths["json"].read_text())["returncode"], 127)
        self.assertIn("permission denied", paths["stderr"].read_text())
        with self.assertRaises(ValueError):
            self.run_jobs(process)
        process.assert_called_once()

    def test_wrong_native_settings_cannot_be_accepted_or_silently_retried(self):
        broken = native_record(self.job, 32768)
        broken["pricing_offset"] = 512
        process = Mock(return_value=SimpleNamespace(returncode=0, stdout=json.dumps(broken), stderr=""))
        with self.assertRaises(ValueError):
            self.run_jobs(process)
        with self.assertRaises(ValueError):
            self.run_jobs(process)
        process.assert_called_once()

    def test_storage_admission_is_required_on_execution_resume_and_retry(self):
        process = Mock(return_value=SimpleNamespace(returncode=0, stdout=self.output, stderr=""))
        parser = partial(parse_storage_record, bits=32)
        arguments = (self.root, (self.job,), 32768, Path("sample"))
        with self.assertRaises(ValueError):
            study.run_jobs(*arguments, process=process, parser=parser)
        with self.assertRaises(ValueError):
            study.run_jobs(*arguments, process=process, parser=parser)
        process.assert_called_once()
        record = native_record(self.job, 32768)
        record.update(storage_bits=32, peak_training_row_bytes=1024, live_training_row_bytes=0)
        process.return_value = SimpleNamespace(returncode=0, stdout=json.dumps(record), stderr="")
        study.run_jobs(*arguments, process=process, retry=True, parser=parser)
        study.run_jobs(*arguments, process=process, parser=parser)
        self.assertEqual(process.call_count, 2)
        self.assertEqual(len(list((self.root / "failed-attempts").glob("*/attempt.stdout"))), 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
