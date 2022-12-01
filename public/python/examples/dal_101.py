from dal import *
import datetime as dt
from datetime import timedelta


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

barrier = 150.0
strike = 120.0
smooth = 0.5

event_dates.append(to_dal_date(dates[0]))
events.append("alive = 1")

for d in dates[1:]:
    event_dates.append(to_dal_date(d))
    events.append(f"if spot() >= {barrier}:{smooth} then alive = 0 endif")

event_dates.append(to_dal_date(dates[-1]))
events.append(f"uoc pays alive * MAX(spot() - {strike}, 0.0)")

product = Product_New(event_dates, events)
model = BSModelData_New(100, 0.15, 0.0, 0.0)
res = MonteCarlo_Value(product, model, 2 ** 20, "sobol", False, True, 1.0)

for k, v in res.items():
    print(f"{k}: {v}")

print(f"# of objects: {Repository_Size()}")