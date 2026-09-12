"""Lazy adapters with a common timing boundary and independent validation."""

from importlib import import_module

from dal_benchmarks.harness import Workload
from .scenarios import BUMP, expected, queries, tolerance, trades, validate

BACKENDS = ("dal", "quantlib", "rateslib")


def prepare(backend, case):
    if backend not in BACKENDS:
        raise ValueError(f"unknown comparison backend: {backend}")
    adapter = import_module(f"dal_comparisons.{backend}_adapter")
    reference = expected(case)
    if case["operation"] == "discount":
        run = adapter.discount_runner(queries(case["size"]))
    elif case["operation"] == "pv":
        run = adapter.pricing_runner(trades(case["size"]), 0.0)
    elif case["operation"] == "dv01":
        up = adapter.pricing_runner(trades(case["size"]), BUMP)
        down = adapter.pricing_runner(trades(case["size"]), -BUMP)

        def run():
            return [(a - b) / 2 for a, b in zip(up(), down())]

    elif case["operation"] == "node_dv01":
        run = adapter.node_risk_runner(trades(case["size"]))
    else:
        runners = {
            "prepared_pv": adapter.prepared_pricing_runner,
            "market_update_pv": adapter.market_update_runner,
            "cold_pv": adapter.cold_pricing_runner,
        }
        run = runners[case["operation"]](trades(case["size"]))

    return Workload(
        run, lambda result: validate(result, reference, abs_tol=tolerance(case))
    )
