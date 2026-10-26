"""Data-only fault controls. Real workers run individually under an outer subreaper."""
from contextlib import ExitStack
import fcntl
import importlib.util
import json
import os
from pathlib import Path
import signal
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

SOURCE = Path(__file__).with_name("dal425-controlled-execution.py")
SPEC = importlib.util.spec_from_file_location("controlled", SOURCE)
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class StaticTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.ledger = self.root / "ledger.jsonl"
        self.identity = TOOL.identity()
        self.allocation = {"kind":"allocation", "schema":2, "domain":"synthetic", "product_actual":0,
                           "wall":10., "cpu":10., "bytes":1048576, "units":100, "original_allocation":"UNKNOWN",
                           "identity":self.identity, "cwd":str(self.root), "env":TOOL.ENV, "action":"ok"}
        self.write_rows([self.allocation])
        self.entry = patch.object(TOOL, "_entry_check")
        self.original_entry = TOOL._entry_check
        self.entry.start()
        self.addCleanup(self.entry.stop)

    def write_rows(self, rows):
        self.ledger.write_text("".join(json.dumps(row) + "\n" for row in rows))
        self.ledger.chmod(0o600)

    def reservation(self):
        return {"kind":"reserve", "id":1, "wall":.7, "cpu":.5, "units":1,
                "identity":self.identity, "cwd":str(self.root), "env":TOOL.ENV,
                "action":"ok", "output_bytes":128}

    def receipt(self):
        return {"kind":"settle", "id":1, "status":"complete", "reason":"exit", "errors":[],
                "pid":999999, "starttime":"1234", "argv":TOOL._argv("ok"), "reaped":True,
                "exit":0, "wait_status":0, "child_cpu":.001, "supervisor_cpu":.002,
                "wall":.01, "maxrss_kib":1234, "stdout":"ok\n", "product_actual":0,
                "identity":self.identity, "cwd":str(self.root), "env":TOOL.ENV, "action":"ok"}

    def load(self):
        with self.ledger.open("rb") as stream:
            return TOOL._load(stream.fileno())

    def test_zero_wait4_usage_is_not_reaped(self):
        result, stop, wait, proc = self.execute_fake()
        stop.assert_called_once()
        self.assertEqual(wait.call_count, 2)
        self.assertTrue(result["reaped"])
        self.assertEqual(result["wait_status"], 9)
        self.assertEqual(result["child_cpu"], .003)
        proc.stdout.close.assert_called_once()

    def execute_fake(self, *, stat_error=False, kill_error=None, wait_error=False, close_error=False, exited=False, interrupted=False, action="burn"):
        usage = SimpleNamespace(ru_utime=.001, ru_stime=.002, ru_maxrss=1234)
        proc = Mock(pid=999999)
        proc.stdout.fileno.return_value = 11
        if close_error:
            proc.stdout.close.side_effect = OSError("close injection")
        initial = (proc.pid, 0, usage) if exited else (0, 0, usage)
        responses = [initial] if exited else [initial, (proc.pid, 9, usage)]
        if stat_error:
            responses = [(proc.pid, 9, usage)]
        if interrupted:
            responses.insert(0, InterruptedError("wait interrupted"))
        if wait_error:
            responses = [OSError("wait injection"), OSError("wait cleanup injection")]
        with ExitStack() as stack:
            stack.enter_context(patch.object(TOOL.subprocess, "Popen", return_value=proc))
            stack.enter_context(patch.object(TOOL, "_stat", side_effect=OSError("stat injection") if stat_error else None, return_value=("1234",0.)))
            stack.enter_context(patch.object(TOOL.os, "pidfd_open", side_effect=OSError("pidfd unavailable"), create=True))
            stack.enter_context(patch.object(TOOL.os, "set_blocking"))
            stack.enter_context(patch.object(TOOL, "_checkpoint"))
            stack.enter_context(patch.object(TOOL, "_drain", return_value=False))
            stop = stack.enter_context(patch.object(TOOL, "_stop", side_effect=kill_error))
            wait = stack.enter_context(patch.object(TOOL.os, "wait4", side_effect=responses))
            result = TOOL._execute(self.root, action, 0, .1, 1, TOOL.time.monotonic(), TOOL.time.process_time())
        return result, stop, wait, proc

    def test_nonfixed_action_rejected_at_dispatch_boundary(self):
        with self.assertRaisesRegex(ValueError, "action disabled"):
            self.execute_fake(action="compile")

    def test_first_stat_failure_still_reaps_and_preserves_error(self):
        result, stop, _, _ = self.execute_fake(stat_error=True)
        self.assertTrue(result["reaped"])
        self.assertEqual(result["status"], "partial")
        self.assertIn("stat injection", result["errors"][0])
        self.assertIsNone(stop.call_args.args[1])

    def test_exited_child_is_not_killed(self):
        result, stop, wait, _ = self.execute_fake(exited=True)
        stop.assert_not_called()
        self.assertEqual(wait.call_count, 1)
        self.assertTrue(result["reaped"])

    def test_kill_failure_does_not_skip_wait_or_close(self):
        result, _, _, proc = self.execute_fake(kill_error=OSError("kill injection"))
        self.assertTrue(result["reaped"])
        self.assertEqual(result["status"], "partial")
        proc.stdout.close.assert_called_once()

    def test_wait_failure_remains_unknown_and_closes(self):
        result, _, _, proc = self.execute_fake(wait_error=True)
        self.assertFalse(result["reaped"])
        self.assertIsNone(result["child_cpu"])
        self.assertEqual(result["status"], "partial")
        proc.stdout.close.assert_called_once()

    def test_wait_interruption_is_retried(self):
        result, _, _, _ = self.execute_fake(interrupted=True)
        self.assertTrue(result["reaped"])

    def test_close_failure_preserves_wait_receipt(self):
        result, _, _, _ = self.execute_fake(close_error=True)
        self.assertTrue(result["reaped"])
        self.assertEqual(result["status"], "partial")
        self.assertIn("close injection", result["errors"][-1])

    def test_stop_uses_only_owned_child_identity(self):
        with patch.object(TOOL.os, "kill") as kill, patch.object(TOOL, "_stat", return_value=("different",0.)):
            with self.assertRaisesRegex(RuntimeError, "identity"):
                TOOL._stop(999999, "1234", None)
            kill.assert_not_called()
            TOOL._stop(999999, None, None)
            kill.assert_called_once_with(999999, signal.SIGKILL)

    def test_forged_complete_cannot_authorize_successor(self):
        self.write_rows([self.allocation, self.reservation(), {"kind":"settle","id":1,"status":"complete"}])
        with self.assertRaisesRegex(ValueError, "ledger"):
            self.load()

    def test_complete_receipt_is_validated_but_not_automatic_restart(self):
        self.write_rows([self.allocation, self.reservation(), self.receipt()])
        self.assertEqual(self.load()[2], 1)
        with patch.object(TOOL, "_execute") as execute:
            with self.assertRaisesRegex(ValueError,"UNKNOWN"):
                TOOL.run(self.root,self.identity)
            execute.assert_not_called()

    def test_malformed_actuals_and_bindings_rejected(self):
        changes = ({"child_cpu":-1}, {"child_cpu":float("nan")}, {"wall":float("inf")},
                   {"wall":2}, {"child_cpu":1}, {"supervisor_cpu":True}, {"maxrss_kib":-1},
                   {"maxrss_kib":False}, {"id":True}, {"exit":7}, {"reaped":False},
                   {"wait_status":9}, {"starttime":None}, {"product_actual":True},
                   {"cwd":"/other"}, {"env":{}}, {"identity":{"source":"other"}},
                   {"argv":["shell"]}, {"action":"burn"}, {"status":"partial"})
        for change in changes:
            with self.subTest(change=change):
                row = dict(self.receipt(), **change)
                self.write_rows([self.allocation, self.reservation(), row])
                with self.assertRaisesRegex(ValueError,"ledger"):
                    self.load()

    def test_duplicate_reordered_and_pending_rows_rejected(self):
        cases = ([self.receipt()], [self.reservation()], [self.reservation(),self.reservation()],
                 [self.reservation(),self.receipt(),self.receipt()])
        for tail in cases:
            self.write_rows([self.allocation,*tail])
            with self.assertRaisesRegex(ValueError,"ledger"):
                self.load()
        self.ledger.write_text('{"kind":"allocation","kind":"allocation"}\n')
        with self.assertRaisesRegex(ValueError,"ledger"):
            self.load()

    def test_write_and_sync_failures_never_authorize_restart(self):
        for kind in ("reserve", "settle"):
            for failure in ("short", "fsync"):
                with self.subTest(kind=kind,failure=failure):
                    self.write_rows([self.allocation])
                    guard = self.root / "pending"
                    # Each subcase is a separate synthetic fixture, never a retry of a real ledger.
                    if guard.exists():
                        guard.unlink()
                    append = TOOL._append
                    def interrupted(fd,row,cap):
                        if row["kind"] != kind:
                            return append(fd,row,cap)
                        if failure == "fsync":
                            with patch.object(TOOL.os,"fsync",side_effect=OSError("sync injection")):
                                return append(fd,row,cap)
                        write = TOOL.os.write
                        def short(fd,data):
                            return write(fd,data[:max(1,len(data)//2)])
                        with patch.object(TOOL.os,"write",side_effect=short):
                            return append(fd,row,cap)
                    with patch.object(TOOL,"_execute",return_value=self.receipt()), patch.object(TOOL,"_append",side_effect=interrupted):
                        with self.assertRaisesRegex(OSError,"sync|partial"):
                            TOOL.run(self.root,self.identity)
                    self.assertTrue(guard.is_file())
                    before = self.ledger.read_bytes()
                    with patch.object(TOOL,"_execute") as execute:
                        with self.assertRaisesRegex(ValueError,"UNKNOWN"):
                            TOOL.run(self.root,self.identity)
                        execute.assert_not_called()
                    self.assertEqual(self.ledger.read_bytes(),before)

    def test_meter_failure_retains_guard_and_reservation(self):
        with patch.object(TOOL,"_execute",side_effect=OSError("meter injection")):
            with self.assertRaisesRegex(OSError,"meter"):
                TOOL.run(self.root,self.identity)
        self.assertTrue((self.root/"pending").is_file())
        self.assertEqual(len(self.ledger.read_text().splitlines()),2)
        with patch.object(TOOL,"_execute") as execute:
            with self.assertRaisesRegex(ValueError,"UNKNOWN"):
                TOOL.run(self.root,self.identity)
            execute.assert_not_called()

    def test_expired_preparation_does_not_spawn(self):
        clock = [0.]
        load = TOOL._load
        def delayed(fd):
            result = load(fd)
            clock[0] = 100.
            return result
        with patch.object(TOOL.time,"monotonic",side_effect=lambda:clock[0]), patch.object(TOOL,"_load",side_effect=delayed), patch.object(TOOL,"_execute") as execute:
            with self.assertRaisesRegex(ValueError,"wall budget"):
                TOOL.run(self.root,self.identity)
            execute.assert_not_called()
        self.assertFalse((self.root/"pending").exists())

    def test_lock_wait_has_deadline_and_zero_spawn(self):
        with self.ledger.open("rb") as held:
            fcntl.flock(held,fcntl.LOCK_EX|fcntl.LOCK_NB)
            with patch.object(TOOL,"_execute") as execute:
                with self.assertRaisesRegex(ValueError,"wall budget"):
                    TOOL.run(self.root,self.identity,wall=.01)
                execute.assert_not_called()
        self.assertEqual(len(self.ledger.read_text().splitlines()),1)

    def test_nonregular_and_nonprivate_ledger_rejected_before_open(self):
        self.ledger.unlink()
        os.mkfifo(self.ledger)
        with patch.object(TOOL.os,"open") as opened:
            with self.assertRaisesRegex(ValueError,"regular"):
                TOOL._open_ledger(self.root)
            opened.assert_not_called()
        self.ledger.unlink()
        self.ledger.symlink_to(self.root/"missing")
        with self.assertRaisesRegex(ValueError,"regular"):
            TOOL._open_ledger(self.root)

    def test_bad_parameters_and_identity_are_zero_spawn(self):
        for values in ({"wall":-1},{"cpu":float("nan")},{"units":True},{"output_bytes":-1},{"products":1},{"action":"compile"}):
            with patch.object(TOOL,"_execute") as execute:
                with self.assertRaises(ValueError):
                    TOOL.run(self.root,self.identity,**values)
                execute.assert_not_called()
        with self.assertRaisesRegex(ValueError,"identity"):
            TOOL.run(self.root,None)

    def test_imported_process_is_not_fixed_entry(self):
        with self.assertRaisesRegex(ValueError,"fixed fresh entry"):
            self.original_entry()

    def test_reservation_action_cannot_escape_allocation(self):
        self.write_rows([self.allocation, dict(self.reservation(),action="burn"),
                         dict(self.receipt(),action="burn",argv=TOOL._argv("burn"))])
        with self.assertRaisesRegex(ValueError,"ledger"):
            self.load()

    def test_allocation_identity_requires_complete_digest_schema(self):
        for value in ({"source":"x"}, {"source":"f"*64,"loaded_code":"f"*64,"runtime":float("nan")},
                      dict(self.identity,extra="forged")):
            self.write_rows([dict(self.allocation,identity=value)])
            with self.assertRaisesRegex(ValueError,"ledger"):
                self.load()

    def test_source_snapshot_is_sealed_and_matches_frozen_bytes(self):
        fd = TOOL._snapshot()
        try:
            self.assertEqual(os.read(fd,65537),TOOL.SOURCE_BYTES)
            with self.assertRaises(OSError):
                os.write(fd,b"change")
        finally:
            os.close(fd)
        before = TOOL.identity()
        with patch.object(TOOL,"SOURCE",self.root/"different-disk-file"):
            self.assertEqual(TOOL.identity(),before)


if __name__ == "__main__":
    unittest.main(failfast=True)
