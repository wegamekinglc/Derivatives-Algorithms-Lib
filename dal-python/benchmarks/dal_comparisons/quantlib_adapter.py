"""QuantLib Python bindings; force fresh NPV calculation on every invocation."""

import QuantLib as ql
import math

from .scenarios import BUMP, NODE_BUMP, NODES, TIMES, TODAY, node_dfs


def date(value):
    return ql.Date(value.day, value.month, value.year)


def curve(shift, node=None, node_shift=0.0):
    dfs = node_dfs(shift)
    if node is not None:
        dfs[node] *= math.exp(-node_shift * TIMES[node])
    return ql.DiscountCurve(
        [date(value) for value in NODES],
        dfs,
        ql.Actual365Fixed(),
        ql.NullCalendar(),
    )


def discount_runner(dates):
    source = curve(0.0)
    native_dates = [date(value) for value in dates]
    return lambda: [source.discount(value) for value in native_dates]


def swap(value, handle):
    calendar, basis = ql.NullCalendar(), ql.Actual365Fixed()
    schedule = ql.Schedule(
        date(value["start"]),
        date(value["end"]),
        ql.Period(12, ql.Months),
        calendar,
        ql.Unadjusted,
        ql.Unadjusted,
        ql.DateGeneration.Forward,
        False,
    )
    index = ql.IborIndex(
        "comparison",
        ql.Period(12, ql.Months),
        0,
        ql.USDCurrency(),
        calendar,
        ql.Unadjusted,
        False,
        basis,
        handle,
    )
    direction = ql.VanillaSwap.Payer if value["sign"] == 1 else ql.VanillaSwap.Receiver
    result = ql.VanillaSwap(
        direction,
        value["notional"],
        schedule,
        value["rate"],
        basis,
        schedule,
        index,
        0.0,
        basis,
    )
    result.setPricingEngine(ql.DiscountingSwapEngine(handle))
    return result


def prices(swaps):
    values = []
    for instrument in swaps:
        instrument.recalculate()
        values.append(instrument.NPV())
    return values


def pricing_runner(portfolio, shift):
    ql.Settings.instance().evaluationDate = date(TODAY)
    handle = ql.YieldTermStructureHandle(curve(shift))
    swaps = [swap(value, handle) for value in portfolio]

    return lambda: prices(swaps)


def node_risk_runner(portfolio):
    ql.Settings.instance().evaluationDate = date(TODAY)
    handle = ql.RelinkableYieldTermStructureHandle(curve(0.0))
    swaps = [swap(value, handle) for value in portfolio]
    shifts = [
        (curve(0.0, i, NODE_BUMP), curve(0.0, i, -NODE_BUMP))
        for i in range(1, len(NODES))
    ]

    def run():
        buckets = []
        for up, down in shifts:
            handle.linkTo(up)
            high = prices(swaps)
            handle.linkTo(down)
            low = prices(swaps)
            buckets.append(
                [(a - b) * BUMP / (2 * NODE_BUMP) for a, b in zip(high, low)]
            )
        return [value for row in zip(*buckets) for value in row]

    return run
