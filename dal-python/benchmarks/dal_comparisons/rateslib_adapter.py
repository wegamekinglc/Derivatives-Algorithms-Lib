"""Rateslib passive pricing with the same unadjusted annual IBOR conventions."""

import rateslib as rl

from .scenarios import NODES, node_dfs


def curve(shift):
    # Public configuration: do not compare a date->DF cache hit with interpolation.
    rl.defaults.curve_caching = False
    return rl.Curve(
        nodes=dict(zip(NODES, node_dfs(shift))),
        interpolation="log_linear",
        convention="Act365F",
        ad=0,
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
