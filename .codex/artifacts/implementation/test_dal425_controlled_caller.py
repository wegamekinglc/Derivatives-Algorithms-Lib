"""Direct caller controls. These tests never create a worker or subprocess."""
import importlib.util
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

PATH = Path(__file__).with_name("dal425-controlled-caller.py")
SPEC = importlib.util.spec_from_file_location("caller", PATH)
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class CallerTest(unittest.TestCase):
    def test_eagain_means_pending(self):
        saved = []
        result = TOOL.collect_reaped({"pid": 1}, saved.append, Mock(side_effect=BlockingIOError()))
        self.assertIsNone(result)

    def test_missing_shared_allocation_rejects_before_dispatch(self):
        dispatched = []
        with self.assertRaisesRegex(ValueError, "shared allocation"):
            TOOL.validate_allocation({}, {}, {}, "task", "owner")
            dispatched.append(True)
        self.assertEqual(dispatched, [])

    def test_reaped_tuple_survives_eagain(self):
        saved = []
        raw = {"pid": 123, "status": 9, "usage": [0.1, 0.2, 3000]}

        def unavailable():
            raise BlockingIOError("injected EAGAIN after wait4")

        try:
            TOOL.collect_reaped(raw, saved.append, unavailable)
        except BlockingIOError:
            pass
        self.assertEqual(saved, [raw])

    def test_log_write_error_keeps_raw_tuple(self):
        saved = []
        raw = {"pid": 123, "status": 256, "usage": [0.1, 0.2, 3000]}
        with self.assertRaisesRegex(OSError, "log write"):
            TOOL.collect_reaped(raw, saved.append, Mock(side_effect=OSError("log write")))
        self.assertEqual(saved, [raw])

    def test_receipt_write_failure_prevents_drain(self):
        drain = Mock()
        with self.assertRaisesRegex(OSError, "receipt fsync"):
            TOOL.collect_reaped({}, Mock(side_effect=OSError("receipt fsync")), drain)
        drain.assert_not_called()

    def test_missing_guard_is_zero_dispatch(self):
        grant = {"schema": 1, "kind": "actual_coordinator_engineering_allocation",
                 "ledger_revision": TOOL.REVISION, "attempt_serial": 1, "attempts_max": 1,
                 "status": "DEBITED_HELD_NO_REFUND_BEFORE_DISPATCH", "owner_agent_id": "owner",
                 "reserved_wall_seconds": 60, "reserved_cpu_core_seconds": 60,
                 "product_evaluations_allowed": 0, "workers_allowed": 0,
                 "new_stage_byte_subcap": 8388608, "new_stage_file_subcap": 256}
        ledger = {"next_S0L_actual_caller_binding": {"grant": grant, "grant_sha256": TOOL.GRANT_SHA256}}
        with self.assertRaisesRegex(ValueError, "exact task/guard"):
            TOOL.validate_allocation(grant, ledger, {}, "task", "owner")

    def test_write_failure_poisoned_attempt_never_drains_or_refunds(self):
        attempt = object.__new__(TOOL.HeldAttempt)
        attempt.root = Path("/unused")
        attempt.slots = {"wait4.json": 8192}
        attempt.spent = set()
        attempt.failed = False
        drain = Mock()
        with patch.object(TOOL, "_exclusive", side_effect=OSError("fsync interruption")):
            with self.assertRaisesRegex(OSError, "fsync interruption"):
                attempt.record_wait4({"pid": 1}, drain)
        self.assertTrue(attempt.failed)
        self.assertEqual(attempt.spent, {"wait4.json"})
        drain.assert_not_called()
        with self.assertRaisesRegex(ValueError, "no successor"):
            attempt.write("wait4.json", b"{}")

    def test_duplicate_consumption_and_changed_root_are_zero_dispatch(self):
        # This synthetic authority exists only under the patched digest in this test.
        root = Path(tempfile.mkdtemp(prefix="caller-duplicate-", dir=Path.cwd()))
        changed_root = Path(tempfile.mkdtemp(prefix="caller-changed-", dir=Path.cwd()))
        wide_root = Path(tempfile.mkdtemp(prefix="caller-wide-", dir=Path.cwd()))
        wide_root.chmod(0o755)
        self.assertEqual(root.stat().st_mode & 0o777, 0o700)
        self.assertEqual(changed_root.stat().st_mode & 0o777, 0o700)
        grant = {"schema": 1, "kind": "actual_coordinator_engineering_allocation",
                 "ledger_revision": TOOL.REVISION, "attempt_serial": 1, "attempts_max": 1,
                 "status": "DEBITED_HELD_NO_REFUND_BEFORE_DISPATCH", "owner_agent_id": "owner",
                 "reserved_wall_seconds": 60, "reserved_cpu_core_seconds": 60,
                 "product_evaluations_allowed": 0, "workers_allowed": 0,
                 "new_stage_byte_subcap": 8388608, "new_stage_file_subcap": 256,
                 "allocation_id": "synthetic-only", "issuer_task_id": "issuer"}
        raw = json.dumps(grant).encode()
        digest = hashlib.sha256(raw).hexdigest()
        ledger = json.dumps({"next_S0L_actual_caller_binding": {"grant": grant, "grant_sha256": digest}}).encode()
        guard = json.dumps({"filter_task_id": "task", "filter_agent_id": "owner",
                            "source_task_id": "issuer", "enabled": True}).encode()
        anchor = root / "anchor.json"
        anchor.write_text(json.dumps({"grant_sha256": digest, "allocation_id": "synthetic-only",
            "ledger_revision": TOOL.REVISION, "task_id": "task", "owner": "owner", "attempt_serial": 1,
            "root": str(root), "state": "PENDING_NO_REFUND"}))
        identities = {k: "synthetic" for k in ("source", "observer", "profile", "argv", "env", "cwd")}
        slots = {"caller.pending": 8192, "wait4.json": 8192}
        dispatch = Mock()
        with patch.object(TOOL, "GRANT_SHA256", digest):
            attempt = TOOL.HeldAttempt(root, anchor, raw, ledger, guard, "task", "owner", identities, slots)
            with self.assertRaises(FileExistsError):
                TOOL.HeldAttempt(root, anchor, raw, ledger, guard, "task", "owner", identities, slots)
                dispatch()
            with self.assertRaisesRegex(ValueError, "root/attempt"):
                TOOL.HeldAttempt(changed_root, anchor, raw, ledger, guard, "task", "owner", identities, slots)
                dispatch()
            with self.assertRaisesRegex(ValueError, "private caller root required"):
                TOOL.HeldAttempt(wide_root, anchor, raw, ledger, guard, "task", "owner", identities, slots)
                dispatch()
        dispatch.assert_not_called()
        attempt.write("wait4.json", b"{}")
        (root / "wait4.json").unlink()
        with self.assertRaisesRegex(ValueError, "no successor"):
            attempt.write("wait4.json", b"{}")


if __name__ == "__main__":
    unittest.main(failfast=True)
