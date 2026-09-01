#!/usr/bin/env python3
"""Calibrate one USD curve and aggregate quote-space DV01."""

import dal


TODAY = dal.Date_(2025, 6, 20)
SPOT = TODAY.AddDays(2)


def make_spec():
    """Build an exact three-quote OIS calibration."""
    fixed_leg = dal.RateLegConvention_New(
        dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_365F")
    )
    float_leg = dal.RateLegConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360")
    )
    overnight_index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("12M"),
        dal.DayBasis_New("ACT_360"),
        dal.CollateralType_OIS(),
    )
    maturities = [SPOT.AddDays(years * 365) for years in (2, 5, 10)]

    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = TODAY
    builder.ccy_ = dal.String_("USD")
    builder.curveName_ = dal.String_("usd_ois")
    builder.calibrateDiscountCurve_ = True
    builder.initialGuess_ = 0.04
    builder.instruments_ = [
        dal.OISSwap_New(
            TODAY,
            SPOT,
            maturity,
            0.04,
            fixed_leg,
            overnight_index,
            float_leg,
        )
        for maturity in maturities
    ]
    builder.knotDates_ = maturities
    return builder.Build(), maturities


def make_trade(maturity):
    """Build a deposit whose PV depends on the calibrated component."""
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"),
        dal.DayBasis_New("ACT_365F"),
        dal.CollateralType_OIS(),
    )
    terms = dal.DepositTradeTerms_(
        notional=1_000_000.0,
        contract_rate=0.022,
        lend=True,
        index=index,
        discount_component_key="discount",
    )
    return dal.RateTradeDefinition_(
        instrument_id="deposit-1",
        instrument_type=dal.RateInstrumentType.DEPOSIT,
        trade_date=TODAY,
        start_date=TODAY,
        maturity_date=maturity,
        currency="USD",
        terms=terms,
    )


def main() -> int:
    """Run the supported single-curve provenance and aggregation workflow."""
    spec, maturities = make_spec()
    options = dal.CurveCalibrationOptions_()
    calibrated = dal.CalibrateSingleCurve(spec, options)
    fixings = dal.MarketFixingSnapshot_New({})
    market = dal.RatePricingMarket_(
        valuation_time=dal.DateTime_(TODAY, 9, 0),
        result_currency="USD",
        curve_components={"discount": calibrated.curve_},
        fixings=fixings,
    )
    provenance = dal.BuildSingleCurveQuoteRiskProvenance(
        spec=spec,
        result=calibrated,
        options=options,
        bound_market=market,
        config=dal.RateQuoteRiskProvenanceConfig_(
            calibration_id="usd-ois-calibration",
            component_key_by_parameter_block={"usd_ois": "discount"},
        ),
    )
    risk = dal.AggregateRatePortfolioQuoteRisk(
        trades=[make_trade(maturities[0])],
        market=market,
        provenances=[provenance],
    )

    print(f"Axis fingerprint: {provenance.axis.fingerprint}")
    print(f"State fingerprint: {provenance.state.fingerprint}")
    print("Quote risk (decimal quote sensitivity, DV01):")
    for bucket in risk.buckets:
        print(
            f"  {bucket.quote_key}: "
            f"{bucket.d_pv_d_decimal_quote:.10g}, {bucket.dv01:.10g} {bucket.actual_pv_ccy}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
