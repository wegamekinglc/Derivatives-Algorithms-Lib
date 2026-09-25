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
print("\n" + "=" * 70)
print("  European call Monte Carlo results")
print("=" * 70 + "\n")
print(f"{'Result':<18}{'Value':>14}")
print("-" * 32)
for k, v in res.items():
    print(f"{k:<18}{v:>14.4f}")
print("-" * 32)
