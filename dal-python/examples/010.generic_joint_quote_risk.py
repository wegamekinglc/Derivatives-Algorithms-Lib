#!/usr/bin/env python3
"""One joint USD calibration, an IRS portfolio, and coupled quote-space DV01."""

import dal


TODAY = dal.Date_(2025, 1, 2)
BASIS = dal.DayBasis_New("ACT_365F")
COLLATERAL = dal.CollateralType_OIS()
FIXED_LEG = dal.RateLegConvention_New(dal.PeriodLength_New("6M"), BASIS)
FLOAT_LEG = dal.RateLegConvention_New(dal.PeriodLength_New("3M"), BASIS)
FLOAT_INDEX = dal.RateIndexConvention_New(dal.PeriodLength_New("3M"), BASIS, COLLATERAL, True)


def make_spec():
    """Three discount deposits and two 3M swaps, with a layered forward curve."""
    discount_index = dal.RateIndexConvention_New(dal.PeriodLength_New("3M"), BASIS, COLLATERAL)
    discount = dal.JointCurveDeclaration_()
    discount.curve_name = "usd-ois"
    discount.parameterization = dal.CurveParameterization.PIECEWISE_CONSTANT_FWD
    discount.knot_dates = [dal.Date_(year, 7, 2) for year in (2025, 2026, 2027)]
    discount.instruments = [
        dal.Deposit_New(TODAY, TODAY, dal.Date_(2026 + i, 1, 2), quote, discount_index)
        for i, quote in enumerate((0.020, 0.021, 0.022))
    ]

    forward = dal.JointCurveDeclaration_()
    forward.curve_name = "usd-3m"
    forward.parameterization = dal.CurveParameterization.PIECEWISE_LINEAR_FWD
    forward.calibrate_discount_curve = False
    forward.target_tenor = dal.PeriodLength_New("3M")
    forward.base_layered_over_discount = True
    forward.knot_dates = [dal.Date_(year, 7, 2) for year in (2025, 2026)]
    forward.instruments = [
        dal.Swap_New(TODAY, TODAY, dal.Date_(2026 + i, 1, 2), quote, FIXED_LEG, FLOAT_INDEX, FLOAT_LEG)
        for i, quote in enumerate((0.035, 0.036))
    ]

    spec = dal.JointMultiCurveCalibrationSpec_()
    spec.today = TODAY
    spec.ccy = "USD"
    spec.curves = [discount, forward]
    spec.tolerance = 1e-11
    spec.initial_guess = 0.025
    spec.max_evaluations = 1000
    spec.max_restarts = 100
    return spec


def make_trade(name, notional, pay_fixed):
    identity = dal.FixingIdentity_()
    identity.index_name = "USD-3M"
    identity.fixing_hour = 11
    identity.fixing_minute = 0
    terms = dal.FixedFloatTradeTerms_(
        notional=notional, contract_rate=0.030, pay_fixed=pay_fixed,
        fixed_leg=FIXED_LEG, float_leg=FLOAT_LEG, float_index=FLOAT_INDEX,
        fixing_identity=identity, forecast_component_key="forward-3m", discount_component_key="discount",
    )
    return dal.RateTradeDefinition_(
        instrument_id=name, instrument_type=dal.RateInstrumentType.IRS,
        trade_date=TODAY, start_date=TODAY, maturity_date=dal.Date_(2027, 1, 2),
        currency="USD", terms=dal.IrsTradeTerms_(value=terms),
    )


def main():
    spec = make_spec()
    options = dal.JointMultiCurveCalibrationOptions_()
    options.compute_eff_jacobian_inverse = True
    # For M>N this explicitly selects the initial-Jacobian affine chart.
    # Keep these guesses and this request fixed when recalibrating bumped quotes.
    calibrated = dal.CalibrateJointMultiCurveBundle(spec, options)
    market = dal.RatePricingMarket_(
        valuation_time=dal.DateTime_(TODAY, 0), result_currency="USD",
        curve_components={"discount": next(iter(calibrated.discount_curves.values())),
                          "forward-3m": next(iter(calibrated.forward_curves.values()))},
        fixings=dal.MarketFixingSnapshot_New({}),
    )
    config = dal.RateQuoteRiskProvenanceConfig_(
        calibration_id="usd-joint",
        component_key_by_parameter_block={"curve:0": "discount", "curve:1": "forward-3m"},
    )
    provenance = dal.BuildJointMultiCurveQuoteRiskProvenance(
        spec=spec, result=calibrated, options=options, bound_market=market, config=config,
    )
    if not provenance.available:
        raise RuntimeError(provenance.reason)
    trades = [make_trade("pay-fixed", 1_000_000.0, True), make_trade("receive-fixed", 250_000.0, False)]
    risk = dal.AggregateRatePortfolioQuoteRisk(trades=trades, market=market, provenances=[provenance])
    if risk.provenance_failures or any(not entry.eligible for entry in risk.meta):
        raise RuntimeError("Joint quote risk lost a provenance or eligible trade")
    print("mapping:", calibrated.eff_jacobian_inverse_mapping)
    print("axis:", provenance.axis.scheme, provenance.axis.fingerprint)
    print("policy:", risk.policy)
    print("quote_key,currency,dPV/dDecimalQuote,DV01")
    for bucket in risk.buckets:
        print(f"{bucket.quote_key},{bucket.actual_pv_ccy},{bucket.d_pv_d_decimal_quote:.12g},{bucket.dv01:.12g}")


if __name__ == "__main__":
    main()
