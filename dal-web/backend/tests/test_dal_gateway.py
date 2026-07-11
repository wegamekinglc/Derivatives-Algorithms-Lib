"""Unit tests for the DAL gateway."""

from __future__ import annotations

import dal  # the fake installed by conftest
import pytest

from app.services.dal_gateway import DalGateway, ValuationRequest


def make_gateway() -> DalGateway:
    return DalGateway()


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


def test_gateway_backend():
    gw = make_gateway()
    assert gw.is_native is True
    assert gw.backend_name == "dal"


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


def test_value_routes_through_monte_carlo():
    """The gateway must price via MonteCarlo_Value.

    The fake ``dal`` records the call, so this asserts pricing routes through
    ``MonteCarlo_Value`` (the symbol exported by
    ``dal-python/src/bindings/value.cpp``) with the right arguments.
    """
    dal.monte_carlo_calls.clear()
    gw = make_gateway()
    res = gw.value(_european_request(num_paths=2048, method="sobol", enable_aad=True))
    assert res["PV"] == 8.0
    assert res["d_spot"] == 0.5
    calls = dal.monte_carlo_calls
    assert len(calls) == 1
    assert calls[0]["num_path"] == 2048
    assert calls[0]["method"] == "sobol"
    assert calls[0]["enable_aad"] is True


def test_value_maps_public_pseudo_method_to_native_mrg32():
    """The web RNG name must resolve to a method accepted by native DAL."""
    dal.monte_carlo_calls.clear()
    gw = make_gateway()

    gw.value(_european_request(method="pseudo"))

    assert len(dal.monte_carlo_calls) == 1
    assert dal.monte_carlo_calls[0]["method"] == "mrg32"


def test_value_rejects_unknown_monte_carlo_method():
    gw = make_gateway()

    with pytest.raises(ValueError, match="Unsupported Monte Carlo method"):
        gw.value(_european_request(method="unknown"))


def test_gateway_builds_non_flat_dupire_surface():
    """The web surface reaches DAL without being flattened or deferred to valuation."""
    gw = make_gateway()
    surface = [[0.24, 0.23], [0.21, 0.20], [0.19, 0.18]]

    model = gw.build_model(
        "DupireModelData_",
        {
            "spot": 100.0,
            "rate": 0.03,
            "repo": 0.01,
            "spots": [90.0, 100.0, 110.0],
            "times": [0.5, 1.0],
            "vols": surface,
        },
    )

    assert model["vols"] == surface  # nosec B101 - pytest assertions are intentional
