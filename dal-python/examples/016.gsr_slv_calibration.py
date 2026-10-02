import math

import dal

from float_format import format_float


TODAY = dal.Date_(2026, 10, 3)
GAUSSIAN_SCALE = 0.0001
PRICE_SCALE = 0.0005
VOL_EXPIRIES = [(365, "1Y"), (547, "18M"), (730, "2Y"), (912, "30M"), (1095, "3Y")]
CAPLET_TENORS = [(91, "3M"), (182, "6M"), (365, "12M")]
SMILE_VOLS = [
    [0.01120, 0.01050, 0.01005],
    [0.01155, 0.01105, 0.01075],
    [0.01175, 0.01140, 0.01105],
    [0.01215, 0.01175, 0.01140],
    [0.01250, 0.01210, 0.01175],
]


def diagnostic_value(value):
    return format_float(0.0 if abs(value) < 1e-12 else value)


def print_row(row, widths):
    print("".join(f"{value:<{width}}" if i == 0 else f"{value:>{width}}"
                  for i, (value, width) in enumerate(zip(row, widths))))


def print_table(title, headers, rows):
    rows = [[str(value) for value in row] for row in rows]
    widths = [max(len(header), *(len(row[i]) for row in rows)) + 2 for i, header in enumerate(headers)]
    line = "-" * sum(widths)
    heading = "=" * max(70, sum(widths), len(title))
    print("\n" + heading)
    print(title)
    print(heading)
    print_row(headers, widths)
    print(line)
    for row in rows:
        print_row(row, widths)
    print(line)


def curve_instruments(knots, rates):
    fixed = dal.RateLegConvention_New(dal.PeriodLength_("12M"), dal.DayBasis_("ACT_365F"))
    floating = dal.RateLegConvention_New(dal.PeriodLength_("12M"), dal.DayBasis_("ACT_360"))
    index = dal.RateIndexConvention_New(dal.PeriodLength_("1M"), dal.DayBasis_("ACT_360"), dal.CollateralType_OIS())
    return [
        dal.Deposit_New(TODAY, TODAY, date, rate, index) if i < 3
        else dal.OISSwap_New(TODAY, TODAY, date, rate, fixed, index, floating)
        for i, (date, rate) in enumerate(zip(knots, rates))
    ]


def curve_snapshot(discount, knots):
    nodes = [TODAY, *knots]
    for days, _ in VOL_EXPIRIES:
        expiry = TODAY.AddDays(days)
        nodes.extend([expiry, *(expiry.AddDays(day) for day, _ in CAPLET_TENORS)])
    nodes = sorted({str(date): date for date in nodes}.values())
    source = dal.CurveBlock_New(discount)
    return dal.GSRCurveDataFromYieldCurve_New("gsr_curve", source, TODAY, nodes, [])


def calibrate_curve():
    days = [30, 90, 180, 365, 730, 1095, 1825, 2555, 3650]
    rates = [0.0270, 0.0280, 0.0290, 0.0302, 0.0310, 0.0315, 0.0322, 0.0325, 0.0328]
    knots = [TODAY.AddDays(day) for day in days]
    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = TODAY
    builder.ccy_ = dal.String_("USD")
    builder.curveName_ = dal.String_("calibrated_ois")
    builder.instruments_ = curve_instruments(knots, rates)
    builder.knotDates_ = knots
    spec = builder.Build()
    if not dal.ValidateSingleCurveAnalyticEligibility(spec).eligible:
        raise RuntimeError("Yield curve instruments must support the AAD Jacobian")
    fit = dal.CalibrateSingleCurve(spec, dal.CurveJacobianMode.ANALYTIC)
    diagnostics = fit.diagnostics_
    if diagnostics.maxAbsResidual_ > builder.fitTolerance_:
        raise RuntimeError("Yield curve calibration exceeded the quote tolerance")
    print_table("Yield curve calibration: deposits and OIS; AAD Jacobian",
                ["Instrument", "Market(%)", "Model(%)", "Error(bp)", "Fitted DF"],
                [[f"{'Deposit' if i < 3 else 'OIS'} {date}", format_float(market * 100), format_float(model * 100),
                  format_float(error * 10000), format_float(fit.curve_(TODAY, date))]
                 for i, (date, market, model, error) in enumerate(zip(knots, diagnostics.marketRates_,
                                                                    diagnostics.modelRates_, diagnostics.residuals_))])
    return fit.curve_, curve_snapshot(fit.curve_, knots)


def forward(curve, expiry, days):
    accrual = days / 365.0
    return (curve(TODAY, expiry) / curve(TODAY, expiry.AddDays(days)) - 1.0) / accrual


def caplet(curve, expiry_days, offset=0.0, days=182, tenor="6M"):
    expiry = TODAY.AddDays(expiry_days)
    end = expiry.AddDays(days)
    accrual = days / 365.0
    return dal.GSRCaplet_(expiry, expiry, end, end, accrual, accrual, tenor,
                          forward(curve, expiry, days) + offset)


def gaussian_model(curve):
    vol = dal.MultiFactorGSRVolData_New(
        "gaussian_vol", ["level", "slope", "curvature"], [TODAY.AddDays(365 * i) for i in range(3)],
        dal.DoubleMatrix_([[0.009] * 3, [0.003] * 3, [0.002] * 3]),
        [TODAY, TODAY.AddDays(365), TODAY.AddDays(1095)],
        dal.DoubleMatrix_([[1.0, 1.0, 1.0], [0.4, 0.3, 0.2], [0.1, 0.2, 0.1]]),
        dal.DoubleMatrix_([[1.0, 0.25, 0.1], [0.25, 1.0, 0.15], [0.1, 0.15, 1.0]]),
    )
    return dal.MultiFactorGSRModelData_New("gaussian", curve, vol)


def report_diagnostics(title, fit, held_out=False):
    rows = [["Converged", str(fit.converged).lower()],
            ["Price fit passed", str(fit.fit_within_tolerance).lower()],
            ["Numerical validation passed", str(fit.numerical_validation_passed).lower()],
            ["Jacobian rank", fit.jacobian_rank], ["Iterations", fit.iterations]]
    if held_out:
        rows.append(["Held-out passed", str(fit.held_out_within_tolerance).lower()])
    print_table(title, ["Diagnostic", "Result"], rows)
    if not (fit.converged and fit.fit_within_tolerance and fit.numerical_validation_passed):
        raise RuntimeError(f"Calibration diagnostics failed: {fit.termination_reason}")
    if held_out and not fit.held_out_within_tolerance:
        raise RuntimeError("Held-out quote validation failed")


def calibrate_gaussian(discount, snapshot):
    inputs = [
        (f"{label} {tenor} ATM", caplet(discount, expiry_days, days=days, tenor=tenor), smile[1])
        for (expiry_days, label), smile in zip(VOL_EXPIRIES, SMILE_VOLS)
        for days, tenor in CAPLET_TENORS
    ]
    market = [dal.GSRMarketQuote_(name, option, vol, GAUSSIAN_SCALE) for name, option, vol in inputs]
    prices = dal.GSRMarketQuotes_Get_Prices(snapshot, market)
    quotes = [dal.GSRCalibrationQuote_(name, option, price.price, GAUSSIAN_SCALE)
              for (name, option, _), price in zip(inputs, prices)]
    parameters = [dal.GSRCalibrationParameter_(0, knot, 0.001, 0.04) for knot in range(3)]
    fit = dal.Calibrate_GSRVolatility(gaussian_model(snapshot), quotes, parameters)
    print_table("Gaussian volatility calibration: 15 ATM caplets",
                ["Instrument", "Normal(bp)", "Market PV", "Fitted PV", "Error/scale"],
                [[name, format_float(vol * 10000), format_float(price.price), format_float(value),
                  format_float(error / GAUSSIAN_SCALE)]
                 for (name, _, vol), price, value, error in zip(inputs, prices, fit.model_prices, fit.residuals)])
    print_table("Calibrated Gaussian parameters; slope, curvature, H and correlations fixed",
                ["Parameter", "Initial", "Fitted"],
                [[f"g:level:{TODAY.AddDays(365 * knot)}", format_float(0.009), format_float(value)]
                 for knot, value in enumerate(fit.parameters)])
    report_diagnostics("Gaussian calibration diagnostics", fit)
    return fit.model


def calibrate_slv(discount, snapshot, gaussian):
    model_settings = dal.GSRSLVSettings_()
    model_settings.kappa = 1.0
    model_settings.vol_of_vol = 0.5
    model_settings.variance_correlations = [-0.2, 0.0, 0.0]
    model_settings.max_step = 1.0 / 12.0
    leverage = dal.GSRLeverageData_New(
        "leverage", [-0.02, 0.0, 0.02], [0.0], dal.DoubleMatrix_([[1.0], [1.0], [1.0]]),
    )
    initial = dal.GSRSLVModelData_New("initial_slv", gaussian, leverage, model_settings)
    settings = dal.GSRSLVCalibrationSettings_()
    settings.pricing.paths = 16384
    settings.pricing.seed = 1729
    settings.validation.paths = 32768
    settings.validation.seed = 81173
    settings.solver.prior_weight = 0.01
    settings.solver.smoothing_weight = 0.0001
    parameters = [dal.GSRSLVCalibrationParameter_(f"leverage:{row}:0", 0.2, 2.0) for row in range(3)]
    inputs = [(f"{label} {offset:+.3f}", caplet(discount, expiry_days, offset), vol)
              for (expiry_days, label), row in zip(VOL_EXPIRIES, SMILE_VOLS)
              for offset, vol in zip([-0.005, 0.0, 0.005], row)]
    quotes = [dal.GSRMarketQuote_(name, option, vol, PRICE_SCALE) for name, option, vol in inputs]
    held_out = [dal.GSRMarketQuote_("2Y +0.0025 held-out", caplet(discount, 730, 0.0025), 0.01120, PRICE_SCALE)]
    fit = dal.Calibrate_GSRSLVMarket(initial, quotes, parameters, settings, held_out=held_out)
    prices = dal.GSRMarketQuotes_Get_Prices(snapshot, quotes)
    print_table("SLV volatility calibration: 15 smile caplets",
                ["Instrument", "Normal(bp)", "Market PV", "Fitted PV", "Error/scale", "Pair SE"],
                [[name, format_float(vol * 10000), format_float(price.price), format_float(value),
                  format_float(error / PRICE_SCALE), format_float(se)]
                 for (name, _, vol), price, value, error, se
                 in zip(inputs, prices, fit.model_prices, fit.residuals, fit.standard_errors)])
    print_table("Calibrated SLV parameters; Gaussian, CIR and correlations fixed",
                ["Parameter", "Initial", "Fitted"],
                [[f"leverage:{row}:0", format_float(1.0), format_float(value)]
                 for row, value in enumerate(fit.parameters)])
    report_diagnostics("SLV calibration diagnostics", fit, held_out=True)
    held_out_error = (abs(fit.held_out_residuals[0]) + settings.validation_sigma * fit.held_out_standard_errors[0]
                      + fit.held_out_conditional_errors[0])
    print_table("Independent validation",
                ["Result", "PV", "Error estimate", "PV budget"],
                [["Max numerical error", "N/A", format_float(max(fit.numerical_errors)),
                  format_float(settings.solver.numerical_error_fraction * PRICE_SCALE)],
                 ["Held-out 2Y +0.0025", format_float(fit.held_out_prices[0]),
                  format_float(held_out_error), format_float(PRICE_SCALE)]])
    return fit.model


def product_contracts(discount):
    expiry = TODAY.AddDays(365)
    first = expiry.AddDays(365)
    end = first.AddDays(365)
    strike = (discount(TODAY, expiry) - discount(TODAY, end)) / (discount(TODAY, first) + discount(TODAY, end))
    swaption = dal.GSRSwaption_(
        expiry, [dal.GSRFixedCoupon_(first, 1.0), dal.GSRFixedCoupon_(end, 1.0)],
        [dal.GSRFloatingCoupon_(expiry, expiry, first, first, 1.0, 1.0, "12M"),
         dal.GSRFloatingCoupon_(first, first, end, end, 1.0, 1.0, "12M")], strike,
    )
    caplet_strike = forward(discount, expiry, 182)
    caplet_script = dal.Product_New(
        ["STRIKE", expiry], [repr(caplet_strike),
                            f"pay PAYS MAX(1 - (1 + {182 / 365.0!r} * STRIKE) * FIX(IR[USD,DF,{expiry.AddDays(182)}]), 0)"],
    )
    swaption_script = dal.Product_New(
        ["STRIKE", expiry], [repr(strike),
                            f"pay PAYS MAX(1 - FIX(IR[USD,DF,{end}]) - STRIKE * "
                            f"(FIX(IR[USD,DF,{first}]) + FIX(IR[USD,DF,{end}])), 0)"],
    )
    return expiry, [("1Y ATM caplet", caplet(discount, 365), caplet_script),
                    ("1Y into 2Y ATM payer", swaption, swaption_script)]


def value_script(name, product, model, price, paths):
    values = []
    for aad in [False, True]:
        execution = dal.MonteCarloSettings_(method="sobol", compiled=True, enable_aad=aad, use_bb=True, smooth=1e-8)
        values.append(dal.MonteCarlo_ValueWithSettings(
            product, model, paths,
            valuation=dal.ScriptValuationSettings_(evaluation_date=TODAY), simulation=execution))
    plain, adjoint = values
    if not math.isclose(plain["PV"], adjoint["PV"], rel_tol=0.0, abs_tol=1e-10):
        raise RuntimeError(f"AAD and plain PV differ for {name}")
    if abs(adjoint["PV"] - price.price) > 5 * price.standard_error + 0.0001:
        raise RuntimeError(f"Script and European pricer differ for {name}")
    if not all(math.isfinite(value) for value in adjoint.values()):
        raise RuntimeError(f"Non-finite AAD result for {name}")
    return plain["PV"], adjoint


def validate_risks(risks, expiry):
    required = [f"d_g:{factor}:{TODAY}" for factor in ["level", "slope", "curvature"]]
    required.extend([f"d_logdf:OIS:{expiry}", "d_kappa", "d_volOfVol", "d_leverage:1:0", "d_STRIKE"])
    if any(key not in risks[0] or abs(risks[0][key]) < 1e-12 for key in required):
        raise RuntimeError("Expected model-input AAD risks are missing")


def report_risks(risks, expiry):
    validate_risks(risks, expiry)
    labels = sorted(key for key in risks[0] if key != "PV" and max(abs(risk[key]) for risk in risks) >= 1e-12)
    print_table("AAD input derivatives; calibration held fixed; active inputs",
                ["Risk", "1Y ATM caplet", "1Y into 2Y payer"],
                [[label, diagnostic_value(risks[0][label]), diagnostic_value(risks[1][label])] for label in labels])


def price_products(discount, model):
    expiry, contracts = product_contracts(discount)
    settings = dal.GSRMonteCarloSettings_()
    settings.paths = 16384
    settings.seed = 27183
    prices = dal.GSRSLV_EuropeanOptionPrices(model, [option for _, option, _ in contracts], settings)
    rows, risks = [], []
    for (name, _, product), price in zip(contracts, prices):
        plain, adjoint = value_script(name, product, model, price, settings.paths)
        rows.append([name, format_float(price.price), format_float(price.standard_error),
                     format_float(plain), format_float(adjoint["PV"]), diagnostic_value(adjoint["PV"] - plain)])
        risks.append(adjoint)
    print_table("Product values: unit notional; identical contracts",
                ["Product", "MRG32 PV", "Pair SE", "Sobol PV", "AAD PV", "AAD-plain"], rows)
    report_risks(risks, expiry)


def main():
    dal.EvaluationDate_Set(TODAY)
    print("Three-factor GSR + SLV: calibrated yield curve, illustrative Normal volatility quotes")
    discount, snapshot = calibrate_curve()
    gaussian = calibrate_gaussian(discount, snapshot)
    smile = calibrate_slv(discount, snapshot, gaussian)
    price_products(discount, smile)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
