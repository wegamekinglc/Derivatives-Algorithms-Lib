from dal import *
import datetime as dt
from datetime import timedelta
import numpy as np


def to_dal_date(date: dt.date):
    return Date_(date.year, date.month, date.day)


EvaluationDate_Set(Date_(2022, 9, 25))
start_date = dt.date(2022, 9, 25)
maturity = dt.date(2025, 9, 25)

current_date = start_date
dates = []
while current_date < maturity:
    dates.append(current_date)
    current_date = current_date + timedelta(weeks=1)
dates.append(maturity)

event_dates = []
events = []

spot = 100.0
barrier = 150.0
strike = 120.0
smooth = 0.5
vol = 0.15
num_paths = 2 ** 18

event_dates.append(to_dal_date(dates[0]))
events.append("alive = 1")

for d in dates[1:]:
    event_dates.append(to_dal_date(d))
    events.append(f"if spot() >= {barrier}:{smooth} then alive = 0 endif")

event_dates.append(to_dal_date(dates[-1]))
events.append(f"uoc pays alive * MAX(spot() - {strike}, 0.0)")

product = Product_New(event_dates, events)

print(f"################# using black - scholes model ##################")
model = BSModelData_New(spot, vol, 0.0, 0.0)
res = MonteCarlo_Value(product, model, num_paths, "sobol", False, True, 1.0)

for k, v in res.items():
    print(f"{k}: {v}")

print(f"\n################# using dupire local volatility model ##################")
# using dupire local volatility model
spots = list(np.linspace(100.0, 250.0, 31))
times = list(np.linspace(0.0, 5.0, 61))
vols = DoubleMatrix_(len(spots), len(times), vol)

model = DupireModelData_New(spot, spots, times, vols)

res = MonteCarlo_Value(product, model, num_paths, "sobol", False, True, 1.0)
for k, v in res.items():
    print(f"{k}: {v}")
