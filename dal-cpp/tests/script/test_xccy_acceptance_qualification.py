"""Adversarial admission and analytic controls for finite-input FD certificates."""

import copy
import unittest

import mpmath as mp

from xccy_acceptance_interval import Dual, exp, interval, sqrt, upper_abs
from xccy_acceptance_financial_qualification import admit_conditional, reciprocal_errors
from xccy_acceptance_qualification import certify_stencil, require_axes


class XccyAcceptanceQualificationTest(unittest.TestCase):
    def test_exact_binary64_input_and_dual_derivative(self):
        mp.iv.dps = 70
        x = Dual(interval(0.1), 1)
        numerator, denominator = (0.1).as_integer_ratio()
        self.assertTrue(interval(numerator) / denominator in x.value)
        self.assertTrue(interval(3) in ((x * x * x).derivative / (x.value * x.value)))
        self.assertLess(upper_abs(exp(x).derivative - exp(x).value), mp.mpf("1e-60"))

    def test_truncation_and_complete_endpoint_error_are_separate(self):
        mp.iv.dps = 70
        h = 0.001
        exact_up, exact_down = interval(1) + interval(h) ** 3, interval(1) - interval(h) ** 3
        actual_up, actual_down = float(1 + h**3), float(1 - h**3)
        fd = (actual_up - actual_down) / (2 * h)
        result = certify_stencil(actual_up, actual_down, fd, exact_up, exact_down, interval(0), h)
        self.assertGreater(result["truncation"], mp.mpf("9.9e-7"))
        self.assertGreater(result["pipeline"], 0)
        self.assertGreaterEqual(result["total"], abs(mp.mpf(fd)))
        bad = certify_stencil(actual_up, actual_down, fd + 1, exact_up, exact_down, interval(0), h)
        self.assertGreater(bad["subtraction"], mp.mpf(".9"))

    def test_missing_duplicate_and_reordered_native_axes_rejected(self):
        expected = ["spot", "node", "vol"]
        require_axes(expected, expected)
        for bad in (expected[:-1], ["spot", "node", "node"], expected[::-1]):
            with self.subTest(axes=bad), self.assertRaises(ValueError):
                require_axes(bad, expected)

    def test_interval_cholesky_rejects_zero_and_negative_pivots(self):
        for value in (0, -1, mp.iv.mpf([-1, 1])):
            with self.subTest(value=value), self.assertRaises(ValueError):
                sqrt(Dual(value, 1))

    def test_independent_dual_sqrt_and_quotient(self):
        mp.iv.dps = 70
        x = Dual(4, 1)
        self.assertTrue(interval(".25") in sqrt(x).derivative)
        self.assertTrue(interval("-.0625") in (1 / x).derivative)

    def test_correlated_reciprocal_error_needs_independent_spot(self):
        mp.iv.dps = 70
        row = dict(spot=2., reciprocal=.5, reciprocal_compiled=.5)
        reciprocal_errors(row, interval(2))
        row.update(spot=2.4, reciprocal=1 / 2.4, reciprocal_compiled=1 / 2.4)
        with self.assertRaisesRegex(ValueError, "independent pathwise"):
            reciprocal_errors(row, interval(2))

    def test_compiled_reciprocal_is_required_and_independently_checked(self):
        mp.iv.dps = 70
        with self.assertRaisesRegex(ValueError, "compiled reciprocal execution required"):
            reciprocal_errors(dict(spot=2., reciprocal=.5), interval(2))
        with self.assertRaisesRegex(ValueError, "independent pathwise"):
            reciprocal_errors(dict(spot=2., reciprocal=.5, reciprocal_compiled=.6), interval(2))

    def test_conditional_prefix_is_unchanged_by_every_future_coordinate(self):
        base = dict(normals=[.2, -.3, .2, -.3, .2, 0, 0, 0, 0, 0], states=[dict(spot=2., reciprocal=.5) for _ in range(3)])
        case = dict(grid=["2026-10-21", "2027-10-21", "2028-10-20"], future_first_slot=5, conditional=[base])
        for coordinate in range(5, 10):
            probe = copy.deepcopy(base)
            probe["normals"][coordinate] = 1
            probe["states"][2]["spot"] += coordinate
            case["conditional"].append(probe)
        admit_conditional(case, 10)
        for index in range(1, 6):
            with self.subTest(probe=index):
                changed = copy.deepcopy(case)
                changed["conditional"][index]["states"][1].update(spot=2.4, reciprocal=1 / 2.4)
                with self.assertRaisesRegex(ValueError, "conditional state prefix"):
                    admit_conditional(changed, 10)


if __name__ == "__main__":
    unittest.main()
