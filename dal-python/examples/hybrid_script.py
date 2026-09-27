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

today = dal.Date_(2026, 9, 27)
middle = dal.Date_(2027, 3, 27)
maturity = dal.Date_(2027, 9, 27)
discount_curve = dal.DiscountLogDF_New("usd_discount", "USD", [today, middle, maturity], [0.0, -0.015, -0.06])
term_components = components.copy()
term_components[2] = dal.HybridLogDfRateDataFromCurve_New("RATE_CURVE", discount_curve, today, [today, middle, maturity])
term_model = dal.HybridModelData_New("two-equity-term-rate", "USD", term_components, provider)
term_product = dal.Product_New([maturity], ["pay PAYS FIX(EQ[A]) + 25"])
term_result = dal.MonteCarlo_ValueWithSettings(term_product, term_model, 16384, simulation=simulation)
print(f"Term rate PV={term_result['PV']:.4f}, d_logdf:USD:2={term_result['d_logdf:USD:2']:.4f}")

exercise_dates = [dal.Date_(2027, 3, 27), dal.Date_(2027, 9, 27)]
exercise_events = [
    "a = FIX(EQ[A])\nb = FIX(EQ[B])\nEXERCISE MAX(a - b + 60, 0)",
    "EXERCISE MAX(b - a + 60, 0)",
]
one_state = dal.Product_New(
    exercise_dates, exercise_events,
    settings=dal.ScriptProductSettings_(regression_features=["VAR[a]"]),
)
bermudan = dal.Product_New(
    exercise_dates, exercise_events,
    settings=dal.ScriptProductSettings_(regression_features=["VAR[a]", "VAR[b]"]),
)
simulation = dal.MonteCarloSettings_(
    use_bb=True, enable_aad=True, compiled=True,
    lsmc_training_paths=8192, lsmc_validation_paths=2048,
)
hard_simulation = dal.MonteCarloSettings_(
    use_bb=True, compiled=True,
    lsmc_training_paths=8192, lsmc_validation_paths=2048,
)
one_state_pv = dal.MonteCarlo_ValueWithSettings(one_state, model, 32768, simulation=hard_simulation)["PV"]
exercise = dal.MonteCarlo_ValueWithSettings(bermudan, model, 32768, simulation=simulation)
time = (dal.Date_(2027, 3, 27) - dal.Date_(2026, 9, 27)) / 365.0
width = math.sqrt(0.20**2 + 0.30**2 - 2 * 0.35 * 0.20 * 0.30) * math.sqrt(time)
d1 = (math.log(120.0 / 100.0) + 0.5 * width**2) / width


def cdf(value):
    return 0.5 * math.erfc(-value / math.sqrt(2.0))


reference = 60.0 + 100.0 - 120.0 + 2.0 * (120.0 * cdf(d1) - 100.0 * cdf(d1 - width))
print(
    f"Bermudan reference={reference:.4f}, one-state PV={one_state_pv:.4f}, "
    f"two-state PV={exercise['PV']:.4f}, "
    f"d_spot:EQ[A]={exercise['d_spot:EQ[A]']:.4f}, "
    f"d_spot:EQ[B]={exercise['d_spot:EQ[B]']:.4f}"
)
