"""Exercise the fixed-point convention used by Python examples."""

from pathlib import Path
from runpy import run_path

module_path = Path(__file__).resolve().parents[1] / "examples" / "float_format.py"
format_float = run_path(str(module_path))["format_float"]


def test_example_float_format():
    assert format_float(0.0) == "0.00"
    assert format_float(12.0) == "12.00"
    assert format_float(1.2345) == "1.234"
    assert format_float(0.00001234) == "0.00001234"
    assert format_float(-0.0000000001234) == "-0.0000000001234"
    assert format_float(0.01, min_decimals=6) == "0.010000"
    assert format_float(float("inf")) == "inf"
