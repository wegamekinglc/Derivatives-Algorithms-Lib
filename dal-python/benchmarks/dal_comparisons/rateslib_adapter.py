"""Rateslib passive pricing and forward AD risk for unadjusted annual IRS."""

import rateslib as rl
from rateslib.dual import gradient

from dal_benchmarks.harness import require
from .scenarios import BUMP, NODES, TIMES, node_dfs


def curve(shift, ad=0):
    # Public configuration: do not compare a date->DF cache hit with interpolation.
    rl.defaults.curve_caching = False
    return rl.Curve(
        nodes=dict(zip(NODES, node_dfs(shift))),
        interpolation="log_linear",
        convention="Act365F",
        id="comparison",
        ad=ad,
    )


def discount_runner(dates):
    source = curve(0.0)
    return lambda: [float(source[value]) for value in dates]


def swap(value):
    return rl.IRS(
        effective=value["start"],
        termination=value["end"],
        frequency="A",
        convention="Act365F",
        modifier="NONE",
        calendar="all",
        payment_lag=0,
        currency="usd",
        notional=value["sign"] * value["notional"],
        fixed_rate=100 * value["rate"],
        leg2_frequency="A",
        leg2_convention="Act365F",
        leg2_modifier="NONE",
        leg2_payment_lag=0,
        leg2_fixing_method="ibor(0)",
        leg2_fixing_frequency="A",
    )


def pricing_runner(portfolio, shift):
    source = curve(shift)
    swaps = [swap(value) for value in portfolio]
    return lambda: [float(instrument.npv(curves=source)) for instrument in swaps]


def node_risk_runner(portfolio):
    source = curve(0.0, ad=1)
    swaps = [swap(value) for value in portfolio]
    variables = [f"comparison{i}" for i in range(1, len(NODES))]
    scales = [-t * BUMP * df for t, df in zip(TIMES[1:], node_dfs()[1:])]

    def risk(instrument):
        value = instrument.npv(curves=source)
        require(
            isinstance(value, rl.Dual), "rateslib risk lost automatic differentiation"
        )
        return [
            float(scale * derivative)
            for scale, derivative in zip(scales, gradient(value, variables))
        ]

    return lambda: [value for instrument in swaps for value in risk(instrument)]
