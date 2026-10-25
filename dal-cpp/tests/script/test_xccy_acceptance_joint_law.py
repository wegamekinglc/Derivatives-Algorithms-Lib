"""Independent exact controls for local law and mixed-moment admission."""

import unittest
from dataclasses import replace
from fractions import Fraction as F

from xccy_acceptance_joint_law import (
    MomentCertificate,
    MomentCondition,
    MomentRequest,
    TinyLaw,
    admit_tiny_law,
    exact_moment,
)


def independent_law():
    return TinyLaw(("x", "y"), tuple((point, F(1, 4)) for point in ((0, 0), (0, 1), (1, 0), (1, 1))))


def conditions():
    return (
        MomentCondition((F(2), F(2)), (), F(9, 4), F(9, 4)),
        MomentCondition((F(2), F(2)), (F(1), F(0)), F(3, 2), F(3, 2)),
    )


def evidence(law, required=None, moments=(F(9, 4), F(3, 2))):
    request = MomentRequest(law.identity(), law.coordinates, conditions() if required is None else required)
    return request, MomentCertificate(law.identity(), request.identity(), moments)


class XccyAcceptanceJointLawTest(unittest.TestCase):
    def test_explicit_independent_law_has_exact_positive_coverage(self):
        law = independent_law()
        request, certificate = evidence(law)
        actual = admit_tiny_law(law, request, certificate)
        self.assertTrue(actual["local_moments_qualified"])
        self.assertEqual(actual["scope"], "synthetic-exact-discrete/1")
        self.assertEqual(actual["moments"], (F(9, 4), F(3, 2)))
        self.assertNotIn("actual_joint_law_qualified", actual)

    def test_same_marginals_do_not_admit_forged_independent_joint_moments(self):
        correlated = TinyLaw(("x", "y"), (((0, 0), F(1, 2)), ((1, 1), F(1, 2))))
        for law in (independent_law(), correlated):
            for j in range(2):
                self.assertEqual(sum(w for z, w in law.atoms if z[j] == 1), F(1, 2))
        request, certificate = evidence(correlated)
        with self.assertRaisesRegex(ValueError, "replay"):
            admit_tiny_law(correlated, request, certificate)

    def test_zeroth_and_all_marginals_do_not_replace_directional_first_moment(self):
        correlated = TinyLaw(("x", "y"), (((0, 0), F(1, 2)), ((1, 1), F(1, 2))))
        required = (
            MomentCondition((F(2), F(1)), (), F(3, 2), F(3, 2)),
            MomentCondition((F(2), F(1)), (F(0), F(1)), F(3, 4), F(3, 4)),
        )
        self.assertEqual(exact_moment(independent_law(), required[0]), F(3, 2))
        self.assertEqual(exact_moment(correlated, required[0]), F(3, 2))
        self.assertEqual(exact_moment(independent_law(), required[1]), F(3, 4))
        self.assertEqual(exact_moment(correlated, required[1]), F(1))
        request, certificate = evidence(correlated, required, (F(3, 2), F(3, 4)))
        with self.assertRaisesRegex(ValueError, "replay"):
            admit_tiny_law(correlated, request, certificate)

    def test_dependent_law_can_meet_its_own_local_obligations(self):
        law = TinyLaw(("x", "y"), (((0, 0), F(1, 2)), ((1, 1), F(1, 2))))
        required = (
            replace(conditions()[0], lower=F(5, 2), upper=F(5, 2)),
            replace(conditions()[1], lower=F(2), upper=F(2)),
        )
        request, certificate = evidence(law, required, (F(5, 2), F(2)))
        self.assertTrue(admit_tiny_law(law, request, certificate)["local_moments_qualified"])

    def test_signed_direction_and_negative_integer_atoms_are_exact(self):
        law = TinyLaw(("x", "y"), (((-1, 0), F(1, 3)), ((1, 1), F(2, 3))))
        required = (MomentCondition((F(2), F(3)), (F(1), F(-2)), F(-25, 6), F(-25, 6)),)
        request, certificate = evidence(law, required, (F(-25, 6),))
        self.assertEqual(admit_tiny_law(law, request, certificate)["moments"], (F(-25, 6),))

    def test_changed_law_weights_invalidate_certificate(self):
        law = independent_law()
        request, certificate = evidence(law)
        changed = replace(law, atoms=tuple((z, F(1, 8) if z[0] == 0 else F(3, 8)) for z, _ in law.atoms))
        with self.assertRaisesRegex(ValueError, "law identity"):
            admit_tiny_law(changed, request, certificate)

    def test_coordinates_and_dimension_are_bound(self):
        law = independent_law()
        request, certificate = evidence(law)
        for coordinates in (("y", "x"), ("x",), ("x", "other")):
            with (
                self.subTest(coordinates=coordinates),
                self.assertRaisesRegex(ValueError, "coordinate identity"),
            ):
                admit_tiny_law(law, replace(request, coordinates=coordinates), certificate)

    def test_changed_tilt_direction_and_envelope_invalidate_request(self):
        law = independent_law()
        request, certificate = evidence(law)
        for condition in (
            replace(conditions()[0], bases=(F(3), F(2))),
            replace(conditions()[0], direction=(F(1), F(0))),
            replace(conditions()[0], upper=F(3)),
        ):
            changed = replace(request, conditions=(condition, conditions()[1]))
            with self.subTest(condition=condition), self.assertRaisesRegex(ValueError, "request identity"):
                admit_tiny_law(law, changed, certificate)

    def test_missing_extra_and_marginal_only_moment_coverage_are_rejected(self):
        law = independent_law()
        request, certificate = evidence(law)
        for moments in ((), (F(9, 4),), (*certificate.moments, F(3, 2))):
            with (
                self.subTest(moments=moments),
                self.assertRaisesRegex(ValueError, "complete moment coverage"),
            ):
                admit_tiny_law(law, request, replace(certificate, moments=moments))
        with self.assertRaisesRegex(ValueError, "nonempty complete"):
            replace(request, conditions=()).identity()

    def test_replayed_moment_must_satisfy_required_envelope(self):
        law = independent_law()
        wrong = (replace(conditions()[0], lower=F(5, 2), upper=F(3)),)
        request, certificate = evidence(law, wrong, (F(9, 4),))
        with self.assertRaisesRegex(ValueError, "moment envelope"):
            admit_tiny_law(law, request, certificate)

    def test_invalid_weights_and_atoms_are_rejected(self):
        law = independent_law()
        for atoms in (
            (((0, 0), F(-1)), ((1, 1), F(2))),
            (((0, 0), F(1, 2)),),
            (((0, 0), F(1, 2)), ((0, 0), F(1, 2))),
            (((0,), F(1)),),
            (((0, 0), 1.0),),
        ):
            with self.subTest(atoms=atoms), self.assertRaises(ValueError):
                replace(law, atoms=atoms).validate()

    def test_unbounded_or_inexact_synthetic_inputs_are_rejected(self):
        law = independent_law()
        for condition in (
            replace(conditions()[0], bases=(F(0), F(2))),
            replace(conditions()[0], bases=(F(1 << 129), F(2))),
            replace(conditions()[0], bases=(2.0, F(2))),
            replace(conditions()[0], direction=(F(1),)),
        ):
            with self.subTest(condition=condition), self.assertRaises(ValueError):
                exact_moment(law, condition)
        with self.assertRaisesRegex(ValueError, "atom cost limit"):
            TinyLaw(("x",), (((9,), F(1)),)).validate()

    def test_no_admission_from_missing_law_or_boolean_claim(self):
        law = independent_law()
        request, certificate = evidence(law)
        for missing in (None, True, {"qualified": True}):
            with self.subTest(law=missing), self.assertRaisesRegex(ValueError, "explicit synthetic law"):
                admit_tiny_law(missing, request, certificate)


if __name__ == "__main__":
    unittest.main()
