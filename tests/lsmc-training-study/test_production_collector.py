import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "production_collector", ROOT / "dal-cpp/experimental/lsmccheckpoint/runproduction.py")
COLLECTOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COLLECTOR)

DRIVER = """import json, os, sys
args = dict(zip(sys.argv[1::2], sys.argv[2::2]))
if os.environ['DAL_NUM_THREADS'] != '1':
    raise SystemExit(9)
mode, schedule = args['--mode'], args['--schedule']
fits = 11 if mode == 'retrained' else 1
print('driver startup')
print('driver diagnostic', file=sys.stderr)
print(json.dumps(dict(mode=mode, schedule=schedule, pv=1.25, risks=[1, 2, 3, 4, 5],
    normal_vectors=(5120 * 10 if schedule == 'checkpoint' else 5121) * fits,
    model_steps=5120 * 65 * 2 * fits, allocation_peak_bytes=100, allocation_bound_bytes=200,
    seconds=0.25, retained_tape_bytes=16, post_pricing_tape_bytes=32, arguments=args)))
"""


def driver(directory, source=DRIVER):
    path = directory / "local driver;$(touch injected)"
    path.write_text(f"#!{sys.executable}\n{source}", encoding="utf-8")
    path.chmod(0o700)
    return path


@unittest.skipUnless(sys.platform == "linux", "collector uses Linux /usr/bin/time and system tools")
class ProductionCollectorTests(unittest.TestCase):
    def test_literal_executable_path_workload_and_raw_logs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = driver(root)
            with mock.patch.object(COLLECTOR, "ROOT", root):
                result = COLLECTOR.collect_sample(binary, root, "hard", 0, "checkpoint")
            self.assertEqual(result["arguments"], {
                "--mode": "hard", "--schedule": "checkpoint", "--events": "65", "--block": "8",
                "--training": "4097", "--validation": "1023", "--pricing": "2049", "--bridge": "true",
            })
            self.assertEqual((root / "hard-0-checkpoint.out").read_text(), result["stdout"])
            self.assertIn("driver startup", result["stdout"])
            self.assertEqual((root / "hard-0-checkpoint.err").read_text(), result["stderr"])
            self.assertIn("Maximum resident set size", result["resource_output"])
            self.assertGreater(result["peak_rss_kib"], 0)
            self.assertFalse((root / "injected").exists())

    def test_complete_collection_retains_order_summaries_and_provenance(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = driver(root)
            output = root / "result.json"
            with mock.patch.object(COLLECTOR, "ROOT", root), \
                    mock.patch.object(COLLECTOR, "git_metadata", side_effect=["commit\n", "dirty\n"]), \
                    mock.patch.object(COLLECTOR, "compiler_version", return_value="compiler"), \
                    mock.patch.object(sys, "argv", ["collector", str(binary), "--output", str(output)]), \
                    contextlib.redirect_stdout(io.StringIO()):
                COLLECTOR.main()
            result = json.loads(output.read_text())
            self.assertEqual(result["source_commit"], "commit")
            self.assertEqual(result["source_status"], "dirty\n")
            self.assertEqual(result["compiler"], "compiler")
            self.assertEqual(result["executable_sha256"], COLLECTOR.digest(binary))
            self.assertEqual(len(result["samples"]), 60)
            self.assertEqual([row["schedule"] for row in result["samples"][:4]],
                             ["materialized", "checkpoint", "checkpoint", "materialized"])
            for schedules in result["summary"].values():
                for summary in schedules.values():
                    self.assertEqual(summary["samples"], 10)
                    self.assertEqual(summary["minimum_seconds"], 0.25)
                    self.assertEqual(summary["maximum_charged_peak_bytes"], 100)
                    self.assertEqual(summary["maximum_retained_tape_bytes"], 16)
                    self.assertEqual(summary["maximum_post_pricing_tape_bytes"], 32)

    def test_driver_failure_preserves_diagnostics(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = driver(root, "import sys\nprint('failed', file=sys.stderr)\nraise SystemExit(7)\n")
            with mock.patch.object(COLLECTOR, "ROOT", root), self.assertRaisesRegex(RuntimeError, "exited 7"):
                COLLECTOR.collect_sample(binary, root, "frozen", 0, "materialized")
            self.assertEqual((root / "frozen-0-materialized.err").read_text(), "failed\n")
            self.assertTrue((root / "frozen-0-materialized.time").is_file())

    def test_invalid_executables_fail_before_spawning(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plain = root / "plain"
            plain.write_text("data")
            for path in (root, plain, root / "missing"):
                with self.subTest(path=path), mock.patch.object(COLLECTOR.subprocess, "run") as spawn:
                    with self.assertRaises((OSError, RuntimeError)):
                        COLLECTOR.collect_sample(path, root, "hard", 0, "checkpoint")
                    spawn.assert_not_called()

    def test_options_resolve_symlink_and_reject_missing_binary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = driver(root)
            link = root / "link"
            link.symlink_to(binary)
            with mock.patch.object(sys, "argv", ["collector", str(link), "--output", str(root / "out")]):
                self.assertEqual(COLLECTOR.parse_options().binary, binary.resolve())
            with mock.patch.object(sys, "argv", ["collector", str(root / "missing"), "--output", str(root / "out")]), \
                    contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                COLLECTOR.parse_options()
            self.assertEqual(error.exception.code, 2)

    def test_workload_and_metadata_are_allowlisted(self):
        with mock.patch.object(COLLECTOR.subprocess, "run") as spawn:
            for mode, sample, schedule in (("bad", 0, "checkpoint"), ("hard", 0, "bad"), ("hard", 10, "checkpoint")):
                with self.subTest(mode=mode, sample=sample, schedule=schedule), self.assertRaises(RuntimeError):
                    COLLECTOR.collect_sample(Path("unused"), ROOT, mode, sample, schedule)
            spawn.assert_not_called()
        with mock.patch.object(COLLECTOR.subprocess, "check_output", return_value="value\n") as query:
            self.assertEqual(COLLECTOR.git_metadata("commit"), "value\n")
            query.assert_called_once_with(["/usr/bin/git", "rev-parse", "HEAD"],
                                          cwd=ROOT, text=True, shell=False)
            query.reset_mock()
            with self.assertRaises(KeyError):
                COLLECTOR.git_metadata("--config=untrusted")
            query.assert_not_called()
            self.assertEqual(COLLECTOR.compiler_version(), "value")
            query.assert_called_once_with(["/usr/bin/c++", "--version"], text=True, shell=False)

    def test_numerical_and_work_bounds_remain_active(self):
        samples = json.loads((ROOT / "tests/lsmc-training-study/lsmc-checkpoint-production.json").read_text())["samples"]
        pair = {row["schedule"]: row for row in samples if row["mode"] == "hard" and row["sample"] == 0}
        COLLECTOR.validate_pair(pair, "hard")
        for schedule, field in (("checkpoint", "pv"), ("checkpoint", "risks"), ("checkpoint", "normal_vectors"),
                                ("checkpoint", "model_steps"), ("materialized", "normal_vectors"),
                                ("checkpoint", "allocation_peak_bytes")):
            changed = copy.deepcopy(pair)
            changed[schedule][field] = [999] if field == "risks" else 10 ** 20
            with self.subTest(field=field, schedule=schedule), self.assertRaises(RuntimeError):
                COLLECTOR.validate_pair(changed, "hard")

    def test_recorded_samples_reproduce_original_payload(self):
        recorded = json.loads((ROOT / "tests/lsmc-training-study/lsmc-checkpoint-production.json").read_text())
        with mock.patch.object(COLLECTOR, "git_metadata", side_effect=[recorded["source_commit"], recorded["source_status"]]), \
                mock.patch.object(COLLECTOR, "compiler_version", return_value=recorded["compiler"]), \
                mock.patch.object(COLLECTOR.platform, "platform", return_value=recorded["platform"]):
            self.assertEqual(COLLECTOR.payload(recorded["executable_sha256"], recorded["samples"]), recorded)

    def test_identity_drift_prevents_publication(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = driver(root)
            output = root / "out.json"
            with mock.patch.object(sys, "argv", ["collector", str(binary), "--output", str(output)]), \
                    mock.patch.object(COLLECTOR, "digest", side_effect=["before", "after"]), \
                    mock.patch.object(COLLECTOR, "collect_pairs", return_value=[]), \
                    self.assertRaisesRegex(RuntimeError, "identity changed"):
                COLLECTOR.main()
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
