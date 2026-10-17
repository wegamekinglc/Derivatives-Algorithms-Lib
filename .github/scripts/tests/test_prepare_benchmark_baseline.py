"""Tests for the historical benchmark timing-assertion migration."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


class PrepareBenchmarkBaselineTest(unittest.TestCase):
    @staticmethod
    def _module():
        script = Path(__file__).resolve().parents[1] / "prepare_benchmark_baseline.py"
        spec = importlib.util.spec_from_file_location("prepare_benchmark_baseline", script)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_removes_only_the_legacy_timing_assertion_and_retains_patch(self):
        module = self._module()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / module.SOURCE_PATH
            source.parent.mkdir(parents=True)
            correctness = '            REQUIRE(actual == expected, "Numerical oracle failed");\n'
            source.write_text(correctness + module.LEGACY_ASSERTION + "        }\n", encoding="utf-8")

            module.prepare_baseline(root, root / "evidence")

            self.assertEqual(source.read_text(encoding="utf-8"), correctness + "        }\n")
            manifest = json.loads((root / "evidence/manifest.json").read_text(encoding="utf-8"))
            self.assertTrue(manifest["adjusted"])
            self.assertNotEqual(manifest["original_sha256"], manifest["prepared_sha256"])
            patch = (root / "evidence/timing-assertion.patch").read_text(encoding="utf-8")
            self.assertIn("-" + module.LEGACY_ASSERTION.rstrip(), patch)
            self.assertIn(" " + correctness.rstrip(), patch)

    def test_prepared_baseline_is_idempotent(self):
        module = self._module()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / module.SOURCE_PATH
            source.parent.mkdir(parents=True)
            source.write_text("// Timing is evaluated by the process-level gate.\n", encoding="utf-8")

            module.prepare_baseline(root, root / "evidence")

            manifest = json.loads((root / "evidence/manifest.json").read_text(encoding="utf-8"))
            self.assertFalse(manifest["adjusted"])
            self.assertEqual(manifest["original_sha256"], manifest["prepared_sha256"])

    def test_unrecognized_or_duplicate_assertion_is_rejected_without_editing(self):
        module = self._module()
        for text in (module.LEGACY_ASSERTION * 2, module.LEGACY_ASSERTION.replace("20.0", "25.0")):
            with self.subTest(text=text), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                source = root / module.SOURCE_PATH
                source.parent.mkdir(parents=True)
                source.write_text(text, encoding="utf-8")

                with self.assertRaises(ValueError):
                    module.prepare_baseline(root, root / "evidence")

                self.assertEqual(source.read_text(encoding="utf-8"), text)

    def test_workflow_prepares_the_baseline_before_compilation_and_retains_evidence(self):
        workflow = Path(__file__).resolve().parents[2] / "workflows/benchmarks.yml"
        text = workflow.read_text(encoding="utf-8")

        preparation = text.index("- name: Prepare historical benchmark timing check")
        self.assertLess(text.index("- name: Test benchmark regression gate"), preparation)
        self.assertLess(preparation, text.index("- name: Build base benchmarks"))
        self.assertIn("--source-root benchmark-base", text)
        self.assertIn("--evidence-dir benchmark-results/baseline-timing-adjustment", text)
