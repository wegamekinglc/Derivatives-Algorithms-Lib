"""Early-exercise put workloads and an independent CRR stopping oracle."""

from datetime import timedelta
from functools import lru_cache
import math

from .constants import DAY_COUNT, TODAY

SPOT, VOL, RATE, DIVIDEND, STRIKE = 100.0, 0.2, 0.05, 0.0, 100.0
MATURITY_DAYS = 546
MATURITY = TODAY + timedelta(days=MATURITY_DAYS)
BUMPS = (1.0, 0.01, 0.01)
KINDS = ("bermudan", "american")
CONVENTIONS = {
    "model": "Black-Scholes put; S=K=100, vol=0.2, r=0.05, q=0",
    "dates": [TODAY.isoformat(), MATURITY.isoformat()],
    "day_count": DAY_COUNT,
    "exercise": "Bermudan: every 91 days (6 dates); American approximation: every 7 days (78 dates); both include maturity",
    "paths": "16384 training, 16384/65536 pricing; smoke: 2048 training, 4096 pricing",
    "dal_sampling": "Sobol, first M training and next N pricing; disjoint blocks, not independently randomized; no Brownian bridge",
    "quantlib_sampling": "pseudorandom, pricing seed 42, calibration seed 43; no antithetics or control variates",
    "basis": "degree 3 monomials; DAL normalized spot, QuantLib strike-scaled spot plus payoff column (linearly dependent for ITM puts); backend regression solvers differ",
    "greeks": "PV, Delta, Vega, Rho per unit spot/decimal vol/decimal rate; both use refitted-policy central differences with common random numbers; DAL frozen-policy AAD is a different estimator and is not used here",
    "bumps": list(BUMPS),
    "boundary": "fresh product/model/engine construction, preprocessing, training, pricing and output conversion; oracle excluded",
    "oracle": "independent CRR tree, 8 steps/day, exercise only on matching dates; Greeks use identical finite bumps; not a continuous-exercise American oracle",
    "interpretation": "equal path budgets, different sampling/solvers; timings do not imply equal precision; tolerances include fixed-training policy error",
}


def exercise_days(kind):
    spacing = {"bermudan": 91, "american": 7}[kind]
    return tuple(range(spacing, MATURITY_DAYS + 1, spacing))


def cases(smoke=False):
    return [
        {
            "name": f"mc_{kind}_{operation}_{paths}",
            "operation": f"mc_{operation}",
            "kind": kind,
            "size": 4096 if smoke else paths,
            "training_paths": 2048 if smoke else 16384,
            "steps": len(exercise_days(kind)),
        }
        for kind in KINDS
        for operation in ("price", "greeks")
        for paths in (16384, 65536)
    ]


@lru_cache(maxsize=128)
def tree_price(days, spot=SPOT, vol=VOL, rate=RATE, steps_per_day=8):
    import numpy as np

    steps = days[-1] * steps_per_day
    dt = 1 / (365 * steps_per_day)
    jump = vol * math.sqrt(dt)
    up, down = math.exp(jump), math.exp(-jump)
    probability = (math.exp((rate - DIVIDEND) * dt) - down) / (up - down)
    discount = math.exp(-rate * dt)
    exercise = {day * steps_per_day for day in days}
    indices = np.arange(steps + 1)
    values = np.maximum(STRIKE - spot * np.exp((2 * indices - steps) * jump), 0)
    for step in range(steps - 1, -1, -1):
        values = discount * ((1 - probability) * values[:-1] + probability * values[1:])
        if step in exercise:
            intrinsic = STRIKE - spot * np.exp((2 * indices[: step + 1] - step) * jump)
            values = np.maximum(values, intrinsic)
    return float(values[0])


def bumped_values(price):
    values = [price(SPOT, VOL, RATE)]
    for index, bump in enumerate(BUMPS):
        up, down = [SPOT, VOL, RATE], [SPOT, VOL, RATE]
        up[index] += bump
        down[index] -= bump
        values.append((price(*up) - price(*down)) / (2 * bump))
    return values


def expected(case):
    days = exercise_days(case["kind"])
    if case["operation"] == "mc_price":
        return [tree_price(days)]
    return bumped_values(lambda s, v, r: tree_price(days, s, v, r))


def tolerance(case):
    sampling = math.sqrt(16384 / case["size"])
    training = math.sqrt(16384 / case["training_paths"])
    # A larger pricing sample cannot eliminate a policy fitted on fixed M paths.
    values = [
        a * sampling + b * training
        for a, b in ((0.25, 0.05), (0.02, 0.01), (1.0, 0.5), (1.5, 1.0))
    ]
    return values if case["operation"] == "mc_greeks" else values[:1]


def method(backend, case):
    sampler = "Sobol LSM" if backend == "dal" else "pseudorandom LSM"
    if case["operation"] == "mc_price":
        return sampler
    return f"{sampler} + refitted-policy central differences (common random numbers)"
