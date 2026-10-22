import gc
import json
import threading

import pytest
import dal


def test_typed_import_binding_and_owned_report():
    today, end = dal.Date_(2026, 10, 21), dal.Date_(2027, 10, 21)
    convention = dal.RateIndexConvention_New(dal.PeriodLength_New("12M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS())
    native = dal.Deposit_New(today, today, end, 0.025, convention)
    settings = dal.ScriptInstrumentImportSettings_(instrument_id="deposit", currency="USD", notional=100.0,
                                                  discount_route="USD.OIS", min_abs_annuity=1e-12)
    instrument = dal.DepositScriptInstrument_New(native, settings)
    quote = dal.CalibrationMarketQuote_New("q", 0.035, dal.CalibrationValueUnit_(kind="Rate"))
    observation = dal.CalibrationObservation_New("o", instrument, "ParRate", quote, 1e-10)
    geometry = dal.CurveInstrumentGeometry_(today=today, currency="USD", knot_dates=[today, end], parameterization="LOG_DISCOUNT")
    valuation = dal.ScriptValuationSettings_(evaluation_date=today, fixings=dal.MarketFixingSnapshot_New({}))
    market = dal.CurveInstrumentMarket_(routes=[dal.CurveScriptRoute_(id="USD.OIS")])
    spec = dal.CurveInstrumentCalibrationSpec_(geometry=geometry, market=market, valuation=valuation, observations=[observation])
    result = dal.Calibrate_CurveInstruments(spec)
    report = result.report
    del result, spec, observation, quote, instrument, geometry, market, valuation, settings, native
    gc.collect()
    data = json.loads(dal.CalibrationReport_Get_Json(report))
    assert data["summary"]["quote_risk_available"] is False
    assert data["observations"][0]["instrument_id"] == "deposit"
    assert data["observations"][0]["model_value"] == pytest.approx(0.035, abs=1e-10)
    assert data["quote_jacobian"] is None
    assert data["quote_map"] == [[1.0]]


def test_new_settings_are_keyword_only_and_strict():
    with pytest.raises(TypeError):
        dal.CalibrationValueUnit_("Rate")
    with pytest.raises(TypeError):
        dal.CalibrationMonteCarloSampling_(paths_per_batch=True, batches=2, seed=1, stream_version="dal.mrg32-normal-paths/1")
    with pytest.raises(TypeError):
        dal.CurveInstrumentCalibrationOptions_(unknown=True)
    with pytest.raises(TypeError):
        dal.CalibrationMarketQuote_New("q", True, dal.CalibrationValueUnit_(kind="Rate"))
    with pytest.raises(dal.CalibrationError) as raised:
        dal.CalibrationMarketQuote_New("q", float("inf"), dal.CalibrationValueUnit_(kind="Rate"))
    assert raised.value.code == "InvalidCalibrationQuote"
    report = raised.value.report
    gc.collect()
    assert json.loads(dal.CalibrationReport_Get_Json(report))["errors"][0]["field"] == "/value"


def _deposit():
    today, end = dal.Date_(2026, 10, 21), dal.Date_(2027, 10, 21)
    convention = dal.RateIndexConvention_New(dal.PeriodLength_New("12M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS())
    native = dal.Deposit_New(today, today, end, 0.025, convention)
    instrument = dal.DepositScriptInstrument_New(native, dal.ScriptInstrumentImportSettings_(
        instrument_id="deposit", currency="USD", notional=100.0, discount_route="USD.OIS", min_abs_annuity=1e-12))
    valuation = dal.ScriptValuationSettings_(evaluation_date=today, fixings=dal.MarketFixingSnapshot_New({}))
    return instrument, valuation


def test_mc_request_failure_owns_instrument_context_after_unwind():
    instrument, valuation = _deposit()
    request = dal.CalibrationMonteCarloRequest_(measure_id="Price", valuation=valuation,
        sampling=dal.CalibrationMonteCarloSampling_(paths_per_batch=8, batches=2, seed=1024, stream_version="dal.mrg32-normal-paths/1"),
        simulation=dal.CalibrationSimulationSettings_(smooth=0), derivative_method="FiniteDifference")
    model = dal.BSModelData_New(100.0, 0.2, 0.05, 0)
    with pytest.raises(dal.CalibrationError) as raised:
        dal.CalibrationInstrument_ValueByMonteCarlo(instrument, model, request)
    error = raised.value
    del instrument, valuation, request, model
    gc.collect()
    assert error.code == "DerivativeUnavailable"
    assert error.diagnostic.instrument_id == "deposit"
    data = json.loads(dal.CalibrationReport_Get_Json(error.report))
    assert data["observations"][0]["instrument_id"] == "deposit"
    assert data["observations"][0]["model_value"] is None
    assert data["quote_jacobian"] is None


def test_fra_import_is_a_shared_definition_and_rejects_wrong_native_handle():
    today = dal.Date_(2026, 10, 21)
    start, end = today.AddDays(90), today.AddDays(180)
    convention = dal.RateIndexConvention_New(dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS())
    native = dal.FRA_New(today, start, end, 0.025, convention)
    settings = dal.ScriptInstrumentImportSettings_(instrument_id="fra", currency="USD", notional=100,
                                                  discount_route="USD.OIS", min_abs_annuity=1e-12)
    instrument = dal.FraScriptInstrument_New(native, end, settings)
    data = json.loads(dal.CalibrationInstrument_Get_Json(instrument))
    assert data["instrument_id"] == "fra"
    assert data["definition"]["measures"][1]["unit"] == "Rate"
    with pytest.raises(dal.CalibrationError):
        dal.SwapScriptInstrument_New(native, settings)


def test_binding_updates_and_getters_are_copied_and_strict():
    instrument, _ = _deposit()
    rate = dal.CalibrationValueUnit_(kind="Rate")
    values = [dal.CalibrationParameterValue_(key="K", value=0.03, unit=rate)]
    updates = dal.CalibrationParameterBindings_(values=values)
    rebound = dal.CalibrationInstrument_BindParameters(instrument, updates)
    values.clear()
    copied = updates.values
    copied.clear()
    assert instrument.binding[1].value == 0.025
    assert rebound.binding[1].value == 0.03
    assert len(updates.values) == 1
    assert dal.ScriptInstrumentDefinition_New(instrument.definition.spec).definition_identity == instrument.definition.definition_identity
    assert dal.CalibrationInstrument_LoadJson(dal.CalibrationInstrument_Get_Json(rebound)).binding_identity == rebound.binding_identity
    for rows in ([dal.CalibrationParameterValue_(key="UNKNOWN", value=1, unit=rate)],
                 [dal.CalibrationParameterValue_(key="K", value=1, unit=rate)] * 2,
                 [dal.CalibrationParameterValue_(key="ALPHA", value=365 / 360, unit=dal.CalibrationValueUnit_(kind="YearFraction"))]):
        with pytest.raises(dal.CalibrationError) as raised:
            dal.CalibrationInstrument_BindParameters(instrument, dal.CalibrationParameterBindings_(values=rows))
        assert raised.value.code == "InvalidCalibrationParameterBinding"
    quote = dal.CalibrationMarketQuote_New("q", 0.03, rate)
    with pytest.raises(TypeError):
        dal.CalibrationInstrument_BindParameters(quote, updates)
    with pytest.raises(dal.CalibrationError) as raised:
        dal.CalibrationMarketQuote_New("bad\x00id", 0.03, rate)
    assert raised.value.code == "InvalidCalibrationQuote"
    assert "\\u0000" in raised.value.diagnostic.quote_id
    with pytest.raises(AttributeError):
        quote.value = 1
    with pytest.raises(AttributeError):
        raised.value.code = "changed"


def _with_mc_gil_handshake(operation):
    ready, released = threading.Event(), threading.Event()
    errors = []

    def release():
        ready.set()
        try:
            assert dal._dal._CalibrationMcGilBarrier_AwaitEntryForTesting()
            assert dal._dal._CalibrationMcGilBarrier_ReleaseForTesting()
            released.set()
        except BaseException as error:
            errors.append(error)

    dal._dal._CalibrationMcGilBarrier_ArmForTesting()
    worker = threading.Thread(target=release)
    worker.start()
    try:
        assert ready.wait(timeout=5)
        return operation()
    finally:
        worker.join(timeout=6)
        assert not worker.is_alive()
        assert not errors
        assert released.is_set()


def test_mc_gil_handshake_rejects_a_native_wait_holding_the_gil():
    with pytest.raises(RuntimeError, match="CalibrationMC: Python GIL test barrier timed out"):
        _with_mc_gil_handshake(dal._dal._CalibrationMcGilBarrier_BlockWithGilForTesting)


def test_mc_native_axes_are_reproducible_and_call_releases_gil():
    instrument, valuation = _deposit()
    today = valuation.evaluation_date
    curve = dal.GSRCurveData_New("curve", today, "USD", [today, today.AddDays(730)], [0.0, -0.08], [], dal.DoubleMatrix_(0, 0))
    volatility = dal.MultiFactorGSRVolData_New("vol", ["level"], [today], dal.DoubleMatrix_([[0.02]]),
        [today], dal.DoubleMatrix_([[1.0]]), dal.DoubleMatrix_([[1.0]]))
    model = dal.MultiFactorGSRModelData_New("matched", curve, volatility)
    request = dal.CalibrationMonteCarloRequest_(measure_id="Price", valuation=valuation,
        sampling=dal.CalibrationMonteCarloSampling_(paths_per_batch=8192, batches=2, seed=1024, stream_version="dal.mrg32-normal-paths/1"),
        simulation=dal.CalibrationSimulationSettings_(smooth=0), derivative_method="AAD")
    result = _with_mc_gil_handshake(lambda: dal.CalibrationInstrument_ValueByMonteCarlo(instrument, model, request))
    repeated = dal.CalibrationInstrument_ValueByMonteCarlo(instrument, model, request)
    assert repeated.mean == result.mean
    assert repeated.standard_error == result.standard_error
    data = json.loads(dal.CalibrationReport_Get_Json(result.report))
    assert data["observations"][0]["backend"] == "ScriptMC"
    assert data["observations"][0]["derivative_method"] == "AAD"
    assert data["summary"]["evaluations"] == 2
    assert data["summary"]["quote_risk_available"] is False
    assert len(data["quote_jacobian"]) == 1
    assert len(data["quote_jacobian"][0]) == len(data["parameters"])
    assert data["execution"]["sampling"]["first_paths"] == [0.0, 8192.0]


def test_settings_accept_existing_typed_core_conventions():
    tenor = dal.PeriodLength_New("3M")
    route = dal.CurveScriptRoute_(id="USD.3M", projection=True, collateral=dal.CollateralType_OIS(), tenor=tenor)
    assert route.tenor == repr(tenor)
    geometry = dal.CurveInstrumentGeometry_(today=dal.Date_(2026, 10, 21), currency="USD", knot_dates=[],
        target_tenor=tenor, libor_basis=dal.DayBasis_New("ACT_360"))
    assert geometry.target_tenor == repr(tenor)


@pytest.mark.parametrize("field,text_key,typed_key", [
    ("discount_curves", "OIS", dal.CollateralType_OIS()),
    ("forward_curves", "QUARTERLY", dal.PeriodLength_New("3M")),
])
@pytest.mark.parametrize("reverse", [False, True])
def test_curve_mapping_rejects_normalized_duplicates(field, text_key, typed_key, reverse):
    instrument, valuation = _deposit()
    today = valuation.evaluation_date

    def curve(rate):
        quote = dal.CalibrationMarketQuote_New("q", rate, dal.CalibrationValueUnit_(kind="Rate"))
        observation = dal.CalibrationObservation_New("o", instrument, "ParRate", quote, 1e-10)
        geometry = dal.CurveInstrumentGeometry_(today=today, currency="USD", knot_dates=[today, today.AddDays(365)],
                                               parameterization="LOG_DISCOUNT")
        market = dal.CurveInstrumentMarket_(routes=[dal.CurveScriptRoute_(id="USD.OIS")])
        return dal.Calibrate_CurveInstruments(dal.CurveInstrumentCalibrationSpec_(
            geometry=geometry, market=market, valuation=valuation, observations=[observation])).curve

    first, second = curve(.03), curve(.04)
    for key in (text_key, typed_key):
        market = dal.CurveInstrumentMarket_(routes=[], **{field: {key: first}})
        assert list(getattr(market, field).values()) == [first]
    entries = [(text_key, first), (typed_key, second)]
    with pytest.raises(dal.CalibrationError) as raised:
        dal.CurveInstrumentMarket_(routes=[], **{field: dict(reversed(entries) if reverse else entries)})
    assert raised.value.code == "InvalidCalibrationPlan"
    data = json.loads(dal.CalibrationReport_Get_Json(raised.value.report))
    assert data["errors"][0]["field"] == "/" + field
