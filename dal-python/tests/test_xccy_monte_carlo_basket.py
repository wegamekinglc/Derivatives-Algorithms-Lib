import gc
import json
import threading

import dal
import pytest

from test_xccy_monte_carlo import model_inputs


def basket_request(phase="HeldOut", **kwargs):
    return dal.XccyMonteCarloBasketRequest_(
        phase,
        dal.CalibrationMonteCarloSampling_(paths_per_batch=3, batches=4, seed=1024,
                                          stream_version="dal.mrg32-normal-paths/1"),
        dal.MarketFixingSnapshot_New({}), **kwargs)


def test_basket_callable_and_owned_complete_result():
    model, instrument, _, _ = model_inputs()
    members = [dal.XccyMcBasketMember_(name, instrument, "Price") for name in ("A", "a")]
    prepared = dal.XccyBasket_PrepareMonteCarlo(members, model, basket_request())
    members.reverse()
    result = prepared.evaluate()
    allocation = prepared.allocation
    saved_member = result.members[0]
    saved = result.details_json
    details = json.loads(saved)
    assert details["schema"] == "dal.xccy-gaussian-public-basket-result/1"
    assert [m["member_id"] for m in details["members"]] == ["A", "a"]
    assert len(result.coefficient_covariance) == 4
    assert len(result.coefficient_axes) == 4
    assert details["numerical_validation_passed"] is None
    del prepared, members, instrument, model, result
    gc.collect()
    assert allocation.members[0].member_id == "A"
    assert allocation.members[0].context.fixings is not None
    assert allocation.members[0].observations.requests[0].uses[0].source.row >= 0
    observations = allocation.members[0].observations.requests
    observations.clear()
    assert allocation.members[0].observations.requests
    assert saved_member.completed_estimate.mean == details["members"][0]["completed_estimate"]["mean"]
    with pytest.raises(AttributeError):
        saved_member.state = "Failed"


@pytest.mark.parametrize("phase", ["Fit", "Validation", "HeldOut"])
@pytest.mark.parametrize("aad", [False, True])
def test_real_python_call_matches_direct_core(phase, aad):
    model, instrument, _, _ = model_inputs()
    members = [dal.XccyMcBasketMember_("left", instrument, "Price"),
               dal.XccyMcBasketMember_("right", instrument, "BasisSpread")]
    options = dict(derivative_method="AAD", parameter_labels=["fxspot:EUR/USD"]) if aad else {}
    request = basket_request(phase, **options)
    core = dal._dal._XccyBasketCoreForTesting(members, model, request)
    result = dal.XccyBasket_PrepareMonteCarlo(members, model, request).evaluate()
    assert result.details_json == core.details_json
    payload = json.loads(result.details_json)
    assert payload["allocation"]["phase"] == phase
    assert payload["allocation"]["batches"] == "4"
    assert payload["members"][0]["completed_estimate"]["validation"] == (phase == "Validation")
    assert (result.members[0].completed_estimate.gradient is not None) == aad
    covariance = result.coefficient_covariance
    covariance[0][0] = -99
    assert result.coefficient_covariance[0][0] != -99
    assert len(result.output_covariance) == (4 if aad else 2)
    with pytest.raises(TypeError):
        dal.XccyMonteCarloResult_Validate(result,
            dal.XccyMonteCarloReference_(value=0., numerical_error=0.),
            dal.XccyMonteCarloPrecision_(error_scale=1.))


def test_required_arguments_strict_nested_and_null_snapshot():
    sampling = dal.CalibrationMonteCarloSampling_(paths_per_batch=2, batches=2, seed=1,
                                                stream_version="dal.mrg32-normal-paths/1")
    fixings = dal.MarketFixingSnapshot_New({})
    for fields, missing in ((dict(sampling=sampling, fixings=fixings), "phase"),
                            (dict(phase="HeldOut", fixings=fixings), "sampling"),
                            (dict(phase="HeldOut", sampling=sampling), "fixings")):
        with pytest.raises(TypeError, match=missing):
            dal.XccyMonteCarloBasketRequest_(**fields)
    for fields, missing in ((dict(instrument=None, measure_id="Price"), "member_id"),
                            (dict(member_id="m", measure_id="Price"), "instrument"),
                            (dict(member_id="m", instrument=None), "measure_id")):
        with pytest.raises(TypeError, match=missing):
            dal.XccyMcBasketMember_(**fields)
    with pytest.raises(dal.XccyBasketCalibrationError) as caught:
        dal.XccyMonteCarloBasketRequest_("HeldOut", sampling, None)
    assert caught.value.diagnostic.field == "/context/fixings"
    assert caught.value.partial is None
    assert caught.value.candidate is None
    for phase in ("", "heldout", "HeldOut/2"):
        with pytest.raises(dal.XccyBasketCalibrationError) as caught:
            dal.XccyMonteCarloBasketRequest_(phase, sampling, fixings)
        assert caught.value.diagnostic.field == "/phase"
    for options in (dict(simulation={}), dict(parameter_labels=[True]), dict(phase2="Fit")):
        with pytest.raises(TypeError):
            basket_request(**options)
    model, instrument, _, _ = model_inputs()
    for members, field in (([], "/members"),
                            ([dal.XccyMcBasketMember_("bad", None, "Price")], "/members/0/instrument"),
                            ([dal.XccyMcBasketMember_("same", instrument, "Price")] * 2, "/members/1/member_id")):
        with pytest.raises(dal.CalibrationError) as caught:
            dal.XccyBasket_PrepareMonteCarlo(members, model, basket_request())
        assert isinstance(caught.value, dal.XccyBasketCalibrationError)
        assert caught.value.diagnostic.field == field
        assert caught.value.report is not None
        assert caught.value.partial is None
    with pytest.raises(dal.XccyBasketCalibrationError) as caught:
        dal.XccyBasket_PrepareMonteCarlo([dal.XccyMcBasketMember_("one", instrument, "Price")], model,
            basket_request(simulation=dal.CalibrationSimulationSettings_(smooth=.1)))
    assert caught.value.diagnostic.field == "/simulation"
    assert caught.value.partial is None


def _failure(fault):
    model, instrument, _, _ = model_inputs()
    members = [dal.XccyMcBasketMember_(name, instrument, "Price") for name in ("one", "two", "three")]
    request = basket_request(derivative_method="AAD", parameter_labels=["fxspot:EUR/USD"])
    return dal._dal._XccyBasketFailureForTesting(fault, members, model, request)


@pytest.mark.parametrize("fault,rows,vectors", [("LateMember", 0, 13), ("LateBatch", 1, 16), ("LastMember", 0, 0)])
def test_derived_error_partial_is_owned_after_gc(fault, rows, vectors):
    with pytest.raises(dal.CalibrationError) as caught:
        _failure(fault)
    error = caught.value
    assert isinstance(error, dal.XccyBasketCalibrationError)
    partial, report, candidate = error.partial, error.report, error.candidate
    member = partial.members[0]
    details = json.loads(partial.details_json)
    consumed = vectors * partial.allocation.members[0].mapping.dimension
    assert partial.report is report
    assert not partial.basket_complete
    assert partial.consumed_coordinates == consumed
    assert partial.header.candidate_geometry_validated == (fault != "LastMember")
    assert (candidate is not None) == (fault != "LastMember")
    if fault != "LastMember":
        assert member.state == "Complete"
        assert len(partial.members[1].batches) == rows
        assert partial.members[1].completed_estimate is None
        assert partial.members[2].state == "NotStarted"
        assert details["members"][1]["completed_estimate"] is None
    assert details["coefficient_covariance"] is None
    assert details["mean_covariance"] is None
    assert details["standard_error"] is None
    assert details["numerical_validation_passed"] is None
    assert details["consumed_coordinates"] == str(consumed)
    del error, caught
    gc.collect()
    assert partial.report is report
    assert partial.members[0].state == member.state
    assert json.loads(partial.details_json) == details
    with pytest.raises(AttributeError):
        partial.completed_members = 999


def test_early_prepare_error_has_no_partial_or_model():
    with pytest.raises(dal.XccyBasketCalibrationError) as caught:
        _failure("Early")
    assert caught.value.partial is None
    assert caught.value.candidate is None
    assert caught.value.report is not None


def test_real_candidate_refresh_and_rejected_geometry_survive_plan_gc():
    model, instrument, market, dynamics = model_inputs()
    plan = dal.XccyBasket_PrepareMonteCarlo([dal.XccyMcBasketMember_("one", instrument, "Price")], model, basket_request())
    market_fields = dict(valuation_time=market.valuation_time, pair=market.pair,
                         collateral_currency=market.collateral_currency, fx_spot=1.2,
                         domestic_block=market.domestic_block, foreign_block=market.foreign_block,
                         basis=market.basis, discount_route=market.discount_route)
    bumped = dal.XccyGaussianModel_New("bump", dal.XccyGaussianModelSpec_(
        market=dal.XccyGaussianMarketSpec_(**market_fields), dynamics=dynamics))
    original = plan.evaluate()
    refreshed = plan.evaluate(bumped)
    assert original.model.identity != refreshed.model.identity
    assert original.allocation.identity == refreshed.allocation.identity
    assert original.coefficient_rows != refreshed.coefficient_rows
    dynamics_fields = dict(domestic_a=dynamics.domestic_a, foreign_a=dynamics.foreign_a,
                           bucket_end_dates=[dal.Date_(2029, 10, 21)], domestic_sigma=dynamics.domestic_sigma,
                           foreign_sigma=dynamics.foreign_sigma, fx_sigma=dynamics.fx_sigma,
                           factor_names=dynamics.factor_names, correlation=dynamics.correlation)
    wrong = dal.XccyGaussianModel_New("geometry", dal.XccyGaussianModelSpec_(
        market=market, dynamics=dal.XccyGaussianDynamicsSpec_(**dynamics_fields)))
    with pytest.raises(dal.XccyBasketCalibrationError) as caught:
        plan.evaluate(wrong)
    partial = caught.value.partial
    assert caught.value.candidate is None
    assert partial.consumed_coordinates == 0
    assert partial.header.attempted_model_identity == wrong.identity
    assert not partial.header.candidate_geometry_validated
    del caught, plan, model, instrument, market, dynamics, wrong, bumped
    gc.collect()
    assert partial.consumed_coordinates == 0
    assert partial.header.native_map_identity is None


def _gil_handshake(operation):
    ready, released = threading.Event(), threading.Event()
    errors = []

    def release():
        ready.set()
        try:
            assert dal._dal._XccyBasketGilBarrier_AwaitEntryForTesting()
            assert dal._dal._XccyBasketGilBarrier_ReleaseForTesting()
            released.set()
        except BaseException as error:
            errors.append(error)

    dal._dal._XccyBasketGilBarrier_ArmForTesting()
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


def test_prepare_and_evaluate_release_gil_and_restore_for_errors():
    model, instrument, _, _ = model_inputs()
    members = [dal.XccyMcBasketMember_("one", instrument, "Price")]
    plan = _gil_handshake(lambda: dal.XccyBasket_PrepareMonteCarlo(members, model, basket_request()))
    result = _gil_handshake(plan.evaluate)
    assert result.members[0].state == "Complete"
    with pytest.raises(dal.XccyBasketCalibrationError) as caught:
        _gil_handshake(lambda: dal.XccyBasket_PrepareMonteCarlo([], model, basket_request()))
    assert caught.value.report is not None
    with pytest.raises(RuntimeError, match="XCCY basket GIL barrier timed out"):
        _gil_handshake(dal._dal._XccyBasketGilBarrier_BlockWithGilForTesting)
