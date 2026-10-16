"""Exercise the runner's process boundary without benchmark sampling."""

import argparse
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import runstudy
from runstudy import Runner


def sample_result(mode="native", case="vanilla", paths=32768):
    result = {
        "mode": mode, "case": case, "paths": paths, "seconds": 1.0, "pv": 1.0,
        "risks": dict.fromkeys(("spot", "vol", "rate", "div", "STRIKE", "BARRIER"), 1.0),
        "hits": 0, "recordings": 0, "guard_failures": 0,
        "peak_nodes": 0, "graph_bytes": 0, "graph_capacity": 0,
        "forward_seconds": 0.0, "reverse_seconds": 0.0, "hit_seconds": 0.0, "miss_seconds": 0.0,
    }
    if mode in ("replay", "record"):
        result.update(peak_nodes=1, graph_bytes=8, graph_capacity=16)
        result["recordings"] = 1 if mode == "replay" else paths
        result["hits"] = paths - result["recordings"]
    return result


def verification_result():
    return {"verification": "passed", "path_comparisons": 214,
            "named_risks_per_path": 6, "forced_guard_failures": 2}


class OutputRunner:
    binary = Path("synthetic/primal_replay_study")

    def __init__(self, corrupt=lambda value: value, target="reference", rss="1024"):
        self.corrupt = corrupt
        self.target = target
        self.rss = rss
        self.samples = 0

    def study(self, mode, case="vanilla", paths=32768, phases=False, rss=None):
        if mode == "verify":
            value = verification_result()
            return json.dumps(self.corrupt(value) if self.target == "verification" else value)
        value = sample_result(mode, case, paths)
        if phases:
            value.update(peak_nodes=1, graph_bytes=8, graph_capacity=16)
        if rss is not None:
            rss.write_text(self.rss)
            self.samples += 1
        selected = {"reference": self.samples == 1, "sample": self.samples == 2, "diagnostic": phases}
        return json.dumps(self.corrupt(value) if selected.get(self.target, False) else value)

    def tool(self, *_):
        return "synthetic metadata"


def collect_mock_output(root, runner):
    args = argparse.Namespace(binary=runner.binary, output=root, paths=32768, cpu=0, cases=("vanilla",))
    with patch.object(runstudy, "read_options", return_value=args), \
            patch.object(runstudy, "Runner", return_value=runner), \
            patch.object(runstudy, "provenance", return_value={"binary_sha256": "synthetic"}), \
            patch.object(runstudy, "digest", return_value="synthetic"), \
            patch.object(runstudy.os, "sched_setaffinity"), \
            patch.dict(runstudy.os.environ), contextlib.redirect_stdout(io.StringIO()):
        runstudy.main()


class CollectorTest(unittest.TestCase):
    def test_rejects_original_review_probes_at_every_sample_entry(self):
        corruptions = {
            "paths": {"paths": 1}, "mode": {"mode": "record"}, "case": {"case": "crossing"},
            "risk-schema": {"risks": {}}, "negative-time": {"seconds": -1.0},
            "nan-time": {"seconds": float("nan")},
            "counters": {"hits": 999999, "recordings": -10, "guard_failures": -20},
        }
        for target in ("reference", "sample", "diagnostic"):
            for label, changes in corruptions.items():
                with self.subTest(target=target, probe=label), tempfile.TemporaryDirectory() as root:
                    output = Path(root) / "output"
                    runner = OutputRunner(lambda value: dict(value, **changes), target)
                    with self.assertRaises(RuntimeError):
                        collect_mock_output(output, runner)
                    self.assertFalse((output / "results.json").exists())
                    retained = tuple(output.glob("diagnostic-*.json" if target == "diagnostic" else "round*.json"))
                    self.assertTrue(retained)

    def test_rejects_failed_verification_before_sampling(self):
        with tempfile.TemporaryDirectory() as root:
            output = Path(root) / "output"
            runner = OutputRunner(lambda value: dict(value, verification="failed"), "verification")
            with self.assertRaises(RuntimeError):
                collect_mock_output(output, runner)
            self.assertEqual(0, runner.samples)
            self.assertIn('"failed"', (output / "verification.txt").read_text())
            self.assertFalse((output / "results.json").exists())

    def test_valid_output_completes_collection(self):
        with tempfile.TemporaryDirectory() as root:
            output = Path(root) / "output"
            collect_mock_output(output, OutputRunner())
            result = json.loads((output / "results.json").read_text())
            self.assertEqual(100, len(result["samples"]))
            self.assertEqual(4, len(result["diagnostics"]))
            self.assertEqual("no-change", result["summaries"][0]["replay_verdict"])


class OutputValidationTest(unittest.TestCase):
    def test_rejects_missing_and_extra_fields(self):
        for field in sample_result():
            value = sample_result()
            del value[field]
            with self.subTest(missing=field), self.assertRaises(RuntimeError):
                runstudy.validate_result(json.dumps(value), "native", "vanilla", 32768)
        value = dict(sample_result(), extra=1)
        with self.assertRaises(RuntimeError):
            runstudy.validate_result(json.dumps(value), "native", "vanilla", 32768)

    def test_rejects_non_object_and_malformed_json(self):
        for raw in ("[]", "null", "true", '"text"', "{}", "", "{", "{} {}"):
            with self.subTest(raw=raw), self.assertRaises((RuntimeError, ValueError)):
                runstudy.validate_result(raw, "native", "vanilla", 32768)

    def test_rejects_invalid_types_and_nonfinite_values(self):
        corruptions = (
            {"mode": True}, {"case": []}, {"paths": "32768"}, {"paths": 32768.0}, {"paths": True},
            {"seconds": True}, {"seconds": "1.0"}, {"seconds": 0}, {"seconds": float("inf")},
            {"pv": float("nan")}, {"pv": float("-inf")}, {"pv": "1.0"}, {"pv": False}, {"pv": 10 ** 400},
            {"risks": []}, {"risks": dict(sample_result()["risks"], extra=1)},
            {"risks": dict(sample_result()["risks"], spot=None)},
            {"risks": dict(sample_result()["risks"], BARRIER=float("inf"))},
            {"risks": dict(sample_result()["risks"], STRIKE=True)},
        )
        for changes in corruptions:
            with self.subTest(changes=changes), self.assertRaises(RuntimeError):
                runstudy.validate_result(json.dumps(dict(sample_result(), **changes)), "native", "vanilla", 32768)

    def test_rejects_invalid_counter_relationships(self):
        corruptions = (
            ("native", {"hits": 1}), ("caller", {"recordings": 32768}),
            ("native", {"guard_failures": 1}), ("replay", {"recordings": 0}),
            ("replay", {"hits": 32768}), ("replay", {"guard_failures": 1}),
            ("record", {"recordings": 1}), ("record", {"hits": 1}), ("record", {"guard_failures": 1}),
            ("replay", {"hits": -1}), ("replay", {"hits": 32767.0}), ("replay", {"recordings": True}),
        )
        for mode, changes in corruptions:
            with self.subTest(mode=mode, changes=changes), self.assertRaises(RuntimeError):
                runstudy.validate_result(json.dumps(dict(sample_result(mode), **changes)), mode, "vanilla", 32768)

    def test_rejects_invalid_memory(self):
        corruptions = (
            ("native", False, {"peak_nodes": 1}), ("replay", False, {"peak_nodes": 0}),
            ("native", True, {"graph_capacity": -1}), ("native", True, {"graph_bytes": 17}),
            ("native", True, {"peak_nodes": 9}), ("native", True, {"graph_capacity": True}),
            ("native", True, {"graph_bytes": 8.0}), ("native", True, {"peak_nodes": float("nan")}),
        )
        for mode, phases, changes in corruptions:
            value = dict(sample_result(mode), peak_nodes=1, graph_bytes=8, graph_capacity=16)
            value.update(changes)
            with self.subTest(mode=mode, phases=phases, changes=changes), self.assertRaises(RuntimeError):
                runstudy.validate_result(json.dumps(value), mode, "vanilla", 32768, phases)

    def test_rejects_invalid_phases(self):
        corruptions = (
            ("replay", False, {"forward_seconds": 0.1, "hit_seconds": 0.1}),
            ("native", True, {"forward_seconds": -0.1}), ("native", True, {"reverse_seconds": float("nan")}),
            ("native", True, {"forward_seconds": 0.6, "reverse_seconds": 0.6}),
            ("native", True, {"hit_seconds": 0.1}), ("caller", True, {"reverse_seconds": 0.1}),
            ("replay", True, {"forward_seconds": 0.2, "hit_seconds": 0.1}),
            ("record", True, {"forward_seconds": 0.1, "hit_seconds": 0.1}),
            ("native", True, {"reverse_seconds": False}), ("native", True, {"miss_seconds": "0"}),
        )
        for mode, phases, changes in corruptions:
            value = dict(sample_result(mode), peak_nodes=1, graph_bytes=8, graph_capacity=16)
            value.update(changes)
            with self.subTest(mode=mode, phases=phases, changes=changes), self.assertRaises(RuntimeError):
                runstudy.validate_result(json.dumps(value), mode, "vanilla", 32768, phases)

    def test_rejects_invalid_verification_schema_and_counts(self):
        invalid = [[], {}, dict(verification_result(), extra=1)]
        for field in verification_result():
            value = verification_result()
            del value[field]
            invalid.append(value)
        invalid.extend(dict(verification_result(), **changes) for changes in (
            {"verification": True}, {"path_comparisons": 0}, {"path_comparisons": 214.0},
            {"named_risks_per_path": 5}, {"named_risks_per_path": True},
            {"forced_guard_failures": -1}, {"forced_guard_failures": 1},
        ))
        for value in invalid:
            with self.subTest(value=value), tempfile.TemporaryDirectory() as root:
                runner = OutputRunner(lambda _: value, "verification")
                with self.assertRaises(RuntimeError):
                    runstudy.verify_study(runner, Path(root))

    def test_rejects_invalid_rss_before_reference(self):
        for rss in ("", "0", "-1", "nan", "1.5"):
            with self.subTest(rss=rss), tempfile.TemporaryDirectory() as root:
                references = {}
                with self.assertRaises((RuntimeError, ValueError)):
                    runstudy.retain_sample(OutputRunner(rss=rss), Path(root), 32768, "vanilla", "native", "probe", references)
                self.assertEqual({}, references)
                self.assertTrue((Path(root) / "probe.json").is_file())

    def test_accepts_native_aa_single_path_and_diagnostics(self):
        for mode in runstudy.MODES:
            actual = "native" if mode == "native-aa" else mode
            value = sample_result(actual, paths=1)
            value["pv"] = -1.0
            with self.subTest(mode=mode):
                self.assertEqual(value, runstudy.validate_result(json.dumps(value), mode, "vanilla", 1))
        value = dict(sample_result("replay"), peak_nodes=1, graph_bytes=8, graph_capacity=16,
                     forward_seconds=0.3, reverse_seconds=0.1, hit_seconds=0.2, miss_seconds=0.1)
        self.assertEqual(value, runstudy.validate_result(json.dumps(value), "replay", "vanilla", 32768, True))
        value.update(hits=0, recordings=32768, guard_failures=32767, hit_seconds=0, miss_seconds=0.3)
        self.assertEqual(value, runstudy.validate_result(json.dumps(value), "replay", "vanilla", 32768, True))


def local_runner(root):
    binary = root / "primal_replay_study"
    binary.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\n')
    binary.chmod(0o700)
    return Runner(binary)


class RunnerTest(unittest.TestCase):
    def test_failed_process_rejects_and_preserves_output(self):
        with tempfile.TemporaryDirectory() as root:
            runner = local_runner(Path(root))
            runner.binary.write_text('#!/bin/sh\nprintf \'{"verification":"passed"}\'; printf "failed" >&2; exit 7\n')
            with self.assertRaises(runstudy.subprocess.CalledProcessError) as failure:
                runner.study("verify")
            self.assertEqual(7, failure.exception.returncode)
            self.assertIn('"passed"', failure.exception.stdout)
            self.assertEqual("failed", failure.exception.stderr)

    def test_executable_path_with_shell_characters_is_literal(self):
        with tempfile.TemporaryDirectory(prefix="primal ; $(false) ' ") as root:
            runner = local_runner(Path(root))
            self.assertEqual(["--mode", "verify"], runner.study("verify").splitlines())
            self.assertEqual(["--mode", "replay", "--case", "barrier", "--paths", "17"],
                             runner.study("replay", "barrier", 17).splitlines())

    def test_rejects_program_outside_allowlist(self):
        with tempfile.TemporaryDirectory() as root:
            runner = local_runner(Path(root))
            with self.assertRaises(ValueError):
                runner.capture(["/bin/sh", "-c", "exit 0"])

    def test_rejects_unsupported_study_requests(self):
        with tempfile.TemporaryDirectory() as root:
            runner = local_runner(Path(root))
            for mode, case, paths in (("replay;false", "vanilla", 17), ("replay", "barrier;false", 17),
                                      ("replay", "vanilla", 0), ("replay", "vanilla", 1000001)):
                with self.assertRaises(ValueError):
                    runner.study(mode, case, paths)


if __name__ == "__main__":
    unittest.main()
