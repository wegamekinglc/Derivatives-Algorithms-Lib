"""Reproducible frozen-vs-retrained LSMC Greek study (run outside CI).

Use a just-built DAL package and numpy on PYTHONPATH. Independent outer seeds
rescramble both training and pricing; bumps within a seed reuse both streams.
The near-zero-volatility case uses the CRR reference because the shared PDE's
central spatial stencil is convection dominated at that parameter boundary.
"""

import argparse
from datetime import date
import json
import math
import statistics
import time

import dal

from dal_comparisons import exercise_scenarios as oracle
from lsmc_rqmc_coverage import bermudan_put_pde

EVALUATION = dal.Date_(2026, 9, 20)
MID = dal.Date_(2027, 9, 20)
MATURITY = dal.Date_(2028, 3, 20)
PDE_MID = date(2027, 9, 20)
PDE_MATURITY = date(2028, 3, 20)
MID_DAYS = MID - EVALUATION
MATURITY_DAYS = MATURITY - EVALUATION
STRIKE = 100.0
RATE = 0.05
CASES = {
    "european": (100.0, 0.20, (MATURITY,), (MATURITY_DAYS,), 4096),
    "bermudan_atm": (100.0, 0.20, (MID, MATURITY), (MID_DAYS, MATURITY_DAYS), 4096),
    "bermudan_itm": (70.0, 0.20, (MID, MATURITY), (MID_DAYS, MATURITY_DAYS), 4096),
    "bermudan_otm": (130.0, 0.20, (MID, MATURITY), (MID_DAYS, MATURITY_DAYS), 4096),
    "bermudan_sparse": (100.0, 0.20, (MID, MATURITY), (MID_DAYS, MATURITY_DAYS), 256),
    "bermudan_low_vol": (95.0, 0.002, (MID, MATURITY), (MID_DAYS, MATURITY_DAYS), 4096),
}


def mean_se(values):
    """Empirical uncertainty across independent training/pricing seed pairs."""
    return {
        "mean": statistics.mean(values),
        "se": statistics.stdev(values) / math.sqrt(len(values)),
    }


def normal_cdf(value):
    return 0.5 * math.erfc(-value / math.sqrt(2.0))


def european_reference(spot, vol, rate, maturity_years):
    sigma_t = vol * math.sqrt(maturity_years)
    d1 = (math.log(spot / STRIKE) + (rate + 0.5 * vol * vol) * maturity_years) / sigma_t
    d2 = d1 - sigma_t
    discount = math.exp(-rate * maturity_years)
    density = math.exp(-0.5 * d1 * d1) / math.sqrt(2.0 * math.pi)
    return {
        "PV": STRIKE * discount * normal_cdf(-d2) - spot * normal_cdf(-d1),
        "d_spot": -normal_cdf(-d1),
        "d_vol": spot * density * math.sqrt(maturity_years),
        "d_rate": -STRIKE * maturity_years * discount * normal_cdf(-d2),
    }


def run_case(
    name,
    spec,
    pricing_paths,
    replicates,
    seeds,
    smooth,
    relative_bump,
    compiled,
    pde_grid,
    pde_steps,
):
    spot, vol, dates, days, training_paths = spec
    product = dal.Product_New(
        list(dates), [f"EXERCISE MAX({STRIKE} - spot(), 0.0)"] * len(dates)
    )
    valuation = dal.ScriptValuationSettings_(evaluation_date=EVALUATION)
    reference = (
        european_reference(spot, vol, RATE, MATURITY_DAYS / 365.0)
        if name == "european"
        else {}
    )
    pde_dates = (PDE_MATURITY,) if name == "european" else (PDE_MID, PDE_MATURITY)
    use_pde_reference = name not in ("european", "bermudan_low_vol")
    pde_pv_diagnostic = None
    if name != "european":
        pde_pv_diagnostic = bermudan_put_pde(
            pde_dates, pde_grid, pde_steps, spot, vol, RATE
        )
        reference["PV"] = (
            pde_pv_diagnostic
            if use_pde_reference
            else oracle.tree_price(days, spot, vol, RATE)
        )
    rows = []
    for seed in seeds:
        common = {
            "compiled": compiled,
            "smooth": smooth,
            "lsmc_training_paths": training_paths,
            "lsmc_rqmc_replicates": replicates,
            "lsmc_training_seed": seed,
            "lsmc_pricing_seed": seed + 100_000,
        }

        def price(
            s, v, r, aad=True, mode="Frozen", training_seed=None, pricing_seed=None
        ):
            streams = common.copy()
            if training_seed is not None:
                streams["lsmc_training_seed"] = training_seed
            if pricing_seed is not None:
                streams["lsmc_pricing_seed"] = pricing_seed
            settings = dal.MonteCarloSettings_(
                **streams,
                enable_aad=aad,
                lsmc_policy_risk_mode=mode,
                lsmc_policy_bump_relative=relative_bump,
            )
            model = dal.BSModelData_New(s, v, r, 0.0)
            return dal.MonteCarlo_ValueWithSettings(
                product, model, pricing_paths, valuation=valuation, simulation=settings
            )

        frozen_start = time.perf_counter()
        frozen = price(spot, vol, RATE)
        frozen_ms = 1000 * (time.perf_counter() - frozen_start)
        retrained_start = time.perf_counter()
        retrained = price(spot, vol, RATE, mode="RetrainedBump")
        retrained_ms = 1000 * (time.perf_counter() - retrained_start)
        hard = price(spot, vol, RATE, aad=False)
        training_only_hard = (
            hard
            if seed == seeds[0]
            else price(spot, vol, RATE, aad=False, pricing_seed=seeds[0] + 100_000)
        )
        pricing_only_hard = (
            hard
            if seed == seeds[0]
            else price(spot, vol, RATE, aad=False, training_seed=seeds[0])
        )
        values = [spot, vol, RATE]
        bumped = {}
        for index, label in enumerate(("spot", "vol", "rate")):
            step = relative_bump * max(1.0, abs(values[index]))
            up, down = values.copy(), values.copy()
            up[index] += step
            down[index] -= step
            if down[index] < 0.0 and label == "vol":
                down[index] = values[index]
            denominator = up[index] - down[index]
            bumped[label] = (price(*up)["PV"] - price(*down)["PV"]) / denominator
        half_step = 0.5 * relative_bump * max(1.0, abs(spot))
        half_spot_bump = (
            price(spot + half_step, vol, RATE)["PV"]
            - price(spot - half_step, vol, RATE)["PV"]
        ) / (2.0 * half_step)
        rows.append(
            {
                "seed": seed,
                "frozen": frozen,
                "retrained": retrained,
                "hard_pv": hard["PV"],
                "training_only_hard_pv": training_only_hard["PV"],
                "pricing_only_hard_pv": pricing_only_hard["PV"],
                "full_retrain_bump": bumped,
                "half_step_spot_bump": half_spot_bump,
                "frozen_ms": frozen_ms,
                "retrained_ms": retrained_ms,
            }
        )

    comparison = {}
    for label in ("spot", "vol", "rate"):
        key = f"d_{label}"
        comparison[label] = {
            "frozen": mean_se([row["frozen"][key] for row in rows]),
            "retrained": mean_se([row["retrained"][key] for row in rows]),
            "full_retrain_bump": mean_se(
                [row["full_retrain_bump"][label] for row in rows]
            ),
            "policy_contribution": mean_se(
                [row["retrained"][key] - row["frozen"][key] for row in rows]
            ),
        }
        if name != "european":
            values = [spot, vol, RATE]
            index = ("spot", "vol", "rate").index(label)
            step = relative_bump * max(1.0, abs(values[index]))
            up, down = values.copy(), values.copy()
            up[index] += step
            down[index] -= step
            if use_pde_reference:
                reference_up = bermudan_put_pde(pde_dates, pde_grid, pde_steps, *up)
                reference_down = bermudan_put_pde(pde_dates, pde_grid, pde_steps, *down)
            else:
                reference_up = oracle.tree_price(days, *up)
                reference_down = oracle.tree_price(days, *down)
            reference[key] = (reference_up - reference_down) / (2 * step)
    return {
        "case": name,
        "training_paths": training_paths,
        "pricing_paths_per_replicate": pricing_paths,
        "pricing_replicates": replicates,
        "independent_seed_pairs": list(seeds),
        "reference": reference,
        "reference_method": (
            "analytic"
            if name == "european"
            else ("Crank-Nicolson PDE" if use_pde_reference else "CRR tree")
        ),
        "pde_pv_diagnostic": pde_pv_diagnostic,
        "crr_pv_crosscheck": oracle.tree_price(days, spot, vol, RATE),
        "smoothing_bias_pv": mean_se(
            [row["frozen"]["PV"] - row["hard_pv"] for row in rows]
        ),
        "hard_vs_reference_pv": mean_se(
            [row["hard_pv"] - reference["PV"] for row in rows]
        ),
        "training_seed_hard_pv": mean_se(
            [row["training_only_hard_pv"] for row in rows]
        ),
        "pricing_seed_hard_pv": mean_se([row["pricing_only_hard_pv"] for row in rows]),
        "spot_bump_step_shift": mean_se(
            [
                row["half_step_spot_bump"] - row["full_retrain_bump"]["spot"]
                for row in rows
            ]
        ),
        "frozen_ms": [row["frozen_ms"] for row in rows],
        "retrained_ms": [row["retrained_ms"] for row in rows],
        "greeks": comparison,
        "rows": rows,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pricing-paths", type=int, default=4096)
    parser.add_argument("--replicates", type=int, default=2)
    parser.add_argument(
        "--seeds", type=int, nargs="+", default=[17, 41, 73, 101, 149, 211, 307, 401]
    )
    parser.add_argument("--smooth", type=float, default=0.1)
    parser.add_argument("--bump-relative", type=float, default=1e-3)
    parser.add_argument("--compiled", action="store_true")
    parser.add_argument("--pde-grid", type=int, default=401)
    parser.add_argument("--pde-steps", type=int, default=200)
    parser.add_argument("--cases", nargs="+", choices=CASES, default=list(CASES))
    args = parser.parse_args()
    if (
        args.pricing_paths <= 0
        or args.replicates < 2
        or len(args.seeds) < 2
        or len(set(args.seeds)) != len(args.seeds)
        or min(args.seeds) < 0
        or max(args.seeds) > 2**31 - 1 - 100_000
    ):
        parser.error(
            "positive pricing paths, at least two replicates, and distinct valid nonnegative seeds are required"
        )
    if not 0 < args.smooth or not 0 < args.bump_relative <= 0.1:
        parser.error("smooth must be positive and bump-relative must be in (0, 0.1]")
    if args.pde_grid < 101 or args.pde_steps < 50:
        parser.error("PDE grid must have at least 101 nodes and 50 steps per interval")
    report = {
        "method": "independent outer seed pairs; common Sobol shifts and disjoint blocks within each finite difference",
        "smooth": args.smooth,
        "bump_relative": args.bump_relative,
        "compiled": args.compiled,
        "pde_grid": args.pde_grid,
        "pde_steps_per_interval": args.pde_steps,
        "cases": [
            run_case(
                name,
                CASES[name],
                args.pricing_paths,
                args.replicates,
                args.seeds,
                args.smooth,
                args.bump_relative,
                args.compiled,
                args.pde_grid,
                args.pde_steps,
            )
            for name in args.cases
        ],
    }
    print("LSMC_POLICY_SENSITIVITY_JSON=" + json.dumps(report, separators=(",", ":")))


if __name__ == "__main__":
    main()
