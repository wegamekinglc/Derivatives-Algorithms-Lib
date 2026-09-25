"""Fixed-point formatting for numerical output in examples."""

import math


def format_float(value, min_decimals=2, min_significant_digits=4):
    """Show small nonzero values with at least four significant digits."""
    decimals = min_decimals
    if math.isfinite(value) and value != 0.0:
        decimals = max(
            decimals,
            min_significant_digits - 1 - math.floor(math.log10(abs(value))),
        )
    return f"{value:.{decimals}f}"
