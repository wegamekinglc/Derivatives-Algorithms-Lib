"""A discrete reference must retain its own coefficient during fallback."""

import unittest
from unittest.mock import patch

import numpy as np

from grid_study import CertifiedGrid
from pde import Coefficients
from qualify import _finish_study, configurations


class GridStudyTest(unittest.TestCase):
    def test_fallback_refines_discrete_coefficient_without_substituting_continuous(
        self,
    ):
        continuous = Coefficients((0.0, 2.0), (0.0, 0.08), (0.04, 0.04))
        discrete = CertifiedGrid(
            continuous,
            np.array([-3.0, 3.0]),
            np.array([0.01, 2.0]),
            np.full((2, 2), 0.5),
        )
        observed = []

        def unresolved(task):
            name, model, maturity, _, setting = task
            observed.append(float(model.variance(0.5, np.array([0.0]))[0]))
            return name, maturity, setting, dict(values=[0.0] * 12)

        runs = {
            (row["nodes"], row["steps"], row["extent"]): dict(values=[0.0] * 12)
            for row in configurations()
        }
        with patch("qualify._study_task", side_effect=unresolved):
            result = _finish_study("discrete", discrete, 1.0, runs)
        self.assertEqual(observed, [0.25] * 4)
        self.assertEqual(result["status"], "REFERENCE_UNAVAILABLE")


if __name__ == "__main__":
    unittest.main()
