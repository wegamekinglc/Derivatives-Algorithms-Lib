import dal

EVAL = dal.Date_(2026, 9, 20)
MID = dal.Date_(2027, 9, 20)
MATURITY = dal.Date_(2028, 3, 20)
SPOT, VOL, RATE, DIV, STRIKE = 100.0, 0.20, 0.05, 0.0, 100.0

dal.EvaluationDate_Set(EVAL)
model = dal.BSModelData_New(SPOT, VOL, RATE, DIV)

# A put exercisable on the listed dates only: EXERCISE-only products, no PAYS.
exercise = f"EXERCISE MAX({STRIKE} - spot(), 0.0)"
european = dal.Product_New([MATURITY], [exercise])  # exercise at maturity only
bermudan = dal.Product_New([MID, MATURITY], [exercise, exercise])
weekly_dates = [EVAL.AddDays(7 * week) for week in range(1, 79)]
american = dal.Product_New(weekly_dates, [exercise] * len(weekly_dates))

simulation = dal.MonteCarloSettings_(lsmc_basis_degree=3)


def value(product):
    return dal.MonteCarlo_ValueWithSettings(product, model, 2**18, simulation=simulation)["PV"]


european_pv = value(european)
bermudan_pv = value(bermudan)
american_pv = value(american)

# The weekly grid and the two-date set are not nested, so their ordering is not
# a theorem — with these fixed seeds the gap is ~0.25; the guards catch gross
# regressions. The weekly premium over the maturity-only put clears the
# half-a-percent-of-spot bar of the PDE benchmark.
if not european_pv <= bermudan_pv + 1e-3:
    raise RuntimeError(f"two-date Bermudan {bermudan_pv} below the maturity-only put {european_pv}")
if not bermudan_pv <= american_pv + 1e-3:
    raise RuntimeError(f"weekly-exercise put {american_pv} below the two-date Bermudan {bermudan_pv}")
if not american_pv - european_pv > 0.005 * SPOT:
    raise RuntimeError(f"early-exercise premium {american_pv - european_pv} under the benchmark floor")

# The simulation diagnostic runs the full valuation and reports per-date
# regression and exercise statistics (dal.script-simulation/1).
diagnostic = dal.ScriptSimulation_Explain(bermudan, model, 4096)
if diagnostic["schema"] != "dal.script-simulation/1":
    raise RuntimeError(f"unexpected schema {diagnostic['schema']!r}")
if diagnostic["n_paths"] != 4096 or len(diagnostic["exercise_events"]) != 2:
    raise RuntimeError(f"unexpected diagnostic shape: {diagnostic!r}")
rates = [event["exercise_rate"] for event in diagnostic["exercise_events"]]
if not all(0.0 < rate <= 1.0 for rate in rates):
    raise RuntimeError(f"exercise rates out of range: {rates!r}")

print(f"European put (exercise at maturity only): {european_pv:.4f}")
print(f"Bermudan put, 2 exercise dates, 2^18 paths: {bermudan_pv:.4f}")
print(f"American put, weekly exercise (78 dates): {american_pv:.4f}")
print(f"Early-exercise premium (weekly - European): {american_pv - european_pv:.4f}")
print(f"Exercise rates per date (4096 paths): {[round(rate, 4) for rate in rates]}")
