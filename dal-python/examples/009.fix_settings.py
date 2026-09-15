import math
import dal

D = dal.Date_(2026, 9, 12)
H = dal.Date_(2026, 9, 11)
P = dal.Date_(2026, 9, 22)
index = "EQ[DAL196_TEST]"

snapshot = dal.MarketFixingSnapshot_New(
    {
        index: {dal.DateTime_(H, 0): 80.0},
    }
)
model = dal.BSModelData_New(spot=100.0, vol=0.0, rate=0.0, div=0.0)
product = dal.Product_New(
    ["SCALE", H, P],
    [
        "2.0",
        "x = SCALE * FIX(EQ[DAL196_TEST])",
        "pay PAYS x + FIX(EQ[DAL196_TEST], 2026-09-15)",
    ],
    settings=dal.ScriptProductSettings_(default_index=index),
)
valuation = dal.ScriptValuationSettings_(
    evaluation_date=D,
    today_fixing=dal.TodayFixingPolicy_.MODEL,
    model_bindings={"spot": index},
    fixings=snapshot,
)
result = dal.MonteCarlo_ValueWithSettings(
    product,
    model,
    4096,
    valuation=valuation,
    simulation=dal.MonteCarloSettings_(enable_aad=True, compiled=True),
)
assert math.isclose(result["PV"], 260.0, rel_tol=0.0, abs_tol=2.6e-10)
assert math.isclose(result["d_SCALE"], 80.0, rel_tol=0.0, abs_tol=1e-10)
assert all(key == "PV" or key.startswith("d_") for key in result)
description = dal.Product_Describe(product)
explanation = dal.ScriptValuation_Explain(product, model, valuation=valuation)
assert description["schema"] == "dal.script-product/2"
assert explanation["schema"] == "dal.script-valuation/1"
print(result)
