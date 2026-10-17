"""Validate the study runner's executable boundary and retained measurement evidence."""

from contextlib import redirect_stdout
import io
import json
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import pairedstudy as study


CASE = ("weekly", 32, "eval", 16387, True)


def record(engine="scalar", seconds=0.01):
    return dict(workload="weekly", engine=engine, phase="eval", paths=16387, lanes=32,
                iterations=1, fallback=True, events=54, shock_fingerprint="fixed", seconds=seconds, pv=7.0)


def executable(directory):
    binary = directory / "compiled_soa_study"
    binary.touch()
    binary.chmod(0o700)
    return binary


class PairedStudyTest(unittest.TestCase):
    def test_executable_rejects_missing_directory_and_unexpected_name(self):
        with tempfile.TemporaryDirectory() as root:
            directory = Path(root)
            with self.assertRaises(FileNotFoundError):
                study.native_executable(directory / "compiled_soa_study")
            with self.assertRaises(ValueError):
                study.native_executable(directory)
            wrong = directory / "unrelated"
            wrong.touch()
            with self.assertRaises(ValueError):
                study.native_executable(wrong)

    @unittest.skipIf(os.name == "nt", "POSIX executable permissions and symlinks")
    def test_executable_checks_permissions_and_resolved_target(self):
        with tempfile.TemporaryDirectory() as root:
            directory = Path(root)
            binary = executable(directory)
            binary.chmod(0o600)
            with self.assertRaises(PermissionError):
                study.native_executable(binary)
            target = directory / "unexpected"
            binary.rename(target)
            binary.symlink_to(target)
            with self.assertRaises(ValueError):
                study.native_executable(binary)

    def test_run_uses_resolved_argv_without_shell_and_retains_raw_output(self):
        with tempfile.TemporaryDirectory(prefix="study ; literal ") as root:
            binary = executable(Path(root))
            output = Path(root) / "sample.json"
            completed = study.subprocess.CompletedProcess([], 0, json.dumps(record()), "diagnostic")
            with patch.object(study.subprocess, "run", return_value=completed) as process:
                self.assertEqual(study.run(binary, CASE, "scalar", 1, output), record())
            args, kwargs = process.call_args
            self.assertEqual(args[0], [str(binary.resolve()), "--workload", "weekly", "--lanes", "32",
                                      "--phase", "eval", "--paths", "16387", "--fallback", "true",
                                      "--engine", "scalar", "--iterations", "1"])
            self.assertIs(kwargs["shell"], False)
            self.assertEqual(json.loads(output.read_text())["stderr"], "diagnostic")

    def test_run_rejects_failure_and_nonfinite_or_nonpositive_measurements(self):
        with tempfile.TemporaryDirectory() as root:
            binary = executable(Path(root))
            output = Path(root) / "sample.json"
            failed = study.subprocess.CompletedProcess([], 9, "", "failed")
            with patch.object(study.subprocess, "run", return_value=failed), \
                    self.assertRaises(study.subprocess.CalledProcessError):
                study.run(binary, CASE, "scalar", 1, output)
            self.assertEqual(json.loads(output.read_text())["returncode"], 9)
            for field, value in (("seconds", 0), ("seconds", float("inf")), ("pv", float("nan"))):
                invalid = dict(record(), **{field: value})
                completed = study.subprocess.CompletedProcess([], 0, json.dumps(invalid), "")
                with self.subTest(field=field, value=value), \
                        patch.object(study.subprocess, "run", return_value=completed), self.assertRaises(ValueError):
                    study.run(binary, CASE, "scalar", 1, output)

    def test_pair_rejects_changed_identity_and_out_of_tolerance_pv(self):
        for field, value in (("paths", 99), ("shock_fingerprint", "changed"), ("pv", 7.0 + 1e-10)):
            with self.subTest(field=field), self.assertRaises(ValueError):
                study.verify_pair(record(), dict(record(), **{field: value}))
        study.verify_pair(record(), dict(record(), pv=7.0 + 1e-12))
        with self.assertRaises(ValueError):
            study.verify_pair(dict(record(), pv=0.0), dict(record(), pv=2e-14))

    def test_run_rejects_wrong_request_fields_and_missing_engine(self):
        faults = (("workload", "vanilla"), ("phase", "end_to_end"), ("paths", 1), ("lanes", 8),
                  ("iterations", 100), ("fallback", False), ("engine", "scalar"), ("paths", True))
        responses = [dict(record("soa"), **{key: value}) for key, value in faults]
        missing = record("soa")
        del missing["engine"]
        responses.append(missing)
        with tempfile.TemporaryDirectory() as root:
            binary = executable(Path(root))
            for response in responses:
                completed = study.subprocess.CompletedProcess([], 0, json.dumps(response), "")
                with self.subTest(response=response), patch.object(study.subprocess, "run", return_value=completed), \
                        self.assertRaises(ValueError):
                    study.run(binary, CASE, "soa", 1, Path(root) / "sample.json")

    def test_measure_preserves_two_best_of_ten_rounds_and_alternating_pairs(self):
        with tempfile.TemporaryDirectory() as root:
            directory = Path(root)
            binary = executable(directory)
            args = SimpleNamespace(baseline=binary, head=binary, output=directory / "results")

            def sample(_binary, _case, engine, iterations, _output):
                return dict(record(engine, 0.01 if engine == "scalar" else 0.004), iterations=iterations)

            with patch.object(study, "cases", return_value=[CASE]), \
                    patch.object(study, "run", side_effect=sample) as process, redirect_stdout(io.StringIO()):
                study.measure(args)
            self.assertEqual(process.call_count, 42)
            result = json.loads((args.output / "results.json").read_text())[0]
            self.assertEqual(result["round_minima"], [{"scalar": 0.01, "soa": 0.004}] * 2)
            self.assertEqual(result["target_2x"], "achieved")
            self.assertEqual(result["speedup"], [2.5, 2.5])
            self.assertEqual([row["order"] for row in result["samples"]], [["scalar", "soa"], ["soa", "scalar"]] * 10)
            self.assertEqual([row["round"] for row in result["samples"]], [0] * 10 + [1] * 10)


if __name__ == "__main__":
    unittest.main()
