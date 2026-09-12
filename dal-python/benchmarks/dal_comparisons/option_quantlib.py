"""QuantLib native MC engines; the barrier has discrete weekly monitoring."""

import QuantLib as ql

from . import option_scenarios as inputs


def runner(case):
    today = ql.Date(inputs.TODAY.day, inputs.TODAY.month, inputs.TODAY.year)
    maturity = today + 364
    ql.Settings.instance().evaluationDate = today
    basis, calendar = ql.Actual365Fixed(), ql.NullCalendar()

    def price(spot, vol, rate):
        process = ql.BlackScholesMertonProcess(
            ql.QuoteHandle(ql.SimpleQuote(spot)),
            ql.YieldTermStructureHandle(ql.FlatForward(today, inputs.DIVIDEND, basis)),
            ql.YieldTermStructureHandle(ql.FlatForward(today, rate, basis)),
            ql.BlackVolTermStructureHandle(
                ql.BlackConstantVol(today, calendar, vol, basis)
            ),
        )
        payoff, exercise = (
            ql.PlainVanillaPayoff(ql.Option.Call, inputs.STRIKE),
            ql.EuropeanExercise(maturity),
        )
        settings = dict(timeSteps=case["steps"], requiredSamples=case["size"], seed=42)
        if case["kind"] == "barrier":
            option = ql.BarrierOption(
                ql.Barrier.UpOut, inputs.BARRIER, 0.0, payoff, exercise
            )
            engine = ql.MCBarrierEngine(
                process, "lowdiscrepancy", isBiased=True, **settings
            )
        else:
            option = ql.VanillaOption(payoff, exercise)
            engine = ql.MCEuropeanEngine(process, "lowdiscrepancy", **settings)
        option.setPricingEngine(engine)
        return option.NPV()

    if case["operation"] == "mc_greeks":
        return lambda: inputs.bumped_values(price)
    return lambda: [price(inputs.SPOT, inputs.VOL, inputs.RATE)]
