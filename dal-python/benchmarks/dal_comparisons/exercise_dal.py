"""DAL LSM with explicit, disjoint training and valuation path budgets."""

from datetime import timedelta

import dal

from . import exercise_scenarios as inputs
from .option_dal import date


def runner(case):
    dates = [
        date(inputs.TODAY + timedelta(days=day))
        for day in inputs.exercise_days(case["kind"])
    ]
    events = [f"EXERCISE MAX({inputs.STRIKE} - spot(), 0.0)"] * len(dates)
    valuation = dal.ScriptValuationSettings_(evaluation_date=date(inputs.TODAY))

    def price(spot, vol, rate):
        result = dal.MonteCarlo_ValueWithSettings(
            dal.Product_New(dates, events),
            dal.BSModelData_New(spot, vol, rate, inputs.DIVIDEND),
            case["size"],
            valuation=valuation,
            simulation=dal.MonteCarloSettings_(
                method="sobol",
                use_bb=False,
                compiled=True,
                enable_aad=False,
                lsmc_basis_degree=3,
                lsmc_training_paths=case["training_paths"],
            ),
        )
        return result["PV"]

    def run():
        if case["operation"] == "mc_price":
            return [price(inputs.SPOT, inputs.VOL, inputs.RATE)]
        return inputs.bumped_values(price)

    return run
