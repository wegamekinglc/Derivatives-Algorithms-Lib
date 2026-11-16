"""A/3 protocol and recovery checks on isolated synthetic ledgers."""

import copy
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from a1_axes import grids, protocol
from a1_budget import Budget, checked_address, digest
from a3_identity import recover, replacement_map, validate_replacements
from a3_protocol import coarse_count


class A3Test(unittest.TestCase):
    def test_controller_repair_preserves_native_paths_cost_and_failed_prefix(self):
        from a3_repair import STOP, append_repair

        with tempfile.TemporaryDirectory() as folder:
            budget = Budget(Path(folder) / "ledger.json", "old", create=True)
            budget.state.update(
                a3_stop_reason=STOP,
                paths=[16, 0],
                paths_completed=[0, 0],
                total_seconds=325.0,
                solves_completed=290,
            )
            budget.save("original-validation-failure")
            prefix = budget.journal.read_bytes()
            append_repair(budget, "new", dict(original_stop=STOP))
            self.assertTrue(budget.journal.read_bytes().startswith(prefix))
            self.assertEqual(budget.state["total_seconds"], 325.0)
            self.assertEqual(budget.state["paths"], [16, 0])
            self.assertEqual(budget.state["paths_completed"], [0, 0])
            self.assertEqual(budget.state["a3_repair"]["original_stop"], STOP)
            self.assertEqual(Budget(budget.path, "new").state["solves_completed"], 290)
            with self.assertRaises(RuntimeError):
                append_repair(budget, "third", {})

    def test_payment_times_accept_canonical_decimal_strings(self):
        import json

        from a2_sampling import payment_targets

        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "request.json").write_text(
                json.dumps(
                    dict(
                        fixtures=[
                            dict(
                                id="F2_linear",
                                mc_cases=[dict(time="0.2493150684931507")],
                            )
                        ]
                    )
                )
            )
            (root / "history.json").write_text(
                json.dumps(
                    dict(
                        rows=[
                            dict(
                                fixture="F2_linear",
                                maturity=91 / 365,
                                product="discount",
                                oracle=0.995,
                            )
                        ]
                    )
                )
            )
            self.assertEqual(payment_targets(root, "F2_linear"), {10: 0.995})

    def test_unique_override_dimensions_endpoints_and_strict_step_bounds(self):
        result = protocol(grids())
        self.assertEqual(result["dimensions"], [1092, 2184, 4368, 8736])
        anchors = result["anchors"]
        self.assertEqual(coarse_count(60, anchors[60], anchors[61]), 4)
        with self.assertRaises(ValueError):
            coarse_count(59, anchors[60], anchors[61])
        with self.assertRaises(ValueError):
            coarse_count(60, anchors[60], anchors[62])
        for level, times in enumerate(result["timelines"]):
            self.assertLessEqual(set(anchors), set(times))
            self.assertTrue(
                all(0 < b - a <= result["h"][level] for a, b in zip(times, times[1:]))
            )
            if level:
                self.assertEqual(times[::2], result["timelines"][level - 1])
        self.assertEqual(checked_address(1, 15, 2**20 - 1, 8735), 293131517951)
        with self.assertRaises(ValueError):
            checked_address(0, 0, 0, 8736)

    def test_replacement_map_rejects_missing_duplicate_or_executed_fields(
        self,
    ):
        context = dict(
            old_identity="old",
            new_identity="new",
            old_control="a2",
            new_control="a3",
            old_request="request2",
            new_request="request3",
            old_family="family2",
            new_family="family3",
            evidence="unexecuted",
        )
        rows = replacement_map(context)
        validate_replacements(rows, context, [16, 0], [0, 0])
        for invalid in (rows[:-1], rows + [rows[0]], list(reversed(rows))):
            with self.assertRaises(RuntimeError):
                validate_replacements(invalid, context, [16, 0], [0, 0])
        changed = copy.deepcopy(rows)
        changed[0]["new_identity"] = "other"
        with self.assertRaises(RuntimeError):
            validate_replacements(changed, context, [16, 0], [0, 0])
        with self.assertRaises(RuntimeError):
            validate_replacements(rows, context, [16, 0], [1, 0])
        changed = copy.deepcopy(rows)
        changed[5]["family"] = 1
        with self.assertRaises(RuntimeError):
            validate_replacements(changed, context, [16, 0], [0, 0])

    def test_recovery_inherits_cost_reservation_and_failure_history(
        self,
    ):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            budget = Budget(root / "ledger.json", "old", create=True)
            budget.state.update(
                solves_reserved=290,
                solves_completed=290,
                total_seconds=317.0,
                seconds={"pde": 221.0, "other": 94.5, "mc0": 1.5},
                paths=[16, 0],
                paths_completed=[0, 0],
                a2_stop_reason="FAIL: dimension",
            )
            budget.save("old-failure")
            expected = dict(
                identity="old",
                ledger=digest(budget.path),
                journal=digest(budget.journal),
                sequence=budget.state["sequence"],
            )
            prefix = budget.journal.read_bytes()
            context = dict(
                old_identity="old",
                new_identity="new",
                old_control="a2",
                new_control="a3",
                old_request="r2",
                new_request="r3",
                old_family="f2",
                new_family="f3",
                evidence="before-fine-RNG",
            )
            result = recover(root, "new", expected, context, replacement_map(context))
            self.assertEqual(result.state["sequence"], expected["sequence"] + 2)
            self.assertTrue(result.journal.read_bytes().startswith(prefix))
            self.assertEqual(result.state["paths"], [16, 0])
            self.assertEqual(result.state["paths_completed"], [0, 0])
            self.assertEqual(result.state["total_seconds"], 317.0)
            self.assertEqual(result.state["a2_stop_reason"], "FAIL: dimension")
            self.assertEqual(result.state["solves_completed"], 290)
            with self.assertRaises(RuntimeError):
                recover(root, "new", expected, context, replacement_map(context))

    def test_stale_writer_cannot_roll_back_and_append_does_not_read_entire_journal(
        self,
    ):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            first = Budget(root / "ledger.json", "same", create=True)
            stale = Budget(root / "ledger.json", "same")
            with patch.object(
                Path,
                "read_text",
                side_effect=AssertionError("full journal scan during append"),
            ):
                first.reserve_solve("one")
            with self.assertRaises(RuntimeError):
                stale.reserve_solve("stale")
            self.assertEqual(Budget(first.path, "same").state["solves_reserved"], 1)

    def test_first_formal_tier_restores_the_exact_tiny_prefix(self):
        from a2_sampling import restore_family

        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            budget = Budget(root / "ledger.json", "new", create=True)
            prior = root / "F2_linear-alignment.json.checkpoint.json"
            prior.write_text("tiny-exact-sums")
            budget.state.update(
                paths=[16, 0],
                paths_completed=[16, 0],
                alignment={"0": "tiny"},
                mc_checkpoints={
                    "0": dict(path=str(prior), sha256=digest(prior), tiny=True)
                },
            )
            evidence, actual, previous = restore_family(root, budget, 0, "F2_linear")
            self.assertIsNone(evidence)
            self.assertEqual(actual, prior)
            self.assertEqual(previous, 1)
            budget.paths(0, 16 * 2**16)
            self.assertEqual(budget.state["paths"][0] - 16, 16 * (2**16 - 1))

    def test_missing_model_pairing_or_tiny_statistics_cannot_pass_alignment(self):
        from a2_sampling import validate_alignment

        group = dict(
            batch_means=[[1.0] * 80 for _ in range(16)],
            estimates=[[1.0, 0.0] for _ in range(80)],
        )
        models = [
            dict(
                grid="P7" if j < 4 else "P8",
                level=j % 4,
                expected_dimension=[1092, 2184, 4368, 8736][j % 4],
                actual_dimension=[1092, 2184, 4368, 8736][j % 4],
                default_paths=64,
                default_price=1.0,
                status="PASS",
            )
            for j in range(8)
        ]
        value = dict(
            sampling="SMOKE_NOT_QUALIFIED",
            dmax=8736,
            seed=1024,
            previous_paths_per_batch=0,
            new_paths_per_batch=1,
            tiers=[
                dict(
                    paths_per_batch=1, groups=[copy.deepcopy(group) for _ in range(14)]
                )
            ],
            protocol_admission=dict(
                status="PASS",
                brownian_pairing="PASS",
                models=models,
                last_scalar_address=293131517951,
            ),
        )
        validate_alignment(value)
        wrong = copy.deepcopy(value)
        wrong["protocol_admission"]["models"].pop()
        with self.assertRaises(RuntimeError):
            validate_alignment(wrong)
        wrong = copy.deepcopy(value)
        wrong["protocol_admission"]["brownian_pairing"] = "NOT_RUN"
        with self.assertRaises(RuntimeError):
            validate_alignment(wrong)
        wrong = copy.deepcopy(value)
        wrong["tiers"][0]["groups"][0]["batch_means"].pop()
        with self.assertRaises(ValueError):
            validate_alignment(wrong)


if __name__ == "__main__":
    unittest.main()
