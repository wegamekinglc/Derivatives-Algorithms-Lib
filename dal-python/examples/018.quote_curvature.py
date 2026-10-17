"""Complete recalibration for rate and Dupire quote curvature in raw quote units."""

import math

import dal
from risk_report import compare, print_reference_header


def rate_quote_curvature():
    today = dal.Date_(2025, 1, 2)
    maturities = [dal.Date_(2026, 1, 2), dal.Date_(2027, 1, 2)]
    quotes = [0.025, 0.03]
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
    builder.instruments_ = [
        dal.Deposit_New(today, today, maturity, quote, index)
        for maturity, quote in zip(maturities, quotes)
    ]
    builder.knotDates_ = [today, *maturities]
    calibration = dal.RateCalibration_New(builder.Build())
    trade = dal.RateTradeDefinition_(
        instrument_id="off-knot", instrument_type=dal.RateInstrumentType.DEPOSIT,
        trade_date=today, start_date=today, maturity_date=dal.Date_(2026, 7, 2),
        currency="USD", terms=dal.DepositTradeTerms_(
            notional=1.0, contract_rate=0.028, lend=True, index=index,
            discount_component_key="rate_curvature",
        ),
    )
    step = 2e-4
    request = dal.BumpOverAADRequest_(directions=dal.DoubleMatrix_([[1.0, 0.0]]), steps=[step])
    result = dal.RateTradeQuoteCurvature([trade], calibration, request).curvature

    # Two exact deposit quotes determine the log-linear discount at this off-knot maturity.
    time = 546 / 365
    first_weight, second_weight = 2 - time, time - 1

    def discounted_payment(point):
        return (1 + 0.028 * time) * (1 + point[0]) ** -first_weight * (1 + 2 * point[1]) ** -second_weight

    def gradient(point):
        payment = discounted_payment(point)
        return [-payment * first_weight / (1 + point[0]), -payment * 2 * second_weight / (1 + 2 * point[1])]

    compare("Rate deposit PV", result.value, discounted_payment(quotes) - 1, 1e-12)
    plus = gradient([quotes[0] + step, quotes[1]])
    minus = gradient([quotes[0] - step, quotes[1]])
    for column in range(2):
        compare(f"Rate quote gradient {column}", result.gradient[column], gradient(quotes)[column], 1e-10)
        compare(f"Rate quote HVP {column}", result.hessian_products[0, column], (plus[column] - minus[column]) / (2 * step), 2e-9)
    if result.execution.calibrations != 3:
        raise RuntimeError("Expected complete base/plus/minus recalibration")


def dupire_quote_curvature():
    calibration = dal.DupireCalibration_New(
        dal.BSModelData_New(100.0, 0.2, 0.05, 0.02),
        dal.DupireRiskInputs_(
            quote_strikes=[75.0, 105.0, 135.0], quote_maturities=[0.4, 1.2],
            quote_spreads=dal.DoubleMatrix_(3, 2, 0.001),
            inclusion_spots=[60.0, 100.0, 140.0], max_spot_spacing=10.0,
            inclusion_times=[0.5, 1.0], max_time_spacing=0.5,
        ),
    )
    model = dal.HybridModelData_New(
        "hybrid", "USD", [
            dal.HybridDeterministicRateData_New("rate", "USD", calibration.rate),
            dal.HybridLocalVolEquityData_New(
                "local", "EQ[LOCAL]", "USD", "F_LOCAL", 100.0,
                calibration.dividend_yield, calibration.surface, max_step=0.25,
            ),
        ],
        dal.HybridConstantCorrelationData_New("correlation", ["F_LOCAL"], dal.DoubleMatrix_([[1.0]])),
    )
    direction = dal.DoubleMatrix_(1, 6, 0.0)
    direction[0, 3] = 1.0
    risk = dal.DupireScriptRiskRequest_(
        num_paths=17,
        direct_bindings=[dal.DupireQuoteBinding_(constant_ordinal=0, quote_id="quote:3")],
        valuation=dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 9, 12)),
        simulation=dal.MonteCarloSettings_(enable_aad=True, compiled=True),
    )
    request = dal.DupireScriptCurvatureRequest_(
        risk=risk, bumps=dal.BumpOverAADRequest_(directions=direction, steps=[2e-4]),
    )

    def evaluate(payoff):
        product = dal.Product_New(["QUOTE", dal.Date_(2027, 9, 12)], ["0.001", "pay PAYS " + payoff])
        plan = dal.DupireScriptCurvaturePlan_New(product, model, calibration, "local", request)
        return dal.DupireScriptCurvatureResult_New(plan)

    result = evaluate("QUOTE * QUOTE")
    discount = math.exp(-0.05)
    compare("Direct Dupire quote gradient", result.gradient[3], 0.002 * discount, 1e-12)
    compare("Direct Dupire quote Gamma", result.hessian_products[0, 3], 2 * discount, 1e-10)
    if result.execution.quote_gradient_evaluations != 3:
        raise RuntimeError("Expected three recalibrated complete quote gradients")
    print("The direct-quote payoff has an exact oracle; its calibrated-surface partial is zero.")
    mixed = evaluate("QUOTE * QUOTE + FIX(EQ[LOCAL]) ^ 2 / 100 + 2 * QUOTE * FIX(EQ[LOCAL])")
    values = [mixed.base.valuation.values[0], mixed.gradient[3], mixed.hessian_products[0, 3]]
    if not all(math.isfinite(value) for value in values) or mixed.execution.quote_gradient_evaluations != 3:
        raise RuntimeError("Expected finite mixed surface/direct quote curvature and three complete gradients")
    print("\nDupire surface and direct quote: common-path finite-step estimates (17 paths)")
    print(f"{'PV':>18}{'Quote gradient':>18}{'Quote HVP':>18}\n{'-' * 54}")
    print("".join(f"{value:>18.8f}" for value in values))
    print("Assess calibration-stencil, outer-step and sampling errors separately.")


def main():
    print_reference_header("Recalibrated native quote curvature: raw decimal quote units")
    rate_quote_curvature()
    dupire_quote_curvature()
    print("Quote HVPs use finite steps; report-scale Hessians require both coordinate scale factors.")
    print("Native quote curvature examples passed.")


if __name__ == "__main__":
    main()
