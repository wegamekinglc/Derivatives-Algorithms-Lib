"""QuantLib native LSM on exactly the DAL exercise grid, with held-out pricing."""

import QuantLib as ql

from . import exercise_scenarios as inputs


def runner(case):
    today = ql.Date(inputs.TODAY.day, inputs.TODAY.month, inputs.TODAY.year)
    ql.Settings.instance().evaluationDate = today
    day_count, calendar = ql.Actual365Fixed(), ql.NullCalendar()
    days = inputs.exercise_days(case["kind"])

    def price(spot, vol, rate):
        process = ql.BlackScholesMertonProcess(
            ql.QuoteHandle(ql.SimpleQuote(spot)),
            ql.YieldTermStructureHandle(
                ql.FlatForward(today, inputs.DIVIDEND, day_count)
            ),
            ql.YieldTermStructureHandle(ql.FlatForward(today, rate, day_count)),
            ql.BlackVolTermStructureHandle(
                ql.BlackConstantVol(today, calendar, vol, day_count)
            ),
        )
        exercise = (
            ql.BermudanExercise([today + day for day in days])
            if case["kind"] == "bermudan"
            else ql.AmericanExercise(today, today + days[-1])
        )
        option = ql.VanillaOption(
            ql.PlainVanillaPayoff(ql.Option.Put, inputs.STRIKE), exercise
        )
        # MCAmericanEngine permits every grid time: do not add intermediate
        # steps to the Bermudan grid. Equal spacing makes this grid exact.
        option.setPricingEngine(
            ql.MCAmericanEngine(
                process,
                "pseudorandom",
                timeSteps=len(days),
                requiredSamples=case["size"],
                nCalibrationSamples=case["training_paths"],
                seed=42,
                seedCalibration=43,
                polynomOrder=3,
                polynomType=ql.LsmBasisSystem.Monomial,
                antitheticVariate=False,
                antitheticVariateCalibration=False,
                controlVariate=False,
            )
        )
        return option.NPV()

    def run():
        if case["operation"] == "mc_price":
            return [price(inputs.SPOT, inputs.VOL, inputs.RATE)]
        return inputs.bumped_values(price)

    return run
