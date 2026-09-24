"""Characterize the UOC example's pricing rows at a fixed Sobol path count."""

import sys
import unittest


NON_AAD = ("32768", "153", "1.200024", "#NA", "#NA", "#NA", "#NA", "#NA", "#NA")
AAD = (
    "32768",
    "153",
    "1.199514",
    "0.033473",
    "12.564193",
    "-16.166020",
    "-6.830707",
    "0.081443",
    "-0.119699",
)
EXPECTED = {
    "Non-AAD": NON_AAD,
    "Non-AAD Comp": NON_AAD,
    "FDM": (
        "32768",
        "153",
        "1.200024",
        "0.125086",
        "18.250356",
        "-21.853883",
        "#NA",
        "0.020418",
        "-0.119743",
    ),
    "AAD": AAD,
    "AAD Comp": AAD,
}


class UocOutputTest(unittest.TestCase):
    def test_fixed_path_prices_and_greeks(self):
        lines = sys.stdin.read().splitlines()
        for label, expected in EXPECTED.items():
            with self.subTest(label=label):
                rows = [line for line in lines if line.startswith(f"{label:<14}")]
                self.assertEqual(len(rows), 1)
                self.assertEqual(tuple(rows[0][14:].split()[:-1]), expected)


if __name__ == "__main__":
    unittest.main()
