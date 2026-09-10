"""RNG and script workloads matching the native benchmark inputs."""

import json
import math

import dal

from .harness import Workload, require


def prepare_rng(kind, paths):
    def run():
        if kind == "mrg32_normal":
            return dal.PseudoRSG_Get_Normal(dal.PseudoRSG_New(1024, 10), paths)
        precise = kind == "sobol_normal_precise"
        generator = dal.SobolRSG_New(0, 10, precise=precise, polish=precise)
        fill = (
            dal.SobolRSG_Get_Uniform
            if kind == "sobol_uniform"
            else dal.SobolRSG_Get_Normal
        )
        return fill(generator, paths)

    # Reinitializing on every call matches rng_perf and makes each sample identical.
    reference = run()
    expected = [reference(i, j) for i in (0, paths // 2, paths - 1) for j in (0, 9)]

    def validate(value):
        require((value.Rows(), value.Cols()) == (paths, 10), "RNG shape changed")
        actual = [value(i, j) for i in (0, paths // 2, paths - 1) for j in (0, 9)]
        require(
            actual == expected and all(math.isfinite(x) for x in actual),
            "RNG is not finite/repeatable",
        )
        if kind == "sobol_uniform":
            require(all(0 <= x <= 1 for x in actual), "uniform RNG outside [0, 1]")

    return Workload(run, validate)


def script_inputs(kind):
    if kind == "construct":
        dal.EvaluationDate_Set(dal.Date_(2022, 9, 25))
        return (
            [
                "BARRIER",
                "STRIKE",
                dal.Date_(2022, 9, 25),
                "START: 2022-09-25\nEND: 2025-09-25\nFREQ: 1W",
                dal.Date_(2025, 9, 25),
            ],
            [
                "150.00",
                "120.00",
                "alive = 1",
                "IF spot() > BARRIER:0.1 THEN alive = 0 END",
                "IF spot() > BARRIER:0.1 THEN alive = 0 END uoc pays alive * MAX(spot() - STRIKE, 0.0)",
            ],
        )
    dal.EvaluationDate_Set(dal.Date_(2024, 1, 1))
    if kind == "vanilla":
        return ["STRIKE", dal.Date_(2025, 1, 1)], [
            "100.0",
            "call pays MAX(spot() - STRIKE, 0.0)",
        ]
    return (
        [
            "STRIKE",
            "BARRIER",
            dal.Date_(2024, 1, 1),
            "START: 2024-01-01 END: 2025-01-01 FREQ: 1W",
            dal.Date_(2025, 1, 1),
        ],
        [
            "100.0",
            "150.0",
            "alive = 1",
            "if spot() >= BARRIER:0.1 then alive = 0 end",
            "uoc pays alive * MAX(spot() - STRIKE, 0.0)",
        ],
    )


def prepare_script():
    dates, events = script_inputs("construct")
    expected = json.loads(dal.Product_DebugJson(dal.Product_New(dates, events)))
    return Workload(
        # Product_New only stores event data. The dump call is what actually
        # constructs ScriptProduct_ and executes the native frontend.
        lambda: dal.Product_DebugJson(dal.Product_New(dates, events)),
        lambda result: require(json.loads(result) == expected, "script AST changed"),
    )


def prepare_mc(kind, aad, compiled, paths):
    dates, events = script_inputs(kind)

    def run(use_compiled=compiled):
        # Like script_mc_perf, product/model construction and preprocessing are timed.
        return dal.MonteCarlo_Value(
            dal.Product_New(dates, events),
            dal.BSModelData_New(100, 0.2, 0.05, 0.02),
            paths,
            "sobol",
            False,
            aad,
            0.01,
            use_compiled,
        )

    reference = run(not compiled)

    def validate(result):
        require(
            result.keys() == reference.keys() and "PV" in result,
            "MC result keys changed",
        )
        require(
            all(
                math.isfinite(v)
                and math.isclose(v, reference[k], rel_tol=1e-10, abs_tol=1e-9)
                for k, v in result.items()
            ),
            "tree/compiled MC results disagree",
        )
        require(result["PV"] > 0, "MC payoff missing")
        if aad:
            require(any(k.startswith("d_") for k in result), "AAD Greeks missing")
        if kind == "vanilla":
            years = (dal.Date_(2025, 1, 1) - dal.Date_(2024, 1, 1)) / 365
            d1 = (0.05 - 0.02 + 0.5 * 0.2**2) * math.sqrt(years) / 0.2
            d2 = d1 - 0.2 * math.sqrt(years)

            def cdf(x):
                return (1 + math.erf(x / math.sqrt(2))) / 2

            price = 100 * (
                math.exp(-0.02 * years) * cdf(d1) - math.exp(-0.05 * years) * cdf(d2)
            )
            require(
                abs(result["PV"] - price) < 0.4, "MC differs from Black-Scholes oracle"
            )

    return Workload(run, validate)
