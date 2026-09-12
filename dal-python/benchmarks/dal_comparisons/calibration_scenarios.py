"""Non-flat curve markets with independent cashflow-derived calibration quotes."""

from bisect import bisect_left
from datetime import datetime, timedelta
import math

TODAY = datetime(2025, 1, 15)
KEYS = (
    "usd_discount",
    "usd_forward",
    "eur_discount",
    "eur_forward",
    "eur_usd_discount",
)
PROFILES = (
    (0.025, 0.0003),
    (0.032, 0.0004),
    (0.018, 0.0002),
    (0.024, 0.00035),
    (0.021, 0.00025),
)
KINDS = ("single", "multi_staged", "multi_joint", "xccy_staged", "xccy_joint")
CONVENTIONS = {
    "valuation_date": TODAY.isoformat(),
    "instruments": "annual fixed/float IRS; annual fixed-notional USD/EUR XCCY basis swaps with both principal exchanges, spread on EUR leg",
    "calendar": "unadjusted, no holidays, zero settlement/fixing/payment lags; ACT/365F",
    "interpolation": "log-linear discount factors, annual pillars, anchor DF=1",
    "market": "five distinct non-flat curves; quotes derived from independent scalar cashflows",
    "profiles": {key: list(profile) for key, profile in zip(KEYS, PROFILES)},
    "multi_staged": "discount calibration followed by forward calibration using the solved discount curve",
    "multi_joint": "simultaneous discount and forward calibration",
    "xccy_staged": "calibrate EUR discounting under USD collateral with four fixed domestic curves",
    "xccy_joint": "simultaneously solve USD/EUR discount+forward and cross-currency discounting: five blocks",
    "fx": "USD per EUR = 1.10; USD collateral; DAL basis DF = EUR domestic DF / EUR-under-USD DF",
    "boundary": "fresh instruments, initial curves, solve and full DF conversion; raw quotes and independent oracle excluded; no reuse of calibrated curves",
    "output": "solved curves in declared order; every annual node then every mid-year DF; fixed input curves excluded",
    "diagnostics": "DAL optional Jacobian/inverse retention disabled; rateslib solver AD state and QuantLib bootstrap state retained by their public APIs",
}


def cases(smoke=False):
    return [
        {
            "name": f"calibration_{kind}_{size}",
            "operation": "calibration",
            "kind": kind,
            "size": 2 if smoke else size,
        }
        for kind in KINDS
        for size in (5, 15)
    ]


def dates(size):
    return [datetime(TODAY.year + i, TODAY.month, TODAY.day) for i in range(size + 1)]


def queries(size):
    nodes = dates(size)
    return nodes[1:] + [
        left + timedelta(days=(right - left).days // 2)
        for left, right in zip(nodes, nodes[1:])
    ]


def curve_keys(kind):
    if kind == "single":
        return KEYS[:1]
    if kind.startswith("multi_"):
        return KEYS[:2]
    return KEYS[-1:] if kind == "xccy_staged" else KEYS


def discount(key, date, size):
    nodes = dates(size)
    i = max(1, bisect_left(nodes, date))
    left, right = nodes[i - 1], nodes[i]
    fraction = (date - left).days / (right - left).days
    level, slope = PROFILES[KEYS.index(key)]

    def log_df(ordinal, value):
        return -(level + slope * ordinal) * (value - TODAY).days / 365

    return math.exp((1 - fraction) * log_df(i - 1, left) + fraction * log_df(i, right))


def swap_rate(forecast, discounting, maturity, size):
    schedule = dates(maturity)
    annuity, floating = 0.0, 0.0
    for start, end in zip(schedule, schedule[1:]):
        df = discount(discounting, end, size)
        annuity += (end - start).days / 365 * df
        floating += (
            discount(forecast, start, size) / discount(forecast, end, size) - 1
        ) * df
    return floating / annuity


def xccy_rate(maturity, size):
    schedule = dates(maturity)
    totals, annuity = [], 0.0
    for forecast, discounting in ((KEYS[1], KEYS[0]), (KEYS[3], KEYS[4])):
        value = discount(discounting, schedule[-1], size)
        for start, end in zip(schedule, schedule[1:]):
            df = discount(discounting, end, size)
            value += (
                discount(forecast, start, size) / discount(forecast, end, size) - 1
            ) * df
            if discounting == KEYS[4]:
                annuity += (end - start).days / 365 * df
        totals.append(value)
    return (totals[0] - totals[1]) / annuity


def quotes(size):
    pairs = (
        (KEYS[0], KEYS[0]),
        (KEYS[1], KEYS[0]),
        (KEYS[2], KEYS[2]),
        (KEYS[3], KEYS[2]),
    )
    result = {
        key: [swap_rate(forecast, disc, y, size) for y in range(1, size + 1)]
        for key, (forecast, disc) in zip(KEYS, pairs)
    }
    result[KEYS[4]] = [xccy_rate(y, size) for y in range(1, size + 1)]
    return result


def expected(case):
    return [
        discount(key, date, case["size"])
        for key in curve_keys(case["kind"])
        for date in queries(case["size"])
    ]


def method(backend, case):
    if backend == "quantlib":
        return "piecewise log-linear discount bootstrap"
    if backend == "rateslib":
        return "forward AD solver, " + (
            "staged" if case["kind"].endswith("staged") else "simultaneous"
        )
    return "analytic Jacobian solve, " + (
        "staged" if case["kind"].endswith("staged") else "simultaneous"
    )
