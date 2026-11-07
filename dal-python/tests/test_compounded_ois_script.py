import gc
import json
import math
import threading

import pytest
import dal


def ois_request(*, method="None", policy="Model", direction="PayFixed", notional=100.0,
                missing=False, parameters=None, measure_ids=None):
    today, a, b, c, pay = [dal.Date_(2024, m, d) for m, d in [(3, 1), (2, 29), (3, 5), (3, 6), (3, 8)]]
    obs = [
        dal.CompoundedOisObservationRow_(start=a, end=today, fixing_time=dal.DateTime_(a, 0), accrual=1 / 360,
                                        coupon_id="coupon", rate_start=a, rate_end=today),
        dal.CompoundedOisObservationRow_(start=today, end=b, fixing_time=dal.DateTime_(today, 0), accrual=4 / 360,
                                        coupon_id="coupon", rate_start=today, rate_end=b),
        dal.CompoundedOisObservationRow_(start=b, end=c, fixing_time=dal.DateTime_(b, 0), accrual=1 / 360,
                                        coupon_id="coupon", rate_start=b, rate_end=c),
    ]
    calendar = dal.HolidayCenterData_(center="explicit", holidays=[dal.Date_(2024, 3, 4)])
    spec = dal.CompoundedOisScriptSpec_(
        instrument_id="python-ois", direction=direction, discount_route="USD.OIS", projection_route="USD.SOFR",
        fixing_index="USD-SOFR", notional=notional, coupon=0.02, currency="USD", collateral_currency="USD",
        collateral_type="OIS", observation_basis="ACT_360", fixed_basis="ACT_365F", calendar=calendar,
        fixed_leg=[dal.CompoundedOisFixedRow_(start=a, end=c, payment=pay, accrual=6 / 365)],
        coupons=[dal.CompoundedOisCouponRow_(coupon_id="coupon", start=a, end=c, payment=pay)], observations=obs)
    history = {} if missing else {"USD-SOFR": [(dal.DateTime_(a, 0), -0.012), (dal.DateTime_(today, 0), 0.021),
                                               (dal.DateTime_(b, 0), 9.0)]}
    valuation = dal.ScriptValuationSettings_(evaluation_date=today, today_fixing=policy,
                                            fixings=dal.MarketFixingSnapshot_New(history))
    tables = dal.CompoundedOis_Prepare(spec, valuation)
    instrument = dal.CompoundedOisScriptInstrument_New(tables)
    geometry = dal.CurveInstrumentGeometry_(today=today, currency="USD", knot_dates=[today, today.AddDays(20)],
        parameterization="LOG_DISCOUNT", calibrate_discount_curve=False, target_overnight_route="USD.SOFR")
    market = dal.CurveInstrumentMarket_(routes=[
        dal.CurveScriptRoute_(id="USD.OIS", use_target_curve=True),
        dal.CurveScriptRoute_(id="USD.SOFR", projection=True, overnight=True)])
    kwargs = dict(geometry=geometry, market=market, valuation=valuation, compounded_ois=tables,
                  parameters=[-0.002] if parameters is None else parameters, derivative_method=method)
    if measure_ids is not None:
        kwargs["measure_ids"] = measure_ids
    request = dal.CurveScriptEvaluationRequest_(**kwargs)
    return instrument, request, spec


def oracle(theta=-0.002, historical_today=False, notional=100.0, direction="PayFixed"):
    growth = (1 - 0.012 / 360) * (1 + 4 * 0.021 / 360 if historical_today else math.exp(-theta * 4 / 20))
    floating = growth * math.exp(-theta / 20) - 1
    df = math.exp(theta * 7 / 20)
    price = notional * (floating - 0.02 * 6 / 365) * df
    return [price if direction == "PayFixed" else -price, floating / (6 / 365)]


@pytest.mark.parametrize("method", ["None", "FiniteDifference", "AAD"])
@pytest.mark.parametrize("policy", ["Model", "RequireHistorical"])
def test_ois_oracle_native_axes_and_owned_results(method, policy):
    instrument, request, spec = ois_request(method=method, policy=policy)
    result = dal.CurveScriptInstrument_Evaluate(instrument, request)
    expected = oracle(historical_today=policy == "RequireHistorical")
    assert result.values == pytest.approx(expected, abs=1e-10)
    assert result.measure_ids == ["Price", "ParRate"]
    assert result.measure_units == ["Price:USD", "Rate"]
    assert len(result.free_parameters) == 1
    assert result.parameters == [-0.002]
    assert result.derivative_method == method
    assert result.observations[0].source == "Historical"
    assert result.observations[1].source == ("Historical" if policy == "RequireHistorical" else "Projection")
    assert result.observations[2].source == "Projection"
    assert result.observations[1].accrual == 4 / 360
    assert result.observations[1].rate_accrual == 4 / 360
    assert len(result.cashflows) == 2
    assert all(row.included and not row.settled for row in result.cashflows)
    assert sum(row.pv for row in result.cashflows) == pytest.approx(expected[0], abs=1e-10)
    assert result.context_identity and result.candidate_identity
    if method == "None":
        assert result.native_partials.Rows() == 0
        with pytest.raises(RuntimeError, match="unavailable"):
            dal.CurveScriptEvaluation_Get_NativePartials(result)
    else:
        partials = dal.CurveScriptEvaluation_Get_NativePartials(result)
        previous = None
        for h in [1e-4, 5e-5]:
            plus = oracle(-0.002 + h, policy == "RequireHistorical")
            minus = oracle(-0.002 - h, policy == "RequireHistorical")
            fd = [(a - b) / (2 * h) for a, b in zip(plus, minus)]
            if previous is not None:
                assert fd == pytest.approx(previous, abs=1e-7, rel=1e-5)
            assert [partials[i, 0] for i in range(2)] == pytest.approx(fd, abs=1e-7, rel=1e-5)
            previous = fd
    report = result.report
    del instrument, request, spec
    gc.collect()
    assert dal.CurveScriptEvaluation_Get_Value(result, "Price") == pytest.approx(expected[0], abs=1e-10)
    assert json.loads(dal.CalibrationReport_Get_Json(report))["summary"]["quote_risk_available"] is False


def test_ois_direction_notional_and_readonly_copies():
    instrument, request, spec = ois_request(direction="ReceiveFixed", notional=1000.0)
    result = dal.CurveScriptInstrument_Evaluate(instrument, request)
    assert result.values == pytest.approx(oracle(notional=1000.0, direction="ReceiveFixed"), abs=1e-10)
    for value, name in [(request, "parameters"), (spec, "notional"), (result, "values"), (result.cashflows[0], "pv")]:
        with pytest.raises(AttributeError):
            setattr(value, name, 0)
    copied = result.values
    copied.clear()
    assert len(result.values) == 2
    copied = spec.observations
    copied.clear()
    assert len(spec.observations) == 3
    assert request.evaluation == "AutoFast" and request.difference_step == 1e-6
    assert request.market.routes[1].overnight
    assert request.market.routes[1].tenor is None
    assert request.geometry.target_tenor is None
    assert request.geometry.target_overnight_route == "USD.SOFR"


def test_ois_owned_missing_fixing_error_and_strict_inputs():
    instrument, request, spec = ois_request(missing=True)
    with pytest.raises(dal.CalibrationError) as raised:
        dal.CurveScriptInstrument_Evaluate(instrument, request)
    error = raised.value
    del instrument, request, spec, raised
    gc.collect()
    assert error.code == "MissingFixing"
    data = json.loads(dal.CalibrationReport_Get_Json(error.report))
    assert data["errors"][0]["instrument_id"] == "python-ois"
    assert data["errors"][0]["field"] is not None
    with pytest.raises(TypeError):
        dal.CompoundedOisFixedRow_(start=True, end=True, payment=True, accrual=True)
    with pytest.raises(TypeError):
        dal.CurveScriptEvaluationRequest_(unknown=True)


def test_ois_requested_measure_only_and_candidate_errors():
    instrument, request, _ = ois_request(measure_ids=["Price"])
    result = dal.CurveScriptInstrument_Evaluate(instrument, request)
    assert result.measure_ids == ["Price"]
    with pytest.raises(RuntimeError, match="not requested"):
        dal.CurveScriptEvaluation_Get_Value(result, "ParRate")
    instrument, request, _ = ois_request(parameters=[])
    with pytest.raises(dal.CalibrationError):
        dal.CurveScriptInstrument_Evaluate(instrument, request)


def test_ois_gil_release_after_input_copy():
    instrument, request, _ = ois_request()
    errors = []

    def release():
        try:
            assert dal._dal._CurveScriptGilBarrier_AwaitEntryForTesting()
            assert dal._dal._CurveScriptGilBarrier_ReleaseForTesting()
        except BaseException as error:
            errors.append(error)

    dal._dal._CurveScriptGilBarrier_ArmForTesting()
    worker = threading.Thread(target=release)
    worker.start()
    result = dal.CurveScriptInstrument_Evaluate(instrument, request)
    worker.join(timeout=10)
    assert not worker.is_alive() and not errors
    assert result.values == pytest.approx(oracle(), abs=1e-10)


def test_ois_calibration_shares_tables_and_p1_quote_risk_rejects():
    instrument, request, _ = ois_request(method="AAD")
    quote = dal.CalibrationMarketQuote_New("q", 0.025, dal.CalibrationValueUnit_(kind="Rate"))
    observation = dal.CalibrationObservation_New("par", instrument, "ParRate", quote, 1e-10)
    spec = dal.CurveInstrumentCalibrationSpec_(
        geometry=request.geometry, market=request.market, valuation=request.valuation,
        compounded_ois=[request.compounded_ois], observations=[observation])
    options = dal.CurveInstrumentCalibrationOptions_(jacobian_mode="AAD")
    fit = dal.Calibrate_CurveInstruments(spec, options=options)
    assert fit.model_measures == pytest.approx([0.025], abs=1e-10)
    assert len(spec.compounded_ois) == 1
    accuracy = dal.ScriptCurveQuoteRiskAccuracy_(residual_absolute_limits=[1e-12],
        min_reciprocal_condition=1e-10, response_backward_error_limit=1e-12)
    snapshot = dal.ScriptCurveQuoteRiskSnapshot_New(fit, accuracy)
    assert not snapshot.risk_available and not snapshot.mapping_available
    assert snapshot.diagnostics[0].code == "ScriptCurveQuoteRiskUnsupportedCapability"
    # Per-PV admission is checked even against an ordinary valid P1 snapshot.
    from test_script_curve_quote_risk import _spec, _accuracy
    p1 = _spec()
    p1_snapshot = dal.ScriptCurveQuoteRiskSnapshot_New(dal.Calibrate_CurveInstruments(p1), _accuracy())
    rejected = dal.CalibrationInstrument_ValueWithQuoteRisk(instrument, p1_snapshot, p1)
    assert not rejected.available
    assert rejected.diagnostics[0].code == "ScriptCurveQuoteRiskUnsupportedCapability"


def test_ois_generic_backend_and_mc_stay_in_their_admitted_domains():
    instrument, request, _ = ois_request()
    generic = dal.CurveScriptEvaluationRequest_(
        geometry=request.geometry, market=request.market, valuation=request.valuation,
        parameters=request.parameters, compounded_ois=request.compounded_ois, evaluation="GenericDeterministic")
    assert dal.CurveScriptInstrument_Evaluate(instrument, generic).values == pytest.approx(oracle(), abs=1e-10)
    mc = dal.CalibrationMonteCarloRequest_(
        measure_id="Price", valuation=request.valuation,
        sampling=dal.CalibrationMonteCarloSampling_(paths_per_batch=8, batches=2, seed=1024,
                                                   stream_version="dal.mrg32-normal-paths/1"),
        simulation=dal.CalibrationSimulationSettings_(smooth=0))
    today = request.geometry.today
    curve = dal.GSRCurveData_New("curve", today, "USD", [today, today.AddDays(20)], [0.0, -0.002], [], dal.DoubleMatrix_(0, 0))
    volatility = dal.MultiFactorGSRVolData_New("vol", ["level"], [today], dal.DoubleMatrix_([[0.02]]),
        [today], dal.DoubleMatrix_([[1.0]]), dal.DoubleMatrix_([[1.0]]))
    model = dal.MultiFactorGSRModelData_New("matched", curve, volatility)
    with pytest.raises(dal.CalibrationError) as raised:
        dal.CalibrationInstrument_ValueByMonteCarlo(instrument, model, mc)
    assert raised.value.code == "UnsupportedCompoundedOisContext"
    assert raised.value.diagnostic.field == "/context/compounded_ois"
    unsupported = dal.CurveScriptEvaluationRequest_(
        geometry=request.geometry, market=request.market, valuation=request.valuation,
        parameters=request.parameters, compounded_ois=request.compounded_ois, evaluation="ScriptMC")
    with pytest.raises(dal.CalibrationError) as raised:
        dal.CurveScriptInstrument_Evaluate(instrument, unsupported)
    assert raised.value.code == "UnsupportedFastEvaluator"
    assert raised.value.diagnostic.field == "/options/evaluation"


@pytest.mark.parametrize("serial_hour", [1, 12, 23])
def test_ois_nonmidnight_fixing_is_rejected_with_owned_report(serial_hour):
    _, request, spec = ois_request()
    original = spec.observations[0]
    wrong = dal.CompoundedOisObservationRow_(
        start=original.start, end=original.end, fixing_time=dal.DateTime_(original.start, serial_hour),
        accrual=original.accrual, coupon_id=original.coupon_id, rate_start=original.rate_start, rate_end=original.rate_end)
    values = {key: getattr(spec, key) for key in [
        "instrument_id", "direction", "discount_route", "projection_route", "fixing_index", "notional", "coupon",
        "currency", "collateral_currency", "collateral_type", "observation_basis", "fixed_basis", "calendar", "fixed_leg", "coupons"]}
    values["observations"] = [wrong] + spec.observations[1:]
    invalid = dal.CompoundedOisScriptSpec_(**values)
    with pytest.raises(dal.CalibrationError) as raised:
        dal.CompoundedOis_Prepare(invalid, request.valuation)
    assert "fixing_time" in raised.value.diagnostic.field


@pytest.mark.parametrize("discount_target", [True, False])
def test_ois_explicit_passive_curve_maps_accept_short_real_dependency_horizons(discount_target):
    instrument, request, _ = ois_request()
    today = request.geometry.today
    passive = dal.DiscountLogDF_New("short-background", "USD", [today, today.AddDays(8)],
                                    [0.0, -0.0008 if discount_target else -0.0012])
    routes = [dal.CurveScriptRoute_(id="USD.OIS"),
              dal.CurveScriptRoute_(id="USD.SOFR", projection=True, overnight=True)]
    mapping = {"USD.SOFR": passive} if discount_target else {"OIS": passive}
    market = dal.CurveInstrumentMarket_(routes=routes, **{
        "overnight_curves" if discount_target else "discount_curves": mapping})
    mapping.clear()
    geometry = dal.CurveInstrumentGeometry_(
        today=today, currency="USD", knot_dates=[today, today.AddDays(20)], parameterization="LOG_DISCOUNT",
        calibrate_discount_curve=discount_target, target_overnight_route="" if discount_target else "USD.SOFR")
    theta = -0.003 if discount_target else -0.002
    owned = dal.CurveScriptEvaluationRequest_(
        geometry=geometry, market=market, valuation=request.valuation, parameters=[theta],
        compounded_ois=request.compounded_ois, derivative_method="AAD")
    result = dal.CurveScriptInstrument_Evaluate(instrument, owned)
    expected = oracle()
    expected[0] *= math.exp(-0.00035)
    assert result.values == pytest.approx(expected, abs=1e-10)
    if discount_target:
        assert result.native_partials[0, 0] == pytest.approx(expected[0] * 7 / 20, abs=1e-10)
        assert result.native_partials[1, 0] == 0.0
    assert len(market.overnight_curves if discount_target else market.discount_curves) == 1


def test_ois_settled_rows_keep_absent_amount_discount_and_rate():
    _, _, spec = ois_request()
    today = dal.Date_(2024, 3, 9)
    valuation = dal.ScriptValuationSettings_(evaluation_date=today, fixings=dal.MarketFixingSnapshot_New({}))
    prepared = dal.CompoundedOis_Prepare(spec, valuation)
    instrument = dal.CompoundedOisScriptInstrument_New(prepared)
    geometry = dal.CurveInstrumentGeometry_(today=today, currency="USD", knot_dates=[today, today.AddDays(20)],
        parameterization="LOG_DISCOUNT", calibrate_discount_curve=False, target_overnight_route="USD.SOFR")
    market = dal.CurveInstrumentMarket_(routes=[
        dal.CurveScriptRoute_(id="USD.OIS", use_target_curve=True),
        dal.CurveScriptRoute_(id="USD.SOFR", projection=True, overnight=True)])
    kwargs = dict(geometry=geometry, market=market, valuation=valuation, parameters=[-0.002], compounded_ois=prepared)
    result = dal.CurveScriptInstrument_Evaluate(instrument, dal.CurveScriptEvaluationRequest_(**kwargs, measure_ids=["Price"]))
    assert result.values == [0.0]
    assert all(row.settled and not row.included and row.amount is None and row.discount is None and row.pv == 0
               for row in result.cashflows)
    assert all(row.source == "Settled" and row.rate is None for row in result.observations)
    with pytest.raises(dal.CalibrationError):
        dal.CurveScriptInstrument_Evaluate(instrument, dal.CurveScriptEvaluationRequest_(**kwargs))
