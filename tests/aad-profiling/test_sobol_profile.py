"""Protect the CLI checker against accepting corrupt native output under -O."""

from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import check_sobol_profile as checker


def native_result(command, **_kwargs):
    arguments = command[2:]
    if len(arguments) != 4 or arguments[0] not in ("1", "52", "366"):
        return SimpleNamespace(returncode=2, stdout="", stderr="invalid request")
    dimension, paths, precision, repeats = arguments
    if (paths, repeats) != ("32", "2") or precision not in ("uniform", "fast", "precise"):
        return SimpleNamespace(returncode=2, stdout="", stderr="invalid request")
    rows = [dict(record="sobol", sample=sample, dimensions=int(dimension), paths=32,
                 precision=precision, wall_ns=1, fingerprint=f"{dimension}-{precision}")
            for sample in range(2)]
    return SimpleNamespace(returncode=0, stdout="\n".join(map(json.dumps, rows)), stderr="")


def corrupt_result(command, fault, **kwargs):
    result = native_result(command, **kwargs)
    if result.returncode:
        return result
    if fault == "returncode":
        return SimpleNamespace(returncode=1, stdout=result.stdout, stderr="native failure")
    rows = [json.loads(line) for line in result.stdout.splitlines()]
    if fault == "row_count":
        rows.pop()
    else:
        field, value = fault
        for row in rows:
            row[field] = value
    return SimpleNamespace(returncode=0, stdout="\n".join(map(json.dumps, rows)), stderr="")


class SobolCheckerTest(unittest.TestCase):
    def invoke(self, process):
        with tempfile.TemporaryDirectory() as root:
            binary = Path(root) / "dal-cpp" / "benchmarks" / "rng_perf" / "rng_perf"
            binary.parent.mkdir(parents=True)
            binary.touch()
            with patch.object(sys, "argv", ["check_sobol_profile.py", str(binary)]), \
                    patch.object(checker.subprocess, "run", side_effect=process) as run, \
                    redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                try:
                    result = checker.main()
                except AssertionError:
                    result = 1
            return 0 if result is None else result, run.call_args_list

    def test_valid_output_replays_every_request(self):
        status, calls = self.invoke(native_result)
        self.assertEqual(status, 0)
        successful = [call for call in calls if native_result(call.args[0]).returncode == 0]
        self.assertEqual(len(successful), 18)

    def test_corrupt_output_fails(self):
        faults = ("returncode", "row_count", ("dimensions", 99), ("paths", 99),
                  ("precision", "unknown"), ("wall_ns", 0), ("fingerprint", "identical"))
        for fault in faults:
            with self.subTest(fault=fault):
                status, _ = self.invoke(lambda command, **kwargs: corrupt_result(command, fault, **kwargs))
                self.assertEqual(status, 1)

    def test_changed_replay_fails(self):
        calls = []

        def process(command, **kwargs):
            result = native_result(command, **kwargs)
            if result.returncode:
                return result
            calls.append(command)
            if len(calls) == 2:
                return corrupt_result(command, ("fingerprint", "changed"), **kwargs)
            return result

        status, _ = self.invoke(process)
        self.assertEqual(status, 1)

    def test_executable_must_be_a_build_tree_file(self):
        with tempfile.TemporaryDirectory() as root:
            binary = Path(root) / "dal-cpp" / "benchmarks" / "rng_perf" / "rng_perf"
            binary.mkdir(parents=True)
            with self.assertRaises(ValueError):
                checker.native_executable(binary)
            misplaced = Path(root) / "rng_perf"
            misplaced.touch()
            with self.assertRaises(ValueError):
                checker.native_executable(misplaced)

    def test_accepted_invalid_request_fails(self):
        def process(command, **kwargs):
            result = native_result(command, **kwargs)
            result.returncode = 0
            return result

        status, _ = self.invoke(process)
        self.assertEqual(status, 1)


if __name__ == "__main__":
    unittest.main()
