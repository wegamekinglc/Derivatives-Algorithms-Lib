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


def test_product_debug_barrier_knockout():
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


def _make_european_call():
    maturity = dal.Date_(2025, 9, 24)
    dates = [dal.Cell_("STRIKE"), dal.Cell_(maturity)]
    events = ["100.0", "call pays MAX(spot() - STRIKE, 0.0)"]
    return dal.Product_New(dates, events)


def test_product_debug_json():
    """Product_DebugJson returns the machine-friendly schema with resolved metadata."""
    product = _make_european_call()

    debug_json = dal.Product_DebugJson(product)
    assert debug_json.startswith('{"schema":"dal.script-product/1"')  # nosec B101
    assert '"payoff_index":0' in debug_json  # nosec B101
    assert '"variables":[{"index":0,"name":"call"}]' in debug_json  # nosec B101
    assert '"constants":[{"index":0,"name":"STRIKE","value":100}]' in debug_json  # nosec B101
    assert '"kind":"pays"' in debug_json  # nosec B101
    assert '"phase":"future"' in debug_json  # nosec B101


def test_product_debug_json_is_repeatable():
    """Product_DebugJson output is deterministic."""
    product = _make_european_call()

    assert dal.Product_DebugJson(product) == dal.Product_DebugJson(product)  # nosec B101


def test_product_debug_tree():
    """Product_DebugTree returns a human-friendly unicode tree."""
    product = _make_european_call()

    tree = dal.Product_DebugTree(product)
    assert "Variables: call*" in tree  # nosec B101
    assert "Constants: STRIKE=100" in tree  # nosec B101
    assert "📅 1 · 2025-09-24 · future" in tree  # nosec B101
    assert "call ⇐ max(spot() − STRIKE, 0)" in tree  # nosec B101


def test_product_debug_tree_ascii_and_width():
    """Product_DebugTree exposes the ascii style and width parameters."""
    product = _make_european_call()

    ascii_tree = dal.Product_DebugTree(product, ascii=True, width=40)
    assert "# 1 @ 2025-09-24 @ future" in ascii_tree  # nosec B101
    assert "`-- (1) call <= max(spot() - STRIKE, 0)" in ascii_tree  # nosec B101

    narrow = dal.Product_DebugTree(product, ascii=True, width=10)
    #  Too narrow to inline: the payoff statement keeps its header but branches
    assert "`-- (1) call <=" in narrow  # nosec B101
    assert "`-- max" in narrow  # nosec B101
    assert "`-- 0" in narrow  # nosec B101
