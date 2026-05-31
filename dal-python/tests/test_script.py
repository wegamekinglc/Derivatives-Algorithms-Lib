"""Tests for script product creation and debug."""

import dal


def test_product_new_european_call():
    """Create a simple European call option product."""
    maturity = dal.Date_(2025, 9, 24)
    dates = [dal.Cell_("STRIKE"), dal.Cell_(maturity)]
    events = ["100.0", "call pays MAX(spot() - STRIKE, 0.0)"]
    product = dal.Product_New(dates, events)
    assert product is not None  # nosec B101 - pytest assertions are intentional


def test_product_new_european_put():
    """Create a simple european put option product."""
    maturity = dal.Date_(2025, 9, 24)
    dates = [dal.Cell_("STRIKE"), dal.Cell_(maturity)]
    events = ["100.0", "put pays MAX(STRIKE - spot(), 0.0)"]
    product = dal.Product_New(dates, events)
    assert product is not None  # nosec B101 - pytest assertions are intentional


def test_product_new_digital():
    """Create a digital (binary) option product."""
    maturity = dal.Date_(2025, 9, 24)
    dates = [dal.Cell_("STRIKE"), dal.Cell_(maturity)]
    events = ["100.0", "call pays IF(spot() > STRIKE, 1.0, 0.0)"]
    product = dal.Product_New(dates, events)
    assert product is not None  # nosec B101 - pytest assertions are intentional


def test_product_new_with_named_params():
    """Create a product using named macro parameters (BARRIER, STRIKE)."""
    maturity = dal.Date_(2025, 9, 24)
    dates = [
        dal.Cell_("BARRIER"),
        dal.Cell_("STRIKE"),
        dal.Cell_(maturity),
    ]
    events = [
        "150.0",
        "120.0",
        "call pays MAX(spot() - STRIKE, 0.0)",
    ]
    product = dal.Product_New(dates, events)
    assert product is not None  # nosec B101 - pytest assertions are intentional


def test_product_new_with_state_variable():
    """Create a product that uses a state variable."""
    maturity = dal.Date_(2025, 9, 24)
    eval_date = dal.Date_(2022, 9, 25)
    dates = [
        dal.Cell_("STRIKE"),
        dal.Cell_(eval_date),
        dal.Cell_(maturity),
    ]
    events = [
        "100.0",
        "alive = 1",
        "call pays alive * MAX(spot() - STRIKE, 0.0)",
    ]
    product = dal.Product_New(dates, events)
    assert product is not None  # nosec B101 - pytest assertions are intentional


def test_product_new_string_dates_auto_convert():
    """Product_New auto-converts string macro names to Cell_."""
    maturity = dal.Date_(2025, 9, 24)
    # "STRIKE" is a string, not a Cell_ -- Product_New should handle it
    dates = ["STRIKE", dal.Cell_(maturity)]
    events = ["100.0", "call pays MAX(spot() - STRIKE, 0.0)"]
    product = dal.Product_New(dates, events)
    assert product is not None  # nosec B101 - pytest assertions are intentional


def test_product_debug():
    """Product_Debug returns a non-empty debug string."""
    maturity = dal.Date_(2025, 9, 24)
    dates = [dal.Cell_("STRIKE"), dal.Cell_(maturity)]
    events = ["100.0", "call pays MAX(spot() - STRIKE, 0.0)"]
    product = dal.Product_New(dates, events)

    debug_str = dal.Product_Debug(product)
    assert isinstance(debug_str, str)  # nosec B101 - pytest assertions are intentional
    assert len(debug_str) > 0  # nosec B101 - pytest assertions are intentional


def test_product_debug_contains_payoff_info():
    """Product_Debug output references the payoff structure."""
    maturity = dal.Date_(2025, 9, 24)
    dates = [dal.Cell_("STRIKE"), dal.Cell_(maturity)]
    events = ["100.0", "call pays MAX(spot() - STRIKE, 0.0)"]
    product = dal.Product_New(dates, events)

    debug_str = dal.Product_Debug(product)
    # Debug output should contain some structural information
    assert len(debug_str) > 10  # nosec B101 - pytest assertions are intentional


def test_product_new_barrier_knockout():
    """Create a barrier knockout product with monitoring schedule."""
    eval_date = dal.Date_(2022, 9, 25)
    maturity = dal.Date_(2025, 9, 25)

    dates = [
        dal.Cell_("BARRIER"),
        dal.Cell_("STRIKE"),
        dal.Cell_(eval_date),
        dal.Cell_(
            "START: 2022-09-25\n"
            "END: 2025-09-25\n"
            "FREQ: 1W"
        ),
        dal.Cell_(maturity),
    ]
    events = [
        "150.0",
        "120.0",
        "alive = 1",
        "IF spot() > BARRIER:0.1 THEN alive = 0 END",
        "IF spot() > BARRIER:0.1 THEN alive = 0 END "
        "uoc pays alive * MAX(spot() - STRIKE, 0.0)",
    ]
    product = dal.Product_New(dates, events)
    assert product is not None  # nosec B101 - pytest assertions are intentional
    debug_str = dal.Product_Debug(product)
    assert len(debug_str) > 0  # nosec B101 - pytest assertions are intentional
