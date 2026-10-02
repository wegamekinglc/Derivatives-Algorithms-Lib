import math

import dal

from float_format import format_float


TODAY = dal.Date_(2026, 10, 3)
INPUT_RATE = 0.03
PRICE_SCALE = 0.0005


def caplet(years, offset=0.0):
    expiry = TODAY.AddDays(365 * years)
    end = expiry.AddDays(182)
    accrual = 182.0 / 365.0
    forward = math.expm1(INPUT_RATE * accrual) / accrual
    return dal.GSRCaplet_(expiry, expiry, end, end, accrual, accrual, "6M", forward + offset)


def gaussian_model(curve):
    vol = dal.MultiFactorGSRVolData_New(
        "gaussian_vol", ["level", "slope", "curvature"], [TODAY, TODAY.AddDays(365)],
        dal.DoubleMatrix_([[0.009, 0.009], [0.003, 0.003], [0.002, 0.002]]),
        [TODAY, TODAY.AddDays(365), TODAY.AddDays(1095)],
        dal.DoubleMatrix_([[1.0, 1.0, 1.0], [0.4, 0.3, 0.2], [0.1, 0.2, 0.1]]),
        dal.DoubleMatrix_([[1.0, 0.25, 0.1], [0.25, 1.0, 0.15], [0.1, 0.15, 1.0]]),
    )
    return dal.MultiFactorGSRModelData_New("gaussian", curve, vol)


def calibrate_gaussian(curve):
    market = [
        dal.GSRMarketQuote_(f"{years}Y ATM", caplet(years), volatility, 0.000001)
        for years, volatility in [(1, 0.0100), (2, 0.0110)]
    ]
    prices = dal.GSRMarketQuotes_Get_Prices(curve, market)
    quotes = [
        dal.GSRCalibrationQuote_(f"{years}Y ATM", caplet(years), price.price, 0.000001)
        for years, price in zip([1, 2], prices)
    ]
    parameters = [dal.GSRCalibrationParameter_(0, knot, 0.001, 0.04) for knot in [0, 1]]
    fit = dal.Calibrate_GSRVolatility(gaussian_model(curve), quotes, parameters)
    print("\nThree-factor Gaussian fit: two level-factor g buckets; slope, curvature, H and correlations fixed")
    print(f"Converged: {fit.converged}; fit: {fit.fit_within_tolerance}; numerical: {fit.numerical_validation_passed}")
    print("Fitted g:", ", ".join(format_float(value) for value in fit.parameters))
    if not (fit.converged and fit.fit_within_tolerance and fit.numerical_validation_passed):
        raise RuntimeError(f"Gaussian calibration failed: {fit.termination_reason}")
    return fit.model


def smile_quotes():
    inputs = [
        (1, -0.005, 0.01120), (1, 0.0, 0.01050), (1, 0.005, 0.01005),
        (2, -0.005, 0.01175), (2, 0.0, 0.01140), (2, 0.005, 0.01105),
    ]
    names = [f"{years}Y {offset:+.3f}" for years, offset, _ in inputs]
    quotes = [
        dal.GSRMarketQuote_(name, caplet(years, offset), volatility, PRICE_SCALE)
        for name, (years, offset, volatility) in zip(names, inputs)
    ]
    return names, quotes


def calibrate_slv(curve, gaussian):
    model_settings = dal.GSRSLVSettings_()
    model_settings.kappa = 1.0
    model_settings.vol_of_vol = 0.5
    model_settings.variance_correlations = [-0.2, 0.0, 0.0]
    model_settings.max_step = 1.0 / 12.0
    leverage = dal.GSRLeverageData_New(
        "leverage", [-0.01, 0.0, 0.01], [0.0], dal.DoubleMatrix_([[1.0], [1.0], [1.0]]),
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
    names, quotes = smile_quotes()
    held_out = [dal.GSRMarketQuote_("2Y +0.0025 held-out", caplet(2, 0.0025), 0.01120, PRICE_SCALE)]
    fit = dal.Calibrate_GSRSLVMarket(initial, quotes, parameters, settings, held_out=held_out)
    print("\nSLV fit: three leverage nodes; Gaussian, CIR and correlations fixed")
    print(
        f"Converged: {fit.converged}; fit: {fit.fit_within_tolerance}; "
        f"numerical: {fit.numerical_validation_passed}; held-out: {fit.held_out_within_tolerance}"
    )
    print(f"Jacobian rank: {fit.jacobian_rank}/3; iterations: {fit.iterations}")
    for row, value in enumerate(fit.parameters):
        print(f"  leverage:{row}:0: {format_float(value)}")
    market_prices = dal.GSRMarketQuotes_Get_Prices(curve, quotes)
    print(f"{'Quote':<15}{'Market PV':>15}{'Fitted PV':>15}{'Residual/scale':>18}{'Pair SE':>15}")
    for name, market, price, residual, error in zip(
        names, market_prices, fit.model_prices, fit.residuals, fit.standard_errors,
    ):
        print(
            f"{name:<15}{format_float(market.price):>15}{format_float(price):>15}"
            f"{format_float(residual / PRICE_SCALE):>18}{format_float(error):>15}"
        )
    print(
        f"Max validation error: {format_float(max(fit.numerical_errors))}; "
        f"budget: {format_float(0.25 * PRICE_SCALE)}"
    )
    print(
        f"Held-out 2Y +0.0025 PV: {format_float(fit.held_out_prices[0])}; "
        f"PV error: {format_float(fit.held_out_residuals[0])}"
    )
    if not (fit.converged and fit.fit_within_tolerance and fit.numerical_validation_passed and fit.held_out_within_tolerance):
        raise RuntimeError(f"SLV calibration diagnostics failed: {fit.termination_reason}")
    return fit.model


def price_products(model):
    expiry = TODAY.AddDays(365)
    start = expiry.AddDays(2)
    second_fixing = expiry.AddDays(365)
    second_start = second_fixing.AddDays(2)
    end = second_start.AddDays(365)
    first_payment = second_start.AddDays(2)
    payment = end.AddDays(2)
    option = dal.GSRSwaption_(
        expiry, [dal.GSRFixedCoupon_(first_payment, 1.0), dal.GSRFixedCoupon_(payment, 1.0)],
        [dal.GSRFloatingCoupon_(expiry, start, second_start, first_payment, 1.0, 1.0, "12M"),
         dal.GSRFloatingCoupon_(second_fixing, second_start, end, payment, 1.0, 1.0, "12M")], 0.03,
    )
    settings = dal.GSRMonteCarloSettings_()
    settings.paths = 8192
    settings.seed = 27183
    settings.conditional_paths = 32
    prices = dal.GSRSLV_EuropeanOptionPrices(model, [caplet(1), option], settings)
    print(f"\n{'Product':<24}{'PV/notional':>15}{'Pair SE':>15}{'Conditional diff':>20}")
    for name, result in zip(["1Y ATM caplet", "1Y payer swaption"], prices):
        print(
            f"{name:<24}{format_float(result.price):>15}{format_float(result.standard_error):>15}"
            f"{format_float(result.conditional_error):>20}"
        )
        if not math.isfinite(result.price) or result.price <= 0.0:
            raise RuntimeError(f"Invalid price for {name}")


def main():
    dal.EvaluationDate_Set(TODAY)
    curve = dal.GSRCurveData_New(
        "flat_ois", TODAY, "USD", [TODAY, TODAY.AddDays(1825)],
        [0.0, -INPUT_RATE * 5.0], [], dal.DoubleMatrix_(0, 0),
    )
    print("GSR + SLV calibration and pricing; illustrative Normal quotes, unit notional")
    print("Flat 3% discount/forecast curve; quotes are annualized decimals.")
    gaussian = calibrate_gaussian(curve)
    smile = calibrate_slv(curve, gaussian)
    price_products(smile)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
