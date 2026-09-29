#!/usr/bin/env python3
"""Price one equity claim with BS/local vol and GSR/local vol in Hybrid."""

import dal

today = dal.Date_(2026, 9, 28)
expiry = dal.Date_(2027, 9, 28)
maturity = dal.Date_(2028, 9, 28)
dal.EvaluationDate_Set(today)

# Grid rows are spot nodes; columns are ACT/365 times.
surface = dal.LocalVolSurfaceData_New(
    "equity_vol",
    [80.0, 100.0, 120.0],
    [0.0, 1.0],
    dal.DoubleMatrix_([[0.25, 0.23], [0.20, 0.19], [0.18, 0.17]]),
)
surface = dal._dal._StorableFromJson(dal._dal._StorableToJson(surface))

bs = dal.BSModelData_New(100.0, 0.20, 0.03, 0.01)
bs_local_vol = dal.BSLocalVolModelData_New(
    "bs_local_vol", "EQ[A]", "USD", "W_EQ", bs, surface
)

curve = dal.GSRCurveData_New(
    "curve",
    today,
    "USD",
    [today, expiry, maturity],
    [0.0, -0.03, -0.06],
    [],
    dal.DoubleMatrix_(0, 0),
)
rate_vol = dal.GSRVolData_New("rate_vol", [today], [0.02], [today], [1.0])
equity = dal.HybridLocalVolEquityData_New(
    "equity", "EQ[A]", "USD", "W_EQ", 100.0, 0.01, surface
)
rate = dal.HybridGSRRateData_New("rate", "W_RATE", curve, rate_vol)
correlation = dal.HybridConstantCorrelationData_New(
    "corr", ["W_EQ", "W_RATE"], dal.DoubleMatrix_([[1.0, 0.30], [0.30, 1.0]])
)
gsr_local_vol = dal.HybridModelData_New(
    "gsr_local_vol", "USD", [equity, rate], correlation
)

equity_claim = dal.Product_New(
    [expiry],
    ["pay PAYS MAX(FIX(EQ[A]) - 100, 0)"],
)
bond_claim = dal.Product_New([expiry], ["pay PAYS FIX(IR[USD,DF,2028-09-28])"])
print("| Model | Product | PV |")
print("|---|---|---:|")
for name, model in [
    ("BS + Local vol", bs_local_vol),
    ("GSR + Local vol", gsr_local_vol),
]:
    pv = dal.MonteCarlo_ValueWithSettings(equity_claim, model, 4096)["PV"]
    print(f"| {name} | Equity call | {pv:.6f} |")
pv = dal.MonteCarlo_ValueWithSettings(bond_claim, gsr_local_vol, 4096)["PV"]
print(f"| GSR + Local vol | Bond | {pv:.6f} |")
