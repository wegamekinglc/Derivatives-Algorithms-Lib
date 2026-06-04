"""Unit tests for the DAL gateway against the in-process stub."""

from __future__ import annotations

import os

os.environ.setdefault("DAL_MODULE", "app.services.dal_stub")

from app.services.dal_gateway import DalGateway, ValuationRequest


def make_gateway() -> DalGateway:
    return DalGateway(module_name="app.services.dal_stub")


def test_gateway_uses_stub_backend():
    gw = make_gateway()
    assert gw.is_native is False
    assert gw.backend_name == "dal_stub"


def test_evaluation_date_roundtrip():
    gw = make_gateway()
    gw.set_evaluation_date(2022, 9, 15)
    assert gw.get_evaluation_date() == "2022-09-15"


def test_debug_product_renders_rows():
    gw = make_gateway()
    debug = gw.debug_product(
        [{"date": "2025-09-15"}, "STRIKE"],
        ["call pays MAX(spot() - STRIKE, 0.0)", "120.0"],
    )
    assert "STRIKE" in debug
    assert "2025-09-15" in debug


def test_value_european_call_matches_black_scholes():
    gw = make_gateway()
    request = ValuationRequest(
        event_dates=["STRIKE", {"date": "2023-09-15"}],
        events=["100.0", "call pays MAX(spot() - STRIKE, 0.0)"],
        model_kind="BSModelData_",
        model_params={"spot": 100.0, "vol": 0.2, "rate": 0.0, "div": 0.0},
        num_paths=1024,
        enable_aad=True,
        evaluation_date=(2022, 9, 15),
    )
    res = gw.value(request)
    assert res["PV"] > 0.0
    # ATM 1Y call, 20% vol, zero rates -> ~7.97 by Black-Scholes
    assert 7.0 < res["PV"] < 9.0
    # delta of an ATM call is around 0.5
    assert 0.4 < res["d_spot"] < 0.6
    # vega is positive
    assert res["d_vol"] > 0.0
