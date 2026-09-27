"""Price a named two-equity payoff with the deterministic-rate hybrid model."""

import math

import dal


dal.EvaluationDate_Set(dal.Date_(2026, 9, 27))
correlation = dal.DoubleMatrix_([[1.0, 0.35], [0.35, 1.0]])
components = [
    dal.HybridBSEquityData_New("A", "EQ[A]", "USD", "FA", 100.0, 0.20, 0.0),
    dal.HybridBSEquityData_New("B", "EQ[B]", "USD", "FB", 120.0, 0.30, 0.0),
    dal.HybridDeterministicRateData_New("RATE", "USD", 0.0),
]
provider = dal.HybridConstantCorrelationData_New("joint", ["FA", "FB"], correlation)
model = dal.HybridModelData_New("two-equity", "USD", components, provider)
product = dal.Product_New([dal.Date_(2027, 9, 27)], ["pay PAYS FIX(EQ[A]) * FIX(EQ[B])"])
simulation = dal.MonteCarloSettings_(use_bb=True, enable_aad=True, compiled=True)
result = dal.MonteCarlo_ValueWithSettings(product, model, 16384, simulation=simulation)
if abs(result["PV"] - 12000.0 * math.exp(0.35 * 0.20 * 0.30)) >= 50.0:
    raise ValueError("hybrid cross-moment price exceeded its Monte Carlo tolerance")
print(f"PV={result['PV']:.4f}, d_spot:EQ[A]={result['d_spot:EQ[A]']:.4f}, d_spot:EQ[B]={result['d_spot:EQ[B]']:.4f}")
