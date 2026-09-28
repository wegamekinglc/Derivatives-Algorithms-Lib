"""DAL GSR example prices through public curve, IRS and script APIs."""

import dal

from . import gsr_scenarios as inputs


def date(value):
    return dal.Date_(value.year, value.month, value.day)


def curve():
    dates = [date(value) for value in inputs.CURVE_DATES]
    log_dfs = [
        -(inputs.INPUT_RATE * (value - inputs.TODAY).days / 365.0)
        for value in inputs.CURVE_DATES
    ]
    return dal.DiscountLogDF_New("gsr_comparison", "USD", dates, log_dfs)


def trade(ordinal):
    fixed_leg = dal.RateLegConvention_New(
        dal.PeriodLength_New("6M"), dal.DayBasis_New("30_360")
    )
    float_leg = dal.RateLegConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360")
    )
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"),
        dal.DayBasis_New("ACT_360"),
        dal.CollateralType_OIS(),
    )
    index.fixing_lag = 2
    identity = dal.FixingIdentity_()
    identity.index_name = "USD-LIBOR-3M-CME"
    identity.fixing_hour, identity.fixing_minute = 11, 0
    terms = dal.FixedFloatTradeTerms_(
        notional=1.0,
        contract_rate=inputs.STRIKE,
        pay_fixed=True,
        fixed_leg=fixed_leg,
        float_leg=float_leg,
        float_index=index,
        fixing_identity=identity,
        forecast_component_key="curve",
        discount_component_key="curve",
    )
    return dal.RateTradeDefinition_(
        instrument_id=f"gsr-swap-{ordinal}",
        instrument_type=dal.RateInstrumentType.IRS,
        trade_date=date(inputs.TODAY),
        start_date=date(inputs.EXPIRY),
        maturity_date=date(inputs.MATURITY),
        currency="USD",
        terms=dal.IrsTradeTerms_(value=terms),
    )


def runner(case):
    today = date(inputs.TODAY)
    dal.EvaluationDate_Set(today)
    source = curve()
    if case["operation"] in inputs.SWAP_OPERATIONS:
        market = dal.RatePricingMarket_(
            valuation_time=dal.DateTime_(today, 9, 0),
            result_currency="USD",
            curve_components={"curve": source},
            fixings=dal.MarketFixingSnapshot_New({}),
        )
        trades = [trade(i) for i in range(case["size"])]
        if case["operation"] == "gsr_swap_reused":
            prepared = dal.PreparedRateTrades_New(trades=trades)

            def price_swaps():
                rows = dal.PreparedRateTrades_Get_Prices(
                    prepared=prepared, market=market
                )
                return [row.pv if row.succeeded else float("nan") for row in rows]

            return price_swaps

        def price_swaps():
            rows = dal.PriceRateTrades(trades=trades, market=market)
            return [row.pv if row.succeeded else float("nan") for row in rows]

        return price_swaps

    if case["operation"] not in inputs.SWAPTION_OPERATIONS:
        raise ValueError(f"unknown GSR operation: {case['operation']}")
    dates = [date(value) for value in inputs.CURVE_DATES]
    snapshot = dal.GSRCurveDataFromYieldCurve_New(
        "gsr_curve", dal.CurveBlock_New(source), today, dates, []
    )
    vol = dal.GSRVolData_New("gsr_vol", [today], [inputs.VOLATILITY], [today], [1.0])
    model = dal.GSRModelData_New("gsr", snapshot, vol)
    annuity = "0.5 * FIX(IR[USD,DF,2028-03-28]) + 0.5 * FIX(IR[USD,DF,2028-09-28])"
    payoff = f"(FIX(IR[USD,SWAP,1Y,2027-09-28]) - STRIKE) * ({annuity})"
    product = dal.Product_New(
        ["STRIKE", date(inputs.EXPIRY)],
        [str(inputs.STRIKE), f"pay PAYS MAX({payoff}, 0)"],
    )
    valuation = dal.ScriptValuationSettings_(evaluation_date=today)
    simulation = dal.MonteCarloSettings_(compiled=True)

    def price_swaption():
        result = dal.MonteCarlo_ValueWithSettings(
            product, model, case["size"], valuation=valuation, simulation=simulation
        )
        return [result["PV"]]

    return price_swaption
