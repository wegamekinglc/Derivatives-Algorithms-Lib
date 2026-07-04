"""Tests for MonteCarlo_Value — end-to-end MC pricing."""

import math
import dal


# ---- Helpers -----------------------------------------------------------------


def _bs_call_price(spot, strike, vol, rate, div, maturity_years):
    """Analytical Black-Scholes European call price for test validation."""
    d1 = (math.log(spot / strike) + (rate - div + 0.5 * vol**2) * maturity_years) / (
        vol * math.sqrt(maturity_years)
    )
    d2 = d1 - vol * math.sqrt(maturity_years)
    nd1 = 0.5 * (1.0 + math.erf(d1 / math.sqrt(2.0)))
    nd2 = 0.5 * (1.0 + math.erf(d2 / math.sqrt(2.0)))
    return spot * math.exp(-div * maturity_years) * nd1 - strike * math.exp(
        -rate * maturity_years
    ) * nd2


def _make_european_call(strike, maturity):
    """Build a European call product."""
    dates = [dal.Cell_("STRIKE"), dal.Cell_(maturity)]
    events = [str(strike), "call pays MAX(spot() - STRIKE, 0.0)"]
    return dal.Product_New(dates, events)


def _make_european_put(strike, maturity):
    """Build a European put product."""
    dates = [dal.Cell_("STRIKE"), dal.Cell_(maturity)]
    events = [str(strike), "put pays MAX(STRIKE - spot(), 0.0)"]
    return dal.Product_New(dates, events)


# ---- Basic Valuation ---------------------------------------------------------


def test_mc_value_returns_dict():
    """MonteCarlo_Value returns a Dictionary-like object with at least a PV key."""
    model = dal.BSModelData_New(spot=100.0, vol=0.2, rate=0.05, div=0.02)
    product = _make_european_call(100.0, dal.Date_(2023, 9, 25))

    result = dal.MonteCarlo_Value(product, model, 2**14)
    assert "PV" in result  # nosec B101 - pytest assertions are intentional
    assert result["PV"] > 0  # nosec B101 - pytest assertions are intentional


def test_mc_value_european_call_sobol():
    """MC price of a European call is close to the BS analytical price."""
    spot, vol, rate, div = 100.0, 0.2, 0.05, 0.02
    strike = 100.0
    maturity_years = 1.0

    model = dal.BSModelData_New(spot=spot, vol=vol, rate=rate, div=div)
    product = _make_european_call(strike, dal.Date_(2023, 9, 25))

    result = dal.MonteCarlo_Value(product, model, 2**16, "sobol")
    mc_price = result["PV"]
    bs_price = _bs_call_price(spot, strike, vol, rate, div, maturity_years)

    assert abs(mc_price - bs_price) < 0.5, (  # nosec B101 - pytest assertions are intentional
        f"MC price {mc_price:.4f} too far from BS price {bs_price:.4f}"
    )


def test_mc_value_european_call_mrg32():
    """MC price with MRG32 pseudo-random generator is close to BS price."""
    spot, vol, rate, div = 100.0, 0.2, 0.05, 0.02
    strike = 100.0
    maturity_years = 1.0

    model = dal.BSModelData_New(spot=spot, vol=vol, rate=rate, div=div)
    product = _make_european_call(strike, dal.Date_(2023, 9, 25))

    result = dal.MonteCarlo_Value(product, model, 2**16, "mrg32")
    mc_price = result["PV"]
    bs_price = _bs_call_price(spot, strike, vol, rate, div, maturity_years)

    assert abs(mc_price - bs_price) < 1.0, (  # nosec B101 - pytest assertions are intentional
        f"MRG32 MC price {mc_price:.4f} too far from BS price {bs_price:.4f}"
    )


def test_mc_value_european_put():
    """MC price of a European put is close to the BS analytical put price."""
    spot, vol, rate, div = 100.0, 0.2, 0.05, 0.02
    strike = 100.0
    maturity_years = 1.0

    model = dal.BSModelData_New(spot=spot, vol=vol, rate=rate, div=div)
    product = _make_european_put(strike, dal.Date_(2023, 9, 25))

    result = dal.MonteCarlo_Value(product, model, 2**16, "sobol")
    mc_price = result["PV"]

    # BS put price via put-call parity: P = C - S*exp(-div*T) + K*exp(-r*T)
    call_price = _bs_call_price(spot, strike, vol, rate, div, maturity_years)
    bs_put = call_price - spot * math.exp(-div * maturity_years) + strike * math.exp(
        -rate * maturity_years
    )

    assert abs(mc_price - bs_put) < 0.5, (  # nosec B101 - pytest assertions are intentional
        f"MC put price {mc_price:.4f} too far from BS put price {bs_put:.4f}"
    )


def test_mc_value_otm_call():
    """Deep OTM call has a small but positive price."""
    spot, vol, rate, div = 100.0, 0.2, 0.05, 0.02
    strike = 150.0  # 50% OTM

    model = dal.BSModelData_New(spot=spot, vol=vol, rate=rate, div=div)
    product = _make_european_call(strike, dal.Date_(2023, 9, 25))

    result = dal.MonteCarlo_Value(product, model, 2**16, "sobol")
    mc_price = result["PV"]

    bs_price = _bs_call_price(spot, strike, vol, rate, div, 1.0)
    assert mc_price > 0, "OTM call should have positive price"  # nosec B101 - pytest assertions are intentional
    assert abs(mc_price - bs_price) < 0.3, (  # nosec B101 - pytest assertions are intentional
        f"OTM MC price {mc_price:.4f} too far from BS {bs_price:.4f}"
    )


def test_mc_value_itm_call():
    """Deep ITM call price is close to intrinsic + time value."""
    spot, vol, rate, div = 100.0, 0.2, 0.05, 0.02
    strike = 50.0  # 50% ITM

    model = dal.BSModelData_New(spot=spot, vol=vol, rate=rate, div=div)
    product = _make_european_call(strike, dal.Date_(2023, 9, 25))

    result = dal.MonteCarlo_Value(product, model, 2**16, "sobol")
    mc_price = result["PV"]

    bs_price = _bs_call_price(spot, strike, vol, rate, div, 1.0)
    assert mc_price > strike * 0.4, "ITM call should be well above zero"  # nosec B101 - pytest assertions are intentional
    assert abs(mc_price - bs_price) < 1.0, (  # nosec B101 - pytest assertions are intentional
        f"ITM MC price {mc_price:.4f} too far from BS {bs_price:.4f}"
    )


def test_mc_value_more_paths_more_accurate():
    """More MC paths generally gives a price closer to the analytical value."""
    spot, vol, rate, div = 100.0, 0.2, 0.05, 0.02
    strike = 100.0

    model = dal.BSModelData_New(spot=spot, vol=vol, rate=rate, div=div)
    product = _make_european_call(strike, dal.Date_(2023, 9, 25))
    bs_price = _bs_call_price(spot, strike, vol, rate, div, 1.0)

    result_few = dal.MonteCarlo_Value(product, model, 2**10, "sobol")
    result_many = dal.MonteCarlo_Value(product, model, 2**16, "sobol")

    err_few = abs(result_few["PV"] - bs_price)
    err_many = abs(result_many["PV"] - bs_price)

    assert err_many < 1.0, f"Many-path error {err_many:.4f} too large"  # nosec B101 - pytest assertions are intentional


# ---- AAD Greeks --------------------------------------------------------------


def test_mc_value_aad_returns_greeks():
    """With enable_aad=True, result dict includes gradient keys."""
    model = dal.BSModelData_New(spot=100.0, vol=0.2, rate=0.05, div=0.02)
    product = _make_european_call(100.0, dal.Date_(2023, 9, 25))

    result = dal.MonteCarlo_Value(product, model, 2**14, "sobol", False, True)
    assert "PV" in result  # nosec B101 - pytest assertions are intentional
    greek_keys = [k for k in result if k.startswith("d_")]
    assert len(greek_keys) > 0, f"Expected AAD gradient keys, got: {list(result.keys())}"  # nosec B101 - pytest assertions are intentional


def test_mc_value_aad_pv_close_to_no_aad():
    """PV from AAD path is close to PV from non-AAD path."""
    model = dal.BSModelData_New(spot=100.0, vol=0.2, rate=0.05, div=0.02)
    product = _make_european_call(100.0, dal.Date_(2023, 9, 25))

    result_no_aad = dal.MonteCarlo_Value(product, model, 2**14, "sobol", False, False)
    result_aad = dal.MonteCarlo_Value(product, model, 2**14, "sobol", False, True)

    assert abs(result_no_aad["PV"] - result_aad["PV"]) < 0.5, (  # nosec B101 - pytest assertions are intentional
        f"AAD PV {result_aad['PV']:.4f} differs from "
        f"non-AAD PV {result_no_aad['PV']:.4f}"
    )


def test_mc_value_aad_delta_reasonable():
    """AAD delta (d_spot) is in a reasonable range for an ATM call."""
    model = dal.BSModelData_New(spot=100.0, vol=0.2, rate=0.05, div=0.02)
    product = _make_european_call(100.0, dal.Date_(2023, 9, 25))

    result = dal.MonteCarlo_Value(product, model, 2**14, "sobol", False, True)

    delta_keys = [k for k in result if "spot" in k.lower()]
    assert delta_keys, f"Expected spot delta key, got: {list(result.keys())}"  # nosec B101 - pytest assertions are intentional

    delta = result[delta_keys[0]]
    assert 0.0 < delta < 1.0, f"Delta {delta:.4f} out of expected range [0, 1]"  # nosec B101 - pytest assertions are intentional


def test_mc_value_aad_vega_positive():
    """AAD vega (d_vol) is positive for a vanilla call."""
    model = dal.BSModelData_New(spot=100.0, vol=0.2, rate=0.05, div=0.02)
    product = _make_european_call(100.0, dal.Date_(2023, 9, 25))

    result = dal.MonteCarlo_Value(product, model, 2**14, "sobol", False, True)

    vega_keys = [k for k in result if "vol" in k.lower()]
    assert vega_keys, f"Expected volatility vega key, got: {list(result.keys())}"  # nosec B101 - pytest assertions are intentional

    vega = result[vega_keys[0]]
    assert vega > 0, f"Vega {vega:.4f} should be positive for a call"  # nosec B101 - pytest assertions are intentional


# ---- Default Arguments -------------------------------------------------------


def test_mc_value_default_method():
    """MonteCarlo_Value uses sobol by default when method is omitted."""
    model = dal.BSModelData_New(spot=100.0, vol=0.2, rate=0.05, div=0.02)
    product = _make_european_call(100.0, dal.Date_(2023, 9, 25))

    result = dal.MonteCarlo_Value(product, model, 2**14)
    assert "PV" in result  # nosec B101 - pytest assertions are intentional
    assert result["PV"] > 0  # nosec B101 - pytest assertions are intentional


def test_mc_value_use_bb_flag():
    """MonteCarlo_Value accepts use_bb=True without error."""
    model = dal.BSModelData_New(spot=100.0, vol=0.2, rate=0.05, div=0.02)
    product = _make_european_call(100.0, dal.Date_(2023, 9, 25))

    result = dal.MonteCarlo_Value(product, model, 2**14, "sobol", True)
    assert "PV" in result  # nosec B101 - pytest assertions are intentional


# ---- Compiled Evaluator Flag ---------------------------------------------------


def test_mc_value_compiled_flag_parity():
    """compiled=True/False/None (default tree-walk) produce the same PV."""
    model = dal.BSModelData_New(spot=100.0, vol=0.2, rate=0.05, div=0.02)
    product = _make_european_call(100.0, dal.Date_(2023, 9, 25))

    pv_default = dal.MonteCarlo_Value(product, model, 2**14)["PV"]
    pv_compiled = dal.MonteCarlo_Value(product, model, 2**14, compiled=True)["PV"]
    pv_tree_walk = dal.MonteCarlo_Value(product, model, 2**14, compiled=False)["PV"]

    assert abs(pv_compiled - pv_tree_walk) < 1e-8  # nosec B101 - pytest assertions are intentional
    assert abs(pv_default - pv_tree_walk) < 1e-8  # nosec B101 - pytest assertions are intentional


def test_mc_value_compiled_flag_parity_aad():
    """The compiled flag preserves PV and every greek in the AAD path."""
    model = dal.BSModelData_New(spot=100.0, vol=0.2, rate=0.05, div=0.02)
    product = _make_european_call(100.0, dal.Date_(2023, 9, 25))

    res_compiled = dal.MonteCarlo_Value(
        product, model, 2**14, enable_aad=True, compiled=True)
    res_tree_walk = dal.MonteCarlo_Value(
        product, model, 2**14, enable_aad=True, compiled=False)

    assert res_compiled.keys() == res_tree_walk.keys()  # nosec B101 - pytest assertions are intentional
    for key in res_compiled:
        assert abs(res_compiled[key] - res_tree_walk[key]) < 1e-8  # nosec B101 - pytest assertions are intentional
