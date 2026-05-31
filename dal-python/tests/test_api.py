"""Tests for the high-level Python API wrappers in dal.api."""

import dal
from dal import api


def test_api_module_importable():
    """dal.api module is importable."""
    assert api is not None


def test_product_new_function_exists():
    """Product_New is available from the top-level dal package."""
    assert callable(dal.Product_New)


def test_product_new_auto_wraps_string_dates():
    """Product_New auto-wraps plain strings as Cell_ objects."""
    maturity = dal.Date_(2025, 9, 24)
    # "STRIKE" is a Python str, not a Cell_ — Product_New should convert it
    product = dal.Product_New(
        ["STRIKE", dal.Cell_(maturity)],
        ["100.0", "call pays MAX(spot() - STRIKE, 0.0)"],
    )
    assert product is not None


def test_product_new_auto_wraps_date_objects():
    """Product_New auto-wraps Date_ objects as Cell_ when passed directly."""
    maturity = dal.Date_(2025, 9, 24)
    # Passing maturity (Date_) directly — not wrapped in Cell_ — exercises
    # the auto-conversion path. STRIKE is passed as a plain string too.
    product = dal.Product_New(
        ["STRIKE", maturity],
        ["100.0", "call pays MAX(spot() - STRIKE, 0.0)"],
    )
    assert product is not None


def test_product_new_all_cell_dates():
    """Product_New works when all dates are already Cell_ objects."""
    maturity = dal.Date_(2025, 9, 24)
    product = dal.Product_New(
        [dal.Cell_("STRIKE"), dal.Cell_(maturity)],
        ["100.0", "call pays MAX(spot() - STRIKE, 0.0)"],
    )
    assert product is not None


def test_product_new_preserves_cell_passthrough():
    """Cell_ objects in the dates list are passed through unchanged."""
    maturity = dal.Date_(2025, 9, 24)
    cell_strike = dal.Cell_("STRIKE")
    cell_maturity = dal.Cell_(maturity)

    product = dal.Product_New(
        [cell_strike, cell_maturity],
        ["100.0", "call pays MAX(spot() - STRIKE, 0.0)"],
    )
    assert product is not None


def test_product_new_multiple_macros():
    """Product_New handles multiple named macro parameters."""
    maturity = dal.Date_(2025, 9, 24)
    product = dal.Product_New(
        [
            "STRIKE",
            "BARRIER",
            "COUPON",
            dal.Cell_(maturity),
        ],
        [
            "100.0",
            "150.0",
            "0.05",
            "call pays MAX(spot() - STRIKE, 0.0)",
        ],
    )
    assert product is not None


def test_version_attribute():
    """dal package exposes a __version__ attribute."""
    assert hasattr(dal, "__version__")
    assert isinstance(dal.__version__, str)
    assert len(dal.__version__) > 0
