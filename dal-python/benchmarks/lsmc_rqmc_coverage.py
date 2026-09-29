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

from dal_comparisons.exercise_pde import put_surface

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
    return STRIKE * math.exp(-RATE * maturity) * normal_cdf(-d2) - SPOT * math.exp(
        -DIVIDEND * maturity
    ) * normal_cdf(-d1)


def bermudan_put_pde(
    exercise_dates, grid_points, steps_per_interval, spot=SPOT, vol=VOL, rate=RATE
):
    """Crank-Nicolson Black-Scholes rollback with exercise-date projection."""
    exercise_times = tuple((day - TODAY).days / 365.0 for day in exercise_dates)
    grid, values = put_surface(
        exercise_times,
        grid_points,
        (steps_per_interval,) * len(exercise_times),
        vol,
        rate,
        STRIKE,
        DIVIDEND,
    )
    return float(np.interp(spot, grid, values))


def dal_date(value):
    return dal.Date_(value.year, value.month, value.day)


def coverage_case(name, dates, reference, args):
    product = dal.Product_New(
        [dal_date(day) for day in dates],
        [f"EXERCISE MAX({STRIKE} - spot(), 0.0)"] * len(dates),
    )
    model = dal.BSModelData_New(SPOT, VOL, RATE, DIVIDEND)
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal_date(TODAY))
    prices = []
    half_widths = []
    covered = 0
    for outer in range(args.outer_seeds):
        settings = dal.MonteCarloSettings_(
            compiled=True,
            lsmc_training_paths=args.training_paths,
            lsmc_rqmc_replicates=args.replicates,
            lsmc_training_seed=17,
            lsmc_pricing_seed=10000 + outer,
        )
        uncertainty = dal.ScriptSimulation_Explain(
            product, model, args.pricing_paths, valuation=valuation, simulation=settings
        )["uncertainty"]
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
    parser.add_argument("--outer-seeds", type=int, default=256)
    parser.add_argument("--replicates", type=int, default=8)
    parser.add_argument("--training-paths", type=int, default=8192)
    parser.add_argument("--pricing-paths", type=int, default=2048)
    parser.add_argument("--pde-grid", type=int, default=1201)
    parser.add_argument("--pde-steps", type=int, default=1200)
    args = parser.parse_args()
    if (
        args.outer_seeds < 2
        or args.replicates < 2
        or args.training_paths < 1
        or args.pricing_paths < 1
    ):
        parser.error(
            "positive budgets and at least two seeds and replicates are required"
        )

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
