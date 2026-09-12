"""Rateslib fresh AD solvers for the common annual IRS and XCCY markets."""

import math

import rateslib as rl

from dal_benchmarks.harness import require
from . import calibration_scenarios as inputs


def make_curve(key, data, fixed=False):
    values = (
        data["dfs"][key]
        if fixed
        else [math.exp(-0.025 * (d - inputs.TODAY).days / 365) for d in data["dates"]]
    )
    return rl.Curve(
        nodes=dict(zip(data["dates"], values)),
        id=key,
        interpolation="log_linear",
        convention="Act365F",
        ad=0 if fixed else 1,
    )


def swap_instruments(key, data, curves):
    discount = key[:3] + "_discount"
    return [
        rl.IRS(
            effective=inputs.TODAY,
            termination=maturity,
            frequency="A",
            convention="Act365F",
            modifier="NONE",
            calendar="all",
            payment_lag=0,
            currency=key[:3],
            leg2_frequency="A",
            leg2_convention="Act365F",
            leg2_modifier="NONE",
            leg2_payment_lag=0,
            leg2_fixing_method="ibor(0)",
            leg2_fixing_frequency="A",
            curves=[curves[key], curves[discount]],
        )
        for maturity in data["dates"][1:]
    ]


def fx_forwards(curves):
    return rl.FXForwards(
        fx_rates=rl.FXRates({"eurusd": 1.10}, settlement=inputs.TODAY),
        fx_curves={
            "usdusd": curves[inputs.KEYS[0]],
            "eureur": curves[inputs.KEYS[2]],
            "eurusd": curves[inputs.KEYS[4]],
        },
    )


def xccy_instruments(data, curves):
    return [
        rl.XCS(
            effective=inputs.TODAY,
            termination=maturity,
            frequency="A",
            convention="Act365F",
            modifier="NONE",
            calendar="all",
            payment_lag=0,
            payment_lag_exchange=0,
            currency="usd",
            pair="eurusd",
            notional=1.10,
            leg2_fx_fixings=1.10,
            fixed=False,
            mtm=False,
            float_spread=0.0,
            fixing_method="ibor(0)",
            fixing_frequency="A",
            leg2_frequency="A",
            leg2_convention="Act365F",
            leg2_modifier="NONE",
            leg2_payment_lag=0,
            leg2_payment_lag_exchange=0,
            leg2_fixed=False,
            leg2_mtm=False,
            leg2_fixing_method="ibor(0)",
            leg2_fixing_frequency="A",
            metric="leg2",
            curves=[
                curves[inputs.KEYS[1]],
                curves[inputs.KEYS[0]],
                curves[inputs.KEYS[3]],
                curves[inputs.KEYS[4]],
            ],
        )
        for maturity in data["dates"][1:]
    ]


def solve(keys, data, curves, *, previous=(), fx=None):
    instruments, quotes = [], []
    for key in keys:
        is_xccy = key == inputs.KEYS[4]
        instruments.extend(
            xccy_instruments(data, curves)
            if is_xccy
            else swap_instruments(key, data, curves)
        )
        quotes.extend(
            value * (10000 if is_xccy else 100) for value in data["quotes"][key]
        )
    kwargs = {} if fx is None else {"fx": fx}
    result = rl.Solver(
        curves=[curves[key] for key in keys],
        instruments=instruments,
        s=quotes,
        pre_solvers=previous,
        id="-".join(keys),
        func_tol=1e-20,
        conv_tol=1e-20,
        grad_tol=1e-16,
        max_iter=100,
        **kwargs,
    )
    require(
        result.result["status"] == "SUCCESS", "rateslib calibration did not converge"
    )
    return result


def runner(case):
    rl.defaults.curve_caching = False
    nodes = inputs.dates(case["size"])
    data = {
        "dates": nodes,
        "quotes": inputs.quotes(case["size"]),
        "dfs": {
            key: [inputs.discount(key, d, case["size"]) for d in nodes]
            for key in inputs.KEYS[:4]
        },
    }
    keys, queries = inputs.curve_keys(case["kind"]), inputs.queries(case["size"])

    def run():
        if case["kind"] == "xccy_staged":
            curves = {
                key: make_curve(key, data, fixed=key not in keys) for key in inputs.KEYS
            }
        else:
            curves = {key: make_curve(key, data) for key in keys}
        if case["kind"] == "multi_staged":
            first = solve(keys[:1], data, curves)
            solve(keys[1:], data, curves, previous=(first,))
        else:
            fx = fx_forwards(curves) if case["kind"].startswith("xccy_") else None
            solve(keys, data, curves, fx=fx)
        return [float(curves[key][d]) for key in keys for d in queries]

    return run
