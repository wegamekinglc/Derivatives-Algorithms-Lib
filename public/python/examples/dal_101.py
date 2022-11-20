from dal import *

EvaluationDate_Set(Date_(2022, 9, 25))
d = Date_(2025, 9, 25)
dates = [d]
events = ["call pays MAX(spot() - 120, 0.0)"]
product = Product_New(dates, events)
model = BSModelData_New(100, 0.15, 0.0, 0.0)
print(MonteCarlo_Value(product, model, 2 ** 20)["value"])