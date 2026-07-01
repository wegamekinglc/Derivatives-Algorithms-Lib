"""Pytest fixtures: in-process DAL stub (default) and a fake native module.

The default ``client`` fixture binds the gateway to the pure-python stub so the
non-valuation workflow (product/model/trade CRUD, debugging) needs no C++ build.
The ``native_client`` fixture binds the gateway to a fake *native* module so the
async valuation flow can be exercised end-to-end without a C++ build and so the
suite proves valuation routes through ``MonteCarlo_Value``.
"""

from __future__ import annotations

import os
import sys
import types
from typing import Any

import pytest

# Default backend: the pure-python stub regardless of environment.
os.environ["DAL_MODULE"] = "app.services.dal_stub"
os.environ.pop("DAL_REQUIRE_NATIVE", None)
os.environ["WEBUI_SEED_DEMO"] = "0"

_FAKE_NATIVE_MODULE_NAME = "fake_dal_native"


def _build_fake_native() -> types.ModuleType:
    """Build a fake *native* dal module for tests.

    Reuses :mod:`app.services.dal_stub` for every non-valuation entry point and
    substitutes a canned, non-Monte-Carlo ``MonteCarlo_Value`` so the async
    valuation machinery can be exercised end-to-end without a C++ build.  The
    module name does not end in ``dal_stub``, so ``DalGateway.is_native`` is
    ``True`` -- the gateway routes valuation through ``MonteCarlo_Value`` exactly
    as it would against the real compiled bindings.
    """
    from app.services import dal_stub

    fake = types.ModuleType(_FAKE_NATIVE_MODULE_NAME)
    for attr in (
        "Date_",
        "Cell_",
        "EvaluationDate_Set",
        "EvaluationDate_Get",
        "Product_New",
        "Product_Debug",
        "BSModelData_New",
        "DupireModelData_New",
    ):
        setattr(fake, attr, getattr(dal_stub, attr))

    calls: list[dict[str, Any]] = []

    def monte_carlo_value(
        product: Any,
        model_data: Any,
        num_path: int,
        method: str = "sobol",
        use_bb: bool = False,
        enable_aad: bool = False,
        smooth: float = 0.01,
    ) -> dict[str, float]:
        # Canned test double -- deliberately not a pricer.
        calls.append(
            {
                "num_path": num_path,
                "method": method,
                "use_bb": use_bb,
                "enable_aad": enable_aad,
                "smooth": smooth,
            }
        )
        out: dict[str, float] = {"PV": 8.0}
        if enable_aad:
            out.update({"d_spot": 0.5, "d_vol": 0.2, "d_rate": 0.0, "d_div": 0.0})
        return out

    fake.MonteCarlo_Value = monte_carlo_value
    fake.monte_carlo_calls = calls
    return fake


def _install_fake_native(monkeypatch: pytest.MonkeyPatch) -> types.ModuleType:
    fake = _build_fake_native()
    monkeypatch.setitem(sys.modules, _FAKE_NATIVE_MODULE_NAME, fake)
    return fake


def _reset_singletons() -> None:
    # Singletons live in list wrappers so getters need no `global` statement.
    import app.services.dal_gateway as gw
    import app.services.store as st

    gw._gateway_box[0] = None
    st._store_box[0] = None


@pytest.fixture()
def fake_native_module(monkeypatch: pytest.MonkeyPatch) -> types.ModuleType:
    """Registered fake native module, for direct gateway unit tests."""
    return _install_fake_native(monkeypatch)


@pytest.fixture()
def client():
    from fastapi.testclient import TestClient

    _reset_singletons()
    from app.main import create_app

    with TestClient(create_app()) as c:
        yield c


@pytest.fixture()
def native_client(monkeypatch: pytest.MonkeyPatch):
    """TestClient whose gateway binds to the fake native module.

    Lets the async valuation flow run to completion (task scheduling, polling,
    Greek aggregation) without a C++ build, and proves pricing goes through
    ``MonteCarlo_Value`` rather than the stub.
    """
    from fastapi.testclient import TestClient

    _install_fake_native(monkeypatch)
    monkeypatch.setenv("DAL_MODULE", _FAKE_NATIVE_MODULE_NAME)
    _reset_singletons()
    from app.main import create_app

    with TestClient(create_app()) as c:
        yield c
