"""Pytest fixtures: a fake ``dal`` module so tests need no C++ build.

The backend imports the compiled ``dal`` package (dal-python) directly. For
tests we register a minimal fake ``dal`` in :data:`sys.modules` before the app
imports the gateway, so the FastAPI wiring can be exercised without building the
C++ extension. In production the real ``dal`` is imported.
"""

from __future__ import annotations

import datetime as _dt
import os
import sys
import types
from typing import Any

import pytest

# Seed a demo portfolio only when explicitly requested.
os.environ.setdefault("WEBUI_SEED_DEMO", "0")


def _build_fake_dal() -> types.ModuleType:
    """Minimal stand-in for the compiled ``dal`` package.

    Implements just the public surface ``DalGateway`` touches. ``MonteCarlo_Value``
    is a canned test double (NOT a pricer) that records its call so tests can
    assert the gateway routes pricing through it.
    """
    fake = types.ModuleType("dal")

    class Date_:  # noqa: N801 - match DAL public naming
        def __init__(self, y: int, m: int, d: int) -> None:
            self._d = _dt.date(y, m, d)

        def __sub__(self, other: "Date_") -> int:
            return (self._d - other._d).days

        def __repr__(self) -> str:
            return self._d.strftime("%Y-%m-%d")

    class Cell_:  # noqa: N801 - match DAL public naming
        def __init__(self, value: Any) -> None:
            self.value = value

        def __repr__(self) -> str:
            return f"Cell_({self.value!r})"

    eval_box: list[Any] = [Date_(2022, 9, 15)]

    def EvaluationDate_Set(d: Date_) -> None:  # noqa: N802
        eval_box[0] = d

    def EvaluationDate_Get() -> Date_:  # noqa: N802
        return eval_box[0]

    def Product_New(dates: list[Any], events: list[str]) -> tuple[list[Any], list[str]]:  # noqa: N802
        return list(dates), list(events)

    def Product_Debug(product: tuple[list[Any], list[str]]) -> str:  # noqa: N802
        dates, events = product
        return "\n".join(f"{d!r}: {e}" for d, e in zip(dates, events))

    def BSModelData_New(spot: float, vol: float, rate: float, div: float) -> dict[str, float]:  # noqa: N802
        return {"spot": spot, "vol": vol, "rate": rate, "div": div}

    def DupireModelData_New(  # noqa: N802
        spot: float, rate: float, repo: float, spots: list[float], times: list[float], vols: Any
    ) -> dict[str, float]:
        return {"spot": spot, "rate": rate, "div": repo, "vol": 0.2}

    calls: list[dict[str, Any]] = []

    def MonteCarlo_Value(  # noqa: N802
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

    fake.Date_ = Date_
    fake.Cell_ = Cell_
    fake.EvaluationDate_Set = EvaluationDate_Set
    fake.EvaluationDate_Get = EvaluationDate_Get
    fake.Product_New = Product_New
    fake.Product_Debug = Product_Debug
    fake.BSModelData_New = BSModelData_New
    fake.DupireModelData_New = DupireModelData_New
    fake.MonteCarlo_Value = MonteCarlo_Value
    fake.monte_carlo_calls = calls
    return fake


# Install before any `import dal` inside the app under test.
sys.modules["dal"] = _build_fake_dal()


def _reset_singletons() -> None:
    import app.services.dal_gateway as gw
    import app.services.store as st

    gw._gateway_box[0] = None
    st._store_box[0] = None


@pytest.fixture()
def client():
    from fastapi.testclient import TestClient

    _reset_singletons()
    from app.main import create_app

    with TestClient(create_app()) as c:
        yield c
