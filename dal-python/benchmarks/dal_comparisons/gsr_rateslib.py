"""Rateslib standard IRS counterpart on the GSR example's input curve."""

import rateslib as rl

from . import gsr_scenarios as inputs


def runner(case):
    if case["operation"] != "gsr_static_swap":
        raise ValueError(f"unknown rateslib GSR operation: {case['operation']}")
    rl.defaults.curve_caching = False
    # The forecast curve's index convention is ACT/360; the dated DF nodes still
    # come from the example's ACT/365F continuously compounded input curve.
    source = rl.Curve(
        nodes={date: inputs.discount(date) for date in inputs.CURVE_DATES},
        interpolation="log_linear",
        convention="Act360",
        id="gsr-comparison",
        ad=0,
    )
    swaps = [
        rl.IRS(
            effective=inputs.EXPIRY,
            termination=inputs.MATURITY,
            frequency="S",
            convention="30E360",
            modifier="NONE",
            calendar="bus",
            payment_lag=0,
            currency="usd",
            notional=1.0,
            fixed_rate=100 * inputs.STRIKE,
            leg2_frequency="Q",
            leg2_convention="Act360",
            leg2_modifier="NONE",
            leg2_payment_lag=0,
            leg2_fixing_method="ibor(2)",
            leg2_fixing_frequency="Q",
        )
        for _ in range(case["size"])
    ]
    return lambda: [float(swap.npv(curves=source)) for swap in swaps]
