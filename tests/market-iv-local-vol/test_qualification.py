"""A failed or missing numerical reference must never become a product PASS."""

import unittest
from unittest.mock import patch

from pde import Coefficients
from qualify import _accepted, _finish_study, configurations


class QualificationTest(unittest.TestCase):
    def test_reference_acceptance_rejects_incomplete_nonfinite_and_large_errors(self):
        self.assertTrue(_accepted({"space": [0, 2.5e-5]}))
        for differences in (
            {},
            {"space": []},
            {"time": [0, float("nan")]},
            {"domain": [float("inf")]},
            {"space": [-1]},
            {"space": [2.50001e-5]},
        ):
            self.assertFalse(_accepted(differences))

    def test_reference_failure_uses_only_one_frozen_refinement_level(self):
        coefficient = Coefficients((0.0, 2.0), (0.0, 0.08), (0.04, 0.04))
        runs = {
            (row["nodes"], row["steps"], row["extent"]): dict(values=[0.0] * 12)
            for row in configurations()
        }
        calls = []

        def unavailable(task):
            name, _, maturity, _, configuration = task
            calls.append(
                (
                    configuration["nodes"],
                    configuration["steps"],
                    configuration["extent"],
                )
            )
            return name, maturity, configuration, dict(values=[0.0] * 12)

        with patch("qualify._study_task", side_effect=unavailable):
            result = _finish_study("smile", coefficient, 1.0, runs)
        self.assertEqual(result["status"], "REFERENCE_UNAVAILABLE")
        self.assertEqual(
            calls, [(6401, 4096, 3), (3201, 4096, 3), (6401, 2048, 3), (6401, 4096, 4)]
        )


if __name__ == "__main__":
    unittest.main()
