"""Analytic cancellation, joint quotient and independent cashflow controls."""

import unittest
from fractions import Fraction
from pathlib import Path
from unittest.mock import patch

import mpmath as mp

from xccy_acceptance_bias_cashflows import Affine, IndependentTerms
from xccy_acceptance_bias_intervals import encode, interval, magnitude, rational
from xccy_acceptance_bias_qualification import load_fixture
from xccy_acceptance_finite_bias import (
    comparison_row,
    finite_column,
    frozen_domain,
    polynomial_terms,
    read_accepted,
)
from xccy_acceptance_finite_math import (
    PolynomialTerm,
    annuity_anchor,
    annuity_controls,
    coefficient_mean_ratio,
    exponent_difference,
    exponent_factors,
    quotient_bounds,
    signed_enclosure,
)
from xccy_acceptance_interval import Dual, FixedCashflows


def at_point(terms, z):
    return sum(
        (
            mp.iv.exp(t.k + sum(x * y for x, y in zip(t.ell, z, strict=True)))
            * (t.constant + sum(x * y for x, y in zip(t.linear, z, strict=True)))
            for t in terms
        ),
        interval(0),
    )


class XccyAcceptanceFiniteBiasTest(unittest.TestCase):
    def setUp(self):
        mp.iv.dps = 70
        mp.mp.dps = 75

    def test_signed_pair_keeps_common_exponent_instead_of_independent_boxes(self):
        terms = [
            PolynomialTerm(interval(0), (interval("10.0001"),), interval(1), (interval(0),)),
            PolynomialTerm(interval(".001"), (interval("10.0001"),), interval(-1), (interval(0),)),
        ]
        actual = signed_enclosure(terms, (interval(0), (interval(10),)), interval("6.25"))
        self.assertLess(abs(actual).b, interval(".00101").a)
        self.assertIn(1 - mp.iv.exp(interval(".001")), actual)

    def test_ratio_uses_coefficient_means_and_identical_positive_weights(self):
        weights = (Fraction(1, 4), Fraction(3, 4))
        value, native = coefficient_mean_ratio([1, 3], [1, 5], [2, 4], [3, 7], weights, weights)
        self.assertIn(interval(8) / 5, value)
        self.assertIn(interval(4) / 25, native)
        self.assertNotEqual(value, interval(1) / 4 + interval(5) / 4)
        for bad in ((Fraction(-1), Fraction(2)), (Fraction(1, 4), Fraction(1, 4))):
            with self.subTest(weights=bad), self.assertRaisesRegex(ValueError, "weights"):
                coefficient_mean_ratio([1, 3], [1, 5], [2, 4], [3, 7], bad, bad)
        with self.assertRaisesRegex(ValueError, "identical"):
            coefficient_mean_ratio([1, 3], [1, 5], [2, 4], [3, 7], weights, weights[::-1])

    def test_amgm_anchor_retains_annuity_count_and_shared_loading(self):
        term = PolynomialTerm(interval(".2"), (interval(".7"),), interval(1), (interval(0),))
        anchor = annuity_anchor([term, term])
        for z in (-6, 0, 6):
            exponent = exponent_difference(term, anchor, interval(abs(z)))
            self.assertLess(magnitude(mp.iv.exp(exponent) - interval(1) / 2).b, interval("1e-60").a)
        for invalid in ([], [PolynomialTerm(interval(0), (interval(0),), interval(-1), (interval(0),))]):
            with self.subTest(invalid=invalid), self.assertRaisesRegex(ValueError, "annuity"):
                annuity_anchor(invalid)

    def test_spatial_native_polynomial_includes_loading_and_polynomial_derivative(self):
        term = PolynomialTerm(interval(".4"), (interval(".5"),), interval(2), (interval(3),))
        actual = at_point([term.spatial(0)], [interval(".2")])
        expected = interval("4.3") * mp.iv.exp(interval(".5"))
        self.assertLess(magnitude(actual - expected).b, interval("1e-60").a)

    def test_native_term_keeps_coefficient_drift_and_loading_derivatives(self):
        terms = [("B", Dual(2, 3), Affine(Dual(".4", 5), (Dual(".5", 7),)))]
        native = polynomial_terms(terms, "B", native=True)
        self.assertLess(
            magnitude(at_point(native, [interval(".2")]) - interval("15.8") * mp.iv.exp(interval(".5"))).b,
            interval("1e-60").a,
        )

    def test_moving_denominator_and_mixed_native_bound_cover_analytic_two_path_ratio(self):
        terms = [
            ("A", Dual(-1), Affine(Dual(), (Dual(1, 1),))),
            ("B", Dual(1), Affine(Dual(), (Dual(),))),
        ]
        controls = finite_column(terms, interval("1.01"), interval(".01"))
        weights = (mp.mpf(1) / 4, mp.mpf(3) / 4)

        def analytic(z):
            denominator = sum(w * mp.exp(x) for w, x in zip(weights, z, strict=True))
            native_denominator = sum(w * x * mp.exp(x) for w, x in zip(weights, z, strict=True))
            return 1 / denominator, -native_denominator / denominator**2

        before = analytic([mp.mpf("-.2"), mp.mpf(".4")])
        after = analytic([mp.mpf("-.19"), mp.mpf(".39")])
        for key, index in (("par", 0), ("dpar", 1)):
            self.assertLess(abs(after[index] - before[index]), mp.make_mpf(controls["bounds"][key]._mpi_[1]))
        self.assertGreater(abs(after[1] - before[1]), 0)
        self.assertGreater(controls["quotient_native_pieces"]["B_times_mixed_D"].a, 0)
        self.assertGreater(controls["quotient_native_pieces"]["double_denominator_movement"].a, 0)

    def test_all_quotient_movement_terms_are_needed_for_independent_rational_control(self):
        relative = {key: interval(1) for key in ("R", "Rt", "V", "Vt")}
        denominator = {key: interval(1) for key in ("minimum", "L", "U", "W")}
        h = mp.mpf("0.000001")
        bounds = quotient_bounds(relative, denominator, interval("0.000001"))

        def native(z):
            return (-1 + z) / (1 + z) - (1 - z) ** 2 / (1 + z) ** 2

        actual = abs(native(h) - native(mp.mpf(0)))
        self.assertGreater(actual, mp.mpf("5.99") * h)
        self.assertLess(actual, mp.make_mpf(bounds["native"]._mpi_[1]))
        self.assertLess(abs((1 - h) / (1 + h) - 1), mp.make_mpf(bounds["value"]._mpi_[1]))

    def test_complete_native_polynomials_match_independent_direct_cashflows(self):
        fixture = Path(__file__).with_name("fixtures") / "xccy_bias_frozen_inputs.json"
        case = load_fixture(fixture)[0]
        z = [interval((i % 5) - 2) / 7 for i in range(30)]
        for label in case["labels"]:
            with self.subTest(axis=label):
                direct = FixedCashflows(case, label, active=True).path(z)
                terms = IndependentTerms(case, label, active=True).terms()
                for group, i in (("A", 0), ("B", 1)):
                    value = at_point(polynomial_terms(terms, group), z)
                    native = at_point(polynomial_terms(terms, group, native=True), z)
                    self.assertLess(magnitude(value - direct[i].value).b, interval("1e-45").a)
                    self.assertLess(magnitude(native - direct[i].derivative).b, interval("1e-45").a)

    def test_nonpositive_coefficient_mean_and_incomplete_coordinates_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "denominator"):
            coefficient_mean_ratio([0], [1], [1], [1], [1], [1])
        with self.assertRaisesRegex(ValueError, "complete"):
            PolynomialTerm(interval(0), (interval(1),), interval(1), ())
        positive = PolynomialTerm(interval(0), (interval(1),), interval(1), (interval(0),))
        with self.assertRaisesRegex(ValueError, "complete"):
            annuity_controls([positive], [], interval(1))

    def test_changed_scalar_error_support_or_arithmetic_precision_cannot_shrink_certificate(self):
        frozen_domain()
        for args in (dict(error="5e-13"), dict(support="6")):
            with self.subTest(args=args), self.assertRaises(ValueError):
                frozen_domain(**args)
        mp.iv.dps = 30
        with self.assertRaisesRegex(ValueError, "precision"):
            frozen_domain()

    def test_native_budget_units_and_old_bound_selection_are_explicit(self):
        result = comparison_row("par", "sigma:EUR/USD:2029-10-20", interval(".1"), interval(".001"))
        self.assertEqual(result["units"], "decimal-spread/(year^-1/2)")
        self.assertIn(interval(1) / 400, result["original_budget"])
        self.assertTrue(result["component_bound_within_budget"])
        result = comparison_row("par", "", interval(".0001"), interval(".0002"))
        self.assertEqual(result["selected"], interval(".0001"))
        self.assertFalse(result["component_bound_within_budget"])
        self.assertIsNone(comparison_row("A", "", interval(1), interval(2))["original_budget"])

    def test_endpoint_display_is_outward_in_rational_arithmetic(self):
        endpoint = encode(interval(1) / 3)
        self.assertLessEqual(Fraction(endpoint["decimal_lower"]), Fraction(1, 3))
        self.assertGreaterEqual(Fraction(endpoint["decimal_upper"]), Fraction(1, 3))
        self.assertLessEqual(rational(endpoint["binary_lower"]), Fraction(1, 3))
        self.assertGreaterEqual(rational(endpoint["binary_upper"]), Fraction(1, 3))

    def test_exponent_cache_separates_anchor_radius_and_arithmetic_precision(self):
        term = PolynomialTerm(interval(0), (interval(1),), interval(1), (interval(0),))
        zero = interval(0), (interval(0),)
        small = exponent_difference(term, zero, interval(1))
        large = exponent_difference(term, zero, interval(2))
        self.assertLess(small.b, large.b)
        self.assertEqual(exponent_difference(term, (interval(0), (interval(1),)), interval(2)), 0)
        constant = PolynomialTerm(interval(1) / 3, (interval(0),), interval(1), (interval(0),))
        high = exponent_factors(constant, zero, interval(1))[1]
        mp.iv.dps = 12
        low = exponent_factors(constant, zero, interval(1))[1]
        self.assertEqual(low, mp.iv.exp(constant.k))
        self.assertLessEqual(low.a, high.a)
        self.assertGreaterEqual(low.b, high.b)

    def test_accepted_certificate_cannot_be_substituted_by_smaller_unidentified_bound(self):
        with patch.object(Path, "read_bytes", return_value=b'{"D1":0}'):
            with self.assertRaisesRegex(ValueError, "identity"):
                read_accepted(Path("substituted.json"))


if __name__ == "__main__":
    unittest.main()
