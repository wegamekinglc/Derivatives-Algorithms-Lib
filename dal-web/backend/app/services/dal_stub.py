"""In-process stub that mirrors the non-valuation part of the DAL public API.

This module is **only** a development / CI fallback used when the compiled
``dal._dal`` extension module is not importable (it requires a full C++ build).  It
mirrors the construction and debugging entry points of the real ``dal`` package
-- ``Date_``, ``Cell_``, ``EvaluationDate_Set`` / ``EvaluationDate_Get``,
``Product_New``, ``Product_Debug``, ``BSModelData_New`` and
``DupireModelData_New`` -- so product and model building works without a native
build.

It deliberately does **not** expose ``MonteCarlo_Value``: pricing is the compiled
engine's job (see ``dal-python/src/bindings/value.cpp``).  Fabricating prices
here would only mislead, so :class:`~app.services.dal_gateway.DalGateway` raises
a loud error rather than call into this stub for valuation.
"""

from __future__ import annotations

import datetime as _dt
from typing import Any


class Date_:  # noqa: N801 - match DAL public naming
    """Minimal stand-in for ``dal.Date_``."""

    def __init__(self, yyyy: int, mm: int, dd: int) -> None:
        self._d = _dt.date(yyyy, mm, dd)

    def AddDays(self, days: int) -> "Date_":  # noqa: N802 - match DAL naming
        nd = self._d + _dt.timedelta(days=days)
        return Date_(nd.year, nd.month, nd.day)

    def __sub__(self, other: "Date_") -> int:
        return (self._d - other._d).days

    def __repr__(self) -> str:
        return self._d.strftime("%Y-%m-%d")


class Cell_:  # noqa: N801 - match DAL public naming
    """Minimal stand-in for ``dal.Cell_`` (a variant date/number/string)."""

    def __init__(self, value: Any) -> None:
        self.value = value

    def __repr__(self) -> str:
        return f"Cell_({self.value!r})"


# ---------------------------------------------------------------------------
# Global evaluation date (module-level mutable container, no `global` needed)
# ---------------------------------------------------------------------------

_EVALUATION_DATE_BOX: list[Date_] = [Date_(2022, 9, 15)]


def EvaluationDate_Set(d: Date_) -> None:  # noqa: N802 - match DAL naming
    _EVALUATION_DATE_BOX[0] = d


def EvaluationDate_Get() -> Date_:  # noqa: N802 - match DAL naming
    return _EVALUATION_DATE_BOX[0]


class _ProductHandle:
    def __init__(self, dates: list[Any], events: list[str]) -> None:
        self.dates = dates
        self.events = events


class _ModelHandle:
    def __init__(self, kind: str, params: dict[str, Any]) -> None:
        self.kind = kind
        self.params = params


def Product_New(dates: list[Any], events: list[str]) -> _ProductHandle:  # noqa: N802
    return _ProductHandle(list(dates), list(events))


def Product_Debug(product: _ProductHandle) -> str:  # noqa: N802
    lines = []
    for d, e in zip(product.dates, product.events):
        lines.append(f"{d!r}: {e}")
    return "\n".join(lines)


def BSModelData_New(spot: float, vol: float, rate: float, div: float) -> _ModelHandle:  # noqa: N802
    return _ModelHandle("BSModelData_", {"spot": spot, "vol": vol, "rate": rate, "div": div})


def DupireModelData_New(  # noqa: N802
    spot: float,
    rate: float,
    repo: float,
    spots: list[float],
    times: list[float],
    vols: Any,
) -> _ModelHandle:
    # Use the average vol of the surface as a flat approximation for the stub.
    # Handle both 2D list (normal case) and scalar (edge case) inputs.
    flat_vol = 0.2
    if isinstance(vols, list) and vols and isinstance(vols[0], list):
        total = sum(sum(row) for row in vols)
        count = sum(len(row) for row in vols)
        flat_vol = total / count if count > 0 else 0.2
    else:
        try:
            flat_vol = float(vols)
        except (TypeError, ValueError):
            pass
    return _ModelHandle(
        "DupireModelData_",
        {"spot": spot, "rate": rate, "div": repo, "vol": flat_vol},
    )
