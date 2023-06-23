from dal import *
import numpy as np

start = Date_(2022, 9, 15)
maturity = Date_(2025, 9, 15)
EvaluationDate_Set(start)

spot = 100.0
vol = 0.15
rate = 0.00
div = 0.00
strike = 120.0
barrier = 150.0

n = int(input("Plz input # of paths (power of 2):"))
n_paths = 2 ** n
use_bb = False
rsg = "sobol"
model_name = input("Plz input model name:")

event_dates = ["STRIKE", "BARRIER", start]
events = [f"{strike:.2f}", f"{barrier:.2f}", "alive = 1"]
event_dates.append(f"START: {start} END: {maturity} FREQ: 1W")
events.append("if spot() >= BARRIER:0.1 then alive = 0 end")
event_dates.append(maturity)
events.append(f"if spot() >= BARRIER:0.1 then alive = 0 end\ncall pays alive * MAX(spot() - STRIKE, 0.0)")
product = Product_New(event_dates, events)

if model_name == "bs":
    model = BSModelData_New(spot, vol, rate, div)
elif model_name == "dupire":
    times = list(np.linspace(0.0, 5.0, 61))
    spots = list(np.linspace(50., 200.0, 31))
    vols = DoubleMatrix_(len(spots), len(times), vol)
    model = DupireModelData_New(spot, rate, div, spots, times, vols)
else:
    raise ValueError(f"{model_name} is not a supported model type")

res = MonteCarlo_Value(product, model, n_paths, rsg, use_bb, True)
vega = 0.0
for k, v in res.items():
    if k.startswith("d_lvol"):
        vega += v
    else:
        print(f"{k:<8}: {v:>10.4f}")
if vega != 0:
    print(f"{'d_vol':<8}: {vega:>10.4f}")
