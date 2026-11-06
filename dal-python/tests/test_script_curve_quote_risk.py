import gc
import threading

import pytest
import dal


def _spec():
    today = dal.Date_(2026, 10, 21)
    dates = [today, dal.Date_(2027, 10, 21), dal.Date_(2028, 10, 23)]
    convention = dal.RateIndexConvention_New(
        dal.PeriodLength_New("12M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS())
    observations = []
    for ordinal, name in enumerate(["first", "last"]):
        native = dal.Deposit_New(today, today, dates[ordinal + 1], 0.025, convention)
        instrument = dal.DepositScriptInstrument_New(native, dal.ScriptInstrumentImportSettings_(
            instrument_id=name, currency="USD", notional=100.0,
            discount_route="USD.OIS", min_abs_annuity=1e-12))
        quote = dal.CalibrationMarketQuote_New(name, 0.035 + 0.005 * ordinal, dal.CalibrationValueUnit_(kind="Rate"))
        observations.append(dal.CalibrationObservation_New(name, instrument, "ParRate", quote, 1e-12))
    return dal.CurveInstrumentCalibrationSpec_(
        geometry=dal.CurveInstrumentGeometry_(today=today, currency="USD", knot_dates=dates, parameterization="LOG_DISCOUNT"),
        market=dal.CurveInstrumentMarket_(routes=[dal.CurveScriptRoute_(id="USD.OIS")]),
        valuation=dal.ScriptValuationSettings_(evaluation_date=today, fixings=dal.MarketFixingSnapshot_New({})),
        observations=observations)


def _accuracy():
    return dal.ScriptCurveQuoteRiskAccuracy_(residual_absolute_limits=[1e-12, 1e-12],
        min_reciprocal_condition=1e-10, response_backward_error_limit=1e-12)


def test_ordered_owned_result_preserves_signed_pv_raw_and_decimal_dv01():
    spec = _spec()
    fit = dal.Calibrate_CurveInstruments(spec)
    snapshot = dal.ScriptCurveQuoteRiskSnapshot_New(fit, _accuracy())
    assert snapshot.structure_valid and snapshot.mapping_available and snapshot.risk_available
    request = dal.ScriptCurveQuoteRiskRequest_(quote_ids=["last", "first"])
    result = dal.CalibrationInstrument_ValueWithQuoteRisk(spec.observations[0].instrument, snapshot, spec, request=request)
    identity = snapshot.identity
    del snapshot, fit, spec, request
    gc.collect()
    assert result.available and result.status == "Available"
    assert result.currency == "USD"
    assert result.snapshot_identity == identity
    assert [q.id for q in result.quote_axis] == ["last", "first"]
    assert [q.ordinal for q in result.quote_axis] == [1, 0]
    assert result.quote_axis[1].unit.token == "Rate"
    assert result.quote_axis[1].convention.id == "DecimalRate"
    accrual = 365 / 360
    denominator = 1 + 0.035 * accrual
    assert result.pv == pytest.approx(-100 + 100 * (1 + 0.025 * accrual) / denominator, abs=1e-10)
    assert result.raw_sensitivity == pytest.approx(
        [0, -100 * (1 + 0.025 * accrual) * accrual / denominator**2], abs=1e-8)
    assert result.dv01 == [x * 1e-4 for x in result.raw_sensitivity]
    assert not result.diagnostics
    copied = result.raw_sensitivity
    copied.clear()
    assert len(result.raw_sensitivity) == 2


def _context(spec, **changes):
    values = dict(geometry=spec.geometry, market=spec.market, valuation=spec.valuation,
                  observations=spec.observations, solver=spec.solver)
    values.update(changes)
    return dal.CurveInstrumentCalibrationSpec_(**values)


def test_stale_context_is_owned_unavailable_and_malformed_context_throws_input_error():
    spec = _spec()
    snapshot = dal.ScriptCurveQuoteRiskSnapshot_New(dal.Calibrate_CurveInstruments(spec), _accuracy())
    instrument = spec.observations[0].instrument
    stale = _context(spec, solver=dal.CurveInstrumentSolver_(max_evaluations=201))
    result = dal.CalibrationInstrument_ValueWithQuoteRisk(instrument, snapshot, stale)
    assert not result.available
    assert result.diagnostics[0].code == "ScriptCurveQuoteRiskIdentityMismatch"
    malformed = _context(spec, valuation=dal.ScriptValuationSettings_(evaluation_date=spec.geometry.today))
    with pytest.raises(dal.CalibrationError) as raised:
        dal.CalibrationInstrument_ValueWithQuoteRisk(instrument, snapshot, malformed)
    assert raised.value.code == "InvalidScriptCurveQuoteRiskInput"
    del spec, stale, malformed, snapshot, instrument
    gc.collect()
    assert result.quote_axis[0].id == "first"
    assert result.diagnostics[0].field
    for field in ("pv", "raw_sensitivity", "dv01", "native_gradient"):
        with pytest.raises(RuntimeError):
            getattr(result, field)


def test_loose_fit_does_not_certify_numerical_risk():
    spec = _spec()
    loose = [dal.CalibrationObservation_New(o.id, o.instrument, o.measure_id, o.quote, 1.0) for o in spec.observations]
    spec = _context(spec, observations=loose)
    fit = dal.Calibrate_CurveInstruments(spec)
    assert fit.fit_within_tolerance
    snapshot = dal.ScriptCurveQuoteRiskSnapshot_New(fit, _accuracy())
    assert snapshot.structure_valid
    assert not snapshot.risk_available and not snapshot.mapping_available
    assert snapshot.numerics.reciprocal_condition is None
    assert snapshot.numerics.factorization_count == 0
    result = dal.CalibrationInstrument_ValueWithQuoteRisk(loose[0].instrument, snapshot, spec)
    del fit, spec, snapshot, loose
    gc.collect()
    assert not result.available
    assert result.diagnostics[0].code == "ScriptCurveQuoteRiskRootResidual"
    assert result.numerics.reciprocal_condition is None


@pytest.mark.parametrize("selection,code", [
    ({"quote_ids": ["first", "first"]}, "ScriptCurveQuoteRiskDuplicateSelection"),
    ({"quote_ids": ["First"]}, "ScriptCurveQuoteRiskUnknownSelection"),
    ({"live_quote_payoff": True}, "ScriptCurveQuoteRiskLiveQuoteUnsupported"),
    ({"measure_id": "ParRate"}, "ScriptCurveQuoteRiskUnsupportedMeasure"),
])
def test_request_rejections_are_owned(selection, code):
    spec = _spec()
    snapshot = dal.ScriptCurveQuoteRiskSnapshot_New(dal.Calibrate_CurveInstruments(spec), _accuracy())
    result = dal.CalibrationInstrument_ValueWithQuoteRisk(spec.observations[0].instrument, snapshot, spec,
        request=dal.ScriptCurveQuoteRiskRequest_(**selection))
    assert not result.available
    assert result.diagnostics[0].code == code


def test_strict_settings_and_readonly_copied_axes():
    with pytest.raises(TypeError):
        dal.ScriptCurveQuoteRiskAccuracy_(residual_absolute_limits=[True],
            min_reciprocal_condition=1e-10, response_backward_error_limit=1e-12)
    for values in ({"quote_ids": "first"}, {"live_quote_payoff": 1}, {"unknown": True}):
        with pytest.raises(TypeError):
            dal.ScriptCurveQuoteRiskRequest_(**values)
    with pytest.raises(TypeError):
        dal.ScriptCurveQuoteRiskSnapshot_New(None, _accuracy())
    spec = _spec()
    snapshot = dal.ScriptCurveQuoteRiskSnapshot_New(dal.Calibrate_CurveInstruments(spec), _accuracy())
    quotes = snapshot.quote_axis
    with pytest.raises(AttributeError):
        quotes[0].value = 0.0
    quotes.clear()
    assert len(snapshot.quote_axis) == 2
    assert len(snapshot.parameter_axis) == 2
    assert snapshot.parameter_axis[0].value is not None
    accuracy = snapshot.accuracy.residual_absolute_limits
    accuracy.clear()
    assert len(snapshot.accuracy.residual_absolute_limits) == 2


@pytest.mark.parametrize("hold_gil", [False, True])
def test_factories_release_gil_after_input_conversion(hold_gil):
    spec = _spec()
    fit = dal.Calibrate_CurveInstruments(spec)
    native = dal._dal

    def checked(operation):
        native._ScriptQuoteRiskGilBarrier_ArmForTesting()
        progressed = []

        def release():
            entered = native._ScriptQuoteRiskGilBarrier_AwaitEntryForTesting()
            progressed.append(entered and native._ScriptQuoteRiskGilBarrier_ReleaseForTesting())

        worker = threading.Thread(target=release)
        worker.start()
        try:
            return operation()
        finally:
            worker.join(timeout=6)
            assert not worker.is_alive()
            assert progressed == [True]

    if hold_gil:
        with pytest.raises(RuntimeError, match="ScriptQuoteRisk: Python GIL test barrier timed out"):
            checked(native._ScriptQuoteRiskGilBarrier_BlockWithGilForTesting)
        return
    snapshot = checked(lambda: dal.ScriptCurveQuoteRiskSnapshot_New(fit, _accuracy()))
    result = checked(lambda: dal.CalibrationInstrument_ValueWithQuoteRisk(spec.observations[0].instrument, snapshot, spec))
    assert result.available


def test_function_getters_match_typed_properties():
    spec = _spec()
    snapshot = dal.ScriptCurveQuoteRiskSnapshot_New(dal.Calibrate_CurveInstruments(spec), _accuracy())
    assert dal.ScriptCurveQuoteRiskSnapshot_Get_Status(snapshot) == snapshot.status
    result = dal.CalibrationInstrument_ValueWithQuoteRisk(spec.observations[0].instrument, snapshot, spec)
    assert dal.ScriptCurveQuoteRiskResult_Get_Status(result) == result.status
    assert dal.ScriptCurveQuoteRiskResult_Get_PV(result) == result.pv
    assert dal.ScriptCurveQuoteRiskResult_Get_RawSensitivity(result) == result.raw_sensitivity
    assert dal.ScriptCurveQuoteRiskResult_Get_DV01(result) == result.dv01


def test_basis_spread_uses_decimal_units_and_fixed_background_curves():
    today, end = dal.Date_(2026, 10, 21), dal.Date_(2027, 10, 21)

    def index(tenor):
        value = dal.RateIndexConvention_()
        value.day_basis = dal.DayBasis_New("ACT_360")
        value.business_day_convention = dal.BizDayConvention_.UNADJUSTED
        value.use_projection_curve = True
        value.forecast_tenor = dal.PeriodLength_New(tenor)
        return value

    def leg(tenor):
        value = dal.RateLegConvention_New(dal.PeriodLength_New(tenor), dal.DayBasis_New("ACT_360"))
        value.business_day_convention = value.payment_convention = dal.BizDayConvention_.UNADJUSTED
        return value

    native = dal.BasisSwap_New(today, today, end, -.001, index("3M"), leg("3M"), index("6M"), leg("6M"))
    instrument = dal.BasisSwapScriptInstrument_New(native, dal.ScriptInstrumentImportSettings_(
        instrument_id="basis", currency="USD", notional=100., discount_route="USD.OIS", min_abs_annuity=1e-12))
    quote = dal.CalibrationMarketQuote_New("basis-q", -.001, dal.CalibrationValueUnit_(kind="BasisSpread"))
    observation = dal.CalibrationObservation_New("basis-o", instrument, "BasisSpread", quote, 1e-13)
    spec = dal.CurveInstrumentCalibrationSpec_(
        geometry=dal.CurveInstrumentGeometry_(today=today, currency="USD", knot_dates=[today, end],
            parameterization="LOG_DISCOUNT", calibrate_discount_curve=False, target_tenor=dal.PeriodLength_New("3M")),
        valuation=dal.ScriptValuationSettings_(evaluation_date=today, fixings=dal.MarketFixingSnapshot_New({})),
        market=dal.CurveInstrumentMarket_(routes=[dal.CurveScriptRoute_(id="USD.OIS")] + [
            dal.CurveScriptRoute_(id=f"IR:USD,LIBOR_{tenor}_LCH", projection=True, tenor=dal.PeriodLength_New(tenor)) for tenor in ("3M", "6M")],
            discount_curves={"OIS": dal.DiscountPWC_New("discount", "USD", [today], [.02])},
            forward_curves={dal.PeriodLength_New("6M"): dal.DiscountPWC_New("six-month", "USD", [today], [.045])}),
        observations=[observation])
    accuracy = dal.ScriptCurveQuoteRiskAccuracy_(residual_absolute_limits=[1e-12],
        min_reciprocal_condition=1e-10, response_backward_error_limit=1e-12)
    snapshot = dal.ScriptCurveQuoteRiskSnapshot_New(dal.Calibrate_CurveInstruments(spec), accuracy)
    result = dal.CalibrationInstrument_ValueWithQuoteRisk(instrument, snapshot, spec)
    assert result.available
    assert result.quote_axis[0].value == -.001
    assert result.quote_axis[0].unit.token == "BasisSpread"
    assert result.quote_axis[0].convention.id == "DecimalSpread"
    assert result.raw_sensitivity[0] != 0
    assert result.dv01[0] == result.raw_sensitivity[0] * 1e-4
