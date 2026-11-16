"""Fast, independent checks; the long qualification is an explicit command."""

import unittest

import mpmath as mp
import numpy as np

from reference import black, carry, local_variance, pchip_slopes, theta_at


class ReferenceTest(unittest.TestCase):
    def test_black_parity_with_negative_rate_and_carry_tail(self):
        with mp.workdps(90):
            for time in (mp.mpf(91) / 365, 1, 2, mp.mpf("2.25")):
                forward, discount = carry(time)
                for ratio in (".7", ".85", "1", "1.15", "1.3"):
                    strike = forward * mp.mpf(ratio)
                    call = black(forward, discount, strike, mp.mpf(".04") * time)
                    put = black(forward, discount, strike, mp.mpf(".04") * time, False)
                    self.assertLess(
                        abs(call - put - discount * (forward - strike)), mp.mpf("1e-85")
                    )

    def test_pchip_knots_and_linear_tail(self):
        with mp.workdps(90):
            times = list(map(mp.mpf, ("0", ".25", ".5", "1", "2")))
            values = list(map(mp.mpf, ("0", ".009", ".018", ".04", ".09")))
            slopes = pchip_slopes(times, values)
            for time, value, slope in zip(times, values, slopes):
                actual, derivative = theta_at(time, times, values, slopes)
                self.assertEqual(actual, value)
                self.assertEqual(derivative, slope)
            actual, derivative = theta_at(mp.mpf("2.25"), times, values, slopes)
            self.assertEqual(actual, values[-1] + slopes[-1] / 4)
            self.assertEqual(derivative, slopes[-1])

    def test_flat_limit_of_high_precision_derivatives(self):
        with mp.workdps(90):
            for time in (mp.mpf(".00001"), mp.mpf(".4"), mp.mpf("2.25")):
                for k in (mp.mpf("-1.6"), mp.mpf("0"), mp.mpf("1.4")):
                    self.assertLess(
                        abs(
                            local_variance(mp.mpf(".04") * time, mp.mpf(".04"), 0, 0, k)
                            - mp.mpf(".04")
                        ),
                        mp.mpf("1e-85"),
                    )

    def test_numpy_coefficients_against_high_precision_diff(self):
        from reference import variance_numpy

        with mp.workdps(90):
            for k in np.linspace(-4, 4, 17):
                ref = local_variance(
                    mp.mpf(".043"),
                    mp.mpf(".049"),
                    mp.mpf("-.35"),
                    mp.mpf(".6"),
                    mp.mpf(float(k)),
                )
                actual = variance_numpy(0.043, 0.049, -0.35, 0.6, np.array([k]))[0]
                self.assertAlmostEqual(actual, float(ref), delta=2e-14)


if __name__ == "__main__":
    unittest.main()
