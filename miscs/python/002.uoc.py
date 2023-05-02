from dal import *
import numpy as np

today = Date_(2022, 9, 15)
EvaluationDate_Set(today)

spot = 100.0
vol = 0.15
rate = 0.00
div = 0.00
strike = 120.0
barrier = 150.0
maturity = Date_(2025, 9, 14)

n = int(input("Plz input # of paths (power of 2):"))
n_paths = 2 ** n
use_bb = False
rsg = "sobol"
model_name = input("Plz input model name:")

start = today

event_dates = [start]
events = ["alive = 1"]

curr = start.AddDays(7)
while curr < maturity:
    event_dates.append(curr)
    events.append("if spot() >= BARRIER:0.1 then alive = 0 endif")
    curr = curr.AddDays(7)

event_dates.append(maturity)
events.append(f"if spot() >= BARRIER:0.1 then alive = 0 endif\ncall pays alive * MAX(spot() - STRIKE, 0.0)")

event_dates = [Cell_(d) for d in event_dates]
event_dates = [Cell_("STRIKE"), Cell_("BARRIER")] + event_dates
events = [f"{strike:.2f}", f"{barrier:.2f}"] + events
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
