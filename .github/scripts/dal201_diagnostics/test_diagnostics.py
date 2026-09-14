import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile

import diagnostics as diag
import worker
import fetch_artifact
import phases


class IdentityTest(unittest.TestCase):
    def test_archive_traversal_is_rejected_before_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "package.tar.gz"
            with tarfile.open(archive, "w:gz") as output:
                member = tarfile.TarInfo("../escaped")
                member.size = 1
                output.addfile(member, io.BytesIO(b"x"))
            with self.assertRaisesRegex(ValueError, "archive member"):
                diag.extract_package(archive, root / "package")
            self.assertFalse((root / "escaped").exists())

    def test_wrong_hash_never_reaches_import(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "native.so"
            path.write_bytes(b"changed extension")
            with self.assertRaisesRegex(ValueError, "SHA256"):
                diag.verify_hash(path, "0" * 64)

    def test_missing_isa_and_wrong_abi_are_explicit(self):
        with self.assertRaisesRegex(ValueError, "avx512f"):
            diag.require_runtime({"sse2", "avx2"}, "cpython-313-x86_64-linux-gnu")
        with self.assertRaisesRegex(ValueError, "ABI"):
            diag.require_runtime(set(diag.REQUIRED_FLAGS), "cpython-312-x86_64-linux-gnu")

    def test_import_signal_and_timeout_keep_raw_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = diag.run_process(
                [sys.executable, "-c", "import os,signal; print('before import',flush=True); os.kill(os.getpid(),signal.SIGILL)"],
                root / "signal", timeout=5,
            )
            self.assertEqual(result["returncode"], -4)
            self.assertIn("before import", (root / "signal/stdout.txt").read_text())
            result = diag.run_process(
                [sys.executable, "-c", "import time; print('started',flush=True); time.sleep(5)"],
                root / "timeout", timeout=0.1,
            )
            self.assertEqual(result["status"], "timeout")
            self.assertIn("started", (root / "timeout/stdout.txt").read_text())
            self.assertEqual(json.loads((root / "timeout/process.json").read_text())["status"], "timeout")


class BoundaryTest(unittest.TestCase):
    def test_tag_wraps_only_execution_and_preserves_validator_failures(self):
        from types import SimpleNamespace
        calls = []
        def validate(value):
            calls.append(("validate", value))
            raise ValueError("real validator failed")
        work = SimpleNamespace(run=lambda: calls.append("run") or 42, validate=validate)
        wrapped = worker.tag_workload(work, 2, calls.append, lambda run, validate: SimpleNamespace(run=run, validate=validate))
        value = wrapped.run()
        self.assertEqual(calls, [2, "run", 0])
        with self.assertRaisesRegex(ValueError, "real validator failed"):
            wrapped.validate(value)
        self.assertEqual(calls[-1], ("validate", 42))

    def test_exception_clears_measurement_tag(self):
        from types import SimpleNamespace
        calls = []
        def fail():
            raise ValueError("native failure")
        wrapped = worker.tag_workload(SimpleNamespace(run=fail, validate=lambda x: None), 1, calls.append,
                                      lambda run, validate: SimpleNamespace(run=run, validate=validate))
        with self.assertRaisesRegex(ValueError, "native failure"):
            wrapped.run()
        self.assertEqual(calls, [1, 0])

    def test_full_prefix_cannot_drop_or_reorder_a_case(self):
        from types import SimpleNamespace
        cases = [SimpleNamespace(name=name) for name in ("prefix", *diag.TARGETS)]
        self.assertEqual(worker.prefix(cases, [case.name for case in cases]), cases)
        with self.assertRaisesRegex(ValueError, "prefix"):
            worker.prefix(cases[1:], [case.name for case in cases])

    def test_schedule_preserves_twenty_samples_and_copy_controls(self):
        jobs = diag.schedule()
        self.assertEqual(len(jobs), 80)
        for arm in diag.ARMS:
            self.assertEqual([i for i, label in jobs if label == arm], list(range(1, 21)))
        self.assertEqual([arm for i, arm in jobs[:8]], [*diag.ARMS, *diag.ARMS[::-1]])

    def test_missing_harvest_cannot_be_reported_as_zero_cost(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            records = [{"tag": 2, "phase": "target_inclusive", "invocation": index, "ns": 10} for index in range(4)]
            (root / "phases.jsonl").write_text("\n".join(json.dumps(row) for row in records))
            with self.assertRaisesRegex(ValueError, "missed or duplicated"):
                phases.verify_phases(root)

    def test_invalid_or_ambiguous_symbol_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "ELF"):
            phases.resolve_gradient(b"not an ELF file", "", "", "")
        with self.assertRaisesRegex(ValueError, "ambiguous"):
            phases.unique([1234, 1235], "Gradient vtable")

    def test_child_phase_cannot_exceed_its_parent_boundary(self):
        records = []
        for tag, harvests in ((2, 3), (3, 1)):
            for invocation in range(4):
                records.append({"tag": tag, "phase": "target_inclusive", "invocation": invocation, "ns": 1000})
                for index in range(harvests):
                    for phase, ns in (("gradient_inclusive", 100), ("register", 10), ("recording_to_harvest", 20), ("harvest", 200)):
                        records.append({"tag": tag, "phase": phase, "invocation": invocation, "gradient": index,
                                        "ns": ns, "rows": 25, "cols": 25, "nodes": 2511})
        with self.assertRaisesRegex(ValueError, "phase boundary"):
            phases.summarize_phases(records)


class DeliveryTest(unittest.TestCase):
    def test_api_pr_head_is_distinct_from_benchmark_merge_source(self):
        lock = diag.load_lock()
        metadata = {"id": lock["artifact_id"], "name": lock["artifact_name"], "expired": False,
                    "digest": "sha256:" + lock["artifact_zip_sha256"],
                    "workflow_run": {"id": lock["run_id"], "head_sha": lock["published_sha"]}}
        fetch_artifact.verify_metadata(metadata, lock)
        metadata["workflow_run"]["head_sha"] = lock["source_shas"]["head"]
        with self.assertRaisesRegex(ValueError, "source"):
            fetch_artifact.verify_metadata(metadata, lock)

    def test_zip_cannot_escape_evidence_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "artifact.zip"
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr("../escape", "bad")
            with self.assertRaisesRegex(ValueError, "artifact member"):
                fetch_artifact.extract_zip(archive, root / "extracted")
            self.assertFalse((root / "escape").exists())

    def test_workflow_isolated_trigger_readonly_and_failure_upload(self):
        import yaml
        root = Path(__file__).resolve().parents[3]
        path = root / ".github/workflows/dal-201-hosted-diagnostics.yml"
        workflow = yaml.load(path.read_text(), Loader=yaml.BaseLoader)
        self.assertEqual(workflow["on"], {"push": {"branches": ["fix/dal-201-hosted-phase-diagnostics"]}})
        self.assertEqual(workflow["permissions"], {"contents": "read", "actions": "read"})
        job = workflow["jobs"]["diagnose"]
        self.assertEqual(job["timeout-minutes"], "40")
        self.assertIn("refs/heads/fix/dal-201-hosted-phase-diagnostics", job["if"])
        old_workflow = (root / ".github/workflows/cmake-linux.yml").read_text()
        for step in job["steps"]:
            if "uses" in step:
                self.assertIn(step["uses"], old_workflow)
            if "run" in step:
                result = subprocess.run(["bash", "-n"], input=step["run"], text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stderr)
        upload = job["steps"][-1]
        self.assertEqual(upload["if"], "always()")
        self.assertEqual(upload["with"]["path"], "dal201-evidence")
        self.assertEqual(upload["with"]["if-no-files-found"], "error")
        self.assertNotIn("secrets.", path.read_text())


if __name__ == "__main__":
    unittest.main()
