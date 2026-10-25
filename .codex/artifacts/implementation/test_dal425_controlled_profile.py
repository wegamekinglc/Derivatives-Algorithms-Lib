"""Data-only controls for the fixed outer protocol; no worker entry exists."""
import importlib.util
from pathlib import Path
import unittest
import tempfile
from unittest.mock import patch

PATH = Path(__file__).with_name("dal425-controlled-profile.py")
spec = importlib.util.spec_from_file_location("profile_candidate", PATH)
TOOL = importlib.util.module_from_spec(spec)
spec.loader.exec_module(TOOL)


class ProfileTest(unittest.TestCase):
    def test_rehashed_source_cannot_hide_loaded_code_difference(self):
        old_source = b"def value(): return 1\n"
        new_source = b"def value(): return 2\n"
        loaded = compile(old_source, "fixed.py", "exec", dont_inherit=True)
        with self.assertRaisesRegex(ValueError, "loaded code/source mismatch"):
            TOOL.verify_loaded_code(loaded, new_source, "fixed.py")

    def test_exhausted_budget_rejects_before_any_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            attempt = TOOL.OneShot(root, {"wall": 0, "cpu": 0, "bytes": 0, "files": 0})
            with self.assertRaisesRegex(ValueError, "outer budget"):
                attempt.reserve()
            self.assertEqual(list(root.iterdir()), [])

    def test_interrupted_reservation_blocks_restart(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            budget = {"wall": 4, "cpu": 6, "bytes": 1048576, "files": 20}
            with patch.object(TOOL.os, "fsync", side_effect=OSError("injected")):
                with self.assertRaises(OSError):
                    TOOL.OneShot(root, budget).reserve()
            self.assertTrue((root / "outer.pending").exists())
            with self.assertRaisesRegex(ValueError, "pending"):
                TOOL.OneShot(root, budget).reserve()

    def test_child_entry_without_outer_ticket_is_zero_dispatch(self):
        with patch.object(TOOL.sys, "argv", ["fixed", "--child", "profile", "/unused"]), \
                patch.object(TOOL, "_child") as child, patch.object(TOOL.os, "chdir"):
            with self.assertRaisesRegex(ValueError, "sealed outer ticket"):
                TOOL.main()
            child.assert_not_called()

    def test_output_slots_do_not_refund_after_delete_or_fsync_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            budget = {"wall": 4, "cpu": 6, "bytes": 1048576, "files": 20}
            attempt = TOOL.OneShot(root, budget)
            attempt.reserve()
            attempt.write("fixture.json", b"{}")
            (root / "fixture.json").unlink()
            with self.assertRaisesRegex(ValueError, "slot exhausted"):
                attempt.write("fixture.json", b"{}")
            with patch.object(TOOL.os, "fsync", side_effect=OSError("settle interrupted")):
                with self.assertRaises(OSError):
                    attempt.write("outer.receipt.json", b"complete")
            with self.assertRaisesRegex(ValueError, "pending"):
                TOOL.OneShot(root, budget).reserve()

    def test_bad_budget_types_and_output_cap_refuse_without_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for wall in (True, -1, float("nan")):
                with self.assertRaisesRegex(ValueError, "outer budget"):
                    TOOL.OneShot(root, {"wall": wall, "cpu": 6, "bytes": 1048576, "files": 20}).reserve()
            self.assertEqual(list(root.iterdir()), [])
            attempt = TOOL.OneShot(root, {"wall": 4, "cpu": 6, "bytes": 1048576, "files": 20})
            attempt.reserve()
            with self.assertRaisesRegex(ValueError, "slot exhausted"):
                attempt.write("fixture.json", b"x" * (TOOL.OUTPUTS["fixture.json"] + 1))
            with self.assertRaisesRegex(ValueError, "slot exhausted"):
                attempt.write("../escape", b"x")
            self.assertFalse((root / "fixture.json").exists())

    def test_observer_failure_after_reserve_keeps_pending(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "budget.json").write_text('{"wall":4,"cpu":6,"bytes":1048576,"files":20}')
            with patch.dict(TOOL.os.environ, TOOL.ENV, clear=True), \
                    patch.object(TOOL, "startup_profile", return_value={}), \
                    patch.object(TOOL.time, "monotonic", return_value=0), \
                    patch.object(TOOL.subprocess, "Popen", side_effect=OSError("observer interrupted")) as launch:
                with self.assertRaisesRegex(OSError, "observer interrupted"):
                    TOOL._outer("profile", root)
                self.assertEqual(launch.call_count, 1)
            self.assertTrue((root / "partial.json").exists())
            with self.assertRaisesRegex(ValueError, "pending"):
                TOOL.OneShot(root, {"wall":4,"cpu":6,"bytes":1048576,"files":20}).reserve()

    def test_matching_executed_source_is_accepted(self):
        source = b"value = 1\n"
        TOOL.verify_loaded_code(compile(source, "fixed.py", "exec", dont_inherit=True), source, "fixed.py")


if __name__ == "__main__":
    unittest.main(failfast=True)
