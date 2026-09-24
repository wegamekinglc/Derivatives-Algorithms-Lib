"""Characterize the UOC example's pricing rows at a fixed Sobol path count."""

import os
import subprocess
import unittest
from pathlib import Path


DEFAULT_BINARY = (
    Path(__file__).resolve().parents[3] / "build/Release-linux/dal-cpp/examples/uoc/uoc"
)
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
        binary = Path(os.environ.get("UOC_BINARY", DEFAULT_BINARY))
        environment = {**os.environ, "DAL_NUM_THREADS": "4"}
        result = subprocess.run(
            (str(binary), "32768"),
            check=True,
            capture_output=True,
            text=True,
            env=environment,
        )
        lines = result.stdout.splitlines()
        for label, expected in EXPECTED.items():
            with self.subTest(label=label):
                rows = [line for line in lines if line.startswith(f"{label:<14}")]
                self.assertEqual(len(rows), 1)
                self.assertEqual(tuple(rows[0][14:].split()[:-1]), expected)


if __name__ == "__main__":
    unittest.main()
