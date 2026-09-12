#!/usr/bin/env python3
"""Reuse an immutable IRS snapshot with current markets for PV and AAD risk."""

import dal


TODAY = dal.Date_(2026, 1, 15)
START = dal.Date_(2026, 10, 15)
MATURITY = dal.Date_(2031, 10, 15)


def make_trade():
    basis = dal.DayBasis_New("ACT_365F")
    leg = dal.RateLegConvention_New(dal.PeriodLength_New("12M"), basis)
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("12M"), basis, dal.CollateralType_OIS(), True
    )
    identity = dal.FixingIdentity_()
    identity.index_name = "USD-EXAMPLE"
    identity.fixing_hour, identity.fixing_minute = 11, 0
    terms = dal.FixedFloatTradeTerms_(
        notional=1_000_000.0,
        contract_rate=0.03,
        pay_fixed=True,
        fixed_leg=leg,
        float_leg=leg,
        float_index=index,
        fixing_identity=identity,
        forecast_component_key="curve",
        discount_component_key="curve",
    )
    return dal.RateTradeDefinition_(
        instrument_id="irs-1",
        instrument_type=dal.RateInstrumentType.IRS,
        trade_date=TODAY,
        start_date=START,
        maturity_date=MATURITY,
        currency="USD",
        terms=dal.IrsTradeTerms_(value=terms),
    )


def validate_prices(prices, expected):
    if len(prices) != len(expected):
        raise RuntimeError("Prepared and ordinary PV widths differ")
    for actual, reference in zip(prices, expected):
        if not actual.succeeded or actual.pv != reference.pv:
            raise RuntimeError(f"Prepared PV differs: {actual.error}")


def validate_risk(risk, expected):
    if len(risk) != len(expected):
        raise RuntimeError("Prepared and ordinary risk widths differ")
    for actual, reference in zip(risk, expected):
        if not actual.result.eligible:
            raise RuntimeError(actual.result.reason)
        if list(actual.result.gradient) != list(reference.result.gradient):
            raise RuntimeError("Prepared gradient differs")


def main():
    trades = [make_trade()]
    prepared = dal.PreparedRateTrades_New(trades=trades)
    for rate in (0.02, 0.04, 0.02):
        curve = dal.DiscountPWC_New("usd", "USD", [MATURITY], [rate])
        market = dal.RatePricingMarket_(
            valuation_time=dal.DateTime_(TODAY, 9, 0),
            result_currency="USD",
            curve_components={"curve": curve},
            fixings=dal.MarketFixingSnapshot_New({}),
        )
        prices = dal.PreparedRateTrades_Get_Prices(prepared=prepared, market=market)
        risk = dal.PreparedRateTrades_Get_NodeSensitivities(
            prepared=prepared, market=market, component_keys=["curve"]
        )
        ordinary = dal.PriceRateTrades(trades=trades, market=market)
        ordinary_risk = dal.RateTradeNodeSensitivitiesBatch(
            trades=trades, market=market, component_keys=["curve"]
        )
        if len(prices) != len(trades) or len(risk) != len(trades):
            raise RuntimeError("Prepared result width changed")
        validate_prices(prices, ordinary)
        validate_risk(risk, ordinary_risk)
        print(
            f"rate={rate:.2%}, PV={prices[0].pv:.6f}, dPV/dforward={list(risk[0].result.gradient)}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
