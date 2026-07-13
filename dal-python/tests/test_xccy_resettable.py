"""Python coverage for resettable and mark-to-market XCCY instruments."""

import pytest

import dal


def _today():
    return dal.Date_(2025, 6, 20)


def _spot():
    return _today().AddDays(2)


def _make_baseline_curves():
    knot_dates = [_spot(), _spot().AddDays(3650)]
    usd_ois = dal.DiscountPWLF_New("usd_ois", "USD", knot_dates, [0.04, 0.04])
    eur_ois = dal.DiscountPWLF_New("eur_ois", "EUR", knot_dates, [0.03, 0.03])
    return (
        dal.CurveBlock_New(usd_ois, libor_basis=dal.DayBasis_New("ACT_365F")),
        dal.CurveBlock_New(eur_ois, libor_basis=dal.DayBasis_New("ACT_360")),
    )


def _make_mtm_config():
    domestic_leg = dal.RateLegConvention_New(dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_365F"))
    foreign_leg = dal.RateLegConvention_New(dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360"))
    domestic_index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS()
    )
    foreign_index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS()
    )

    convention = dal.CrossCurrencyConvention_()
    convention.initial_notional_exchange = True
    convention.finalNotionalExchange_ = True
    convention.spread_on_foreign_leg = True
    convention.domestic_leg = domestic_leg
    convention.domesticIndex_ = domestic_index
    convention.foreign_leg = foreign_leg
    convention.foreignIndex_ = foreign_index

    domestic_fixing = dal.FixingIdentity_()
    domestic_fixing.index_name = "USD-SOFR-3M"
    domestic_fixing.fixingHour_ = 11
    domestic_fixing.fixing_minute = 0

    foreign_fixing = dal.FixingIdentity_()
    foreign_fixing.indexName_ = "EUR-EURIBOR-3M"
    foreign_fixing.fixing_hour = 11
    foreign_fixing.fixingMinute_ = 0

    builder = dal.CrossCurrencySwapConfigBuilder_()
    builder.pair = dal.CurrencyPair_New("USD", "EUR")
    builder.domesticNotional_ = 110.0
    builder.foreign_notional = 100.0
    builder.convention = convention
    builder.notional_mode = dal.XccyNotionalMode.MARK_TO_MARKET
    builder.fx_reset = dal.FxResetConvention_New(
        0, dal.Holidays_(""), dal.BizDayConvention_.FOLLOWING, 10, 30
    )
    builder.domestic_rate_fixing = domestic_fixing
    builder.foreignRateFixing_ = foreign_fixing
    return builder.Build()


def test_resettable_config_and_config_overload_preserve_field_aliases():
    """Config values are writable through old and snake-case spellings."""
    assert dal.XccyNotionalMode.RESETTABLE is not None  # nosec B101
    config = _make_mtm_config()

    assert config.notionalMode_ == dal.XccyNotionalMode.MARK_TO_MARKET  # nosec B101
    assert config.notional_mode == dal.XccyNotionalMode.MARK_TO_MARKET  # nosec B101
    assert config.domesticNotional_ == 110.0  # nosec B101
    assert config.foreign_notional == 100.0  # nosec B101
    assert config.fxReset_.fixingHour_ == 10  # nosec B101
    assert config.domestic_rate_fixing.index_name == "USD-SOFR-3M"  # nosec B101

    instrument = dal.CrossCurrencySwap_New(
        _today(), _today().AddDays(-92), _today().AddDays(273), 0.001, config
    )
    assert instrument is not None  # nosec B101


def test_market_fixing_snapshot_accepts_nested_mapping():
    """Nested Python mappings copy rate and FX fixing histories."""
    rate_fixing_time = dal.DateTime_(_today().AddDays(-92), 11, 0)
    foreign_fixing_time = dal.DateTime_(_today().AddDays(-92), 11, 0)
    fx_fixing_time = dal.DateTime_(_today().AddDays(-1), 10, 30)
    values = {
        "USD-SOFR-3M": {rate_fixing_time: 0.0525},
        "EUR-EURIBOR-3M": {foreign_fixing_time: 0.0310},
        "FX[EUR/USD]": {fx_fixing_time: 1.10},
    }

    snapshot = dal.MarketFixingSnapshot_New(values)
    values["USD-SOFR-3M"][rate_fixing_time] = 0.99

    assert snapshot is not None  # nosec B101
    assert snapshot.find("USD-SOFR-3M", rate_fixing_time) == pytest.approx(0.0525)  # nosec B101
    assert snapshot.Require("FX[EUR/USD]", fx_fixing_time, "Python snapshot test") == pytest.approx(1.10)  # nosec B101


def test_in_progress_mtm_swap_requires_historical_fixings():
    """An explicit empty snapshot cannot silently fall back to global history."""
    domestic, foreign = _make_baseline_curves()
    start = _today().AddDays(-92)
    maturity = _today().AddDays(273)
    swap = dal.CrossCurrencySwap_New(_today(), start, maturity, 0.001, _make_mtm_config())

    builder = dal.CrossCurrencyCalibrationSpecBuilder_()
    builder.today_ = _today()
    builder.valuation_time = dal.DateTime_(_today(), 12, 0)
    builder.collateral_currency = dal.Ccy_("USD")
    builder.fixings = dal.MarketFixingSnapshot_New({})
    builder.basisPair_ = dal.CurrencyPair_New("USD", "EUR")
    builder.domestic_curve_block = domestic
    builder.foreignCurveBlock_ = foreign
    builder.fx_spot = 1.10
    builder.instruments = [swap]
    builder.knot_dates = [maturity]
    builder.solveMode_ = dal.CurveSolveMode.APPROXIMATE

    with pytest.raises(RuntimeError, match=r"FX\[EUR/USD\]"):
        dal.CalibrateXccyMarket(builder.Build())
