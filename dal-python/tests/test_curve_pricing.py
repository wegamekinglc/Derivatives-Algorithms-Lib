"""Typed native Curve Lab pricing surface."""

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
