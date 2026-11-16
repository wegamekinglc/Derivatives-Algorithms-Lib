"""Focused A/2 migration and evidence admission checks, on isolated ledgers."""

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from a1_budget import Budget, digest
from a2_identity import migrate, verify_origin
from a2_reference import require_mapping, reusable_layer


class A2Test(unittest.TestCase):
    def origin(self, root):
        path = root / "ledger.json"
        budget = Budget(path, "old", create=True)
        budget.reserve_solve("completed-origin")
        budget.completed()
        budget.state["seconds"] = {"pde": 20.0}
        budget.state["total_seconds"] = 20.0
        budget.save("sealed")
        manifest = root / "manifest.json"
        manifest.write_text('{"identity":"old"}\n')
        return budget, {
            "manifest": digest(manifest),
            "ledger": digest(path),
            "journal": digest(path.with_suffix(".jsonl")),
            "sequence": budget.state["sequence"],
            "identity": "old",
        }

    def test_transition_inherits_counters_and_prefix_without_new_budget(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            old, expected = self.origin(root)
            prefix = old.journal.read_bytes()
            resumed = migrate(root, "new", expected, {"approval": "approved"})
            self.assertEqual(resumed.state["sequence"], expected["sequence"] + 1)
            self.assertEqual(resumed.state["solves_reserved"], 1)
            self.assertEqual(resumed.state["solves_completed"], 1)
            self.assertEqual(resumed.state["total_seconds"], 20.0)
            self.assertEqual(resumed.state["paths_completed"], [0, 0])
            self.assertTrue(resumed.journal.read_bytes().startswith(prefix))
            self.assertEqual(resumed.state["origin_identity"], "old")
            self.assertEqual(resumed.state["transition"]["approval"], "approved")
            reopened = Budget(old.path, "new")
            self.assertEqual(reopened.state, resumed.state)
            with self.assertRaises(RuntimeError):
                Budget(old.path, "new", create=True)
            with self.assertRaises(RuntimeError):
                Budget(old.path, "old")
            with self.assertRaises(RuntimeError):
                migrate(root, "another", expected, {})

    def test_tampered_origin_or_missing_namespace_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            budget, expected = self.origin(root)
            verify_origin(root, expected)
            budget.journal.write_text(budget.journal.read_text() + " ")
            with self.assertRaises(RuntimeError):
                verify_origin(root, expected)
            with self.assertRaises(RuntimeError):
                migrate(root / "empty", "new", expected, {})

    def test_capacity_and_unclosed_checkpoint_cannot_be_reset(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            _, expected = self.origin(root)
            budget = migrate(root, "new", expected, {})
            budget.check_capacity(375)
            with self.assertRaises(RuntimeError):
                budget.check_capacity(376)
            budget.begin("pde")
            with self.assertRaises(RuntimeError):
                Budget(budget.path, "new").begin("pde")
            budget.end("pde")
            state = json.loads(budget.path.read_text())
            state["total_seconds"] = 0
            budget.path.write_text(json.dumps(state))
            with self.assertRaises(RuntimeError):
                Budget(budget.path, "new")

    def test_mapping_is_exact_and_expanded_raw_cannot_be_reused(self):
        expected = [{"id": "case", "sha256": "hash", "action": "CENTRAL_REUSE"}]
        require_mapping(expected, expected)
        with self.assertRaises(RuntimeError):
            require_mapping(expected, [])
        row = {
            "expanded": False,
            "mesh": [0.0],
            "timeline": [0.0, 1.0],
            "scheme": "scheme",
            "actual_steps": 1,
            "requested_steps": 1,
        }
        self.assertTrue(reusable_layer(row, [0.0], [0.0, 1.0], "scheme"))
        row["expanded"] = True
        self.assertFalse(reusable_layer(row, [0.0], [0.0, 1.0], "scheme"))
        row["expanded"] = False
        self.assertFalse(reusable_layer(row, [0.0], [0.0, 0.5, 1.0], "scheme"))

    def test_changed_manifest_or_journal_entry_cannot_be_resumed(self):
        from a2_identity import APPROVAL, CONTROL_SHA, verify_migrated

        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            _, expected = self.origin(root)
            metadata = dict(approval=APPROVAL, control_sha256=CONTROL_SHA)
            budget = migrate(root, "new", expected, metadata)
            with patch("a2_identity.ORIGIN", expected):
                verify_migrated(root, "new")
                (root / "manifest.json").write_text("rebound")
                with self.assertRaises(RuntimeError):
                    verify_migrated(root, "new")
            lines = budget.journal.read_text().splitlines()
            entry = json.loads(lines[-1])
            entry["total_seconds"] = 0
            lines[-1] = json.dumps(entry)
            budget.journal.write_text("\n".join(lines) + "\n")
            with self.assertRaises(RuntimeError):
                Budget(budget.path, "new")

    def test_group_capacity_is_checked_before_any_worker_or_reservation(self):
        from a1_run import execute

        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            budget = Budget(
                root / "ledger.json", "test", limits={"solves": 2}, create=True
            )
            with patch("a1_run.ProcessPoolExecutor") as executor:
                with self.assertRaises(RuntimeError):
                    execute([None] * 3, "over-capacity", root, budget)
                executor.assert_not_called()
            self.assertEqual(budget.state["solves_reserved"], 0)

    def test_diagnostic_unavailability_continues_but_black_and_input_failures_stop(
        self,
    ):
        from a2_run import certify_case

        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            budget = Budget(root / "ledger.json", "test", create=True)
            case = ("study", None, 1.0, [0.0], False)
            record = dict(
                id="study",
                status="REFERENCE_UNAVAILABLE",
                refined=True,
                normalized_differences={"space": [1e-3]},
            )
            with patch("a2_run.certify", return_value=record):
                result = certify_case(case, root, None, budget, True)
                self.assertEqual(result["status"], "REFERENCE_UNAVAILABLE")
                with self.assertRaises(RuntimeError):
                    certify_case(case, root, None, budget, False)
            with patch("a2_run.certify", side_effect=RuntimeError("input mismatch")):
                with self.assertRaises(RuntimeError):
                    certify_case(case, root, None, budget, True)

    def test_checkpoint_hash_and_fixed_alignment_schema_are_required(self):
        from a1_process import REPO, validate_arguments
        from a2_reference import validate_checkpoint
        from a2_sampling import restore_family

        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            budget = Budget(root / "ledger.json", "test", create=True)
            (root / "layer.json").write_text("sealed")
            budget.state["a2_files"] = {"layer.json": digest(root / "layer.json")}
            validate_checkpoint(root, budget)
            (root / "layer.json").write_text("changed")
            with self.assertRaises(RuntimeError):
                validate_checkpoint(root, budget)
            budget.paths(0, 16)
            with self.assertRaises(RuntimeError):
                restore_family(root, budget, 0, "F2_linear")
        prefix = REPO / "build/alignment-unit"
        args = [
            str(prefix / "request.json"),
            str(prefix / "F2_linear-alignment.json"),
            "F2_linear",
            "0",
        ]
        validate_arguments("mc", args)
        args[1] = str(prefix / "F2_linear-tier-16.json")
        with self.assertRaises(ValueError):
            validate_arguments("mc", args)

    def test_new_namespace_and_legacy_freeze_cannot_create_a_fresh_budget(self):
        from a1_run import freeze
        from a2_freeze import fixed_root

        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            with self.assertRaises(RuntimeError):
                fixed_root(root)
            with self.assertRaises(RuntimeError):
                freeze(root, root, root)
            self.assertFalse((root / "ledger.json").exists())

    def test_front_window_keeps_prefix_and_never_reopens_a_genuine_gate_failure(self):
        from a2_run import require_resumable
        from a2_sampling import check_front_window

        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            budget = Budget(root / "ledger.json", "test", create=True)
            budget.foreground_deadline = 0
            with self.assertRaises(RuntimeError):
                check_front_window(budget, 2**16, 0)
            self.assertEqual(budget.state["paths"], [0, 0])
            budget.state["a2_stop_reason"] = "INCOMPLETE: foreground window"
            require_resumable(budget)
            budget.state["a2_stop_reason"] = "FAIL: grid"
            with self.assertRaises(RuntimeError):
                require_resumable(budget)


if __name__ == "__main__":
    unittest.main()
