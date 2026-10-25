import gc
import json
import math
import threading

import dal
import pytest

from test_xccy_acceptance import FIXTURES


IDS = ["A", "a", 'tail"\n']
LABELS = ["sigma:EUR/USD:2029-10-20", "fxspot:EUR/USD"]


def shared_basket():
    model = dal._dal._StorableFromJson((FIXTURES / "model.json").read_bytes())
    instruments = [dal.CalibrationInstrument_LoadJson((FIXTURES / "basket" / f"{i}.json").read_text())
                   for i in range(3)]
    members = [dal.XccyMcBasketMember_(name, instrument, "BasisSpread" if i == 1 else "Price")
               for i, (name, instrument) in enumerate(zip(IDS, instruments))]
    return model, members


def settings(phase="HeldOut", **options):
    return dal.XccyMonteCarloBasketRequest_(
        phase=phase,
        sampling=dal.CalibrationMonteCarloSampling_(paths_per_batch=3, batches=4, seed=1024,
                                                  stream_version="dal.mrg32-normal-paths/1"),
        fixings=dal.MarketFixingSnapshot_New({}),
        simulation=dal.CalibrationSimulationSettings_(compiled=True, use_bb=False, smooth=0., normal_precision="Precise"),
        derivative_method="AAD", parameter_labels=options.pop("parameter_labels", list(LABELS)), **options)


def covariance(rows):
    means = [math.fsum(column) / len(rows) for column in zip(*rows)]
    return [[math.fsum((row[i] - means[i]) * (row[j] - means[j]) for row in rows) / (len(rows) - 1)
             for j in range(len(means))] for i in range(len(means))]


def assert_matrix(actual, expected):
    assert len(actual) == len(expected)
    for row, reference in zip(actual, expected):
        assert len(row) == len(reference)
        assert row == pytest.approx(reference, rel=5e-12, abs=1e-12)


def projected_row(left, covariance_matrix, jacobian):
    weights = [math.fsum(a * row[j] for a, row in zip(left, covariance_matrix)) for j in range(len(left))]
    return [math.fsum(a * b for a, b in zip(weights, right)) for right in jacobian]


def assert_complete_statistics(data):
    assert len(data["coefficient_axes"]) == 18
    assert len(data["output_axes"]) == 9
    assert len(data["coefficient_rows"]) == 4
    assert all(len(row) == 18 for row in data["coefficient_rows"])
    assert len(data["output_jacobian"]) == 9
    assert all(len(row) == 18 for row in data["output_jacobian"])
    assert_matrix(data["coefficient_covariance"], covariance(data["coefficient_rows"]))
    mean_cov = [[value / 4 for value in row] for row in data["coefficient_covariance"]]
    assert_matrix(data["mean_covariance"], mean_cov)
    projected = [projected_row(row, mean_cov, data["output_jacobian"]) for row in data["output_jacobian"]]
    assert_matrix(data["output_covariance"], projected)


@pytest.mark.parametrize("phase,first", [("Fit", 0), ("Validation", 262144), ("HeldOut", 1310720)])
def test_acceptance_basket_complete_dated_mappings(phase, first, record_property):
    model, members = shared_basket()
    request = settings(phase)
    core = dal._dal._XccyBasketCoreForTesting(members, model, request)
    prepared = dal.XccyBasket_PrepareMonteCarlo(members, model, request)
    model_id = model.identity
    sources = [dal.CalibrationInstrument_Get_Json(member.instrument) for member in members]
    result = prepared.evaluate()
    assert result.details_json == core.details_json
    assert sources == [dal.CalibrationInstrument_Get_Json(member.instrument) for member in members]
    saved_member, allocation, report = result.members[-1], result.allocation, result.report
    data = json.loads(result.details_json)
    record_property("payload", result.details_json)
    del model, members, request, prepared, core, result
    gc.collect()
    assert data["schema"] == "dal.xccy-gaussian-public-basket-result/1"
    assert data["model_identity"] == model_id
    assert data["numerical_validation_passed"] is None
    assert data["quote_risk_available"] is False
    assert data["report"]["summary"]["schema"] == "dal.calibration-report/1"
    assert [member["member_id"] for member in data["members"]] == IDS
    assert_complete_statistics(data)
    dimensions = [int(member["mapping"]["dimension"]) for member in data["allocation"]["members"]]
    assert len(set(dimensions)) == 3
    for member in allocation.members:
        assert member.ranges[0].first_path == first
        assert member.ranges[-1].path_end == first + 12
    assert data["consumed_coordinates"] == str(12 * sum(dimensions))
    estimate = saved_member.completed_estimate
    assert estimate.parameter_labels == LABELS
    assert estimate.gradient == data["members"][-1]["completed_estimate"]["gradient"]
    assert estimate.gradient_standard_error == data["members"][-1]["completed_estimate"]["gradient_standard_error"]
    assert estimate.validation == (phase == "Validation")
    assert estimate.held_out == (phase == "HeldOut")
    assert not report.quote_risk_available


@pytest.mark.parametrize("fault,batches", [("LateMember", 0), ("LateBatch", 1), ("LastMember", None), ("Early", None)])
def test_acceptance_basket_partial_survives_all_input_and_error_owners(fault, batches):
    model, members = shared_basket()
    request = settings()
    with pytest.raises(dal.XccyBasketCalibrationError) as caught:
        dal._dal._XccyBasketFailureForTesting(fault, members, model, request)
    error = caught.value
    partial, report, candidate = error.partial, error.report, error.candidate
    del error, caught, members, model, request
    gc.collect()
    assert report is not None
    if fault == "Early":
        assert partial is candidate is None
        return
    data = json.loads(partial.details_json)
    assert data["schema"] == "dal.xccy-gaussian-mc-basket-partial/1"
    assert partial.report is report
    assert partial.basket_complete is False
    assert (candidate is not None) == (batches is not None)
    for key in ("coefficient_rows", "coefficient_means", "coefficient_axes", "output_axes", "coefficient_covariance",
                "mean_covariance", "output_jacobian", "output_covariance", "mean", "standard_error", "numerical_validation_passed"):
        assert data[key] is None
    assert data["quote_risk_available"] is False
    if batches is None:
        assert partial.consumed_coordinates == 0
        assert partial.header.candidate_geometry_validated is False
        assert all(not member.batches and member.completed_estimate is None for member in partial.members)
        return
    assert candidate.identity == partial.header.attempted_model_identity
    assert partial.members[0].state == "Complete"
    assert partial.members[2].state == "NotStarted"
    assert len(partial.members[1].batches) == batches
    assert partial.members[1].completed_estimate is None
    allocation = partial.allocation.members
    assert partial.consumed_coordinates == 12 * allocation[0].mapping.dimension + (3 * batches + 1) * allocation[1].mapping.dimension
    assert data["failure_location"]["member"] == "1"
    assert data["failure_location"]["batch"] == str(batches)


def test_acceptance_basket_native_copy_before_gil_release():
    model, members = shared_basket()
    labels = list(LABELS)
    request = settings(parameter_labels=labels)
    failures = []

    def mutate_after_entry():
        try:
            assert dal._dal._XccyBasketGilBarrier_AwaitEntryForTesting()
            members.clear()
            labels[:] = ["a:USD"]
            assert dal._dal._XccyBasketGilBarrier_ReleaseForTesting()
        except BaseException as error:
            failures.append(error)

    dal._dal._XccyBasketGilBarrier_ArmForTesting()
    worker = threading.Thread(target=mutate_after_entry)
    worker.start()
    try:
        prepared = dal.XccyBasket_PrepareMonteCarlo(members, model, request)
    finally:
        worker.join(timeout=6)
    assert not worker.is_alive()
    assert not failures
    assert members == []
    del model, request
    gc.collect()
    result = prepared.evaluate()
    assert [member.member_id for member in result.members] == IDS
    assert result.members[0].completed_estimate.parameter_labels == LABELS


@pytest.mark.parametrize("field,bad", [("simulation", {}), ("parameter_labels", [True]),
                                      ("derivative_method", True), ("unknown", 0)])
def test_acceptance_basket_strict_nested_types(field, bad):
    options = dict(phase="HeldOut", sampling=dal.CalibrationMonteCarloSampling_(paths_per_batch=3, batches=4, seed=1024,
                                                                                 stream_version="dal.mrg32-normal-paths/1"),
                   fixings=dal.MarketFixingSnapshot_New({}))
    options[field] = bad
    with pytest.raises(TypeError):
        dal.XccyMonteCarloBasketRequest_(**options)


@pytest.mark.parametrize("bad", [True, 2**31, -(2**31)-1])
def test_acceptance_basket_sampling_rejects_bool_and_int32_overflow(bad):
    with pytest.raises((TypeError, dal.CalibrationError)):
        dal.CalibrationMonteCarloSampling_(paths_per_batch=bad, batches=4, seed=1024,
                                           stream_version="dal.mrg32-normal-paths/1")
