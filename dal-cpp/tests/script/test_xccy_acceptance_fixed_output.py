"""Exact controls for fixed saved payloads and inherited Gaussian enclosures."""

import tempfile
import unittest
from fractions import Fraction as F
from pathlib import Path

from xccy_acceptance_fixed_output import (
    MODEL_DECISION,
    Bounds,
    binary_endpoint,
    compare_payload,
    complete_rows,
    gaussian_measures,
    inherited_bounds,
    load_bound_json,
    model_decision_identity,
    rational_text,
)


class XccyAcceptanceFixedOutputTest(unittest.TestCase):
    def test_accepted_model_header_retains_frozen_identity_and_records_actual_source(
        self,
    ):
        raw = (Path(__file__).resolve().parents[3] / MODEL_DECISION).read_bytes()
        identity = model_decision_identity(raw)
        self.assertEqual(
            identity["admitted_sha256"],
            "6f1231c60cd1667601a84c1bf1d5c00b855a3bae81b9a88e0d115fc637e6e1f4",
        )
        self.assertEqual(
            identity["frozen_sha256"],
            "c2ff68e1a696b620682b896ca9b292ec6d577bf92cadb3597991b427267733d5",
        )

    def test_unregistered_model_body_and_header_changes_are_rejected(self):
        raw = (Path(__file__).resolve().parents[3] / MODEL_DECISION).read_bytes()
        for changed in (
            raw.replace(b"Status: accepted", b"Status: unregistered", 1),
            raw.replace(b"## 1. Audience", b"## 1. Changed", 1),
        ):
            self.assertNotEqual(changed, raw)
            with self.assertRaisesRegex(ValueError, "inherited source identity"):
                model_decision_identity(changed)

    def test_exact_gaussian_constant_cashflows_have_price_and_mean_quotient(self):
        reference = gaussian_measures(Bounds(F(-2), F(-2)), Bounds(F(1), F(1)), F(1, 4))
        self.assertEqual(reference["Price"], Bounds(F(1, 2), F(1, 2)))
        self.assertEqual(reference["par"], Bounds(F(1, 2), F(1, 2)))
        result = compare_payload(
            F(5, 8), reference["par"], F(1, 8), "par", "decimal-spread"
        )
        self.assertEqual(result["absolute"], Bounds(F(1, 8), F(1, 8)))
        self.assertEqual(result["status"], "within")

    def test_quotient_is_ratio_of_means_for_unequal_annuities(self):
        a, b = Bounds(F(-3), F(-3)), Bounds(F(2), F(2))
        reference = gaussian_measures(a, b, F(0))
        self.assertEqual(reference["par"], Bounds(F(2, 3), F(2, 3)))
        self.assertNotEqual(reference["par"].lo, (F(1, 2) + F(3, 4)) / 2)

    def test_uncertain_coefficient_corners_enclose_both_price_and_par(self):
        reference = gaussian_measures(Bounds(F(-4), F(-2)), Bounds(F(1), F(2)), F(1, 4))
        self.assertEqual(reference["Price"], Bounds(F(0), F(3, 2)))
        self.assertEqual(reference["par"], Bounds(F(1, 4), F(1)))

    def test_nonpositive_or_crossing_annuity_is_rejected(self):
        for a in (Bounds(F(0), F(1)), Bounds(F(-1), F(0)), Bounds(F(1), F(2))):
            with self.assertRaisesRegex(ValueError, "positive Gaussian annuity"):
                gaussian_measures(a, Bounds(F(1), F(1)), F(0))

    def test_lower_bound_alone_proves_red_and_signed_error_keeps_direction(self):
        for value, signed in ((F(3), Bounds(F(1), F(2))), (F(0), Bounds(F(-2), F(-1)))):
            result = compare_payload(value, Bounds(F(1), F(2)), F(1, 2), "Price", "USD")
            self.assertEqual(result["signed"], signed)
            self.assertEqual(result["absolute"], Bounds(F(1), F(2)))
            self.assertEqual(result["status"], "RED")

    def test_loose_upper_bound_or_budget_touch_is_inconclusive(self):
        for reference in (Bounds(F(-2), F(2)), Bounds(F(-2), F(-1))):
            result = compare_payload(F(0), reference, F(1), "Price", "USD")
            self.assertEqual(result["status"], "inconclusive")

    def test_reversed_nonexact_and_point_pretending_to_be_encoded_bounds_are_rejected(
        self,
    ):
        with self.assertRaisesRegex(ValueError, "ordered rational"):
            Bounds(F(2), F(1))
        with self.assertRaisesRegex(ValueError, "ordered rational"):
            Bounds(0.1, 0.2)
        for value in (
            [0, 0, -1, 0],
            [0, 3, -2, 1],
            [0, 1, -9999, 1],
            [0, -1, 0, 1],
            1.1,
        ):
            with self.assertRaises(ValueError):
                binary_endpoint(value)

    def test_binary64_and_signed_binary_endpoints_remain_exact(self):
        self.assertEqual(binary_endpoint([1, 3, -2, 2]), F(-3, 4))
        self.assertEqual(F.from_float(0.1), F(3602879701896397, 36028797018963968))
        self.assertNotEqual(F.from_float(0.1), F(1, 10))

    def test_point_and_nonoutward_decimal_cannot_replace_inherited_interval(self):
        with self.assertRaisesRegex(ValueError, "not an oracle point"):
            inherited_bounds("0.5")
        encoded = {
            "binary_lower": [0, 1, -1, 1],
            "binary_upper": [0, 1, -1, 1],
            "decimal_lower": "0.4",
            "decimal_upper": "0.6",
        }
        self.assertEqual(inherited_bounds(encoded), Bounds(F(1, 2), F(1, 2)))
        encoded["decimal_lower"] = "0.5000000000000001"
        with self.assertRaisesRegex(ValueError, "outward inherited"):
            inherited_bounds(encoded)

    def test_decimal_display_is_outward_for_positive_and_negative_rationals(self):
        for value in (F(1, 3), F(-1, 3), F(0), F(1, 2)):
            self.assertLessEqual(F(rational_text(value, False)), value)
            self.assertGreaterEqual(F(rational_text(value, True)), value)

    def test_wrong_units_and_nonpositive_budget_are_rejected(self):
        for name, units, budget in (("par", "USD", F(1)), ("Price", "USD", F(0))):
            with self.assertRaises(ValueError):
                compare_payload(F(0), Bounds(F(0), F(0)), budget, name, units)

    def test_input_identity_rejects_changed_reference_or_source(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "reference.json"
            path.write_text('{"point": 1}\n')
            with self.assertRaisesRegex(ValueError, "input identity"):
                load_bound_json(path, "0" * 64)

    def test_missing_duplicate_or_extra_profile_rows_are_rejected(self):
        keys = [
            (mode, factor, member, name)
            for mode in ("EXACT", "APPROXIMATE")
            for factor in (1, 2, 4)
            for member in range(3)
            for name in ("Price", "par")
        ]
        complete_rows(keys)
        for bad in (keys[:-1], keys + [keys[0]], keys[:-1] + [keys[0]]):
            with self.assertRaisesRegex(ValueError, "complete unique"):
                complete_rows(bad)


if __name__ == "__main__":
    unittest.main()
