"""Unit tests for the DAL gateway."""

from __future__ import annotations

import os

os.environ.setdefault("DAL_MODULE", "app.services.dal_stub")

import pytest

from app.services import dal_stub
from app.services.dal_gateway import DalGateway, ValuationRequest


def make_gateway() -> DalGateway:
    return DalGateway(module_name="app.services.dal_stub")


def _european_request(**overrides) -> ValuationRequest:
    base: dict[str, object] = dict(
        event_dates=["STRIKE", {"date": "2023-09-15"}],
        events=["100.0", "call pays MAX(spot() - STRIKE, 0.0)"],
        model_kind="BSModelData_",
        model_params={"spot": 100.0, "vol": 0.2, "rate": 0.0, "div": 0.0},
        num_paths=1024,
        enable_aad=True,
        evaluation_date=(2022, 9, 15),
    )
    base.update(overrides)
    return ValuationRequest(**base)  # type: ignore[arg-type]


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


def test_value_requires_native_module():
    """The stub does not perform Monte Carlo; valuation must fail loudly."""
    gw = make_gateway()
    with pytest.raises(RuntimeError, match="native"):
        gw.value(_european_request())


def test_value_routes_through_native_monte_carlo(fake_native_module):
    """Against a native module the gateway must call MonteCarlo_Value.

    The fake native module records the call, so this asserts the gateway routes
    pricing through ``MonteCarlo_Value`` -- the symbol exported by
    ``dal-python/src/bindings/value.cpp`` -- rather than synthesizing numbers.
    """
    gw = DalGateway(module_name="fake_dal_native")
    assert gw.is_native is True
    res = gw.value(_european_request(num_paths=2048, method="sobol", enable_aad=True))
    assert res["PV"] == 8.0
    assert res["d_spot"] == 0.5
    calls = fake_native_module.monte_carlo_calls
    assert len(calls) == 1
    assert calls[0]["num_path"] == 2048
    assert calls[0]["method"] == "sobol"
    assert calls[0]["enable_aad"] is True


def test_dupire_stub_averages_vol_surface():
    """DupireModelData_New flattens a 2D surface to its average vol in the stub."""
    model = dal_stub.DupireModelData_New(
        spot=100.0,
        rate=0.0,
        repo=0.0,
        spots=[90.0, 100.0, 110.0],
        times=[0.5, 1.0],
        vols=[[0.30, 0.30], [0.30, 0.30], [0.30, 0.30]],
    )
    assert model.params["vol"] == pytest.approx(0.30)
