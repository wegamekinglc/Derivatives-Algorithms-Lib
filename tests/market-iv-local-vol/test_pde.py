"""Small solver checks; full independent convergence evidence is opt-in."""

import unittest

import mpmath as mp
import numpy as np

from pde import Coefficients, GridCoefficients, solve, timeline
from reference import black, carry


class PdeTest(unittest.TestCase):
    def test_black_limit_probability_and_forward_with_carry_reversals(self):
        coefficients = Coefficients((0.0, 2.0), (0.0, 0.08), (0.04, 0.04))
        time = 2.25
        forward, discount = map(float, carry(time))
        strikes = np.array([0.7, 1, 1.3]) * forward
        result = solve(coefficients, time, strikes, nodes=801, steps=512)["values"]
        expected = [
            float(
                black(
                    mp.mpf(forward),
                    mp.mpf(discount),
                    mp.mpf(strike),
                    mp.mpf(".09"),
                    call,
                )
            )
            for call in (True, False)
            for strike in strikes
        ]
        for actual, oracle in zip(result[:6], expected):
            self.assertAlmostEqual(actual / 100, oracle / 100, delta=2.5e-5)
        self.assertAlmostEqual(result[-2] / discount, 1, delta=2.5e-5)
        self.assertAlmostEqual(result[-1] / (discount * forward), 1, delta=2.5e-5)

    def test_discrete_constant_vol_pde_matches_continuous_black_limit(self):
        continuous = Coefficients((0.0, 2.0), (0.0, 0.08), (0.04, 0.04))
        discrete = GridCoefficients(
            continuous,
            np.array([-3.0, 3.0]),
            np.array([1 / 36500, 2.25]),
            np.full((2, 2), 0.2),
        )
        result = solve(discrete, 1.0, [100.0], nodes=801, steps=512)
        reference = solve(continuous, 1.0, [100.0], nodes=801, steps=512)
        for actual, expected in zip(result["values"], reference["values"]):
            self.assertAlmostEqual(actual, expected, delta=1e-10)
        self.assertLess(result["max_relative_linear_residual"], 1e-10)

    def test_interpolation_is_vol_linear_with_flat_boundaries(self):
        continuous = Coefficients((0.0, 2.0), (0.0, 0.08), (0.04, 0.04))
        coefficients = GridCoefficients(
            continuous,
            np.log(np.array([0.8, 1.2])),
            np.array([0.1, 1.0]),
            np.array([[0.2, 0.4], [0.3, 0.5]]),
        )
        middle = np.mean(coefficients.log_spots)
        self.assertAlmostEqual(
            coefficients.variance(0.55, np.array([middle]))[0], 0.35**2, delta=1e-15
        )
        self.assertAlmostEqual(
            coefficients.variance(-1.0, np.array([-10.0]))[0], 0.2**2, delta=1e-15
        )
        self.assertAlmostEqual(
            coefficients.variance(3.0, np.array([10.0]))[0], 0.5**2, delta=1e-15
        )

    def test_timeline_preserves_unquoted_carry_knot(self):
        times = timeline(2.25, 512)
        for day in (91, 182, 273, 365, 730):
            self.assertIn(day / 365, times)
        self.assertTrue(np.all(np.diff(times) > 0))
        self.assertLessEqual(np.max(np.diff(times)), 2.25 / 512 * (1 + 1e-12))


if __name__ == "__main__":
    unittest.main()
