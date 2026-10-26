"""Data-only controls for the fixed outer protocol; no worker entry exists."""
import importlib.util
import os
import json
import py_compile
from pathlib import Path
import unittest
import tempfile
from types import SimpleNamespace
from unittest.mock import Mock, patch

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

    def test_ticket_persistence_failure_closes_owned_descriptor_before_dispatch(self):
        descriptors = []
        create = TOOL.os.memfd_create

        def capture(*args):
            fd = create(*args)
            descriptors.append(fd)
            return fd

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "budget.json").write_text('{"wall":4,"cpu":6,"bytes":1048576,"files":20}')
            try:
                with patch.dict(TOOL.os.environ, TOOL.ENV, clear=True), \
                        patch.object(TOOL, "startup_profile", return_value={}), \
                        patch.object(TOOL.time, "monotonic", return_value=0), \
                        patch.object(TOOL, "OneShot") as attempt, \
                        patch.object(TOOL.os, "memfd_create", side_effect=capture), \
                        patch.object(TOOL.subprocess, "Popen") as launch:
                    attempt.return_value.write.side_effect = OSError("ticket persistence")
                    with self.assertRaisesRegex(OSError, "ticket persistence"):
                        TOOL._outer("profile", root)
                    launch.assert_not_called()
                self.assertEqual(len(descriptors), 1)
                with self.assertRaises(OSError):
                    os.fstat(descriptors[0])
            finally:
                for fd in descriptors:
                    try:
                        os.close(fd)
                    except OSError:
                        pass

    def test_tool_executes_captured_source_even_if_disk_read_changes(self):
        source = b"value = 'captured'\n"
        with patch.object(TOOL, "_read", return_value=source):
            loaded = TOOL._tool()
        self.assertEqual(loaded.get("value"), "captured")
        self.assertEqual(loaded["_SOURCE_BYTES"], source)

    def test_source_loader_bypasses_valid_stale_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "fixed.py"
            source.write_text("value = 'stale'\n")
            before = source.stat()
            cache = Path(py_compile.compile(str(source), doraise=True))
            cached_bytes = cache.read_bytes()
            source.write_text("value = 'fresh'\n")
            os.utime(source, ns=(before.st_atime_ns, before.st_mtime_ns))
            loader = TOOL.SourceLoader("fixed", str(source))
            spec = importlib.util.spec_from_loader(loader.name, loader)
            module = importlib.util.module_from_spec(spec)
            loader.exec_module(module)
            self.assertEqual(module.value, "fresh")
            self.assertEqual(TOOL.SOURCES[str(source)][0], source.read_bytes())
            self.assertEqual(cache.read_bytes(), cached_bytes)
            with self.assertRaisesRegex(ValueError, "loaded code/source mismatch"):
                TOOL._initial_source("fixed", module, str(source))

    def test_nonfixed_helper_argv_is_zero_dispatch(self):
        helper = TOOL._OuterHelper(["shell"])
        with patch.object(TOOL.subprocess, "Popen") as launch:
            with self.assertRaisesRegex(ValueError, "fixed helper command"):
                helper.collect("profile", Path("/unused"), 3)
            launch.assert_not_called()

    def test_nonfixed_main_mode_is_zero_dispatch(self):
        with patch.object(TOOL.sys, "argv", ["fixed", "run", "/unused"]), \
                patch.object(TOOL, "_outer") as outer, patch.object(TOOL, "_child") as child:
            with self.assertRaisesRegex(ValueError, "fixed profile/verify/init"):
                TOOL.main()
            outer.assert_not_called()
            child.assert_not_called()

    def test_wrong_loaded_source_prevents_init_dispatch(self):
        command = Mock()
        with patch.object(TOOL, "_tool", return_value={"_command": command}), \
                patch.object(TOOL, "startup_profile", side_effect=ValueError("loaded code/source mismatch")), \
                patch.object(TOOL.os, "write") as output:
            with self.assertRaisesRegex(ValueError, "loaded code/source mismatch"):
                TOOL._child("init", Path("/unused"))
            command.assert_not_called()
            output.assert_not_called()

    def test_outer_log_failure_preserves_wait4_without_second_reap(self):
        usage = SimpleNamespace(ru_utime=.001, ru_stime=.002, ru_maxrss=1234)
        child = Mock(pid=999999)
        write = TOOL.OneShot.write

        def fail_log(attempt, name, data):
            if name == "stdout.log":
                raise OSError("log persistence")
            return write(attempt, name, data)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "budget.json").write_text('{"wall":4,"cpu":6,"bytes":1048576,"files":20}')
            with patch.dict(TOOL.os.environ, TOOL.ENV, clear=True), \
                    patch.object(TOOL, "startup_profile", return_value={}), \
                    patch.object(TOOL.time, "monotonic", return_value=0), \
                    patch.object(TOOL.subprocess, "Popen", return_value=child), \
                    patch.object(TOOL.os, "set_blocking"), \
                    patch.object(TOOL, "_helper_output", return_value=b'{"profile":{}}'), \
                    patch.object(TOOL.signal, "signal"), patch.object(TOOL.signal, "setitimer"), \
                    patch.object(TOOL.os, "wait4", return_value=(child.pid, 0, usage)) as wait, \
                    patch.object(TOOL.os, "kill") as kill, \
                    patch.object(TOOL.OneShot, "write", fail_log):
                with self.assertRaisesRegex(OSError, "log persistence"):
                    TOOL._outer("profile", root)
            wait.assert_called_once_with(child.pid, 0)
            kill.assert_not_called()
            partial = json.loads((root / "partial.json").read_bytes())
            self.assertEqual(partial.get("wait4"), {"pid": child.pid, "wait_status": 0, "child_cpu": .003, "child_rss_kib": 1234})
            self.assertTrue((root / "outer.pending").exists())


if __name__ == "__main__":
    unittest.main(failfast=True)
