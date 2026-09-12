"""Lazy adapters with a common timing boundary and independent validation."""

from importlib import import_module

from dal_benchmarks.harness import Workload
from .scenarios import (
    BUMP,
    expected,
    queries,
    tolerance,
    trades,
    unsupported_reason,
    validate,
)

BACKENDS = ("dal", "quantlib", "rateslib")


def prepare(backend, case):
    if backend not in BACKENDS:
        raise ValueError(f"unknown comparison backend: {backend}")
    reason = unsupported_reason(backend, case)
    if reason:
        raise ValueError(reason)
    reference = expected(case)
    if case["operation"].startswith("mc_") or case["operation"] == "calibration":
        family = "calibration" if case["operation"] == "calibration" else "option"
        adapter = import_module(f"dal_comparisons.{family}_{backend}")
        run = adapter.runner(case)
    else:
        adapter = import_module(f"dal_comparisons.{backend}_adapter")
        run = rate_runner(adapter, case)
    return Workload(
        run, lambda result: validate(result, reference, abs_tol=tolerance(case))
    )


def rate_runner(adapter, case):
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

    return run
