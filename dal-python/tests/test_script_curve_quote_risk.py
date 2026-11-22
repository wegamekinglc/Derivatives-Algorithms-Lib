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


def _lookback_risk(high_floor=False, quote_ids=()):
    def date(day):
        return dal.Date_(2024, 3, day)

    valuation = dal.ScriptValuationSettings_(evaluation_date=date(1), fixings=dal.MarketFixingSnapshot_New(
        {"USD-SOFR": [(dal.DateTime_(dal.Date_(2024, 2, 29), 0), -.012)]}))
    calendar = dal.HolidayCenterData_(center="explicit", holidays=[date(4)], work_weekends=[date(9)])
    geometry = dal.CurveInstrumentGeometry_(today=date(1), currency="USD", knot_dates=[date(1), date(5), date(20)],
        parameterization="LOG_DISCOUNT", calibrate_discount_curve=False, target_overnight_route="USD.SOFR")
    market = dal.CurveInstrumentMarket_(routes=[dal.CurveScriptRoute_(id="USD.OIS", use_target_curve=True),
        dal.CurveScriptRoute_(id="USD.SOFR", projection=True, overnight=True)])
    tables, observations = [], []
    for name, quote in [("short", .030179570014802), ("long", .0362870928259698), ("registered-par", None)]:
        registered = quote is None
        periods = [(1, 6, 6)] if name == "short" else [(1, 6, 6), (6, 11 if registered else 12, 13)]
        fixed = [(1, 11, 15)] if registered else periods
        convention = (dal.CompoundedOisObservationConvention_("Lookback", lookback_business_days=1) if registered else
                      dal.CompoundedOisObservationConvention_("None"))
        trade = dal.CompoundedOisScriptSpec_(instrument_id=name, direction="PayFixed", notional=100., coupon=.02,
            currency="USD", collateral_currency="USD", collateral_type="OIS", discount_route="USD.OIS",
            projection_route="USD.SOFR", fixing_index="USD-SOFR", observation_basis="ACT_360", fixed_basis="ACT_365F",
            calendar=calendar, rule_id="dal.compounded-ois-cashflows/2", observation_convention=convention,
            fixed_leg=[dal.CompoundedOisFixedRow_(start=date(a), end=date(b), payment=date(p), accrual=(b-a)/365)
                       for a, b, p in fixed],
            coupons=[dal.CompoundedOisCouponRow_(coupon_id=key, start=date(a), end=date(b), payment=date(p))
                     for key, (a, b, p) in zip(("first", "second"), periods)],
            **({"min_abs_annuity": 100.} if high_floor and registered else {}))
        tables.append(dal.CompoundedOis_Prepare(trade, valuation))
        instrument = dal.CompoundedOisScriptInstrument_New(tables[-1])
        if quote is not None:
            observations.append(dal.CalibrationObservation_New(name, instrument, "ParRate",
                dal.CalibrationMarketQuote_New(name, quote, dal.CalibrationValueUnit_(kind="Rate")), 1e-13))
    spec = dal.CurveInstrumentCalibrationSpec_(geometry=geometry, market=market, valuation=valuation,
        observations=observations, compounded_ois=tables, solver=dal.CurveInstrumentSolver_(initial_guess_per_node=[-.0003, -.002]))
    fit = dal.Calibrate_CurveInstruments(spec, options=dal.CurveInstrumentCalibrationOptions_(jacobian_mode="AAD"))
    snapshot = dal.ScriptCurveQuoteRiskSnapshot_New(fit, dal.ScriptCurveQuoteRiskAccuracy_(residual_absolute_limits=[1e-12, 1e-12],
        min_reciprocal_condition=1e-10, response_backward_error_limit=1e-12, relative_pivot_tolerance=1e-12))
    selection = {"request": dal.CompoundedOisParRateRequest_(quote_ids=quote_ids)} if quote_ids else {}
    return snapshot.identity, dal.CompoundedOisParRate_ValueWithQuoteRisk(instrument, tables[-1], snapshot, spec, **selection)


def test_lookback_par_rate_owns_signed_selected_buckets():
    identity, result = _lookback_risk(quote_ids=["long", "short"])
    gc.collect()
    assert identity and result.snapshot_identity == identity
    assert result.available and result.diagnostics == []
    assert result.par_rate == pytest.approx(.01856212179263, abs=1e-12, rel=0)
    assert [(q.ordinal, q.id) for q in result.quote_axis] == [(1, "long"), (0, "short")]
    assert result.sensitivity_bp_per_bp == pytest.approx([.87138316352446, -.27115795540455], abs=1e-10, rel=0)
    with pytest.raises(AttributeError):
        result.par_rate = 0


def test_invalid_par_annuity_keeps_axis_and_guards_all_numeric_getters():
    identity, result = _lookback_risk(high_floor=True)
    assert not result.available and result.snapshot_identity == identity
    assert [q.id for q in result.quote_axis] == ["short", "long"]
    error, = result.diagnostics
    assert (error.code, error.field, error.instrument_id, error.measure_id) == (
        "InvalidCalibrationAnnuity", "/annuity", "registered-par", "ParRate")
    for name in ("par_rate", "sensitivity_bp_per_bp", "native_gradient", "transpose_backward_errors"):
        with pytest.raises(RuntimeError, match="unavailable"):
            getattr(result, name)


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


def test_pv_only_snapshot_skips_mapping_without_changing_pv_risk():
    spec = _spec()
    fit = dal.Calibrate_CurveInstruments(spec)
    accuracy = dal.ScriptCurveQuoteRiskAccuracy_(residual_absolute_limits=[1e-12, 1e-12],
        min_reciprocal_condition=1e-10, response_backward_error_limit=1e-12, pv_only=True)
    snapshot = dal.ScriptCurveQuoteRiskSnapshot_New(fit, accuracy)
    full = dal.ScriptCurveQuoteRiskSnapshot_New(fit, _accuracy())
    assert snapshot.risk_available and not snapshot.mapping_available
    assert snapshot.accuracy.pv_only
    assert snapshot.numerics.transpose_backward_errors == []
    assert snapshot.numerics.response_backward_errors == []
    risk = dal.CalibrationInstrument_ValueWithQuoteRisk(spec.observations[0].instrument, snapshot, spec)
    expected = dal.CalibrationInstrument_ValueWithQuoteRisk(spec.observations[0].instrument, full, spec)
    assert risk.available
    assert risk.pv == expected.pv
    assert risk.native_gradient == expected.native_gradient
    assert risk.raw_sensitivity == expected.raw_sensitivity
    assert risk.dv01 == expected.dv01


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
