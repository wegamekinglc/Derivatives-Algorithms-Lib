"""QuantLib single-curve swap and GSR Sobol swaption counterparts."""

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
    underlying = swap(handle)
    fixed_cashflows = underlying.fixedLeg()
    if len(fixed_cashflows) != 2 or fixed_cashflows[-1].date() != date(inputs.MATURITY):
        raise ValueError("GSR comparison expects two fixed coupons ending at maturity")
    first_coupon = (fixed_cashflows[0].date(), fixed_cashflows[0].amount())
    last_coupon = (fixed_cashflows[1].date(), fixed_cashflows[1].amount())
    expiry = date(inputs.EXPIRY)
    expiry_time = ql.Actual365Fixed().yearFraction(date(inputs.TODAY), expiry)
    process = model.stateProcess()
    state_mean = process.expectation(0.0, 0.0, expiry_time)
    state_scale = process.stdDeviation(0.0, 0.0, expiry_time)
    initial_numeraire = model.numeraire(0.0)

    def price_swaption():
        sequence = ql.GaussianLowDiscrepancySequenceGenerator(
            ql.UniformLowDiscrepancySequenceGenerator(1, 0)
        )
        paths = ql.GaussianSobolPathGenerator(process, expiry_time, 1, sequence, False)
        payoff_sum = 0.0
        for _ in range(case["size"]):
            state = (paths.next().value().back() - state_mean) / state_scale
            first_bond = model.zerobond(first_coupon[0], expiry, state)
            last_bond = model.zerobond(last_coupon[0], expiry, state)
            # With one forecast/discount curve, the floating leg telescopes to
            # 1 - P(expiry, maturity). The Gsr process uses a terminal measure.
            exercise_value = (
                1.0 - first_coupon[1] * first_bond - (1.0 + last_coupon[1]) * last_bond
            )
            payoff_sum += max(exercise_value, 0.0) / model.numeraire(expiry_time, state)
        return [initial_numeraire * payoff_sum / case["size"]]

    return price_swaption
