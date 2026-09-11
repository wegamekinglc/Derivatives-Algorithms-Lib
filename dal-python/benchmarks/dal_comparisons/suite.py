"""Lazy adapters with a common timing boundary and independent validation."""

from importlib import import_module

from dal_benchmarks.harness import Workload
from .scenarios import BUMP, expected, queries, trades, validate

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
    else:
        up = adapter.pricing_runner(trades(case["size"]), BUMP)
        down = adapter.pricing_runner(trades(case["size"]), -BUMP)

        def run():
            return [(a - b) / 2 for a, b in zip(up(), down())]

    tolerance = 2e-12 if case["operation"] == "discount" else 2e-7
    return Workload(run, lambda result: validate(result, reference, abs_tol=tolerance))
