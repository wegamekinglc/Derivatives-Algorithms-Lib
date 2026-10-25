"""Independent analytic and adversarial controls for financial bias bounds."""

import copy
import unittest
from fractions import Fraction
from pathlib import Path
from unittest.mock import patch

import mpmath as mp

from xccy_acceptance_bias_qualification import (
    admit_claim,
    budget_row,
    finite_term,
    load_fixture,
    mean_quotient,
    moment_errors,
    normal_domain,
    path_quotient,
)
from xccy_acceptance_bias_intervals import encode, interval, upper
from xccy_acceptance_bias_cashflows import IndependentTerms, admit_case, term_arguments
from xccy_acceptance_bias_budget import augmented_row, read_report
from xccy_acceptance_interval import Dual, FixedCashflows, exp, upper_abs


class XccyAcceptanceBiasQualificationTest(unittest.TestCase):
    def setUp(self):
        mp.iv.dps = 70
        mp.mp.dps = 75

    def test_endpoint_encoding_is_exact_and_decimal_is_outward(self):
        result = encode(interval(1) / 3)
        self.assertGreaterEqual(Fraction(result["decimal_upper"]), Fraction(1, 3))
        self.assertLessEqual(Fraction(result["decimal_lower"]), Fraction(1, 3))
        self.assertEqual(len(result["binary_lower"]), 4)
        self.assertEqual(len(result["binary_upper"]), 4)

    def test_support_and_uniform_quantization_are_separate(self):
        domain = normal_domain()
        self.assertGreater(domain["uniform_quantile_error"].a, 0)
        self.assertLess(domain["uniform_quantile_error"].b, interval("4.224587095e-8"))
        self.assertGreater(domain["accepted_scalar_error"].a, interval("9.006864424e-9"))
        self.assertLessEqual(domain["accepted_scalar_error"].b, interval("9.006864426e-9"))
        self.assertLess(domain["mills_upper"].b, (interval(1) / 4294967088 - interval(2) ** -54).a)

    def test_finite_gradient_includes_loading_and_coefficient_derivatives(self):
        error = interval(".001")
        result = finite_term(2, 0, [interval(".3")], 1, 0, [interval(1)], error)
        x, y = mp.mpf(".2"), mp.mpf(".201")

        def value(z):
            return 2 * mp.exp(mp.mpf(".3") * z)

        def gradient(z):
            return mp.exp(mp.mpf(".3") * z) * (1 + 2 * z)

        self.assertLess(abs(value(y) - value(x)), mp.make_mpf(upper(result["value"])._mpi_[0]))
        self.assertLess(abs(gradient(y) - gradient(x)), mp.make_mpf(upper(result["gradient"])._mpi_[0]))
        omitted = finite_term(2, 0, [interval(".3")], 0, 0, [interval(0)], error)
        self.assertEqual(omitted["gradient"], 0)
        self.assertGreater(abs(gradient(y) - gradient(x)), 0)

    def test_small_grid_moments_need_tilted_tail_and_weighted_first_moment(self):
        a, denominator = mp.mpf("1.2"), 16
        z = [mp.sqrt(2) * mp.erfinv(2 * mp.mpf(k) / denominator - 1) for k in range(1, denominator)]
        m0 = mp.exp(a * a / 2)
        exact0 = sum(mp.exp(a * x) for x in z) / (denominator - 1)
        exact1 = sum(x * mp.exp(a * x) for x in z) / (denominator - 1)
        bound = moment_errors(interval("1.2"), interval(0), denominator)
        self.assertLess(abs(exact0 / m0 - 1), mp.make_mpf(upper(bound["r0"])._mpi_[0]))
        self.assertLess(abs(exact1 / m0 - a), mp.make_mpf(upper(bound["r1"])._mpi_[0]))
        self.assertGreater(abs(exact0 / m0 - 1), mp.mpf(1) / (denominator - 1))
        self.assertGreater(abs(exact1 / m0 - a), abs(exact0 / m0 - 1))
        self.assertGreater(bound["tail0"].a, 0)
        self.assertGreater(bound["tail1"].a, 0)

    def test_coefficient_derivative_has_its_own_transform_error(self):
        result = finite_term(2, 0, [interval(".3")], 1, 0, [interval(0)], interval(".001"))
        difference = mp.exp(mp.mpf(".3") * mp.mpf(".201")) - mp.exp(mp.mpf(".3") * mp.mpf(".2"))
        self.assertGreater(result["gradient"].a, 0)
        self.assertLess(difference, mp.make_mpf(upper(result["gradient"])._mpi_[0]))

    def test_zero_loading_has_unit_moment_and_zero_first_moment(self):
        bound = moment_errors(interval(0), interval(0), 16)
        self.assertGreaterEqual(bound["r0"].a, 0)
        self.assertGreaterEqual(bound["r1"].a, 0)

    def test_uniform_rounding_cannot_be_deleted_from_moment_coupling(self):
        result = moment_errors(interval(".2"), interval(0))
        self.assertGreater(result["coupling0"].a, 0)
        self.assertGreater(result["coupling1"].a, 0)

    def test_mean_quotient_is_not_mean_pathwise_quotient(self):
        result = mean_quotient(interval(-2), interval(3), interval(1), interval(4))
        self.assertTrue(interval("1.5") in result[0])
        self.assertTrue(interval("2.75") in result[1])
        self.assertNotEqual(mp.mpf("1.5"), (mp.mpf(1) / 1 + mp.mpf(5) / 3) / 2)

    def test_nonpositive_or_unproved_annuity_lower_bound_is_rejected(self):
        for lower in (0, -1, mp.iv.mpf([-1, 1])):
            with self.subTest(lower=str(lower)), self.assertRaisesRegex(ValueError, "denominator"):
                path_quotient(lower, 2, 3, 4, 5, 6, 7, 8)
        with self.assertRaisesRegex(ValueError, "denominator"):
            mean_quotient(mp.iv.mpf([-1, 1]), interval(3), interval(1), interval(4))

    def test_integer_quotient_arguments_do_not_round_through_binary64(self):
        value, _ = path_quotient(3, 0, 0, 0, 0, 1, 0, 0)
        self.assertGreaterEqual(value.b, (interval(1) / 3).a)

    def test_new_bias_cannot_hide_sampling_error_or_restore_old_red(self):
        result = budget_row("Price", "USD", ".0025", ".06", ".001", "1e-12", ".0002", False)
        self.assertFalse(result["precision_pass"])
        self.assertFalse(result["combined_pass"])
        self.assertFalse(result["original_pass"])
        self.assertGreater(result["new_precision"].a, interval(".06019"))

    def test_precision_and_combined_flags_remain_separate(self):
        old = dict(
            scale=0.01,
            se=0.0001,
            mean=1,
            reference=0,
            numeric_indicator=0,
            parity=False,
            precision=True,
            combined=False,
            precision_indicator=0.0003,
            combined_indicator=1.0003,
            epsilon=1e-10,
        )
        result = augmented_row(old, "Price", "USD", interval(0), budget_row)
        self.assertTrue(result["precision_pass"])
        self.assertFalse(result["combined_pass"])
        self.assertEqual(result["original_flags"], dict(parity=False, precision=True, combined=False))

    def test_wrong_units_and_negative_budget_inputs_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "units"):
            budget_row("par", "basis-points", ".000025", ".001", ".001", 0, 0, False)
        with self.assertRaisesRegex(ValueError, "nonnegative"):
            budget_row("Price", "USD", ".0025", "-.001", ".001", 0, 0, False)

    def test_unproved_joint_law_or_caller_cannot_claim_global_qualification(self):
        good = dict(
            financial_bias_bound_qualified=False,
            actual_joint_law_qualified=False,
            caller_binding_qualified=False,
            original_numerical_pass=False,
        )
        admit_claim(good)
        for field in (
            "financial_bias_bound_qualified",
            "actual_joint_law_qualified",
            "caller_binding_qualified",
            "original_numerical_pass",
        ):
            with self.subTest(field=field), self.assertRaises(ValueError):
                admit_claim(dict(good, **{field: True}))

    def test_affine_path_and_native_loading_derivative_match_independent_path(self):
        fixture = Path(__file__).with_name("fixtures") / "xccy_bias_frozen_inputs.json"
        case = load_fixture(fixture)[0]
        label = case["labels"][-1]
        normals = [(-1 if i % 2 else 1) * (i + 1) / 31 for i in range(30)]
        expected = FixedCashflows(case, label, active=True).path(normals)
        actual = {"A": Dual(), "B": Dual()}
        without_loading = {"A": interval(0), "B": interval(0)}
        for term in IndependentTerms(case, label, active=True).terms():
            group, c, exponent = term
            argument = exponent.k + sum((x * z for x, z in zip(exponent.ell, normals, strict=True)), Dual())
            actual[group] += c * exp(argument)
            cv, _, _, dc, dk, _ = term_arguments(term)
            without_loading[group] += mp.iv.exp(argument.value) * (dc + cv * dk)
        for i, group in enumerate(("A", "B")):
            self.assertLess(upper_abs(actual[group].value - expected[i].value), mp.mpf("1e-45"))
            self.assertLess(upper_abs(actual[group].derivative - expected[i].derivative), mp.mpf("1e-45"))
        self.assertGreater(upper_abs(actual["B"].derivative - without_loading["B"]), 1)

    def test_frozen_identity_schedule_and_native_axes_reject_changes(self):
        fixture = Path(__file__).with_name("fixtures") / "xccy_bias_frozen_inputs.json"
        case = load_fixture(fixture)[0]
        admit_case(case)
        for field, changed in (("labels", case["labels"][::-1]), ("dimension", 25), ("foreign_spread", True)):
            bad = dict(case, **{field: changed})
            with self.subTest(field=field), self.assertRaises(ValueError):
                admit_case(bad)
        bad = copy.deepcopy(case)
        bad["foreign_periods"][0]["payment"] = bad["foreign_periods"][0]["end"]
        with self.assertRaisesRegex(ValueError, "calendar"):
            admit_case(bad)

    def test_changed_frozen_input_or_statistical_report_is_rejected(self):
        with patch.object(Path, "read_bytes", return_value=b'{"scale": 1, "se": 0}'):
            with self.assertRaisesRegex(ValueError, "input identity"):
                load_fixture(Path("mutated-input.json"))
            with self.assertRaisesRegex(ValueError, "budget/report identity"):
                read_report(Path("mutated-report.json"))

    def test_time_grid_slots_and_native_jacobian_are_independently_admitted(self):
        fixture = Path(__file__).with_name("fixtures") / "xccy_bias_frozen_inputs.json"
        case = load_fixture(fixture)[0]
        grid = dict(case, grid=case["grid"][:-1] + ["2027-11-07"])
        slots = dict(case, coordinates=case["coordinates"][::-1])
        jacobian = copy.deepcopy(case)
        jacobian["model"]["curves"][0]["jacobian"][1][0] = 2
        for name, changed in zip(("grid", "slots", "jacobian"), (grid, slots, jacobian), strict=True):
            with self.subTest(input=name), self.assertRaises(ValueError):
                admit_case(changed)


if __name__ == "__main__":
    unittest.main()
