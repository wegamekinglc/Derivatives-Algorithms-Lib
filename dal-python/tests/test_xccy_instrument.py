import gc
import json

import pytest

import dal


def inputs(mode="FIXED", *, solver=None, hour=0):
    today, start, end = dal.Date_(2026, 10, 21), dal.Date_(2027, 5, 4), dal.Date_(2027, 11, 4)
    convention = dal.CrossCurrencyConvention_()
    convention.initial_notional_exchange = convention.final_notional_exchange = True
    for side, tenor in (("domestic", "3M"), ("foreign", "6M")):
        leg = dal.RateLegConvention_New(dal.PeriodLength_New(tenor), dal.DayBasis_New("ACT_360"))
        leg.business_day_convention = leg.payment_convention = dal.BizDayConvention_.UNADJUSTED
        leg.payment_lag = 2 if side == "foreign" else 0
        index = dal.RateIndexConvention_New(dal.PeriodLength_New(tenor), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS(), True)
        index.fixing_lag = 2
        setattr(convention, side + "_leg", leg)
        setattr(convention, side + "_index", index)
    config = dal.CrossCurrencySwapConfigBuilder_()
    config.pair = dal.CurrencyPair_New("USD", "EUR")
    config.domestic_notional, config.foreign_notional = 110.0, 100.0
    config.convention = convention
    config.notional_mode = getattr(dal.XccyNotionalMode, mode)
    config.fx_reset = dal.FxResetConvention_New(0, dal.Holidays_(""), dal.BizDayConvention_.PRECEDING, 0, 0)
    for side, name in (("domestic", "IR:USD,LIBOR_3M_LCH"), ("foreign", "IR:EUR,LIBOR_6M_LCH")):
        fixing = dal.FixingIdentity_()
        fixing.index_name, fixing.fixing_hour, fixing.fixing_minute = name, 0, 0
        setattr(config, side + "_rate_fixing", fixing)
    native = dal.CrossCurrencySwap_New(today, start, end, 0.001, config.Build())
    instrument = dal.XccyScriptInstrument_New(native, dal.XccyScriptImportSettings_(instrument_id="xccy", discount_route="USD.OIS"))
    context = dal.XccyScriptContext_(valuation_time=dal.DateTime_(today, hour), pair=config.pair,
                                    collateral_currency="USD", discount_route="USD.OIS", fixings=dal.MarketFixingSnapshot_New({}))
    blocks = []
    for ccy, rate, tenor in (("USD", 0.04, "3M"), ("EUR", 0.02, "6M")):
        curve = dal.DiscountPWLF_New(ccy, ccy, [today, end], [rate, rate])
        blocks.append(dal.CurveBlock_New(ccy, ccy, {dal.CollateralType_OIS(): curve}, {dal.PeriodLength_New(tenor): curve},
                                        dal.DayBasis_New("ACT_365F")))
    targets = {"FIXED": -0.002983490837363863, "RESETTABLE": 0.032239173433225896, "MARK_TO_MARKET": 0.06746183770381563}
    quote = dal.CalibrationMarketQuote_New("basis", targets[mode], dal.CalibrationValueUnit_(kind="BasisSpread"))
    observation = dal.CalibrationObservation_New("row", instrument, "BasisSpread", quote, 1e-10)
    values = dict(context=context, domestic_curve_block=blocks[0], foreign_curve_block=blocks[1], fx_spot=1.1,
                  knot_dates=[end], observations=[observation])
    if solver is not None:
        values["solver"] = solver
    return values, instrument, quote


@pytest.mark.parametrize("mode", ["FIXED", "RESETTABLE", "MARK_TO_MARKET"])
def test_native_import_solve_archive_and_capabilities(mode):
    values, instrument, quote = inputs(mode)
    saved = dal.CalibrationInstrument_Get_Json(instrument)
    loaded = dal.CalibrationInstrument_LoadJson(saved)
    assert dal.CalibrationInstrument_Get_Json(loaded) == saved
    spec = dal.XccyInstrumentCalibrationSpec_(**values)
    values["observations"].clear()
    result = dal.Calibrate_XccyInstruments(spec, options=dal.CurveInstrumentCalibrationOptions_(jacobian_mode="AAD", compute_quote_jacobian=True))
    assert result.fit_within_tolerance
    assert result.parameters == pytest.approx([0.003], abs=1e-10)
    report = result.report
    capability = dal.CalibrationInstrument_Get_XccyCapabilities(instrument, "BasisSpread", spec.context)
    attempts = json.loads(dal.CalibrationReport_Get_Json(capability))["plans"]
    assert attempts[0]["admitted"] is True
    assert attempts[-1]["reason_code"] == "MCCrossCurrencyUnsupported"
    del spec, result, values, instrument, loaded, quote
    gc.collect()
    data = json.loads(dal.CalibrationReport_Get_Json(report))
    assert data["summary"]["domain"] == "Xccy"
    assert data["summary"]["quote_risk_available"] is False
    assert len(data["quote_jacobian"]) == 1
    assert data["execution"]["today_fixing_policy"] == "NativeXccyTimestamp"


def test_budget_failure_owns_candidate_and_report():
    values, _, _ = inputs(solver=dal.CurveInstrumentSolver_(initial_guess=0.0, max_evaluations=1))
    with pytest.raises(dal.CalibrationError) as raised:
        dal.Calibrate_XccyInstruments(dal.XccyInstrumentCalibrationSpec_(**values))
    error = raised.value
    del values
    gc.collect()
    assert error.code == "CalibrationBudgetExceeded"
    assert error.candidate is not None
    data = json.loads(dal.CalibrationReport_Get_Json(error.report))
    assert data["observations"][0]["observation_id"] == "row"
    assert data["parameters"][0]["value"] == 0.0


def test_strict_types_and_timestamp_refusal():
    values, _, _ = inputs(hour=9)
    with pytest.raises(dal.CalibrationError) as raised:
        dal.Calibrate_XccyInstruments(dal.XccyInstrumentCalibrationSpec_(**values))
    assert raised.value.code == "UnsupportedCalibrationFixingTime"
    with pytest.raises(TypeError):
        dal.XccyInstrumentCalibrationSpec_(**(values | {"fx_spot": True}))
    with pytest.raises(TypeError):
        dal.XccyInstrumentCalibrationSpec_(**values, unknown=1)
    with pytest.raises(TypeError):
        dal.XccyScriptImportSettings_(instrument_id="x", discount_route="USD.OIS", min_abs_annuity=True)
    with pytest.raises(dal.CalibrationError):
        dal.XccyInstrumentCalibrationSpec_(**(values | {"fx_spot": float("nan")}))


def test_candidate_refresh_and_fixed_quote_identity():
    values, instrument, quote = inputs()
    spec = dal.XccyInstrumentCalibrationSpec_(**values)
    before = dal.CalibrationInstrument_Get_Json(instrument)
    first = dal.Evaluate_XccyInstruments(spec, [0.003])
    second = dal.Evaluate_XccyInstruments(spec, [0.004])
    assert first.model_measures != second.model_measures
    assert dal.CalibrationInstrument_Get_Json(instrument) == before
    first_report = json.loads(dal.CalibrationReport_Get_Json(first.report))
    second_report = json.loads(dal.CalibrationReport_Get_Json(second.report))
    assert first_report["quotes"] == second_report["quotes"]
    assert first_report["quote_jacobian"] != second_report["quote_jacobian"]


def test_final_jacobian_failure_owns_completed_values_after_input_destruction():
    values, _, _ = inputs(solver=dal.CurveInstrumentSolver_(initial_guess=0.003))
    spec = dal.XccyInstrumentCalibrationSpec_(**values)
    options = dal.CurveInstrumentCalibrationOptions_(difference_step=1e-30, compute_quote_jacobian=True)
    with pytest.raises(dal.CalibrationError) as raised:
        dal.Calibrate_XccyInstruments(spec, options=options)
    error = raised.value
    del spec, values, options
    gc.collect()
    assert error.code == "CalibrationNumericalFailure"
    assert error.candidate is not None
    data = json.loads(dal.CalibrationReport_Get_Json(error.report))
    assert data["parameters"][0]["value"] == 0.003
    assert data["observations"][0]["model_value"] is not None
    assert data["observations"][0]["raw_residual"] is not None
    assert data["observations"][0]["observation_id"] == "row"
    assert data["summary"]["evaluations"] == 1
    assert data["summary"]["jacobian_evaluations"] == 0
    assert data["quote_jacobian"] is None
