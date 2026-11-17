"""Numerical admission cannot hide insufficient SE or a failed grid/reference."""

import unittest

from formal_report import gate, qualified_status


class FormalReportTest(unittest.TestCase):
    def test_exact_price_with_insufficient_se_is_incomplete(self):
        self.assertEqual(gate(10.0, 0.02, 10.0, True, False, "PASS"), "INCOMPLETE")
        self.assertEqual(gate(None, None, 10.0, True, False, "PASS"), "NOT_RUN")
        self.assertEqual(
            gate(10.0, 0.001, 10.0, True, False, "REFERENCE_UNAVAILABLE"),
            "REFERENCE_UNAVAILABLE",
        )

    def test_price_pass_cannot_override_failed_grid_or_pair(self):
        failed = dict(status="FAIL", reference_self_check=dict(status="PASS"))
        unavailable = dict(
            status="INCOMPLETE",
            reference_self_check=dict(status="REFERENCE_UNAVAILABLE"),
        )
        self.assertEqual(qualified_status("PASS", failed, "PASS", False, False), "FAIL")
        self.assertEqual(
            qualified_status("PASS", unavailable, "PASS", False, False),
            "REFERENCE_UNAVAILABLE",
        )
        self.assertEqual(
            qualified_status("PASS", failed, "INCOMPLETE", False, False), "INCOMPLETE"
        )

    def test_deterministic_payment_uses_its_frozen_relative_scale(self):
        self.assertEqual(gate(1 + 1e-13, 0.0, 1.0, True, True, "PASS"), "PASS")
        self.assertEqual(gate(1 + 3e-12, 0.0, 1.0, True, True, "PASS"), "FAIL")


if __name__ == "__main__":
    unittest.main()
