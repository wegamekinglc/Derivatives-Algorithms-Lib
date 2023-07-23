from dal import *

today = Date_(2022, 9, 15)
EvaluationDate_Set(today)

spot = 100.0
vol = 0.15
rate = 0.05
div = 0.03
strike = 120.0
maturity = Date_(2025, 9, 15)

n_paths = 2 ** 20
use_bb = False
rsg = "sobol"

event_dates = ["STRIKE", maturity]
events = [f"{strike}", f"call pays MAX(spot() - STRIKE, 0.0)"]

product = Product_New(event_dates, events)
model = BSModelData_New(spot, vol, rate, div)

res = MonteCarlo_Value(product, model, n_paths, rsg, False, True)
vega = 0.0
for k, v in res.items():
    print(f"{k:<8}: {v:>10.4f}")
