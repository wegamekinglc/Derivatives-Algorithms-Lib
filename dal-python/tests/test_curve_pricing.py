"""Typed native Curve Lab pricing surface."""

import dal


def test_keyword_only_deposit_pricing_calls_native_cashflow_kernel():
    today = dal.Date_(2026, 1, 15)
    maturity = dal.Date_(2027, 1, 15)
    curve = dal.DiscountPWC_New("usd", "USD", [maturity], [0.04])
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"),
        dal.DayBasis_New("ACT_365F"),
        dal.CollateralType_OIS(),
    )
    terms = dal.DepositTradeTerms_(
        notional=100.0,
        contract_rate=0.05,
        lend=True,
        index=index,
        discount_component_key="discount",
    )
    trade = dal.RateTradeDefinition_(
        instrument_id="deposit-1",
        instrument_type=dal.RateInstrumentType.DEPOSIT,
        trade_date=today,
        start_date=today,
        maturity_date=maturity,
        currency="USD",
        terms=terms,
    )
    market = dal.RatePricingMarket_(
        valuation_time=dal.DateTime_(today, 10, 30),
        result_currency="USD",
        curve_components={"discount": curve},
        fixings=dal.MarketFixingSnapshot_New({}),
    )

    result = dal.PriceRateTrades(trades=[trade], market=market)
    aad = dal.RateTradeNodeSensitivities(
        trade=trade,
        market=market,
        component_key="discount",
    )

    assert len(result) == 1  # nosec B101
    assert result[0].instrument_id == "deposit-1"  # nosec B101
    assert result[0].instrument_type == dal.RateInstrumentType.DEPOSIT  # nosec B101
    assert result[0].succeeded is True  # nosec B101
    assert result[0].currency == "USD"  # nosec B101
    assert result[0].dependency_component_keys == ("discount",)  # nosec B101
    assert aad.eligible is True  # nosec B101
    assert len(aad.gradient) == 1  # nosec B101
    assert aad.reason == ""  # nosec B101
