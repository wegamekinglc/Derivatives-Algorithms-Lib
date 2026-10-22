import gc
import json
import threading

import dal
import pytest


def _fixture(g=0.02):
    today, expiry, maturity = dal.Date_(2026, 10, 21), dal.Date_(2027, 10, 21), dal.Date_(2028, 10, 21)
    curve = dal.GSRCurveData_New("curve", today, "USD", [today, maturity], [0.0, -0.08], [], dal.DoubleMatrix_(0, 0))
    vol = dal.MultiFactorGSRVolData_New("vol", ["level"], [today], dal.DoubleMatrix_([[g]]),
                                     [today], dal.DoubleMatrix_([[1.0]]), dal.DoubleMatrix_([[1.0]]))
    model = dal.MultiFactorGSRModelData_New("model", curve, vol)
    option = dal.BondOption_(expiry, maturity, 0.96)
    instrument = dal.EuropeanRateOptionScriptInstrument_New(option, dal.ScriptInstrumentImportSettings_(
        instrument_id="bond", currency="USD", notional=100.0, discount_route="USD.OIS"))
    target = 100 * dal.GSR_EuropeanOptionPrice(model, option).price
    quote = dal.CalibrationMarketQuote_New("quote", target, dal.CalibrationValueUnit_(kind="Price", currency="USD"))
    row = dal.CalibrationObservation_New("obs", instrument, "Price", quote, 0.1)
    valuation = dal.ScriptValuationSettings_(evaluation_date=today, fixings=dal.MarketFixingSnapshot_New({}))
    parameter = dal.GSRCalibrationParameter_New(factor=0, knot=0, lower=0.0, upper=0.1)
    return model, [row], [parameter], valuation


def test_gsr_settings_require_valuation_and_strict_keyword_types():
    with pytest.raises(TypeError, match="valuation"):
        dal.GSRInstrumentCalibrationSettings_()
    for value in (None, {}, True):
        with pytest.raises(TypeError, match="valuation"):
            dal.GSRInstrumentCalibrationSettings_(valuation=value)
    with pytest.raises(TypeError):
        dal.GSRCalibrationParameter_New(factor=True, knot=0, lower=0.0, upper=0.1)
    with pytest.raises(TypeError):
        dal.GSRCalibrationParameter_New(factor=0.0, knot=0, lower=0.0, upper=0.1)


@pytest.mark.parametrize("field,value", [("solver", None), ("evaluation", None), ("held_out_observations", None),
    ("max_paths_per_view", True), ("max_basket_evaluations", 1.0), ("compiled", 1), ("validation_sigma", False),
    ("unknown", 1)])
def test_gsr_settings_reject_wrong_shapes(field, value):
    _, _, _, valuation = _fixture()
    with pytest.raises(TypeError, match=field):
        dal.GSRInstrumentCalibrationSettings_(valuation=valuation, **{field: value})


def _mc_settings(valuation, **updates):
    solver = dal.GSRCalibrationSettings_()
    solver.max_iterations = 1
    fields = dict(valuation=valuation, solver=solver,
        evaluation=dal.CalibrationEvaluationOptions_(evaluation="ScriptMC", allow_monte_carlo_calibration=True),
        fit_sampling=dal.CalibrationMonteCarloSampling_(paths_per_batch=16, batches=2, seed=1024, stream_version="dal.mrg32-normal-paths/1"),
        validation_sampling=dal.CalibrationMonteCarloSampling_(paths_per_batch=32, batches=2, seed=1024, stream_version="dal.mrg32-normal-paths/1"),
        max_paths_per_view=96)
    fields.update(updates)
    return dal.GSRInstrumentCalibrationSettings_(**fields)


@pytest.mark.parametrize("field,value,path,code", [
    ("fit_sampling", None, "/settings/fit_sampling", "InvalidCalibrationPlan"),
    ("max_paths_per_view", 95, "/settings/max_paths_per_view", "CalibrationBudgetExceeded"),
    ("validation_sigma", -1.0, "/settings/validation_sigma", "InvalidCalibrationPlan"),
    ("validation_sigma", float("nan"), "/settings/validation_sigma", "InvalidCalibrationPlan"),
    ("max_paths_per_view", 2**40, "/settings/max_paths_per_view", "InvalidCalibrationPlan"),
])
def test_gsr_typed_invalid_settings_own_diagnostic(field, value, path, code):
    _, _, _, valuation = _fixture()
    with pytest.raises(dal.CalibrationError) as raised:
        _mc_settings(valuation, **{field: value})
    error = raised.value
    del valuation
    gc.collect()
    assert error.code == code
    assert error.diagnostic.field == path
    assert error.report is not None


def test_gsr_fast_result_copies_settings_and_preserves_legacy_entry():
    model, rows, parameters, valuation = _fixture()
    solver = dal.GSRCalibrationSettings_()
    settings = dal.GSRInstrumentCalibrationSettings_(valuation=valuation, solver=solver)
    solver.max_iterations = 0
    assert settings.solver.max_iterations == 100
    with pytest.raises(TypeError):
        dal.Calibrate_GSRInstruments(model, rows, parameters)
    with pytest.raises(TypeError):
        dal.Calibrate_GSRInstruments(model, rows, parameters, settings)
    result = dal.Calibrate_GSRInstruments(model, rows, parameters, settings=settings)
    del model, rows, parameters, valuation, settings
    gc.collect()
    data = json.loads(dal.CalibrationReport_Get_Json(result.report))
    assert result.model is not None
    assert result.parameter_labels == ["g:level:2026-10-21"]
    assert result.backend == "FastGaussian"
    assert result.derivative_method == "FiniteDifference"
    assert result.quote_risk_available is False
    assert data["quote_map"] == [[1.0]]
    assert data["observations"][0]["model_value"] == result.model_prices[0]
    assert data["summary"]["status"] == "Succeeded"
    assert hasattr(dal, "Calibrate_GSRVolatility")
    assert dal.GSRCalibrationSettings_().max_iterations == 100


def test_gsr_mc_candidate_and_failed_outcome_are_independent_and_own_models():
    model, rows, parameters, valuation = _fixture()
    result = dal.Calibrate_GSRInstruments(model, rows, parameters, settings=_mc_settings(valuation))
    data = json.loads(dal.CalibrationReport_Get_Json(result.report))
    assert result.backend == "ScriptMC"
    assert result.derivative_method == "AAD"
    assert data["observations"][0]["standard_error"] == result.fit[0]["standard_error"]
    assert data["validation"][0]["standard_error"] == result.validation[0]["standard_error"]
    assert data["execution"]["sampling"]["first_paths"] == [0.0, 16.0]
    assert data["execution"]["validation_sampling"]["first_paths"] == [32.0, 64.0]
    with pytest.raises(dal.CalibrationError) as raised:
        dal.Calibrate_GSRInstruments(model, rows, parameters, settings=_mc_settings(valuation, max_basket_evaluations=1))
    error = raised.value
    del model, rows, parameters, valuation
    gc.collect()
    assert error.code == "CalibrationBudgetExceeded"
    assert error.diagnostic.field == "/solver/basket_evaluations"
    assert error.candidate is not None
    failed = json.loads(dal.CalibrationReport_Get_Json(error.report))
    assert failed["summary"]["evaluations"] == 1
    assert failed["summary"]["status"] == "Failed"
    assert failed["quote_jacobian"] is None


def _with_gsr_gil_handshake(operation):
    ready, released = threading.Event(), threading.Event()
    errors = []

    def release():
        ready.set()
        try:
            assert dal._dal._GsrCalibrationGilBarrier_AwaitEntryForTesting()
            assert dal._dal._GsrCalibrationGilBarrier_ReleaseForTesting()
            released.set()
        except BaseException as error:
            errors.append(error)

    dal._dal._GsrCalibrationGilBarrier_ArmForTesting()
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


def test_gsr_call_releases_gil_after_owning_inputs():
    model, rows, parameters, valuation = _fixture()
    settings = _mc_settings(valuation)
    result = _with_gsr_gil_handshake(lambda: dal.Calibrate_GSRInstruments(model, rows, parameters, settings=settings))
    assert result.model is not None


def test_gsr_gil_handshake_rejects_held_gil_negative_control():
    with pytest.raises(RuntimeError, match="GSRCalibration: Python GIL test barrier timed out"):
        _with_gsr_gil_handshake(dal._dal._GsrCalibrationGilBarrier_BlockWithGilForTesting)


def test_gsr_unavailable_jacobian_is_a_structured_error():
    model, rows, parameters, valuation = _fixture(0.0)
    result = dal.Calibrate_GSRInstruments(model, rows, parameters, settings=dal.GSRInstrumentCalibrationSettings_(valuation=valuation))
    assert result.native_jacobian_available is False
    with pytest.raises(dal.CalibrationError) as raised:
        _ = result.quote_jacobian
    assert raised.value.code == "DerivativeUnavailable"


def test_gsr_held_out_axes_and_values_are_owned_and_not_fit_rows():
    model, rows, parameters, valuation = _fixture()
    held = dal.CalibrationMonteCarloSampling_(paths_per_batch=8, batches=2, seed=1024, stream_version="dal.mrg32-normal-paths/1")
    settings = _mc_settings(valuation, held_out_sampling=held, held_out_observations=rows, max_paths_per_view=112)
    result = dal.Calibrate_GSRInstruments(model, rows, parameters, settings=settings)
    del model, rows, parameters, valuation, settings, held
    gc.collect()
    assert len(result.observations) == len(result.held_out_observations) == 1
    assert result.held_out_requested is True
    assert len(result.held_out) == 1
    data = json.loads(dal.CalibrationReport_Get_Json(result.report))
    assert len(data["observations"]) == 1
    assert len(data["quote_jacobian"]) == 1
    record = next(row for row in data["validation"] if row["edge"] == "HeldOut")
    assert record["value"] == result.held_out[0]["mean"]
    assert record["standard_error"] == result.held_out[0]["standard_error"]
    assert record["accepted"] == result.held_out_passed
    assert "held_out_sampling" not in data["execution"]
