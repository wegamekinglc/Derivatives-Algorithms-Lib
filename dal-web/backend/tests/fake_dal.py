"""Canned DAL module shared by backend and browser tests.

This module is a test double, not a numerical implementation.  Callers must
install the result of :func:`build_fake_dal` in :data:`sys.modules` before
importing the web application's DAL gateway.
"""

from __future__ import annotations

import datetime as _dt
import types
from typing import Any


def build_fake_dal() -> types.ModuleType:
    """Return the minimal compiled-``dal`` stand-in used by web tests."""
    fake = types.ModuleType("dal")

    class Date_:  # noqa: N801 - match DAL public naming
        def __init__(self, y: int, m: int, d: int) -> None:
            self._d = _dt.date(y, m, d)

        def __sub__(self, other: Date_) -> int:
            return (self._d - other._d).days

        def __repr__(self) -> str:
            return self._d.strftime("%Y-%m-%d")

    class Cell_:  # noqa: N801 - match DAL public naming
        def __init__(self, value: Any) -> None:
            self.value = value

        def __repr__(self) -> str:
            return f"Cell_({self.value!r})"

    class DoubleMatrix_:  # noqa: N801 - match DAL public naming
        def __init__(self, rows: list[list[float]]) -> None:
            self.values = [[float(value) for value in row] for row in rows]

    eval_box: list[Any] = [Date_(2022, 9, 15)]

    def EvaluationDate_Set(d: Date_) -> None:  # noqa: N802
        eval_box[0] = d

    def EvaluationDate_Get() -> Date_:  # noqa: N802
        return eval_box[0]

    def Product_New(dates: list[Any], events: list[str]) -> tuple[list[Any], list[str]]:  # noqa: N802
        return list(dates), list(events)

    def Product_Debug(product: tuple[list[Any], list[str]]) -> str:  # noqa: N802
        dates, events = product
        return "\n".join(f"{d!r}: {e}" for d, e in zip(dates, events, strict=False))

    def BSModelData_New(spot: float, vol: float, rate: float, div: float) -> dict[str, float]:  # noqa: N802
        return {"spot": spot, "vol": vol, "rate": rate, "div": div}

    def DupireModelData_New(  # noqa: N802
        spot: float, rate: float, repo: float, spots: list[float], times: list[float], vols: Any
    ) -> dict[str, Any]:
        return {
            "spot": spot,
            "rate": rate,
            "repo": repo,
            "spots": spots,
            "times": times,
            "vols": vols.values,
        }

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
        # Deliberately canned: browser smoke tests verify integration, not pricing.
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
    fake.DoubleMatrix_ = DoubleMatrix_
    fake.EvaluationDate_Set = EvaluationDate_Set
    fake.EvaluationDate_Get = EvaluationDate_Get
    fake.Product_New = Product_New
    fake.Product_Debug = Product_Debug
    fake.BSModelData_New = BSModelData_New
    fake.DupireModelData_New = DupireModelData_New
    fake.MonteCarlo_Value = MonteCarlo_Value
    fake.monte_carlo_calls = calls
    return fake
