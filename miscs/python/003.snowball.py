import pprint
import datetime as dt
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

event_dates = [Date_(d.year, d.month, d.day) for d in event_dates]
EvaluationDate_Set(event_dates[0])

n_paths = 2 ** 20
use_bb = False
rsg = "sobol"
model_name = "bs"

events = ["alive = 1 ki = 0"]  # initial and 3m lock

for d in event_dates[1:]:
    if d <= Date_(2023, 6, 1):
        events.append("alive = 1")   # initial and 3m lock
    elif d < event_dates[-1]:
        events.append(
            f"""if spot() < {ki:.2f}:0.001 then ki = 1 endif\nif spot() > {ko:.2f}:0.001 then call pays alive * {coupon * (d - event_dates[0]) / 365.0:.4f} alive = 0 endif"""
        )
events.append(
    f"""if spot() < {ki:.2f}:0.001 then ki = 1 endif\nif spot() > {ko:.2f}:0.001 then call pays alive * {coupon * (d - event_dates[0]) / 365.0:.4f} alive = 0 endif\ncall pays alive * ki * (spot() - {spot:.4f})"""
)

for d, e in zip(event_dates, events):
    print(d)
    print(e + "\n")

product = Product_New(event_dates, events)
model = BSModelData_New(spot, vol, rate, div)

print("------   Model Parameters  ------")
print(f"rate : {rate* 100:.2f}%")
print(f"div  : {div * 100:.2f}%")
print(f"vol  : {vol * 100:.2f}%\n")

print("------ Product Description ------")
print(f"NPV date  : {event_dates[0]}")
print(f"Maturity  : {event_dates[-1]}")
print(f"knock in  : {ki:.2f}")
print(f"knock out : {ko:.2f}")
print(f"# of obs  : {len(event_dates)}")
print(f"# of paths: {n_paths}\n")

print("------ Product Evaluation  ------")
now = dt.datetime.now()
res = MonteCarlo_Value(product, model, n_paths, rsg, use_bb, False)
for k, v in res.items():
    print(f"{k:<8}: {v:>10.4f}")
print(f"Non-AAD w.t {model_name} using - {dt.datetime.now() - now}\n")

now = dt.datetime.now()
res = MonteCarlo_Value(product, model, n_paths, rsg, use_bb, True)
for k, v in res.items():
    print(f"{k:<8}: {v:>10.4f}")
print(f"AAD     w.t {model_name} using - {dt.datetime.now() - now}\n")
