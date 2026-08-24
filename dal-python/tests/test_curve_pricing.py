"""Typed native rate cashflow pricing surface."""

import dal
import pytest


def _deposit_inputs(*, notional=100.0, contract_rate=0.05):
    today = dal.Date_(2026, 1, 15)
    maturity = dal.Date_(2027, 1, 15)
    curve = dal.DiscountPWC_New("usd", "USD", [maturity], [0.04])
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"),
        dal.DayBasis_New("ACT_365F"),
        dal.CollateralType_OIS(),
    )
    terms = dal.DepositTradeTerms_(
        notional=notional,
        contract_rate=contract_rate,
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
    return trade, market


def test_keyword_only_deposit_pricing_calls_native_cashflow_kernel():
    trade, market = _deposit_inputs()

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


def test_invalid_deposit_node_sensitivity_is_canonical_and_does_not_raise():
    trade, market = _deposit_inputs(notional=float("nan"))

    priced = dal.PriceRateTrades(trades=[trade], market=market)[0]
    sensitivity = dal.RateTradeNodeSensitivities(
        trade=trade,
        market=market,
        component_key="discount",
    )

    assert priced.succeeded is False  # nosec B101
    assert "Deposit notional must be positive and finite" in priced.error  # nosec B101
    assert sensitivity.eligible is False  # nosec B101
    assert sensitivity.pv == 0.0  # nosec B101
    assert sensitivity.gradient == []  # nosec B101
    assert sensitivity.reason == "TRADE_VALIDATION_FAILED"  # nosec B101


@pytest.mark.parametrize("contract_rate", [0.0, -0.01])
def test_zero_and_negative_deposit_rates_keep_complete_raw_native_parameter_rows(contract_rate):
    trade, market = _deposit_inputs(contract_rate=contract_rate)

    sensitivity = dal.RateTradeNodeSensitivities(
        trade=trade,
        market=market,
        component_key="discount",
    )

    assert sensitivity.eligible is True  # nosec B101
    assert isinstance(sensitivity.gradient, list)  # nosec B101
    assert len(sensitivity.gradient) == 1  # nosec B101
    assert sensitivity.reason == ""  # nosec B101


def test_node_sensitivity_binding_remains_keyword_only_and_read_only():
    signature = dal.RateTradeNodeSensitivities.__doc__.splitlines()[0]
    assert signature == (  # nosec B101
        "RateTradeNodeSensitivities(*, trade: dal._dal.RateTradeDefinition_, "
        "market: dal._dal.RatePricingMarket_, component_key: str) -> dal._dal.RateTradeNodeSensitivityResult_"
    )
    trade, market = _deposit_inputs()
    with pytest.raises(TypeError):
        dal.RateTradeNodeSensitivities(trade, market, "discount")
    with pytest.raises(TypeError):
        dal.RateTradeNodeSensitivities(trade=trade, market=market, component="discount")
    sensitivity = dal.RateTradeNodeSensitivities(trade=trade, market=market, component_key="discount")
    with pytest.raises(AttributeError):
        sensitivity.eligible = False
    with pytest.raises(AttributeError):
        sensitivity.pv = 0.0
    with pytest.raises(AttributeError):
        sensitivity.gradient = []
    with pytest.raises(AttributeError):
        sensitivity.reason = "changed"


def _batch_inputs(*, notional=100.0):
    today = dal.Date_(2026, 1, 15)
    maturity = dal.Date_(2027, 1, 15)
    curve = dal.DiscountPWC_New("usd", "USD", [maturity], [0.04])
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"),
        dal.DayBasis_New("ACT_365F"),
        dal.CollateralType_OIS(),
    )
    terms = dal.DepositTradeTerms_(
        notional=notional,
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
    return trade, market


def test_batch_node_sensitivities_match_single_trade_calls_in_deterministic_order():
    trade, market = _batch_inputs()

    cells = dal.RateTradeNodeSensitivitiesBatch(trades=[trade], market=market, component_keys=["discount", "forecast"])
    single = dal.RateTradeNodeSensitivities(trade=trade, market=market, component_key="discount")

    assert len(cells) == 2  # nosec B101
    assert cells[0].instrument_id == "deposit-1"  # nosec B101
    assert cells[0].component_key == "discount"  # nosec B101
    assert cells[0].result.eligible is True  # nosec B101
    assert cells[0].result.pv == single.pv  # nosec B101
    assert list(cells[0].result.gradient) == list(single.gradient)  # nosec B101
    assert cells[1].component_key == "forecast"  # nosec B101
    assert cells[1].result.eligible is False  # nosec B101
    assert cells[1].result.reason == "TRADE_DOES_NOT_DEPEND_ON_COMPONENT"  # nosec B101


def test_batch_node_sensitivities_isolate_partial_failure():
    valid, market = _batch_inputs()
    invalid, _ = _batch_inputs(notional=float("nan"))

    cells = dal.RateTradeNodeSensitivitiesBatch(trades=[valid, invalid], market=market, component_keys=["discount"])

    assert len(cells) == 2  # nosec B101
    assert cells[0].result.eligible is True  # nosec B101
    assert cells[1].result.eligible is False  # nosec B101
    assert cells[1].result.reason == "TRADE_VALIDATION_FAILED"  # nosec B101


def test_batch_node_sensitivities_binding_remains_keyword_only_and_read_only():
    signature = dal.RateTradeNodeSensitivitiesBatch.__doc__.splitlines()[0]
    assert signature == (  # nosec B101
        "RateTradeNodeSensitivitiesBatch(*, trades: collections.abc.Sequence[dal._dal.RateTradeDefinition_], "
        "market: dal._dal.RatePricingMarket_, component_keys: collections.abc.Sequence[str]) -> list"
    )
    trade, market = _batch_inputs()
    with pytest.raises(TypeError):
        dal.RateTradeNodeSensitivitiesBatch([trade], market, ["discount"])
    with pytest.raises(TypeError):
        dal.RateTradeNodeSensitivitiesBatch(trades=[trade], market=market, keys=["discount"])
    cells = dal.RateTradeNodeSensitivitiesBatch(trades=[trade], market=market, component_keys=["discount"])
    with pytest.raises(AttributeError):
        cells[0].result = cells[0].result
    with pytest.raises(AttributeError):
        cells[0].result.eligible = False


def test_aggregate_portfolio_node_risk_groups_by_actual_pv_currency():
    trade, market = _batch_inputs()

    aggregate = dal.AggregateRatePortfolioNodeRisk(trades=[trade], market=market, component_keys=["discount"])

    assert aggregate.policy == "UnconvertedByActualPvCcy"  # nosec B101
    assert len(aggregate.components) == 1  # nosec B101
    component = aggregate.components[0]
    assert component.component_key == "discount"  # nosec B101
    assert component.node_count == 1  # nosec B101
    assert component.node_dates == (dal.Date_(2027, 1, 15),)  # nosec B101
    assert component.node_components == ("RIGHT_FORWARD",)  # nosec B101
    single = dal.RateTradeNodeSensitivities(trade=trade, market=market, component_key="discount")
    assert list(component.values) == list(single.gradient)  # nosec B101
    assert set(aggregate.pv_by_actual_pv_ccy) == {"USD"}  # nosec B101
    assert aggregate.pv_by_actual_pv_ccy["USD"] == pytest.approx(single.pv)  # nosec B101
    assert len(aggregate.meta) == 1  # nosec B101
    assert aggregate.meta[0].actual_pv_ccy == "USD"  # nosec B101
    assert aggregate.meta[0].eligible is True  # nosec B101
    assert aggregate.meta[0].reason == ""  # nosec B101


def test_aggregate_portfolio_node_risk_binding_remains_keyword_only_and_read_only():
    signature = dal.AggregateRatePortfolioNodeRisk.__doc__.splitlines()[0]
    assert signature == (  # nosec B101
        "AggregateRatePortfolioNodeRisk(*, trades: collections.abc.Sequence[dal._dal.RateTradeDefinition_], "
        "market: dal._dal.RatePricingMarket_, component_keys: collections.abc.Sequence[str]) -> dal._dal.RatePortfolioNodeRisk_"
    )
    trade, market = _batch_inputs()
    with pytest.raises(TypeError):
        dal.AggregateRatePortfolioNodeRisk([trade], market, ["discount"])
    aggregate = dal.AggregateRatePortfolioNodeRisk(trades=[trade], market=market, component_keys=["discount"])
    with pytest.raises(AttributeError):
        aggregate.policy = "converted"
    with pytest.raises(AttributeError):
        aggregate.meta[0].eligible = False


def test_rate_risk_batch_releases_gil_for_the_whole_native_execution():
    import sys
    import threading

    started = threading.Event()
    ready = threading.Event()
    stopped = threading.Event()
    heartbeat_count = [0]

    def heartbeat() -> None:
        ready.set()
        assert started.wait(timeout=5.0)  # nosec B101
        while not stopped.is_set():
            heartbeat_count[0] += 1

    trade, market = _batch_inputs()
    previous_interval = sys.getswitchinterval()
    sys.setswitchinterval(1.0)
    try:
        thread = threading.Thread(target=heartbeat)
        thread.start()
        assert ready.wait(timeout=5.0)  # nosec B101
        dal._dal._RateRiskGilBarrier_EnableForTesting(75)
        started.set()
        dal.RateTradeNodeSensitivitiesBatch(trades=[trade], market=market, component_keys=["discount"])
        count_seen_on_return = heartbeat_count[0]
    finally:
        stopped.set()
        sys.setswitchinterval(previous_interval)
        thread.join(timeout=5.0)
    assert not thread.is_alive()  # nosec B101
    assert count_seen_on_return > 0  # nosec B101
