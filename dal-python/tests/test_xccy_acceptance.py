import gc
import json
import threading
from pathlib import Path

import dal
import pytest

from test_xccy_instrument import inputs
from test_xccy_monte_carlo import model_inputs, request


SELECTED = ["sigma:EUR/USD:2029-10-20", "fxspot:EUR/USD"]
FIXTURES = Path(__file__).resolve().parents[2] / "dal-public/tests/fixtures/xccy_acceptance"


def shared_inputs(mode="MARK_TO_MARKET"):
    instrument = dal.CalibrationInstrument_LoadJson((FIXTURES / (mode + ".json")).read_text())
    model = dal._dal._StorableFromJson((FIXTURES / "model.json").read_bytes())
    return model, instrument


@pytest.mark.parametrize("mode,measure,phase,first", [
    ("FIXED", "Price", "Fit", 0),
    ("RESETTABLE", "BasisSpread", "Validation", 262144),
    ("MARK_TO_MARKET", "Price", "HeldOut", 1310720),
])
def test_acceptance_core_parity_ordered_partials_and_owning_getters(mode, measure, phase, first, record_property):
    model, instrument = shared_inputs(mode)
    source = dal.CalibrationInstrument_Get_Json(instrument)
    labels = list(SELECTED)
    settings = request(phase, measure=measure, derivative_method="AAD", parameter_labels=labels)
    expected = dal._dal._XccyMcEvaluateCoreForTesting(instrument, model, settings)
    result = dal.XccyInstrument_ValueByMonteCarlo(instrument, model, settings)
    assert dal.CalibrationInstrument_Get_Json(instrument) == source
    labels.clear()
    report = result.report
    del model, instrument, settings
    gc.collect()
    data = json.loads(result.details_json)
    record_property("payload", result.details_json)
    assert data == json.loads(expected["details_json"])
    assert result.mean == expected["mean"]
    assert result.standard_error == expected["standard_error"]
    assert data["mapping_identity"] == expected["mapping_identity"]
    assert data["range_identity"] == expected["range_identity"]
    assert result.parameter_labels == data["parameter_labels"] == SELECTED
    assert result.gradient == data["gradient"]
    assert result.gradient_standard_error == data["gradient_standard_error"]
    assert len(data["coefficient_covariance"]) == 6
    assert all(len(row) == 6 for row in data["coefficient_covariance"])
    assert int(data["ranges"][0]["first_path"]) == first
    assert int(data["ranges"][-1]["path_end"]) == first + 32
    assert int(data["consumed_coordinates"]) == 32 * int(data["dimension"])
    assert data["validation"] == (phase == "Validation")
    assert data["held_out"] == (phase == "HeldOut")
    exposed = result.gradient
    exposed[0] = float("nan")
    assert result.gradient == data["gradient"]
    assert not report.quote_risk_available


def test_acceptance_native_copy_barrier_survives_mutated_python_inputs():
    model, instrument = shared_inputs()
    labels = list(SELECTED)
    settings = request(derivative_method="AAD", parameter_labels=labels)
    failures = []

    def mutate_after_entry():
        try:
            assert dal._dal._XccyMcGilBarrier_AwaitEntryForTesting()
            labels[:] = ["a:USD"]
            assert dal._dal._XccyMcGilBarrier_ReleaseForTesting()
        except BaseException as error:
            failures.append(error)

    dal._dal._XccyMcGilBarrier_ArmForTesting()
    worker = threading.Thread(target=mutate_after_entry)
    worker.start()
    try:
        result = dal.XccyInstrument_ValueByMonteCarlo(instrument, model, settings)
    finally:
        worker.join(timeout=6)
    assert not worker.is_alive()
    assert not failures
    assert labels == ["a:USD"]
    del settings, instrument, model
    gc.collect()
    assert result.parameter_labels == SELECTED
    assert len(result.gradient) == len(result.gradient_standard_error) == 2


def test_acceptance_staged_candidate_to_mc_after_spec_destruction(record_property):
    _, _, market, dynamics = model_inputs("MARK_TO_MARKET")
    _, instrument = shared_inputs()
    values, _, quote = inputs("MARK_TO_MARKET")
    values["observations"] = [dal.CalibrationObservation_New("row", instrument, "BasisSpread", quote, 1e-10)]
    values.update(domestic_curve_block=market.domestic_block, foreign_curve_block=market.foreign_block,
                  knot_dates=[dal.Date_(2027, 11, 8)],
                  gaussian_dynamics=dal.XccyGaussianDynamics_New("acceptance", dynamics))
    spec = dal.XccyInstrumentCalibrationSpec_(**values)
    candidate = dal.Evaluate_XccyInstruments(spec, [.003])
    model = candidate.gaussian_model
    assert model is not None
    settings = request(measure="BasisSpread", derivative_method="AAD", parameter_labels=list(SELECTED))
    expected = dal._dal._XccyMcEvaluateCoreForTesting(instrument, model, settings)
    fast = dal.XccyInstrument_ValueByGaussian(instrument, model, "BasisSpread", fixings=settings.fixings)
    del spec, values, market, dynamics
    gc.collect()
    assert fast.value == pytest.approx(candidate.model_measures[0], abs=1e-12, rel=0.)
    result = dal.XccyInstrument_ValueByMonteCarlo(instrument, model, settings)
    record_property("payload", result.details_json)
    del candidate, model, instrument, settings
    gc.collect()
    assert result.mean == expected["mean"]
    assert result.standard_error == expected["standard_error"]
    assert result.parameter_labels == SELECTED
    assert json.loads(result.details_json) == json.loads(expected["details_json"])


@pytest.mark.parametrize("settings,field", [
    (dict(derivative_method="AAD", parameter_labels=["a:USD"]), "/mc/parameter_labels"),
    (dict(derivative_method="AAD", parameter_labels=["fxspot:EUR/USD"] * 2), "/mc/parameter_labels"),
    (dict(simulation=dal.CalibrationSimulationSettings_(smooth=.1)), "/simulation"),
])
def test_acceptance_owning_failure_preserves_diagnostic(settings, field):
    model, instrument = shared_inputs()
    instrument_id = instrument.id
    with pytest.raises(dal.CalibrationError) as caught:
        dal.XccyInstrument_ValueByMonteCarlo(instrument, model, request(**settings))
    error = caught.value
    del model, instrument, settings, caught
    gc.collect()
    assert error.diagnostic.field == field
    assert error.diagnostic.instrument_id == instrument_id
    assert error.report is not None
    assert error.code in ("DerivativeUnavailable", "InvalidCalibrationPlan")


@pytest.mark.parametrize("bad", [True, "1", None, float("inf"), float("nan")])
def test_acceptance_nested_numerical_input_is_strict(bad):
    error = TypeError if bad is None or isinstance(bad, (bool, str)) else dal.CalibrationError
    with pytest.raises(error):
        dal.XccyMonteCarloReference_(value=0., numerical_error=0., gradient=[bad])


@pytest.mark.parametrize("bad", [2**31, -(2**31)-1])
def test_acceptance_int32_endpoints_are_checked(bad):
    with pytest.raises(dal.CalibrationError) as caught:
        dal.CalibrationMonteCarloSampling_(paths_per_batch=bad, batches=4, seed=1024,
                                           stream_version="dal.mrg32-normal-paths/1")
    assert caught.value.diagnostic.field == "/paths_per_batch"
