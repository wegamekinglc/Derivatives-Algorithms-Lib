"""Scoped successful-call costs for the positioned-batch cancellation repair."""

import argparse
import json
import math
import time

import dal


CASES = ["passive-tree", "passive-compiled", "aad-tree", "aad-compiled", "weighted-compiled", "sobol-control"]
PATHS = 4 * 8192 + 17


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def operation(case):
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 1, 1))
    model = dal.BSModelData_New(spot=1, vol=0, rate=0, div=0)
    weighted = case == "weighted-compiled"
    product = dal.Product_New(
        ["X", "Y", dal.Date_(2027, 1, 1)],
        ["2", "3", "a = X * Y b = X + Y pay PAYS 5" if weighted else "pay PAYS X * SPOT() + Y"],
    )
    native = case.startswith("aad-") or weighted
    simulation = dal.MonteCarloSettings_(
        method="sobol" if case == "sobol-control" else "irn",
        enable_aad=native,
        compiled=case.endswith("compiled") or case == "sobol-control",
    )
    if weighted:
        request = dal.WeightedRiskRequest_(
            outputs=["output:0", "output:1", "payoff"], weights=[2, -1, 0.5],
            inputs=["constant:0", "constant:1"], report_factors=[0.5, 2],
        )

        def run():
            result = dal.MonteCarlo_ValueWithWeightedRisk(
                product, model, PATHS, request=request, valuation=valuation, simulation=simulation,
            )
            return [result.weighted_value, *result.jacobian.to_rows()[0]]

        return run, [9.5, 5, 3]

    def run():
        result = dal.MonteCarlo_ValueWithSettings(product, model, PATHS, valuation=valuation, simulation=simulation)
        return [result["PV"], result["d_X"], result["d_Y"], result["d_spot"]] if native else [result["PV"]]

    return run, [5, 1, 1, 2] if native else [5]


def validate(values, expected):
    require(len(values) == len(expected), "Unexpected result shape")
    require(all(math.isfinite(value) and abs(value - reference) < 1e-10 for value, reference in zip(values, expected)),
            "Analytic successful-call value or gradient mismatch")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--case", choices=CASES, required=True)
    parser.add_argument("--repeats", type=int, required=True)
    arguments = parser.parse_args()
    run, expected = operation(arguments.case)
    validate(run(), expected)
    started = time.perf_counter_ns()
    for _ in range(arguments.repeats):
        values = run()
    elapsed = time.perf_counter_ns() - started
    validate(values, expected)
    print(json.dumps(dict(case=arguments.case, paths=PATHS, repeats=arguments.repeats,
                          elapsed_ns=elapsed, request_ns=elapsed / arguments.repeats)))


if __name__ == "__main__":
    main()
