"""Explicit mapping of every native target to the callable Python surface."""

from functools import partial

from .harness import Case


# 'partial' means reachable public workloads, never native-kernel timing parity.
CPP_COVERAGE = {
    "rng_perf": {
        "status": "partial",
        "detail": "100K x 10D Sobol fast/precise/uniform and MRG32 normal; public matrix allocation included. MRG32 is fixed to precise=true in Python versus false in C++. BrownianBridge and IRN have no direct binding.",
    },
    "script_perf": {
        "status": "partial",
        "detail": "Same three-year weekly barrier events; Product_New + Product_DebugJson times frontend construction with indexing/JSON output. Product_New alone only stores events. Parser/preprocessor stages are not separately bound.",
    },
    "script_mc_perf": {
        "status": "partial",
        "detail": "All eight vanilla/barrier x double/AAD x tree/compiled cases; native path counts, dates and model. Includes public result conversion.",
    },
    "curve_calibration_perf": {
        "status": "partial",
        "detail": "All 21 cases: five representations x analytic/bumped x diagnostics/solve, plus approximate. Same 23 swaps/24 future knots.",
    },
    "xccy_perf": {
        "status": "partial",
        "detail": "Joint/staged analytic/bumped x diagnostics/solve using five quotes per block. Adapted square quote ladder; native 15-instrument/reset-aware/approximate and precompute pricing cases are not reproduced.",
    },
    "rate_risk_perf": {
        "status": "partial",
        "detail": "120-IRS batch/single, 5Y daily OIS, 24-XCCY batch, quote portfolios, generic joint N=5/10/16 x 100/1000 IRS and 32/256-IRS AAD node DV01. XCCY market ladder adapted; internal counters and joint-node reference are unbound.",
    },
    "quote_risk_perf": {
        "status": "partial",
        "detail": "Single, joint XCCY and staged provenance at N=8/16; joint XCCY has five parameter blocks. Adds generic joint N=5/10/16. Internal component state probe is unbound.",
    },
    "matrix_perf": {
        "status": "unavailable",
        "detail": "Python exposes matrix storage/conversion, not Multiply/MultiplyLeft kernels.",
    },
    "tape_perf": {
        "status": "indirect",
        "detail": "Tape clear/rewind/propagation APIs are unbound; AAD MC and rate risk exercise them indirectly.",
    },
    "jacobian_perf": {
        "status": "indirect",
        "detail": "Synthetic AAD sweep/harvesting APIs are unbound; calibration covers real Jacobians.",
    },
    "pde_perf": {
        "status": "unavailable",
        "detail": "ThetaScheme and PDE grid/rollback APIs are unbound.",
    },
    "interp_perf": {
        "status": "indirect",
        "detail": "Cubic interpolation microkernel is unbound; LOG_CUBIC_NATURAL calibration exercises interpolation.",
    },
    "krylov_perf": {
        "status": "indirect",
        "detail": "Conjugate-gradient solver is unbound; approximate calibration exercises it indirectly.",
    },
    "banded_perf": {
        "status": "unavailable",
        "detail": "Banded matrix-vector kernel is unbound.",
    },
    "cholesky_perf": {
        "status": "unavailable",
        "detail": "Cholesky decomposition API is unbound.",
    },
    "specialfunctions_perf": {
        "status": "indirect",
        "detail": "Special-function APIs are unbound; normal RNG exercises inverse-normal functions.",
    },
    "black_perf": {
        "status": "unavailable",
        "detail": "Analytic Black/Bachelier pricing and Greek APIs are unbound; MC is a different algorithm.",
    },
    "iv_brent_perf": {
        "status": "unavailable",
        "detail": "Implied-volatility and Brent solver APIs are unbound.",
    },
    "ycinstrument_perf": {
        "status": "indirect",
        "detail": "Instrument factories are bound, but isolated Precompute/operator() is not. Calibration exercises instrument pricing.",
    },
    "threadpool_perf": {
        "status": "indirect",
        "detail": "Thread-pool scheduling API is unbound; MC uses the native pool.",
    },
    "stacks_perf": {
        "status": "indirect",
        "detail": "Block-list/stack primitives are unbound; native AAD workloads use them internally.",
    },
}


def build_cases(smoke=False):
    # --coverage reads the mapping without importing the native extension.
    from . import calibration, risk, simulation, xccy

    cases = []

    def add(name, target, workload, prepare):
        cases.append(
            Case(
                name,
                target,
                dict(workload, profile="smoke" if smoke else "full"),
                prepare,
            )
        )

    _simulation_cases(add, smoke, simulation)
    _calibration_cases(add, calibration)
    _xccy_cases(add, xccy)
    _node_cases(add, smoke, risk, xccy)
    _aad_comparison_cases(add, smoke)
    _quote_cases(add, smoke, risk, xccy)
    _generic_cases(add, smoke, risk)
    return cases


def _aad_comparison_cases(add, smoke):
    from dal_comparisons.scenarios import cases, RISK_METHODS, NODES
    from dal_comparisons.suite import prepare

    for case in cases(smoke):
        if case["operation"] == "node_dv01":
            size = case["name"].rsplit("_", 1)[1]
            add(
                f"nodes.aad_dv01.t{size}",
                "rate_risk_perf",
                {
                    "trades": case["size"],
                    "nodes": len(NODES) - 1,
                    "method": RISK_METHODS["dal"],
                    "risk": "dPV/dzero_i * 1bp",
                },
                partial(prepare, "dal", case),
            )


def _simulation_cases(add, smoke, simulation):
    paths = 1024 if smoke else 100_000
    for kind in (
        "sobol_normal_fast",
        "sobol_normal_precise",
        "sobol_uniform",
        "mrg32_normal",
    ):
        add(
            f"rng.{kind}",
            "rng_perf",
            {
                "paths": paths,
                "dimensions": 10,
                "boundary": "generator creation + matrix fill",
                "precise": kind in ("sobol_normal_precise", "mrg32_normal"),
                "polish": kind in ("sobol_normal_precise", "mrg32_normal"),
            },
            partial(simulation.prepare_rng, kind, paths),
        )
    add(
        "script.construct_and_parse",
        "script_perf",
        {
            "years": 3,
            "frequency": "1W",
            "constructions": 1,
            "boundary": "event storage + native frontend + variable indexing + JSON serialization",
        },
        simulation.prepare_script,
    )
    for kind, double_paths, aad_paths in (
        ("vanilla", 200_000, 20_000),
        ("barrier", 100_000, 10_000),
    ):
        for aad in (False, True):
            for compiled in (False, True):
                paths = 2048 if smoke else aad_paths if aad else double_paths
                name = f"mc.{kind}.{'aad' if aad else 'double'}.{'compiled' if compiled else 'tree'}"
                add(
                    name,
                    "script_mc_perf",
                    {
                        "paths": paths,
                        "aad": aad,
                        "compiled": compiled,
                        "boundary": "product + model + preprocessing + simulation + result conversion",
                    },
                    partial(simulation.prepare_mc, kind, aad, compiled, paths),
                )


def _calibration_cases(add, calibration):
    for representation in ("PWC", "PWL", "LOG_LINEAR", "LOG_CUBIC_NATURAL", "MIXED"):
        for mode in ("ANALYTIC", "BUMPED"):
            for diagnostic in (True, False):
                add(
                    f"calibration.{representation}.{mode}.{'diagnostics' if diagnostic else 'solve'}",
                    "curve_calibration_perf",
                    {
                        "swaps": 23,
                        "future_knots": 24,
                        "representation": representation,
                        "jacobian": mode,
                        "diagnostics": diagnostic,
                    },
                    partial(
                        calibration.prepare_calibration,
                        representation,
                        mode,
                        diagnostic,
                    ),
                )
    add(
        "calibration.LOG_LINEAR.APPROXIMATE",
        "curve_calibration_perf",
        {
            "swaps": 23,
            "future_knots": 24,
            "representation": "LOG_LINEAR",
            "solve_mode": "APPROXIMATE",
        },
        partial(calibration.prepare_calibration, "LOG_LINEAR", "ANALYTIC", True, True),
    )


def _xccy_cases(add, xccy):
    for kind in ("joint", "staged"):
        for mode in ("ANALYTIC", "BUMPED"):
            for diagnostic in (True, False):
                add(
                    f"xccy.{kind}.{mode}.{'diagnostics' if diagnostic else 'solve'}",
                    "xccy_perf",
                    {
                        "quotes_per_block": 5,
                        "blocks": 5 if kind == "joint" else 1,
                        "jacobian": mode,
                        "diagnostics": diagnostic,
                        "fixture": "adapted square quote ladder",
                    },
                    partial(xccy.prepare_calibration, kind, mode, diagnostic),
                )


def _node_cases(add, smoke, risk, xccy):
    count = 2 if smoke else 120
    for operation in ("batch", "single", "ois"):
        add(
            f"nodes.{operation}",
            "rate_risk_perf",
            {
                "trades": 1 if operation == "ois" else count,
                "components": 1 if operation == "ois" else 2,
                "years": 5 if operation == "ois" else 10,
            },
            partial(risk.prepare_nodes, count, operation),
        )
    count = 2 if smoke else 24
    add(
        "nodes.xccy",
        "rate_risk_perf",
        {
            "trades": count,
            "components": 5,
            "fixture": "adapted joint calibration market",
        },
        partial(xccy.prepare_nodes, count),
    )


def _provenance_cases(add, risk, xccy):
    for kind in ("single", "joint", "staged"):
        for width in (8, 16):
            factory = (
                partial(risk.single_fixture, width, 1, "ANALYTIC")
                if kind == "single"
                else partial(xccy.fixture, kind, width, 1, "ANALYTIC")
            )
            add(
                f"provenance.{kind}.n{width}",
                "quote_risk_perf",
                {
                    "quotes_per_block": width,
                    "blocks": 5 if kind == "joint" else 1,
                    "boundary": "provenance construction only",
                },
                partial(risk.prepare_quote, factory, True),
            )


def _quote_cases(add, smoke, risk, xccy):
    _provenance_cases(add, risk, xccy)
    portfolio_shapes = [
        ("single", 2, "ANALYTIC", 1),
        ("joint", 2, "ANALYTIC", 1),
        ("staged", 2, "ANALYTIC", 1),
        ("single", 5, "ANALYTIC", 120),
        ("single", 16, "BUMPED", 120),
        ("joint", 10, "ANALYTIC", 24),
        ("joint", 10, "BUMPED", 24),
        ("staged", 16, "ANALYTIC", 24),
        ("staged", 5, "BUMPED", 24),
    ]
    for kind, width, mode, full_count in portfolio_shapes:
        count = min(full_count, 2) if smoke else full_count
        factory = (
            partial(risk.single_fixture, width, count, mode)
            if kind == "single"
            else partial(xccy.fixture, kind, width, count, mode)
        )
        add(
            f"quotes.{kind}.n{width}.{mode}.t{full_count}",
            "rate_risk_perf",
            {
                "quotes_per_block": width,
                "blocks": 5 if kind == "joint" else 1,
                "trades": count,
                "jacobian": mode,
                "boundary": "aggregation only; calibration and provenance excluded",
            },
            partial(risk.prepare_quote, factory),
        )


def _generic_cases(add, smoke, risk):
    for width in (5, 10, 16):
        factory = partial(risk.generic_fixture, width, 1)
        add(
            f"provenance.generic.n{width}",
            "quote_risk_perf",
            {"quotes": width, "blocks": 3, "layered": True},
            partial(risk.prepare_quote, factory, True),
        )
        for full_count in (100, 1000):
            count = 2 if smoke else full_count
            add(
                f"quotes.generic.n{width}.t{full_count}",
                "rate_risk_perf",
                {
                    "quotes": width,
                    "blocks": 3,
                    "trades": count,
                    "layered": True,
                    "fixture": "native flat curve quotes derived analytically; third block is a structural zero",
                    "boundary": "aggregation only; calibration and provenance excluded",
                },
                partial(
                    risk.prepare_quote, partial(risk.generic_fixture, width, count)
                ),
            )
