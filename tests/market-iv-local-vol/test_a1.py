"""Focused A/1 invariants before any formal numerical execution."""

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from a1_axes import grids, protocol, validate_axis
from a1_budget import Budget, checked_address
from a1_pde import admitted, configurations, derivative_weights, mesh
from a1_process import REPO, output, program, validate_arguments
from a1_report import report
from a1_run import stop_children
from a1_study import certify
from a1_verify import verify


class A1Test(unittest.TestCase):
    def test_only_fixed_read_only_programs_and_registered_native_arguments(self):
        with self.assertRaises(ValueError):
            program("shell")
        with self.assertRaises(ValueError):
            validate_arguments("git", ["push", "--force"])
        with self.assertRaises(ValueError):
            validate_arguments("probe", [str(REPO / "outside-build-input")])
        prefix = REPO / "build/a1-unit"
        args = [
            str(prefix / "request.json"),
            str(prefix / "F2_linear-tier-16.json"),
            "F2_linear",
            "16",
        ]
        validate_arguments("mc", args)
        bad = list(args)
        bad[3] = "21"
        with self.assertRaises(ValueError):
            validate_arguments("mc", bad)
        head = output("git", ["-C", str(REPO), "rev-parse", "HEAD"]).strip()
        self.assertEqual(len(head), 40)

    def test_stop_never_terminates_reported_daemon_pid(self):
        from unittest.mock import Mock

        daemon = Mock(pid=123)
        child = Mock(pid=456)
        with patch("a1_run.output", return_value='{"pid":123}'):
            with self.assertRaises(RuntimeError):
                stop_children([daemon])
            stop_children([child])
        daemon.terminate.assert_not_called()
        child.terminate.assert_called_once()

    def test_canonical_axes_counts_anchors_and_binary64_round_trip(self):
        import json

        rows = grids()
        self.assertEqual(
            [len(r["spots"]) * len(r["times"]) for r in rows],
            [13332, 19932, 26466, 50526, 98646, 98646, 98646, 98646, 98245],
        )
        self.assertEqual(json.loads(json.dumps(rows)), rows)
        for row in rows:
            for day in (91, 182, 273, 365, 730):
                if day / 365 <= row["times"][-1]:
                    self.assertIn(day / 365, row["times"])
        p = protocol(rows)
        self.assertEqual(len(p["anchors"]), 438)
        self.assertEqual(p["dimensions"], [1091, 2182, 4364, 8728])
        with self.assertRaises(ValueError):
            validate_axis([1.0, np.nextafter(1.0, 2.0)])

    def test_nonuniform_weights_and_mandatory_matched_mesh(self):
        x = np.array([-0.8, -0.3, 0.0, 0.1, 0.7])
        first, second = derivative_weights(x)
        for power in (0, 1, 2):
            y = x**power
            actual1 = sum(w * y[j : j + len(x) - 2] for j, w in enumerate(first))
            actual2 = sum(w * y[j : j + len(x) - 2] for j, w in enumerate(second))
            expected1 = power * x[1:-1] ** max(0, power - 1)
            expected2 = power * max(0, power - 1) * x[1:-1] ** max(0, power - 2)
            np.testing.assert_allclose(actual1, expected1, atol=2e-14)
            np.testing.assert_allclose(actual2, expected2, atol=2e-14)
        central = mesh([-0.31, 0.0, 0.29], [-0.2, 0.14], 2, False)
        expanded = mesh([-0.31, 0.0, 0.29], [-0.2, 0.14], 2, True)
        self.assertEqual(len(central), 2401)
        self.assertEqual(len(expanded), 3201)
        np.testing.assert_array_equal(central, expanded[400:-400])
        for point in (-0.31, 0.0, 0.29, -0.2, 0.14):
            self.assertIn(point, central)

    def test_reference_admission_refuses_missing_nonfinite_and_failed_components(self):
        self.assertFalse(admitted({}))
        self.assertFalse(admitted({"space": []}))
        self.assertFalse(admitted({"space": [float("nan")]}))
        self.assertFalse(admitted({"space": [3e-5]}))
        self.assertTrue(
            admitted(
                {k: [2e-5] for k in ("space", "time", "domain", "mass", "forward")}
            )
        )
        self.assertEqual(len(configurations()), 6)
        self.assertEqual(len(configurations(True)), 4)

    def test_checked_reservations_and_persistent_budget(self):
        with self.assertRaises(ValueError):
            checked_address(0, 0, 0, 0, 0)
        self.assertEqual(checked_address(1, 15, 2**20 - 1, 8727), 292863082495)
        with self.assertRaises(ValueError):
            checked_address(2, 0, 0, 0)
        with self.assertRaises(OverflowError):
            checked_address(1, 15, 2**20 - 1, 8727, 2**63)
        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "ledger.json"
            budget = Budget(path, "test", limits={"solves": 2}, create=True)
            budget.reserve_solve("one")
            budget = Budget(path, "test", limits={"solves": 2})
            self.assertEqual(budget.state["solves_reserved"], 1)
            budget.reserve_solve("two")
            with self.assertRaises(RuntimeError):
                budget.reserve_solve("three")
            with self.assertRaises(RuntimeError):
                Budget(path, "other")

    def test_fallback_keeps_discrete_coefficient_and_missing_evidence_stays_unavailable(
        self,
    ):
        model = type("Model", (), {"flat": False, "variance": lambda *args: 0.25})()
        observed = []

        def execute(arguments, name):
            observed.extend(arg[0] is model for arg in arguments)
            return [dict(values=[0.0] * 12) for _ in arguments]

        result = certify("discrete", model, 1.0, [0.0], execute)
        self.assertEqual(observed, [True] * 10)
        self.assertEqual(result["status"], "REFERENCE_UNAVAILABLE")

    def test_all_pass_labels_without_admission_seal_cannot_qualify(self):
        with tempfile.TemporaryDirectory() as name:
            with patch("a1_report.rows", return_value=[{"status": "PASS"}] * 480):
                self.assertFalse(
                    report(Path(name), {"counts": {}}, {}, None)["accepted"]
                )

    def test_time_path_and_missing_ledger_stops_survive_reopen(self):
        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "ledger.json"
            limits = {"pde_seconds": 1.0, "total_seconds": 2.0}
            budget = Budget(path, "test", limits=limits, create=True)
            budget.begin("pde")
            budget.last_tick -= 3.0
            with self.assertRaises(RuntimeError):
                budget.tick("pde")
            reopened = Budget(path, "test", limits=limits)
            self.assertGreaterEqual(reopened.state["total_seconds"], 3.0)
            with self.assertRaises(RuntimeError):
                reopened.begin("pde")
            budget.paths(0, 16 * 2**20)
            with self.assertRaises(RuntimeError):
                budget.paths(0, 16 * 2**20 + 1)
            with self.assertRaises(RuntimeError):
                budget.paths(0, 0)
            path.unlink()
            with self.assertRaises(RuntimeError):
                Budget(path, "test", limits=limits)
            with self.assertRaises(RuntimeError):
                Budget(path, "test", limits=limits, create=True)

    def test_batch_se_recomputed_and_bad_se_or_post_sufficient_tier_rejected(self):
        import copy

        group = {
            "batch_means": [[1.0] * 80 for _ in range(16)],
            "estimates": [[1.0, 0.0] for _ in range(80)],
        }
        mc = {
            "sampling": "FORMAL",
            "seed": 1024,
            "dmax": 8728,
            "tiers": [
                {
                    "paths_per_batch": 2**16,
                    "se_sufficient": True,
                    "groups": [copy.deepcopy(group) for _ in range(14)],
                }
            ],
        }
        self.assertEqual(verify(mc)["status"], "PASS")
        wrong = copy.deepcopy(mc)
        wrong["tiers"][0]["groups"][0]["estimates"][0][1] = 0.01
        with self.assertRaises(ValueError):
            verify(wrong)
        wrong = copy.deepcopy(mc)
        wrong["tiers"].append(copy.deepcopy(wrong["tiers"][0]))
        wrong["tiers"][-1]["paths_per_batch"] = 2**17
        with self.assertRaises(ValueError):
            verify(wrong)


if __name__ == "__main__":
    unittest.main()
