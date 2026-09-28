"""QuantLib single-curve swap and one-factor Gaussian swaption counterparts."""

import QuantLib as ql

from . import gsr_scenarios as inputs


def date(value):
    return ql.Date(value.day, value.month, value.year)


def curve():
    return ql.DiscountCurve(
        [date(value) for value in inputs.CURVE_DATES],
        [inputs.discount(value) for value in inputs.CURVE_DATES],
        ql.Actual365Fixed(),
        ql.WeekendsOnly(),
    )


def swap(handle):
    calendar = ql.WeekendsOnly()

    def schedule(months):
        return ql.Schedule(
            date(inputs.EXPIRY),
            date(inputs.MATURITY),
            ql.Period(months, ql.Months),
            calendar,
            ql.ModifiedFollowing,
            ql.ModifiedFollowing,
            ql.DateGeneration.Forward,
            False,
        )

    index = ql.IborIndex(
        "USD-LIBOR-3M-CME",
        ql.Period(3, ql.Months),
        2,
        ql.USDCurrency(),
        calendar,
        ql.ModifiedFollowing,
        False,
        ql.Actual360(),
        handle,
    )
    result = ql.VanillaSwap(
        ql.VanillaSwap.Payer,
        1.0,
        schedule(6),
        inputs.STRIKE,
        ql.Thirty360(ql.Thirty360.BondBasis),
        schedule(3),
        index,
        0.0,
        ql.Actual360(),
    )
    result.setPricingEngine(ql.DiscountingSwapEngine(handle))
    return result


def quote_handles(value):
    result = ql.QuoteHandleVector()
    result.push_back(ql.QuoteHandle(ql.SimpleQuote(value)))
    return result


def runner(case):
    ql.Settings.instance().evaluationDate = date(inputs.TODAY)
    handle = ql.YieldTermStructureHandle(curve())
    if case["operation"] == "gsr_static_swap":
        instruments = [swap(handle) for _ in range(case["size"])]

        def price_swaps():
            values = []
            for instrument in instruments:
                instrument.recalculate()
                values.append(instrument.NPV())
            return values

        return price_swaps

    if case["operation"] != "gsr_swaption":
        raise ValueError(f"unknown GSR operation: {case['operation']}")
    model = ql.Gsr(
        handle,
        ql.DateVector(),
        quote_handles(inputs.VOLATILITY),
        quote_handles(0.0),
        ql.Actual365Fixed().yearFraction(date(inputs.TODAY), date(inputs.MATURITY)),
    )
    # DAL pays the underlying swap's exercise value at expiry, matching a
    # physically settled option's expiry value under this single-curve model.
    instrument = ql.Swaption(swap(handle), ql.EuropeanExercise(date(inputs.EXPIRY)))
    instrument.setPricingEngine(
        ql.Gaussian1dSwaptionEngine(model, 128, 7.0, True, False, handle)
    )

    def price_swaption():
        instrument.recalculate()
        return [instrument.NPV()]

    return price_swaption
