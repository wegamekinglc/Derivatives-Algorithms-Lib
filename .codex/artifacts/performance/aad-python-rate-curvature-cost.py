"""Installed rate-curvature boundaries and the pre-existing API composition."""

import argparse
import json
import time

import dal


POINT = [0.025, 0.03]
ROWS = [[1.0, 0.3], [-2.0, -0.6], [0.0, 1.0]]
STEPS = [2e-4, 1e-4, 2e-4]


def inputs(quotes):
    today = dal.Date_(2025, 1, 2)
    index = dal.RateIndexConvention_()
    index.day_basis = dal.DayBasis_New("ACT_365F")
    index.business_day_convention = dal.BizDayConvention_.UNADJUSTED
    index.accrual_holidays = dal.Holidays_("")
    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = today
    builder.ccy_ = dal.String_("USD")
    builder.curveName_ = dal.String_("rate_curvature")
    builder.parameterization_ = dal.CurveParameterization.LOG_DISCOUNT
    builder.knotPolicy_ = dal.CurveKnotPolicy.INPUT
    builder.initialGuess_ = 0.025
    builder.tolerance_ = 1e-14
    maturities = [dal.Date_(2026, 1, 2), dal.Date_(2027, 1, 2)]
    builder.instruments_ = [dal.Deposit_New(today, today, maturity, quote, index)
                            for maturity, quote in zip(maturities, quotes)]
    builder.knotDates_ = [today, *maturities]
    terms = dal.DepositTradeTerms_(notional=1.0, contract_rate=0.028, lend=True, index=index,
                                  discount_component_key="rate_curvature")
    trade = dal.RateTradeDefinition_(instrument_id="off-knot", instrument_type=dal.RateInstrumentType.DEPOSIT,
                                    trade_date=today, start_date=today, maturity_date=dal.Date_(2026, 7, 2),
                                    currency="USD", terms=terms)
    return builder.Build(), trade


def gradient_at(quotes):
    spec, trade = inputs(quotes)
    options = dal.CurveCalibrationOptions_()
    calibrated = dal.CalibrateSingleCurve(spec, options)
    market = dal.RatePricingMarket_(valuation_time=dal.DateTime_(dal.Date_(2025, 1, 2), 0),
                                    result_currency="USD", curve_components={"rate_curvature": calibrated.curve_},
                                    fixings=dal.MarketFixingSnapshot_New({}))
    config = dal.RateQuoteRiskProvenanceConfig_(calibration_id="rate-quote-curvature",
                                               component_key_by_parameter_block={"rate_curvature": "rate_curvature"})
    provenance = dal.BuildSingleCurveQuoteRiskProvenance(spec=spec, result=calibrated, options=options,
                                                        bound_market=market, config=config)
    risk = dal.AggregateRatePortfolioQuoteRisk(trades=[trade], market=market, provenances=[provenance])
    return [bucket.d_pv_d_decimal_quote for bucket in risk.buckets]


def composed(count):
    if not count:
        spec, _ = inputs(POINT)
        dal.CalibrateSingleCurve(spec)
        dal.CalibrateSingleCurve(spec)
    gradient = gradient_at(POINT)
    products = []
    for row, step in zip(ROWS[:count], STEPS[:count]):
        plus = gradient_at([q + step * d for q, d in zip(POINT, row)])
        minus = gradient_at([q - step * d for q, d in zip(POINT, row)])
        products.append([(p - m) / (2 * step) for p, m in zip(plus, minus)])
    return gradient, products


def native(spec, trade, source, bumps, count):
    if not count:
        source = dal.RateCalibration_New(spec)
        source = dal.RateCalibration_Recalibrate(source, POINT)
    result = dal.RateTradeQuoteCurvature([trade], source, bumps).curvature
    if result.execution.calibrations != 1 + 2 * count:
        raise RuntimeError("Incorrect financial work count")
    return result.gradient, result.hessian_products.to_rows()


def oracle(quotes):
    t = 546 / 365
    a, b = 2 - t, t - 1
    payment = (1 + 0.028 * t) * (1 + quotes[0]) ** -a * (1 + 2 * quotes[1]) ** -b
    return [-payment * a / (1 + quotes[0]), -payment * b * 2 / (1 + 2 * quotes[1])]


def require_close(actual, expected, tolerance):
    if len(actual) != len(expected) or any(abs(a - b) > tolerance for a, b in zip(actual, expected)):
        raise RuntimeError("Independent analytic gradient/secant mismatch")


def validate(result, count):
    gradient, products = result
    require_close(gradient, oracle(POINT), 1e-10)
    if len(products) != count:
        raise RuntimeError("Incorrect product shape")
    for product, row, step in zip(products, ROWS, STEPS):
        plus = oracle([q + step * d for q, d in zip(POINT, row)])
        minus = oracle([q - step * d for q, d in zip(POINT, row)])
        require_close(product, [(p - m) / (2 * step) for p, m in zip(plus, minus)], 2e-9)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=["native", "composition"], required=True)
    parser.add_argument("--directions", type=int, choices=[0, 3], required=True)
    parser.add_argument("--repeats", type=int, required=True)
    args = parser.parse_args()
    if args.repeats <= 0:
        raise ValueError("repeats must be positive")
    if args.mode == "native":
        spec, trade = inputs(POINT)
        source = dal.RateCalibration_New(spec)
        matrix = dal.DoubleMatrix_(ROWS) if args.directions else dal.DoubleMatrix_(0, 2)
        bumps = dal.BumpOverAADRequest_(directions=matrix, steps=STEPS[:args.directions])
        operation = lambda: native(spec, trade, source, bumps, args.directions)
    else:
        operation = lambda: composed(args.directions)
    validate(operation(), args.directions)
    started = time.perf_counter_ns()
    for _ in range(args.repeats):
        result = operation()
    elapsed = time.perf_counter_ns() - started
    validate(result, args.directions)
    print(json.dumps(dict(mode=args.mode, directions=args.directions, repeats=args.repeats,
                         elapsed_ns=elapsed, ns_per_operation=elapsed / args.repeats)))


if __name__ == "__main__":
    main()
