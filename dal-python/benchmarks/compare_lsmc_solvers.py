"""Compare fixed and held-out-selected LSMC policies against a CRR oracle.

Run outside CI from an environment containing the just-built DAL wheel and numpy:
python dal-python/benchmarks/compare_lsmc_solvers.py --repeats 3
"""

import argparse
from datetime import timedelta
import json
import statistics
import time

import dal
import numpy as np

from dal_comparisons import exercise_scenarios as scenarios


def date(value):
    return dal.Date_(value.year, value.month, value.day)


def compare_orthogonalized_bases():
    """Check numerical changes from a basis transform without changing its span."""
    states = {
        "wide": np.linspace(20.0, 250.0, 257),
        "deep_tail": np.r_[np.linspace(80.0, 120.0, 256), 1e7],
        "discrete": np.repeat([85.0, 100.0, 115.0], [85, 86, 86]),
    }

    def target(spot):
        return np.exp(-spot / 80.0) + 0.05 * np.sin(spot / 30.0)

    output = []
    for name, training_x in states.items():
        validation_x = (
            np.array([85.0, 100.0, 115.0])
            if name == "discrete"
            else np.linspace(82.0, 118.0, 101)
        )
        center = training_x.mean()
        scale = np.max(np.abs(training_x - center))
        training_z = (training_x - center) / scale
        validation_z = (validation_x - center) / scale
        for degree in (3, 8):
            fits = {}
            for family, vandermonde in (
                ("monomial", lambda z: np.vander(z, degree + 1, increasing=True)),
                ("chebyshev", lambda z: np.polynomial.chebyshev.chebvander(z, degree)),
            ):
                coefficients, _, rank, _ = np.linalg.lstsq(
                    vandermonde(training_z), target(training_x), rcond=1e-12
                )
                prediction = vandermonde(validation_z) @ coefficients
                fits[family] = {
                    "rank": int(rank),
                    "validation_mse": float(
                        np.mean((prediction - target(validation_x)) ** 2)
                    ),
                    "prediction": prediction,
                }
            output.append(
                {
                    "state": name,
                    "degree": degree,
                    "monomial_rank": fits["monomial"]["rank"],
                    "chebyshev_rank": fits["chebyshev"]["rank"],
                    "monomial_validation_mse": fits["monomial"]["validation_mse"],
                    "chebyshev_validation_mse": fits["chebyshev"]["validation_mse"],
                    "max_prediction_gap": float(
                        np.max(
                            np.abs(
                                fits["monomial"]["prediction"]
                                - fits["chebyshev"]["prediction"]
                            )
                        )
                    ),
                }
            )
    return output


def run_case(kind, training_paths, validation_paths, pricing_paths, repeats):
    days = scenarios.exercise_days(kind)
    exercise_dates = [date(scenarios.TODAY + timedelta(days=day)) for day in days]
    product = dal.Product_New(
        exercise_dates,
        [f"EXERCISE MAX({scenarios.STRIKE} - spot(), 0.0)"] * len(days),
    )
    model = dal.BSModelData_New(
        scenarios.SPOT, scenarios.VOL, scenarios.RATE, scenarios.DIVIDEND
    )
    valuation = dal.ScriptValuationSettings_(evaluation_date=date(scenarios.TODAY))
    reference = scenarios.tree_price(days)
    configurations = (
        ("fixed_1", 1, None),
        ("fixed_3", 3, None),
        ("fixed_8", 8, None),
        ("adaptive_8", 8, validation_paths),
    )
    samples = {name: [] for name, _, _ in configurations}
    prices = {}
    for repetition in range(repeats):
        # Rotate order so process warm-up and machine drift do not favor one mode.
        for name, degree, held_out in configurations[repetition % 4 :] + configurations[: repetition % 4]:
            simulation = dal.MonteCarloSettings_(
                compiled=True,
                lsmc_basis_degree=degree,
                lsmc_training_paths=training_paths,
                lsmc_validation_paths=held_out,
            )
            start = time.perf_counter()
            result = dal.MonteCarlo_ValueWithSettings(
                product, model, pricing_paths, valuation=valuation, simulation=simulation
            )
            samples[name].append((time.perf_counter() - start) * 1000.0)
            price = result["PV"]
            if name in prices and price != prices[name]:
                raise RuntimeError(f"non-deterministic repeated price for {name}")
            prices[name] = price
    return {
        "schedule": kind,
        "exercise_dates": len(days),
        "oracle_crr": reference,
        "training_paths": training_paths,
        "validation_paths": validation_paths,
        "pricing_paths": pricing_paths,
        "results": [
            {
                "mode": name,
                "pv": prices[name],
                "absolute_error": abs(prices[name] - reference),
                "time_ms": samples[name],
                "median_time_ms": statistics.median(samples[name]),
            }
            for name, _, _ in configurations
        ],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--training-paths", type=int, default=4096)
    parser.add_argument("--validation-paths", type=int, default=1024)
    parser.add_argument("--pricing-paths", type=int, default=16384)
    parser.add_argument("--repeats", type=int, default=3)
    args = parser.parse_args()
    if min(args.training_paths, args.validation_paths, args.pricing_paths, args.repeats) <= 0:
        parser.error("all path counts and repeats must be positive")
    report = {
        "basis_study": compare_orthogonalized_bases(),
        "pricing": [
            run_case(kind, args.training_paths, args.validation_paths, args.pricing_paths, args.repeats)
            for kind in scenarios.KINDS
        ],
    }
    print("LSMC_COMPARISON_JSON=" + json.dumps(report, separators=(",", ":")))


if __name__ == "__main__":
    main()
