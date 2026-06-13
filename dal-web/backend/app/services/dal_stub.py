"""In-process stub that mirrors the DAL Python *public* API.

This module is **only** a development / CI fallback used when the compiled
``dal._dal`` extension module is not importable (it requires a full C++ build).  It
deliberately re-implements the exact same public entry points that the real
``dal`` package exposes -- ``Date_``, ``Cell_``, ``EvaluationDate_Set`` /
``EvaluationDate_Get``, ``Product_New``, ``BSModelData_New``,
``DupireModelData_New`` and ``MonteCarlo_Value`` -- so that the rest of the
backend never has to know whether it is talking to the native library or to
this stub.

The numbers produced here are intentionally simple (a closed-form
Black-Scholes for European-style payoffs, finite-difference Greeks).  They are
good enough to exercise the UI end-to-end but are **not** a substitute for the
real DAL Monte Carlo engine.
"""

from __future__ import annotations

import datetime as _dt
import math
import re
from typing import Any

# ---------------------------------------------------------------------------
# Primitive value types (mirroring dal.Date_ / dal.Cell_)
# ---------------------------------------------------------------------------


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


# ---------------------------------------------------------------------------
# Product / model handles
# ---------------------------------------------------------------------------


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


# ---------------------------------------------------------------------------
# Valuation
# ---------------------------------------------------------------------------

_STRIKE_RE = re.compile(r"STRIKE", re.IGNORECASE)
_NUMBER_RE = re.compile(r"[-+]?\d*\.?\d+")


def _norm_cdf(x: float) -> float:
    return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))


def _infer_strike(product: _ProductHandle) -> float:
    """Best-effort extraction of a numeric strike from the event script."""
    for d, e in zip(product.dates, product.events):
        label = d.value if isinstance(d, Cell_) else d
        if isinstance(label, str) and _STRIKE_RE.fullmatch(label.strip()):
            nums = _NUMBER_RE.findall(e)
            if nums:
                return float(nums[0])
    # fall back to ATM-ish default
    return 100.0


def _infer_is_put(product: _ProductHandle) -> bool:
    text = " ".join(product.events).lower()
    return "put" in text and "call" not in text


def _years_to_maturity(product: _ProductHandle) -> float:
    last_date = None
    for d in product.dates:
        label = d.value if isinstance(d, Cell_) else d
        if isinstance(label, Date_):
            last_date = label
    if last_date is None:
        return 1.0
    days = last_date - EvaluationDate_Get()
    return max(days / 365.0, 1.0 / 365.0)


def _bs_price(
    spot: float, strike: float, vol: float, rate: float, div: float, t: float, is_put: bool
) -> float:
    if vol <= 0 or t <= 0:
        fwd_intrinsic = (strike - spot) if is_put else (spot - strike)
        return max(fwd_intrinsic, 0.0) * math.exp(-rate * t)
    d1 = (math.log(spot / strike) + (rate - div + 0.5 * vol * vol) * t) / (vol * math.sqrt(t))
    d2 = d1 - vol * math.sqrt(t)
    if is_put:
        return strike * math.exp(-rate * t) * _norm_cdf(-d2) - spot * math.exp(-div * t) * _norm_cdf(-d1)
    return spot * math.exp(-div * t) * _norm_cdf(d1) - strike * math.exp(-rate * t) * _norm_cdf(d2)


def MonteCarlo_Value(  # noqa: N802 - match DAL naming
    product: _ProductHandle,
    model: _ModelHandle,
    num_path: int,
    method: str = "sobol",
    use_bb: bool = False,
    enable_aad: bool = False,
    smooth: float = 0.01,
) -> dict[str, float]:
    p = model.params
    spot, vol, rate, div = p["spot"], p["vol"], p["rate"], p["div"]
    strike = _infer_strike(product)
    is_put = _infer_is_put(product)
    t = _years_to_maturity(product)

    pv = _bs_price(spot, strike, vol, rate, div, t, is_put)
    result: dict[str, float] = {"PV": pv}

    if enable_aad:
        eps = 1e-4
        result["d_spot"] = (
            _bs_price(spot * (1 + eps), strike, vol, rate, div, t, is_put)
            - _bs_price(spot * (1 - eps), strike, vol, rate, div, t, is_put)
        ) / (2 * spot * eps)
        result["d_vol"] = (
            _bs_price(spot, strike, vol + eps, rate, div, t, is_put)
            - _bs_price(spot, strike, vol - eps, rate, div, t, is_put)
        ) / (2 * eps)
        result["d_rate"] = (
            _bs_price(spot, strike, vol, rate + eps, div, t, is_put)
            - _bs_price(spot, strike, vol, rate - eps, div, t, is_put)
        ) / (2 * eps)
        result["d_div"] = (
            _bs_price(spot, strike, vol, rate, div + eps, t, is_put)
            - _bs_price(spot, strike, vol, rate, div - eps, t, is_put)
        ) / (2 * eps)
    return result
