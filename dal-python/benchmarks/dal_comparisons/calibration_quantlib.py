"""QuantLib log-linear bootstraps with fresh helpers on every valuation."""

import QuantLib as ql

from . import calibration_scenarios as inputs


def date(value):
    return ql.Date(value.day, value.month, value.year)


def index(currency, handle=None):
    return ql.IborIndex(
        f"calibration-{currency}",
        ql.Period(12, ql.Months),
        0,
        ql.USDCurrency() if currency == "usd" else ql.EURCurrency(),
        ql.NullCalendar(),
        ql.Unadjusted,
        False,
        ql.Actual365Fixed(),
        handle if handle is not None else ql.YieldTermStructureHandle(),
    )


def bootstrap(helpers):
    return ql.PiecewiseLogLinearDiscount(
        date(inputs.TODAY), helpers, ql.Actual365Fixed(), ql.IterativeBootstrap(1e-12)
    )


def swap_curve(key, quotes, discount=None):
    handle = (
        ql.YieldTermStructureHandle(discount)
        if discount is not None
        else ql.YieldTermStructureHandle()
    )
    ibor = index(key[:3])
    helpers = [
        ql.SwapRateHelper(
            quote,
            ql.Period(i, ql.Years),
            ql.NullCalendar(),
            ql.Annual,
            ql.Unadjusted,
            ql.Actual365Fixed(),
            ibor,
            ql.QuoteHandle(),
            ql.Period(0, ql.Days),
            handle,
            0,
        )
        for i, quote in enumerate(quotes, 1)
    ]
    curve = bootstrap(helpers)
    curve.discount(helpers[-1].latestDate())
    return curve


def xccy_curve(data):
    curves = {
        key: ql.DiscountCurve(
            data["dates"], dfs, ql.Actual365Fixed(), ql.NullCalendar()
        )
        for key, dfs in data["dfs"].items()
    }
    domestic = index("usd", ql.YieldTermStructureHandle(curves[inputs.KEYS[1]]))
    foreign = index("eur", ql.YieldTermStructureHandle(curves[inputs.KEYS[3]]))
    collateral = ql.YieldTermStructureHandle(curves[inputs.KEYS[0]])
    helpers = [
        ql.ConstNotionalCrossCurrencyBasisSwapRateHelper(
            ql.QuoteHandle(ql.SimpleQuote(quote)),
            ql.Period(i, ql.Years),
            0,
            ql.NullCalendar(),
            ql.Unadjusted,
            False,
            domestic,
            foreign,
            collateral,
            True,
            False,
            ql.Annual,
            0,
            ql.Annual,
        )
        for i, quote in enumerate(data["quotes"][inputs.KEYS[4]], 1)
    ]
    return {inputs.KEYS[4]: bootstrap(helpers)}


def runner(case):
    ql.Settings.instance().evaluationDate = date(inputs.TODAY)
    nodes = inputs.dates(case["size"])
    data = {
        "quotes": inputs.quotes(case["size"]),
        "dates": [date(d) for d in nodes],
        "dfs": {
            key: [inputs.discount(key, d, case["size"]) for d in nodes]
            for key in inputs.KEYS[:4]
        },
    }
    queries = [date(d) for d in inputs.queries(case["size"])]

    def run():
        if case["kind"] == "xccy_staged":
            curves = xccy_curve(data)
        else:
            discount = swap_curve(inputs.KEYS[0], data["quotes"][inputs.KEYS[0]])
            curves = {inputs.KEYS[0]: discount}
            if case["kind"] == "multi_staged":
                curves[inputs.KEYS[1]] = swap_curve(
                    inputs.KEYS[1], data["quotes"][inputs.KEYS[1]], discount
                )
        return [
            curves[key].discount(d)
            for key in inputs.curve_keys(case["kind"])
            for d in queries
        ]

    return run
