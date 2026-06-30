"""In-process stub that mirrors the DAL Python *public* API.

This module is **only** a development / CI fallback used when the compiled
``dal._dal`` extension module is not importable (it requires a full C++ build).  It
deliberately re-implements the exact same public entry points that the real
``dal`` package exposes -- ``Date_``, ``Cell_``, ``EvaluationDate_Set`` /
``EvaluationDate_Get``, ``Product_New``, ``BSModelData_New``,
``DupireModelData_New`` and ``MonteCarlo_Value`` -- so that the rest of the
backend never has to know whether it is talking to the native library or to
this stub.

Valuation is a genuine Monte Carlo: under the Black-Scholes GBM the terminal
spot on each path is ``S_T = S0 * exp((r - div - 0.5*vol^2)*t + vol*sqrt(t)*Z)``
with ``Z ~ N(0,1)``; the discounted payoff is averaged over ``num_path`` draws,
mirroring ``dal-cpp/examples/european_mc`` and the C++ ``BlackScholes_`` path
generation.  Greeks come from finite differences on the same MC pricer.  This is
not the production script engine, but it is a real stochastic sampling -- the
price genuinely converges to the closed-form value as ``num_path`` grows.
"""

from __future__ import annotations

import datetime as _dt
import math
import random
import re
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


_STRIKE_RE = re.compile(r"STRIKE", re.IGNORECASE)
_NUMBER_RE = re.compile(r"[-+]?\d*\.?\d+")


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


_MC_SEED_SALT = 0xA5C3  # fixed salt so the same trade reproduces the same draws across calls


def _mc_seed(
    spot: float, strike: float, vol: float, rate: float, div: float, t: float, is_put: bool,
) -> int:
    # Stable per-trade seed (independent of num_path) so that finite-difference bumps at the
    # same path count share the same Gaussian draws -- common random numbers slash Greek noise.
    key = (
        round(spot, 10),
        round(strike, 10),
        round(vol, 10),
        round(rate, 10),
        round(div, 10),
        round(t, 10),
        bool(is_put),
        _MC_SEED_SALT,
    )
    return hash(key) & 0x7FFFFFFF


def _mc_price(
    spot: float,
    strike: float,
    vol: float,
    rate: float,
    div: float,
    t: float,
    is_put: bool,
    num_path: int,
    seed: int,
) -> float:
    """Discounted Monte-Carlo price of a European option under Black-Scholes GBM.

    Each path's terminal spot is ``S0*exp((r-div-0.5*vol^2)*t + vol*sqrt(t)*Z)`` with
    ``Z ~ N(0,1)``; antithetic pairs ``(Z, -Z)`` halve the draw count. ``num_path`` drives
    the result through genuine sampling and convergence to the closed-form value.
    """
    n = max(int(num_path), 1)
    if vol <= 0.0 or t <= 0.0:
        return max((strike - spot) if is_put else (spot - strike), 0.0) * math.exp(-rate * t)
    drift = (rate - div - 0.5 * vol * vol) * t
    spread = vol * math.sqrt(t)
    growth = math.exp(drift)
    discount = math.exp(-rate * t)
    rng = random.Random(seed)  # nosec B311 - Monte-Carlo sampling, not cryptographic
    total = 0.0
    for _ in range((n + 1) // 2):  # antithetic: each Z feeds two paths
        z = rng.gauss(0.0, 1.0)
        for sign in (1.0, -1.0):
            s_t = spot * growth * math.exp(spread * sign * z)
            payoff = (strike - s_t) if is_put else (s_t - strike)
            if payoff > 0.0:
                total += payoff
    return discount * total / n


def MonteCarlo_Value(  # noqa: N802 - match DAL naming
    product: _ProductHandle,
    model: _ModelHandle,
    num_path: int,
    method: str = "sobol",
    use_bb: bool = False,
    enable_aad: bool = False,
    smooth: float = 0.01,
) -> dict[str, float]:
    n = max(int(num_path), 1)
    p = model.params
    spot, vol, rate, div = p["spot"], p["vol"], p["rate"], p["div"]
    strike = _infer_strike(product)
    is_put = _infer_is_put(product)
    t = _years_to_maturity(product)

    seed = _mc_seed(spot, strike, vol, rate, div, t, is_put)

    def price_with(spot_: float, vol_: float, rate_: float, div_: float) -> float:
        return _mc_price(spot_, strike, vol_, rate_, div_, t, is_put, n, seed)

    pv = price_with(spot, vol, rate, div)
    result: dict[str, float] = {"PV": pv}

    if enable_aad:
        eps = 1e-4
        result["d_spot"] = (
            price_with(spot * (1 + eps), vol, rate, div)
            - price_with(spot * (1 - eps), vol, rate, div)
        ) / (2 * spot * eps)
        result["d_vol"] = (
            price_with(spot, vol + eps, rate, div) - price_with(spot, vol - eps, rate, div)
        ) / (2 * eps)
        result["d_rate"] = (
            price_with(spot, vol, rate + eps, div) - price_with(spot, vol, rate - eps, div)
        ) / (2 * eps)
        result["d_div"] = (
            price_with(spot, vol, rate, div + eps) - price_with(spot, vol, rate, div - eps)
        ) / (2 * eps)
    return result
