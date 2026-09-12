"""Native DAL scripted Monte Carlo with explicit path and Greek contracts."""

from datetime import timedelta

import dal

from . import option_scenarios as inputs


def date(value):
    return dal.Date_(value.year, value.month, value.day)


def runner(case):
    dal.EvaluationDate_Set(date(inputs.TODAY))
    dates, events = ["STRIKE"], [str(inputs.STRIKE)]
    if case["kind"] == "barrier":
        dates += ["BARRIER", date(inputs.TODAY)]
        events += [str(inputs.BARRIER), "alive = 1"]
        for step in range(1, 53):
            dates.append(date(inputs.TODAY + timedelta(days=7 * step)))
            events.append("if spot() >= BARRIER then alive = 0 end")
        events[-1] += " call pays alive * MAX(spot() - STRIKE, 0.0)"
    else:
        dates.append(date(inputs.MATURITY))
        events.append("call pays MAX(spot() - STRIKE, 0.0)")

    def value(spot, vol, rate, aad=False):
        return dal.MonteCarlo_Value(
            dal.Product_New(dates, events),
            dal.BSModelData_New(spot, vol, rate, inputs.DIVIDEND),
            case["size"],
            "sobol",
            False,
            aad,
            0.01,
            True,
        )

    def run():
        if case["operation"] == "mc_price":
            return [value(inputs.SPOT, inputs.VOL, inputs.RATE)["PV"]]
        if case["kind"] == "vanilla":
            result = value(inputs.SPOT, inputs.VOL, inputs.RATE, True)
            return [result[name] for name in ("PV", "d_spot", "d_vol", "d_rate")]
        return inputs.bumped_values(lambda s, v, r: value(s, v, r)["PV"])

    return run
