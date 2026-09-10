"""QuantLib Python bindings; force fresh NPV calculation on every invocation."""

import QuantLib as ql

from .scenarios import NODES, TODAY, node_dfs


def date(value):
    return ql.Date(value.day, value.month, value.year)


def curve(shift):
    return ql.DiscountCurve(
        [date(value) for value in NODES],
        node_dfs(shift),
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


def pricing_runner(portfolio, shift):
    ql.Settings.instance().evaluationDate = date(TODAY)
    handle = ql.YieldTermStructureHandle(curve(shift))
    swaps = [swap(value, handle) for value in portfolio]

    def run():
        values = []
        for instrument in swaps:
            instrument.recalculate()
            values.append(instrument.NPV())
        return values

    return run
