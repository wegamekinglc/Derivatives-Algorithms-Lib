import math

import dal
from float_format import format_float


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


today = dal.Date_(2026, 11, 16)
terms, quotes = [], []
for i in range(1, 9):
    days = 365 * i // 8
    t = days / 365
    expiry = today.AddDays(days)
    forward = 100 * math.exp(.02 * t + .01 * t * t)
    terms.append(dal.MarketIvTerm_(expiry, forward, math.exp(-.015 * t - .01 * t * t)))
    if i % 2 == 0:
        for k in (-.15, 0., .15):
            theta = .04 * t
            phi = .4 / math.sqrt(theta * (1 + theta))
            w = .5 * theta * (1 - .3 * phi * k + math.sqrt((phi * k - .3)**2 + .91))
            quotes.append(dal.MarketIvQuote_(id=str(len(quotes)), expiry=expiry,
                          strike=forward * math.exp(k), option_type="CALL", quote_type="DECIMAL_IV", mid=math.sqrt(w / t)))
market = dal.MarketIvMarket_(today, 100., terms)
fit = dal.MarketIvFitResult_New(dal.MarketIvFitRequest_(market, quotes))
require(fit.is_success, fit.failure)
print("Calibration (decimal IV)")
print(f"{'Quote':<12}{'Market':>16}{'Fitted':>16}{'Residual':>16}")
print("-" * 60)
for quote, row in zip(quotes, fit.quotes):
    print(f"{quote.id:<12}{format_float(quote.mid):>16}{format_float(row.native_fitted):>16}{format_float(row.native_residual):>16}")
print(f"Fit status={fit.status} rho={format_float(fit.snapshot.rho)} "
      f"eta={format_float(fit.snapshot.eta)} certified={fit.snapshot.certificate.certified}")
grid = dal.MarketIvDupireResult_New(fit.snapshot, dal.MarketIvDupireRequest_(
    [100 * math.exp(-.7 + 1.4 * i / 20) for i in range(21)], [.01 + .99 * j / 12 for j in range(13)]))
require(grid.is_success, grid.failure)
settings = dal.MarketIvLocalVolModelSettings_("EQ[A]", "USD", "W_EQ", 1.25)
model = dal.MarketIvLocalVolModelData_New("market", grid, settings)
print(f"source={model.source_identity}\nmodel={model.model_identity}")
valuation = dal.ScriptValuationSettings_(evaluation_date=today)
simulation = dal.MonteCarloSettings_(method="mrg32")
print("Valuation (USD, 16384 paths; smoke tolerances)")
print(f"{'Product':<16}{'Method':<18}{'PV':>16}{'Reference':>16}{'Error':>16}")
print("-" * 82)
call = dal.Product_New([today.AddDays(365)], ["pay PAYS MAX(FIX(EQ[A]) - 100, 0)"])
reference = dal.MarketIvPointResult_New(fit.snapshot, dal.MarketIvQuery_(100., 1.)).call_price
for label, event, expected, tolerance in (
    ("Call", "pay PAYS MAX(FIX(EQ[A]) - 100, 0)", reference, .5),
    ("Forward claim", "pay PAYS FIX(EQ[A])", 100 * math.exp(.005), .8),
    ("Unit payment", "pay PAYS 1", math.exp(-.025), 2e-12),
):
    product = dal.Product_New([today.AddDays(365)], [event])
    pv = dal.MonteCarlo_ValueWithSettings(product, model, 16384, valuation=valuation, simulation=simulation)["PV"]
    require(math.isfinite(pv) and abs(pv - expected) < tolerance, f"{label} smoke check failed")
    print(f"{label:<16}{'Local-vol MC':<18}{format_float(pv):>16}{format_float(expected):>16}{format_float(pv - expected):>16}")
report = dal.MarketIvLocalVolModelData_Get_Execution(call, model, valuation=valuation, simulation=simulation)
require(report["per_path_market_queries"] == 0 and report["prepared_columns"] == report["internal_steps"], "Preparation check failed")
for warning in model.warnings:
    print(warning)
try:
    dal.MonteCarlo_ValueWithSettings(call, model, 16, valuation=valuation, simulation=dal.MonteCarloSettings_(enable_aad=True))
except dal.MarketIvLocalVolError as error:
    require(error.code == "DerivativeUnavailable", str(error))
    print(f"{error.code}: {error.field}")
else:
    raise RuntimeError("Expected passive-only rejection")
print("Market-grid checks passed")
