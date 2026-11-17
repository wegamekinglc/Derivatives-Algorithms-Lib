"""Focused canonical-input, reference and statistical regression tests."""

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from a1_axes import grids, protocol, validate_axis
from a1_pde import admitted, configurations, derivative_weights, mesh
from a1_report import report
from a1_study import certify
from a1_verify import verify


class A1Test(unittest.TestCase):
    def test_generated_request_matches_the_executed_input_without_overwriting(self):
        import hashlib

        from a1_axes import write_request

        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "request.json"
            write_request(path)
            self.assertEqual(
                hashlib.sha256(path.read_bytes()).hexdigest(),
                "2818bfc153f3ae8b12980bcdf2adc5d400deb47a2abee474d013ec58cc7721db",
            )
            write_request(path)
            path.write_text("old frozen data")
            with self.assertRaises(ValueError):
                write_request(path)

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
        self.assertEqual(p["dimensions"], [1092, 2184, 4368, 8736])
        with self.assertRaises(ValueError):
            validate_axis([1.0, np.nextafter(1.0, 2.0)])

    def test_nonuniform_weights_differentiate_polynomials(self):
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

    def test_expanded_mesh_has_exact_parent_endpoints_and_midpoints(self):
        parent = mesh([-0.31, 0.0, 0.29], [-0.2, 0.14], 0, True)
        np.testing.assert_array_equal(parent[:101], np.linspace(-4.0, -3.0, 101))
        np.testing.assert_array_equal(parent[-101:], np.linspace(3.0, 4.0, 101))
        for level in (1, 2, 3):
            with self.subTest(level=level):
                parent = mesh([-0.31, 0.0, 0.29], [-0.2, 0.14], level - 1, True)
                child = mesh([-0.31, 0.0, 0.29], [-0.2, 0.14], level, True)
                np.testing.assert_array_equal(
                    child[::2].view(np.uint64), parent.view(np.uint64)
                )
                np.testing.assert_array_equal(
                    child[1::2].view(np.uint64),
                    ((parent[:-1] + parent[1:]) / 2).view(np.uint64),
                )
                self.assertEqual(len(child), 800 * 2**level + 1)

    def test_expanded_mesh_reproduces_left_midpoint_rounding(self):
        expanded = mesh([-0.31, 0.0, 0.29], [-0.2, 0.14], 1, True)
        self.assertEqual(expanded[3].hex(), (-3.9850000000000003).hex())

    def test_expanded_mesh_preserves_matched_central_knots_at_every_level(self):
        knots = [-0.31, 0.0, 0.29]
        strikes = [-0.2, 0.14]
        for level in (0, 1, 2, 3):
            with self.subTest(level=level):
                central = mesh(knots, strikes, level)
                expanded = mesh(knots, strikes, level, True)
                count = 100 * 2**level
                np.testing.assert_array_equal(
                    expanded[count:-count].view(np.uint64), central.view(np.uint64)
                )
                self.assertEqual(len(central), 600 * 2**level + 1)
                self.assertEqual(len(expanded), 800 * 2**level + 1)
                self.assertLessEqual(
                    set(knots + strikes + [-3.0, 0.0, 3.0]), set(central)
                )

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

    def test_batch_se_recomputed_and_bad_se_or_post_sufficient_tier_rejected(self):
        import copy

        group = {
            "batch_means": [[1.0] * 80 for _ in range(16)],
            "estimates": [[1.0, 0.0] for _ in range(80)],
        }
        mc = {
            "sampling": "FORMAL",
            "seed": 1024,
            "dmax": 8736,
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
