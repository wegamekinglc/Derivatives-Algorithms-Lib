"""Shared conventions and independent scalar cashflow oracle (decimal rates)."""

from bisect import bisect_left
from datetime import datetime, timedelta
import math

TODAY = datetime(2025, 1, 15)
NODES = [datetime(2025 + year, 1, 15) for year in range(22)]
TIMES = [(date - TODAY).days / 365 for date in NODES]
LOG_DFS = [-(0.02 + 0.0005 * i) * t for i, t in enumerate(TIMES)]
BUMP = 1e-4
CONVENTIONS = {
    "valuation_date": TODAY.isoformat(),
    "currency": "USD",
    "day_count": "ACT/365F",
    "calendar": "none",
    "adjustment": "unadjusted",
    "fixed_and_float_frequency_months": 12,
    "payment_and_fixing_lag_days": 0,
    "interpolation": "log-linear discount factors",
    "discount_and_forecast": "same curve",
    "risk": "(PV(zero + 1bp) - PV(zero - 1bp)) / 2",
    "risk_curve_construction": "excluded; both shifted curves prepared before timing",
    "cache_policy": "QuantLib NPV forced recalculation; rateslib curve_caching=False",
    "output": "one float per query or trade, in input order",
    "node_dates": [date.isoformat() for date in NODES],
    "node_log_dfs": LOG_DFS,
}


def cases(smoke=False):
    result = [{"name": "discount_queries", "operation": "discount", "size": 4096}]
    for operation in ("pv", "dv01"):
        for size in (32, 256):
            result.append(
                {
                    "name": f"irs_{operation}_{size}",
                    "operation": operation,
                    "size": size,
                }
            )
    if smoke:
        return [dict(case, size=4) for case in result]
    return result


def queries(size):
    # Distinct dates, including off-node dates, in a deterministic permuted order.
    return [TODAY + timedelta(days=1 + (i * 1777) % 7665) for i in range(size)]


def trades(size):
    return [
        {
            "start": datetime(2026, 1, 15),
            "end": datetime(2028 + i % 18, 1, 15),
            "notional": 1_000_000 + 10_000 * i,
            "rate": 0.018 + 0.0001 * (i % 17),
            "sign": 1 if i % 2 == 0 else -1,
        }
        for i in range(size)
    ]


def node_dfs(shift=0.0):
    return [math.exp(log_df - shift * t) for log_df, t in zip(LOG_DFS, TIMES)]


def discount(date, shift=0.0):
    t = (date - TODAY).days / 365
    right = max(1, bisect_left(TIMES, t))
    left = right - 1
    weight = (t - TIMES[left]) / (TIMES[right] - TIMES[left])
    log_df = LOG_DFS[left] + weight * (LOG_DFS[right] - LOG_DFS[left])
    return math.exp(log_df - shift * t)


def pv(trade, shift=0.0):
    previous, annuity = trade["start"], 0.0
    for year in range(previous.year + 1, trade["end"].year + 1):
        date = datetime(year, 1, 15)
        annuity += (date - previous).days / 365 * discount(date, shift)
        previous = date
    floating = discount(trade["start"], shift) - discount(trade["end"], shift)
    return trade["sign"] * trade["notional"] * (floating - trade["rate"] * annuity)


def expected(case):
    if case["operation"] == "discount":
        return [discount(date) for date in queries(case["size"])]
    portfolio = trades(case["size"])
    if case["operation"] == "pv":
        return [pv(trade) for trade in portfolio]
    return [(pv(trade, BUMP) - pv(trade, -BUMP)) / 2 for trade in portfolio]


def validate(values, reference, *, abs_tol=2e-7):
    if len(values) != len(reference):
        raise ValueError("comparison output width mismatch")
    for i, (value, target) in enumerate(zip(values, reference)):
        if not math.isfinite(value) or not math.isclose(
            value, target, rel_tol=1e-10, abs_tol=abs_tol
        ):
            raise ValueError(
                f"comparison numerical mismatch at {i}: {value} != {target}"
            )
