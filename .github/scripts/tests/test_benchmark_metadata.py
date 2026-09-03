"""Tests for reproducible benchmark-environment metadata."""

import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / "write_benchmark_metadata.py"


def load_metadata_script():
    spec = importlib.util.spec_from_file_location("write_benchmark_metadata", SCRIPT)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load {SCRIPT}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class BenchmarkMetadataTest(unittest.TestCase):
    def test_cli_records_provenance_and_toolchain(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "environment.json"

            argv = [
                str(SCRIPT),
                "--output",
                str(output),
                "--head-sha",
                "head-sha",
                "--base-sha",
                "base-sha",
                "--compiler",
                "python3",
                "--aad-backend",
                "aadet",
                "--threads",
                "4",
                "--runner-image",
                "test-image",
            ]
            with mock.patch.object(sys, "argv", argv):
                self.assertEqual(load_metadata_script().main(), 0)
            metadata = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(metadata["head_sha"], "head-sha")
            self.assertEqual(metadata["base_sha"], "base-sha")
            self.assertEqual(metadata["aad_backend"], "aadet")
            self.assertEqual(metadata["dal_num_threads"], 4)
            self.assertEqual(metadata["runner_image"], "test-image")
            self.assertTrue(metadata["cpu_model"])
            self.assertIn("Python", metadata["compiler_version"])
            # cmake is absent on some contributor machines; the script records
            # "unavailable: ..." there, so assert against the environment we have.
            if shutil.which("cmake") is None:
                self.assertTrue(metadata["cmake_version"].startswith("unavailable"))
            else:
                self.assertIn("cmake", metadata["cmake_version"].lower())


if __name__ == "__main__":
    unittest.main()
