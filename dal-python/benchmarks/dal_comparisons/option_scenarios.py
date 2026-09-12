"""Black-Scholes and deterministic integration oracles for Monte Carlo cases."""

from datetime import datetime, timedelta
from functools import lru_cache
import math

TODAY = datetime(2025, 1, 15)
MATURITY = TODAY + timedelta(days=364)
SPOT, VOL, RATE, DIVIDEND = 100.0, 0.2, 0.03, 0.01
STRIKE, BARRIER = 100.0, 130.0
YEARS = 364 / 365
BUMPS = (1.0, 0.01, 0.01)
CONVENTIONS = {
    "model": "Black-Scholes; S=K=100, vol=0.2, r=0.03, q=0.01",
    "dates": [TODAY.isoformat(), MATURITY.isoformat()],
    "day_count": "ACT/365F",
    "barrier": "up-and-out call, H=130, zero rebate; 52 weekly observations including maturity",
    "sampling": "Sobol: DAL initial point 0, QuantLib seed 42; no Brownian bridge or antithetics; generators restart per valuation",
    "greeks": "PV, Delta, Vega, Rho; derivatives per unit spot/decimal vol/decimal rate",
    "bumps": list(BUMPS),
    "methods": "DAL vanilla: reverse AAD; other Greeks: central differences with common random numbers; barrier reference uses identical finite bumps",
    "boundary": "fresh product/model/engine construction, preprocessing, simulation and output conversion; oracle excluded",
    "oracle": "vanilla analytic Black-Scholes; discrete barrier Gaussian transition quadrature; field-specific MC tolerances",
}


def cases(smoke=False):
    return [
        {
            "name": f"mc_{kind}_{operation}_{paths}",
            "operation": f"mc_{operation}",
            "kind": kind,
            "size": 4096 if smoke else paths,
            "steps": 1 if kind == "vanilla" else 52,
        }
        for kind in ("vanilla", "barrier")
        for operation in ("price", "greeks")
        for paths in (16384, 65536)
    ]


def cdf(x):
    return 0.5 * math.erfc(-x / math.sqrt(2))


def vanilla(spot=SPOT, vol=VOL, rate=RATE):
    root = math.sqrt(YEARS)
    d1 = (math.log(spot / STRIKE) + (rate - DIVIDEND + vol * vol / 2) * YEARS) / (
        vol * root
    )
    d2 = d1 - vol * root
    df, dividend_df = math.exp(-rate * YEARS), math.exp(-DIVIDEND * YEARS)
    return [
        spot * dividend_df * cdf(d1) - STRIKE * df * cdf(d2),
        dividend_df * cdf(d1),
        spot * dividend_df * math.exp(-d1 * d1 / 2) * root / math.sqrt(2 * math.pi),
        STRIKE * YEARS * df * cdf(d2),
    ]


@lru_cache(maxsize=64)
def barrier_price(spot=SPOT, vol=VOL, rate=RATE, steps=52, nodes=256):
    import numpy as np

    low, high = math.log(spot) - 10 * vol * math.sqrt(YEARS), math.log(BARRIER)
    points, weights = np.polynomial.legendre.leggauss(nodes)
    points = low + (points + 1) * (high - low) / 2
    weights = weights * (high - low) / 2
    dt = YEARS / steps
    drift, deviation = (rate - DIVIDEND - vol * vol / 2) * dt, vol * math.sqrt(dt)

    def transition(starts):
        z = (points - starts - drift) / deviation
        return np.exp(-z * z / 2) * weights / (deviation * math.sqrt(2 * math.pi))

    def terminal_value(start):
        def bounds(strike):
            d2 = (start - math.log(strike) + drift) / deviation
            return cdf(d2 + deviation), cdf(d2)

        first, last = bounds(STRIKE), bounds(BARRIER)
        return math.exp(start + (rate - DIVIDEND) * dt) * (
            first[0] - last[0]
        ) - STRIKE * (first[1] - last[1])

    if steps == 1:
        return terminal_value(math.log(spot)) * math.exp(-rate * YEARS)
    # Integrate the final payoff analytically so the strike kink cannot pollute quadrature.
    values = np.array([terminal_value(point) for point in points])
    matrix = transition(points[:, None])
    for _ in range(steps - 2):
        values = matrix @ values
    return float(transition(math.log(spot)) @ values) * math.exp(-rate * YEARS)


def bumped_values(price):
    values = [price(SPOT, VOL, RATE)]
    for i, bump in enumerate(BUMPS):
        up, down = [SPOT, VOL, RATE], [SPOT, VOL, RATE]
        up[i] += bump
        down[i] -= bump
        values.append((price(*up) - price(*down)) / (2 * bump))
    return values


def expected(case):
    values = vanilla() if case["kind"] == "vanilla" else bumped_values(barrier_price)
    return values if case["operation"] == "mc_greeks" else values[:1]


def tolerance(case):
    scale = math.sqrt(16384 / case["size"])
    bounds = (
        (0.08, 0.01, 0.5, 0.5) if case["kind"] == "vanilla" else (0.25, 0.05, 3.0, 4.0)
    )
    values = [value * scale for value in bounds]
    return values if case["operation"] == "mc_greeks" else values[:1]


def method(backend, case):
    if case["operation"] == "mc_price":
        return "Sobol Monte Carlo"
    if backend == "dal" and case["kind"] == "vanilla":
        return "Sobol Monte Carlo + reverse AAD"
    return "Sobol Monte Carlo + central differences (common random numbers)"
