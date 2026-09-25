"""Measure conditional RQMC error-bar coverage against independent references.

Run outside CI with a just-built DAL extension on PYTHONPATH. The interval is
mean +/- two replicate-mean standard errors: it is descriptive and conditional
on one frozen LSMC policy, not a guarantee for the optimal Bermudan value.
"""

import argparse
from datetime import date
import json
import math
import statistics

import dal
import numpy as np


TODAY = date(2026, 9, 20)
FIRST = date(2027, 9, 20)
LAST = date(2028, 3, 20)
SPOT = STRIKE = 100.0
VOL = 0.2
RATE = 0.05
DIVIDEND = 0.0


def normal_cdf(value):
    return 0.5 * math.erfc(-value / math.sqrt(2.0))


def european_put(maturity):
    scale = VOL * math.sqrt(maturity)
    d1 = (math.log(SPOT / STRIKE) + (RATE - DIVIDEND + 0.5 * VOL**2) * maturity) / scale
    d2 = d1 - scale
    return STRIKE * math.exp(-RATE * maturity) * normal_cdf(-d2) - SPOT * math.exp(-DIVIDEND * maturity) * normal_cdf(-d1)


def tridiagonal(lower, diagonal, upper, rhs):
    """Thomas solve, independent of the DAL PDE and LSMC implementations."""
    diag = diagonal.copy()
    values = rhs.copy()
    for i in range(1, len(diag)):
        factor = lower[i - 1] / diag[i - 1]
        diag[i] -= factor * upper[i - 1]
        values[i] -= factor * values[i - 1]
    values[-1] /= diag[-1]
    for i in range(len(diag) - 2, -1, -1):
        values[i] = (values[i] - upper[i] * values[i + 1]) / diag[i]
    return values


def bermudan_put_pde(exercise_dates, grid_points, steps_per_interval):
    """Crank-Nicolson Black-Scholes rollback with exercise-date projection."""
    grid = np.linspace(0.0, 400.0, grid_points)
    step = grid[1] - grid[0]
    inner = grid[1:-1]
    lower = 0.5 * VOL**2 * inner**2 / step**2 - (RATE - DIVIDEND) * inner / (2.0 * step)
    center = -VOL**2 * inner**2 / step**2 - RATE
    upper = 0.5 * VOL**2 * inner**2 / step**2 + (RATE - DIVIDEND) * inner / (2.0 * step)
    payoff = np.maximum(STRIKE - grid, 0.0)
    value = payoff.copy()
    exercise_times = [(day - TODAY).days / 365.0 for day in exercise_dates]
    boundaries = (0.0, *exercise_times)

    for index in range(len(boundaries) - 1, 0, -1):
        left, right = boundaries[index - 1], boundaries[index]
        dt = (right - left) / steps_per_interval
        lhs_lower = -0.5 * dt * lower[1:]
        lhs_center = 1.0 - 0.5 * dt * center
        lhs_upper = -0.5 * dt * upper[:-1]
        for step_index in range(steps_per_interval):
            time_new = right - step_index * dt
            time_old = time_new - dt
            boundary_new = STRIKE * math.exp(-RATE * (right - time_new))
            boundary_old = STRIKE * math.exp(-RATE * (right - time_old))
            rhs = (1.0 + 0.5 * dt * center) * value[1:-1]
            rhs[1:] += 0.5 * dt * lower[1:] * value[1:-2]
            rhs[:-1] += 0.5 * dt * upper[:-1] * value[2:-1]
            rhs[0] += 0.5 * dt * lower[0] * (boundary_new + boundary_old)
            value[1:-1] = tridiagonal(lhs_lower, lhs_center, lhs_upper, rhs)
            value[0] = boundary_old
            value[-1] = 0.0
        if left > 0.0:
            value = np.maximum(value, payoff)

    return float(np.interp(SPOT, grid, value))


def dal_date(value):
    return dal.Date_(value.year, value.month, value.day)


def coverage_case(name, dates, reference, args):
    product = dal.Product_New([dal_date(day) for day in dates], [f"EXERCISE MAX({STRIKE} - spot(), 0.0)"] * len(dates))
    model = dal.BSModelData_New(SPOT, VOL, RATE, DIVIDEND)
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal_date(TODAY))
    prices = []
    half_widths = []
    covered = 0
    for outer in range(args.outer_seeds):
        settings = dal.MonteCarloSettings_(compiled=True, lsmc_training_paths=args.training_paths,
                                           lsmc_rqmc_replicates=args.replicates, lsmc_training_seed=17,
                                           lsmc_pricing_seed=10000 + outer)
        uncertainty = dal.ScriptSimulation_Explain(product, model, args.pricing_paths, valuation=valuation, simulation=settings)["uncertainty"]
        mean = statistics.mean(uncertainty["replicate_means"])
        half_width = 2.0 * uncertainty["replicate_mean_se"]
        prices.append(mean)
        half_widths.append(half_width)
        covered += abs(mean - reference) <= half_width
    return {
        "name": name,
        "reference": reference,
        "mean_price": statistics.mean(prices),
        "mean_bias_to_reference": statistics.mean(prices) - reference,
        "mean_two_se_half_width": statistics.mean(half_widths),
        "covered": covered,
        "outer_seeds": args.outer_seeds,
        "coverage_fraction": covered / args.outer_seeds,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--outer-seeds", type=int, default=64)
    parser.add_argument("--replicates", type=int, default=8)
    parser.add_argument("--training-paths", type=int, default=8192)
    parser.add_argument("--pricing-paths", type=int, default=2048)
    parser.add_argument("--pde-grid", type=int, default=1201)
    parser.add_argument("--pde-steps", type=int, default=1200)
    args = parser.parse_args()
    if args.outer_seeds < 2 or args.replicates < 2 or args.training_paths < 1 or args.pricing_paths < 1:
        parser.error("positive budgets and at least two seeds and replicates are required")

    maturity = (LAST - TODAY).days / 365.0
    pde = bermudan_put_pde((FIRST, LAST), args.pde_grid, args.pde_steps)
    finer = bermudan_put_pde((FIRST, LAST), 2 * args.pde_grid - 1, 2 * args.pde_steps)
    analytic_european = european_put(maturity)
    pde_european = bermudan_put_pde((LAST,), 2 * args.pde_grid - 1, 2 * args.pde_steps)
    results = {
        "method": "independent 32-bit digital shifts; mean +/- 2 replicate-mean SE",
        "conditional_policy_training_seed": 17,
        "replicates": args.replicates,
        "training_paths": args.training_paths,
        "pricing_paths_per_replicate": args.pricing_paths,
        "pde_grid_gap": finer - pde,
        "european_pde_vs_analytic_gap": pde_european - analytic_european,
        "cases": [
            coverage_case("european_put", (LAST,), analytic_european, args),
            coverage_case("two_date_bermudan_put", (FIRST, LAST), finer, args),
        ],
    }
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
