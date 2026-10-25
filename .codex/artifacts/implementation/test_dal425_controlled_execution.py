"""Finite controls for the private execution candidate."""
import importlib.util
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

SOURCE = Path(__file__).with_name("dal425-controlled-execution.py")
SPEC = importlib.util.spec_from_file_location("controlled", SOURCE)
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class ControlledTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.ledger = self.root / "ledger.jsonl"
        self.ledger.write_text(json.dumps({"kind": "allocation", "domain": "synthetic",
            "wall": 10.0, "cpu": 10.0, "bytes": 1048576, "units": 100,
            "original_allocation": "UNKNOWN", "product_actual": 0}) + "\n")
        self.identity = {"source": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
            "python": hashlib.sha256(Path(sys.executable).resolve().read_bytes()).hexdigest()}

    def records(self):
        return [json.loads(line) for line in self.ledger.read_text().splitlines()]

    def test_invalid_budgets_reject_before_spawn(self):
        for values in ({"wall": -1}, {"cpu": float("nan")}, {"units": -1},
                       {"output_bytes": -1}, {"products": 1}, {"action": "compile"}):
            with self.subTest(values=values), patch("subprocess.Popen") as spawn:
                with self.assertRaisesRegex(ValueError, "budget|disabled|action"):
                    TOOL.run(self.root, self.identity, **values)
                spawn.assert_not_called()

    def test_bad_ledger_rejects_before_spawn(self):
        self.ledger.write_text("broken\n")
        with patch("subprocess.Popen") as spawn:
            with self.assertRaisesRegex(ValueError, "ledger"):
                TOOL.run(self.root, self.identity)
            spawn.assert_not_called()

    def test_normal_exit_settles_real_wait4(self):
        result = TOOL.run(self.root, self.identity)
        self.assertEqual(result.get("status"), "complete")
        self.assertEqual(result["exit"], 0)
        self.assertEqual(len(self.records()), 3)
        self.assertGreater(result["child_cpu"], 0)
        self.assertEqual(result["product_actual"], 0)

    def test_nonzero_exit_is_partial_and_blocks_successor(self):
        result = TOOL.run(self.root, self.identity, action="nonzero")
        self.assertEqual(result.get("status"), "partial")
        self.assertEqual(result["exit"], 7)
        with self.assertRaisesRegex(ValueError, "UNKNOWN|partial"):
            TOOL.run(self.root, self.identity)

    def test_cpu_stop_reaps_owned_child(self):
        result = TOOL.run(self.root, self.identity, action="burn", cpu=.04, wall=1)
        self.assertEqual(result.get("reason"), "cpu")
        self.assertLess(result["wall"], 1)
        self.assertFalse(Path("/proc", str(result["pid"])).exists())

    def test_deadline_stop_reaps_owned_child(self):
        result = TOOL.run(self.root, self.identity, action="burn", cpu=1, wall=.04)
        self.assertEqual(result.get("reason"), "wall")
        self.assertFalse(Path("/proc", str(result["pid"])).exists())

    def test_output_overage_preserves_small_evidence(self):
        result = TOOL.run(self.root, self.identity, action="flood", output_bytes=1)
        self.assertEqual(result.get("reason"), "output")
        self.assertLessEqual(len(result["stdout"]), 1)

    def test_pending_reservation_is_not_refunded_on_restart(self):
        with self.ledger.open("a") as stream:
            stream.write(json.dumps({"kind": "reserve", "id": 1, "wall": .5,
                "cpu": .5, "units": 1}) + "\n")
        before = self.ledger.read_bytes()
        with patch("subprocess.Popen") as spawn:
            with self.assertRaisesRegex(ValueError, "UNKNOWN"):
                TOOL.run(self.root, self.identity)
            spawn.assert_not_called()
        self.assertEqual(self.ledger.read_bytes(), before)

    def test_missing_identity_rejects_before_spawn(self):
        with tempfile.TemporaryDirectory() as root:
            with self.assertRaisesRegex(ValueError, "identity"):
                TOOL.run(Path(root), None)


if __name__ == "__main__":
    unittest.main()
