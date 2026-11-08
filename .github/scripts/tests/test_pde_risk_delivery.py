import contextlib
import importlib.util
import io
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[3]


def load_tool(relative):
    spec = importlib.util.spec_from_file_location("pde_risk_contract", ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    with mock.patch.object(sys, "path", [str(ROOT / ".github/scripts"), *sys.path]), mock.patch.dict(sys.modules, {
        "dal": types.ModuleType("dal"),
        "_dal_pde_risk_test": types.ModuleType("_dal_pde_risk_test"),
        "float_format": types.SimpleNamespace(format_float=str),
    }):
        spec.loader.exec_module(module)
    return module


class RuntimeChecksTest(unittest.TestCase):
    def test_availability_mismatch_is_rejected_in_optimized_python(self):
        example = load_tool("dal-python/examples/019.early_exercise_pde_risk.py")
        row = types.SimpleNamespace(status="AVAILABLE", value=None, risk_id="Gamma", unit="unit")
        result = types.SimpleNamespace(status="PARTIAL", method="method", price_available=True, price=1.0, risks=[row])
        with contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(RuntimeError):
                example.display("invalid", result)

    def test_projection_drift_is_rejected_in_optimized_python(self):
        projection = load_tool("dal-python/benchmarks/pde_risk_projection.py")
        result = types.SimpleNamespace(execution=types.SimpleNamespace(work_units=1, total_solves=2, numeric_payload_bytes=3))
        before = (1, 2, 3)
        reference = {"records": 22, "work": 1, "solves": 2, "payload": 3}
        for field in ("records", "work", "solves", "payload"):
            invalid = dict(reference)
            invalid[field] += 1
            with self.subTest(field=field), self.assertRaises(RuntimeError):
                projection.validate_projection(result, result, before, 22, invalid, 1, 22)
        changed = types.SimpleNamespace(execution=types.SimpleNamespace(work_units=2, total_solves=2, numeric_payload_bytes=3))
        with self.assertRaises(RuntimeError):
            projection.validate_projection(result, changed, before, 22, reference, 1, 22)


def build_path(root):
    binary = root / "dal-cpp/benchmarks/pde_perf/pde_perf"
    binary.parent.mkdir(parents=True)
    binary.write_text("mocked native executable")
    binary.chmod(0o755)
    return binary


class SamplingContractTest(unittest.TestCase):
    def test_arguments_reject_noncanonical_commands(self):
        tool = load_tool(".github/scripts/measure_pde_risk.py")
        invalid = [
            ("--help",), ("--risk", "Bermudan17", "risk;echo", 1),
            ("--risk", "../case", "risk", 1), ("--risk", "Bermudan17", "risk", True),
            ("--risk", "Bermudan17", "risk", "1"), ("--risk", "Bermudan17", "risk", 0),
            ("--risk", "Bermudan17", "risk", 10001),
        ]
        for args in invalid:
            with self.subTest(arguments=args), self.assertRaises(ValueError):
                tool.validated_arguments(args)

    def test_failed_process_retains_outputs_and_exit_status(self):
        tool = load_tool(".github/scripts/measure_pde_risk.py")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build_path(root / "build")
            args = types.SimpleNamespace(base_build=root / "build", head_build=root / "build", output=root / "output")
            recorder = tool.Recorder(args)
            failed = types.SimpleNamespace(returncode=7, stdout="partial output", stderr="failed")
            with mock.patch.object(tool.subprocess, "run", return_value=failed) as run:
                with self.assertRaises(RuntimeError):
                    recorder.run("failed", "head_risk", ("--risk", "Bermudan17", "risk", 1))
            self.assertFalse(run.call_args.kwargs["shell"])
            self.assertEqual(recorder.invocations[0]["returncode"], 7)
            self.assertEqual((recorder.raw / "failed.stdout").read_text(), "partial output")
            self.assertEqual((recorder.raw / "failed.stderr").read_text(), "failed")

    def test_executable_symlink_cannot_escape_build_root(self):
        tool = load_tool(".github/scripts/measure_pde_risk.py")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            outside = build_path(root / "outside")
            binary = root / "build/dal-cpp/benchmarks/pde_perf/pde_perf"
            binary.parent.mkdir(parents=True)
            try:
                binary.symlink_to(outside)
            except OSError as error:
                self.skipTest(str(error))
            args = types.SimpleNamespace(base_build=root / "build", head_build=root / "build", output=root / "output")
            with self.assertRaises(ValueError):
                tool.Recorder(args)
