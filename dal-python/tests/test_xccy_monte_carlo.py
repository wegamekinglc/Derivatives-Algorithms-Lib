import gc
import json
import threading

import dal
import pytest

from test_xccy_instrument import inputs

def _with_mc_gil_handshake(operation):
    ready, released = threading.Event(), threading.Event()
    errors = []

    def release():
        ready.set()
        try:
            assert dal._dal._XccyMcGilBarrier_AwaitEntryForTesting()
            assert dal._dal._XccyMcGilBarrier_ReleaseForTesting()
            released.set()
        except BaseException as error:
            errors.append(error)

    dal._dal._XccyMcGilBarrier_ArmForTesting()
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


def test_native_handshake_rejects_holding_gil():
    with pytest.raises(RuntimeError, match="XCCY MC GIL barrier timed out"):
        _with_mc_gil_handshake(dal._dal._XccyMcGilBarrier_BlockWithGilForTesting)


def model_inputs(mode="MARK_TO_MARKET"):
    values, instrument, _ = inputs(mode)
    today = dal.Date_(2026, 10, 21)
    end = dal.Date_(2029, 10, 20)
    blocks = []
    for ccy, rate, projection, tenor in (("USD", .04, .045, "3M"), ("EUR", .02, .026, "6M")):
        curve = dal.DiscountLogDF_New(ccy, ccy, [today, end], [0., -rate * 1095 / 365])
        forward = dal.DiscountLogDF_New(ccy, ccy, [today, end], [0., -projection * 1095 / 365])
        blocks.append(dal.CurveBlock_New(ccy, ccy, {dal.CollateralType_OIS(): curve}, {dal.PeriodLength_New(tenor): forward}, dal.DayBasis_New("ACT_365F")))
    basis = dal.DiscountLogDF_New("basis", "USD", [today, end], [0., -.009])
    market = dal.XccyGaussianMarketSpec_(valuation_time=dal.DateTime_(today, 0), pair=dal.CurrencyPair_New("USD", "EUR"),
        collateral_currency="USD", fx_spot=1.1, domestic_block=blocks[0], foreign_block=blocks[1], basis=basis, discount_route="USD.OIS")
    dynamics_values = dict(domestic_a=.03, foreign_a=.07, bucket_end_dates=[end], domestic_sigma=[.01], foreign_sigma=[.009], fx_sigma=[.12],
        factor_names=["rate:USD", "rate:EUR", "fx:EUR/USD"], correlation=[[1., .3, -.2], [.3, 1., -.25], [-.2, -.25, 1.]])
    dynamics = dal.XccyGaussianDynamicsSpec_(**dynamics_values)
    dynamics_values["domestic_sigma"][0] = 99.
    assert dynamics.domestic_sigma == [.01]
    model = dal.XccyGaussianModel_New("python", dal.XccyGaussianModelSpec_(market=market, dynamics=dynamics))
    return model, instrument, market, dynamics


def request(phase="Validation", *, measure="Price", **settings):
    fixings = settings.pop("fixings", dal.MarketFixingSnapshot_New({}))
    return dal.XccyMonteCarloRequest_(measure_id=measure, phase=phase,
        sampling=dal.CalibrationMonteCarloSampling_(paths_per_batch=8, batches=4, seed=1024, stream_version="dal.mrg32-normal-paths/1"),
        fixings=fixings, **settings)


def test_xccy_public_constructor_exists():
    assert hasattr(dal, "XccyGaussianModel_New")


def test_copied_model_native_handshake_and_independent_phases():
    model, instrument, _, _ = model_inputs()
    first = _with_mc_gil_handshake(lambda: dal.XccyInstrument_ValueByMonteCarlo(instrument, model, request()))
    second = dal.XccyInstrument_ValueByMonteCarlo(instrument, model, request("HeldOut"))
    repeated = dal.XccyInstrument_ValueByMonteCarlo(instrument, model, request())
    assert first.mean == repeated.mean
    saved = first.details_json
    details = json.loads(saved)
    held = json.loads(second.details_json)
    assert details["schema"] == "dal.xccy-gaussian-public-result/1"
    assert details["validation"] and not details["held_out"]
    assert held["held_out"] and not held["validation"]
    assert details["ranges"][0]["first_path"] == "262144"
    assert held["ranges"][0]["first_path"] == "1310720"
    assert details["range_identity"] != held["range_identity"]
    assert details["mapping_identity"] == held["mapping_identity"]
    assert int(details["dimension"]) == len(details["normal_coordinates"])
    assert int(details["dimension"]) % 5 == 0
    assert details["gradient"] == []
    report = first.report
    del first, second, repeated, instrument, model
    gc.collect()
    assert json.loads(saved)["mean"] == details["mean"]
    assert json.loads(dal.CalibrationReport_Get_Json(report))["summary"]["numerical_validation_passed"] is None


def test_raw_partials_and_failure_lifetime():
    model, instrument, _, _ = model_inputs()
    result = dal.XccyInstrument_ValueByMonteCarlo(instrument, model, request(derivative_method="AAD", parameter_labels=["fxspot:EUR/USD"]))
    assert len(result.gradient) == len(result.gradient_standard_error) == 1
    labels = result.parameter_labels
    labels.clear()
    assert result.parameter_labels == ["fxspot:EUR/USD"]
    assert not result.report.quote_risk_available
    with pytest.raises(dal.CalibrationError) as caught:
        dal.XccyInstrument_ValueByMonteCarlo(instrument, model, request(derivative_method="AAD", parameter_labels=["a:USD"]))
    error = caught.value
    del result, model, instrument
    gc.collect()
    assert error.code == "DerivativeUnavailable"
    assert error.report is not None
    assert error.diagnostic.instrument_id == "xccy"


def test_strict_required_phase_and_nested_types():
    sampling = dal.CalibrationMonteCarloSampling_(paths_per_batch=2, batches=2, seed=1, stream_version="dal.mrg32-normal-paths/1")
    with pytest.raises(TypeError, match="phase"):
        dal.XccyMonteCarloRequest_(measure_id="Price", sampling=sampling)
    for token in ("", "validation", "Unknown"):
        with pytest.raises(dal.CalibrationError) as caught:
            dal.XccyMonteCarloRequest_(measure_id="Price", sampling=sampling, phase=token)
        assert caught.value.code == "InvalidCalibrationPlan"
        assert caught.value.diagnostic.field == "/phase"
    with pytest.raises(TypeError):
        request(simulation={})
    with pytest.raises(TypeError):
        request(unknown=True)
    with pytest.raises(TypeError):
        request(parameter_labels=[1])


def test_staged_dynamics_selection_and_model_getter():
    model, instrument, market, dynamics = model_inputs()
    values, _, _ = inputs()
    values.update(domestic_curve_block=market.domestic_block, foreign_curve_block=market.foreign_block,
        knot_dates=[dal.Date_(2027, 11, 8)], gaussian_dynamics=dal.XccyGaussianDynamics_New("d", dynamics))
    spec = dal.XccyInstrumentCalibrationSpec_(**values)
    candidate = dal.Evaluate_XccyInstruments(spec, [.003])
    assert candidate.gaussian_model is not None
    assert json.loads(dal.CalibrationReport_Get_Json(candidate.report))["plans"][0]["backend"] == "FastGaussian"
    fast = dal.XccyInstrument_ValueByGaussian(instrument, model, "Price", fixings=dal.MarketFixingSnapshot_New({}))
    assert fast.value == pytest.approx(fast.b + .001 * fast.a, abs=1e-10)


def test_validation_flags_and_phase_eligibility():
    model, instrument, _, _ = model_inputs()
    result = dal.XccyInstrument_ValueByMonteCarlo(instrument, model, request())
    reference = dal.XccyMonteCarloReference_(value=result.mean, numerical_error=0.)
    precision = dal.XccyMonteCarloPrecision_(error_scale=1e-12, epsilon_value=1e-16)
    flags = dal.XccyMonteCarloResult_Validate(result, reference, precision)
    assert flags.parity_passed
    assert not flags.precision_passed
    assert not flags.value_error_passed
    assert not flags.numerical_validation_passed
    assert flags.budget_exhausted
    assert flags.gradient_combined_passed is None
    for phase in ("Fit", "HeldOut"):
        estimate = dal.XccyInstrument_ValueByMonteCarlo(instrument, model, request(phase))
        with pytest.raises(dal.CalibrationError) as caught:
            dal.XccyMonteCarloResult_Validate(estimate, reference, precision)
        del estimate
        gc.collect()
        assert caught.value.report is not None


@pytest.mark.parametrize("phase", ["Fit", "Validation", "HeldOut"])
@pytest.mark.parametrize("mode", ["FIXED", "RESETTABLE", "MARK_TO_MARKET"])
@pytest.mark.parametrize("measure", ["Price", "BasisSpread"])
def test_same_input_direct_core_parity(phase, mode, measure):
    model, instrument, _, _ = model_inputs(mode)
    settings = request(phase, measure=measure)
    expected = dal._dal._XccyMcEvaluateCoreForTesting(instrument, model, settings)
    actual = dal.XccyInstrument_ValueByMonteCarlo(instrument, model, settings)
    details = json.loads(actual.details_json)
    assert actual.mean == expected["mean"]
    assert actual.standard_error == expected["standard_error"]
    assert details["mapping_identity"] == expected["mapping_identity"]
    assert details["range_identity"] == expected["range_identity"]


def test_staged_failure_retains_gaussian_model_separately_from_curve():
    _, _, market, dynamics = model_inputs()
    values, _, _ = inputs(solver=dal.CurveInstrumentSolver_(max_evaluations=1))
    values.update(domestic_curve_block=market.domestic_block, foreign_curve_block=market.foreign_block,
        knot_dates=[dal.Date_(2027, 11, 8)], gaussian_dynamics=dal.XccyGaussianDynamics_New("d", dynamics))
    with pytest.raises(dal.CalibrationError) as caught:
        dal.Calibrate_XccyInstruments(dal.XccyInstrumentCalibrationSpec_(**values))
    error = caught.value
    del market, dynamics, values
    gc.collect()
    assert error.candidate is not None
    assert error.gaussian_model is not None
    assert error.gaussian_model.identity


def test_snapshot_markers_future_entries_and_strict_sampling_inputs():
    model, instrument, _, _ = model_inputs()
    absent = request(fixings=None)
    empty = request()
    assert absent.fixings is None
    assert empty.fixings is not None
    future = request(fixings=dal.MarketFixingSnapshot_New({"FX[EUR/USD]": {dal.DateTime_(dal.Date_(2027, 5, 4), 0): 99.}}))
    first = dal.XccyInstrument_ValueByMonteCarlo(instrument, model, absent)
    second = dal.XccyInstrument_ValueByMonteCarlo(instrument, model, empty)
    ignored = dal.XccyInstrument_ValueByMonteCarlo(instrument, model, future)
    assert first.mean == second.mean == ignored.mean
    for value in (True, 2.0, "2"):
        with pytest.raises(TypeError):
            dal.CalibrationMonteCarloSampling_(paths_per_batch=value, batches=2, seed=1, stream_version="dal.mrg32-normal-paths/1")
    with pytest.raises(dal.CalibrationError):
        dal.CalibrationMonteCarloSampling_(paths_per_batch=2**31, batches=2, seed=1, stream_version="dal.mrg32-normal-paths/1")
    with pytest.raises(dal.CalibrationError) as caught:
        dal.XccyInstrument_ValueByMonteCarlo(instrument, model, request(simulation=dal.CalibrationSimulationSettings_(smooth=.1)))
    assert caught.value.code == "InvalidCalibrationPlan"
    assert caught.value.diagnostic.field == "/simulation"
    over_capacity = dal.XccyMonteCarloRequest_(measure_id="Price", phase="Validation",
        sampling=dal.CalibrationMonteCarloSampling_(paths_per_batch=2048, batches=1024, seed=1024, stream_version="dal.mrg32-normal-paths/1"))
    with pytest.raises(dal.CalibrationError) as caught:
        dal.XccyInstrument_ValueByMonteCarlo(instrument, model, over_capacity)
    assert caught.value.code == "CalibrationBudgetExceeded"
    assert caught.value.diagnostic.field == "/sampling"
