"""Tests for reproducible benchmark-environment metadata."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "write_benchmark_metadata.py"


class BenchmarkMetadataTest(unittest.TestCase):
    def test_cli_records_provenance_and_toolchain(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "environment.json"

            completed = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--output",
                    str(output),
                    "--head-sha",
                    "head-sha",
                    "--base-sha",
                    "base-sha",
                    "--compiler",
                    sys.executable,
                    "--aad-backend",
                    "aadet",
                    "--threads",
                    "4",
                    "--runner-image",
                    "test-image",
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertEqual(completed.returncode, 0, completed.stderr)
            metadata = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(metadata["head_sha"], "head-sha")
            self.assertEqual(metadata["base_sha"], "base-sha")
            self.assertEqual(metadata["aad_backend"], "aadet")
            self.assertEqual(metadata["dal_num_threads"], 4)
            self.assertEqual(metadata["runner_image"], "test-image")
            self.assertTrue(metadata["cpu_model"])
            self.assertIn("Python", metadata["compiler_version"])
            self.assertIn("cmake", metadata["cmake_version"].lower())


if __name__ == "__main__":
    unittest.main()
