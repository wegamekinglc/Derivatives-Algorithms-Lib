"""Aligned reference checks shared by the native AAD examples."""

import math


def print_reference_header(title):
    print(f"\n{title}")
    print(f"{'Quantity':<36}{'Native':>18}{'Reference':>18}{'Absolute error':>16}\n{'-' * 88}")


def compare(label, actual, reference, tolerance=1e-8):
    error = abs(actual - reference)
    print(f"{label:<36}{actual:>18.8f}{reference:>18.8f}{error:>16.3e}")
    if not math.isfinite(actual) or error > tolerance:
        raise RuntimeError(f"{label}: expected {reference}, received {actual}")
