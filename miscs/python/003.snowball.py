import datetime as dt
import pandas as pd
import numpy as np
from dateutil.relativedelta import relativedelta
from dal import *

spot = 1.00
vol = 0.15
rate = 0.0
div = 0.0
ko = 1.00
ki = 0.88
coupon = 0.069
today = dt.date(2023, 3, 1)
maturity = dt.date(2025, 3, 1)

event_dates = []
while today <= maturity:
    event_dates.append(today)
    today = today + relativedelta(months=1)

event_dates = ["KI", "KO", "STRIKE", "COUPON"] + [Date_(d.year, d.month, d.day) for d in event_dates]
start = event_dates[4]
events = [f"{ki:.6f}", f"{ko:.6f}", f"{spot:.6f}", f"{coupon:.6f}", "alive = 1 is_ki = 0"]
EvaluationDate_Set(start)

n_paths = 500_000
use_bb = False
rsg = "sobol"
model_name = "bs"

for d in event_dates[5:]:
    if d <= Date_(2023, 6, 1):
        events.append("alive = 1")   # initial and 3m lock
    elif d < event_dates[-1]:
        events.append(
    f"""
    if spot() < KI:0.001 then is_ki = 1 endif
    if spot() > KO:0.001 then call pays alive * COUPON * DCF(ACT365F, {start}, {d}) alive = 0 endif
    """
        )
events.append(
    f"""
    if spot() < KI:0.001 then is_ki = 1 endif
    if spot() > KO:0.001 then call pays alive * COUPON * DCF(ACT365F, {start}, {d}) alive = 0 endif
    call pays alive * is_ki * (spot() - STRIKE)
    """
)

for d, e in zip(event_dates, events):
    print(d)
    print(e + "\n")

print("------   Model Parameters  ------")
print(f"rate : {rate* 100:.2f}%")
print(f"div  : {div * 100:.2f}%")
print(f"vol  : {vol * 100:.2f}%\n")

print("------ Product Description ------")
print(f"NPV date  : {event_dates[4]}")
print(f"Maturity  : {event_dates[-1]}")
print(f"knock in  : {ki:.2f}")
print(f"knock out : {ko:.2f}")
print(f"# of obs  : {len(event_dates)}")
print(f"# of paths: {n_paths}\n")


product = Product_New(event_dates, events)
model = BSModelData_New(spot, vol, rate, div)

print("------ Product Evaluation  ------")
now = dt.datetime.now()
res = MonteCarlo_Value(product, model, n_paths, rsg, use_bb, False)
all_res = {"Non-AAD": [res["value"], np.nan, np.nan, np.nan, np.nan, (dt.datetime.now() - now).total_seconds() * 1000]}

now = dt.datetime.now()
res = MonteCarlo_Value(product, model, n_paths, rsg, use_bb, True)
all_res["AAD"] = [res["value"], res["d_spot"], res["d_vol"], res["d_rate"], res["d_div"], (dt.datetime.now() - now).total_seconds() * 1000]

df = pd.DataFrame.from_dict(all_res)
df.index = ["NPV", "delta", "vega", "dP/dR", "dP/dDiv", "Elapsed (ms)"]
print(df.T.to_markdown())


