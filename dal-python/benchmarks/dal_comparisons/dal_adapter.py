"""DAL public batch pricing, including Python/native argument and result conversion."""

import dal

from .scenarios import LOG_DFS, NODES, TIMES, TODAY


def date(value):
    return dal.Date_(value.year, value.month, value.day)


def curve(shift):
    return dal.DiscountLogDF_New(
        "comparison",
        "USD",
        [date(value) for value in NODES],
        [value - shift * t for value, t in zip(LOG_DFS, TIMES)],
        day_count=dal.DayBasis_New("ACT_365F"),
        log_df_scheme=dal.LogDfScheme.LOG_LINEAR,
    )


def leg():
    result = dal.RateLegConvention_New(
        dal.PeriodLength_New("12M"), dal.DayBasis_New("ACT_365F")
    )
    result.payment_lag = 0
    result.business_day_convention = dal.BizDayConvention_.UNADJUSTED
    result.payment_convention = dal.BizDayConvention_.UNADJUSTED
    return result


def index():
    result = dal.RateIndexConvention_New(
        dal.PeriodLength_New("12M"),
        dal.DayBasis_New("ACT_365F"),
        dal.CollateralType_OIS(),
        True,
    )
    result.fixing_lag = result.spot_lag = 0
    result.business_day_convention = dal.BizDayConvention_.UNADJUSTED
    return result


def swap(value, ordinal):
    identity = dal.FixingIdentity_()
    identity.index_name = "USD-COMPARISON"
    identity.fixing_hour, identity.fixing_minute = 11, 0
    terms = dal.FixedFloatTradeTerms_(
        notional=value["notional"],
        contract_rate=value["rate"],
        pay_fixed=value["sign"] == 1,
        fixed_leg=leg(),
        float_leg=leg(),
        float_index=index(),
        fixing_identity=identity,
        forecast_component_key="curve",
        discount_component_key="curve",
    )
    return dal.RateTradeDefinition_(
        instrument_id=f"comparison-{ordinal}",
        instrument_type=dal.RateInstrumentType.IRS,
        trade_date=date(TODAY),
        start_date=date(value["start"]),
        maturity_date=date(value["end"]),
        currency="USD",
        terms=dal.IrsTradeTerms_(value=terms),
    )


def discount_runner(dates):
    source, anchor = curve(0.0), date(TODAY)
    native_dates = [date(value) for value in dates]
    return lambda: [source(anchor, value) for value in native_dates]


def pricing_runner(portfolio, shift):
    source = curve(shift)
    market = dal.RatePricingMarket_(
        valuation_time=dal.DateTime_(date(TODAY), 9, 0),
        result_currency="USD",
        curve_components={"curve": source},
        fixings=dal.MarketFixingSnapshot_New({}),
    )
    native_trades = [swap(value, i) for i, value in enumerate(portfolio)]

    def run():
        rows = dal.PriceRateTrades(trades=native_trades, market=market)
        # Native failures are data; NaN makes the common out-of-timer validator fail.
        return [row.pv if row.succeeded else float("nan") for row in rows]

    return run
