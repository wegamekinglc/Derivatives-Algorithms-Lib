"""Automatic Dupire plans reuse native ownership, coordinates and execution."""

import copy
from concurrent.futures import ThreadPoolExecutor
from enum import IntEnum
import gc
import json
import math
import os
from pathlib import Path
import subprocess
import sys

import dal
import pytest

from test_dupire_risk import flat_calibration, hybrid_model, inputs, smooth_product
from test_quote_risk import _run_with_quote_risk_gil_heartbeat


def valuation():
    return dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 9, 12))


def plan_for(request, calibration=None, product=None):
    calibration = flat_calibration() if calibration is None else calibration
    return dal.DupireScriptRiskPlan_New(
        smooth_product() if product is None else product,
        hybrid_model(calibration), calibration, "Z_LOCAL", request,
    )


def test_plan_resolves_required_surface_and_bound_constant_before_execution():
    calibration = flat_calibration()
    request = dal.DupireScriptRiskRequest_(
        num_paths=257,
        quotes=dal.CalibrationRiskRequest_(
            inputs=["quote:3", "quote:0"], report_factors=[0.01, 0.5],
            numeric_payload_budget_bytes=304,
        ),
        direct_bindings=[dal.DupireQuoteBinding_(constant_ordinal=0, quote_id="quote:3")],
        valuation=dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 9, 12)),
    )
    plan = dal.DupireScriptRiskPlan_New(
        smooth_product(), hybrid_model(calibration), calibration, "Z_LOCAL", request,
    )
    assert plan.component == "Z_LOCAL"
    assert plan.num_paths == 257
    assert plan.numeric_payload_bytes == 304
    assert plan.quote_plan.numeric_payload_bytes == 144
    assert plan.quote_plan.selected_ordinals == [3, 0]
    assert plan.simulation_settings.enable_aad
    assert len(plan.complete_input_axis) == 25
    assert [coordinate.id for coordinate in plan.required_input_axis] == [
        f"model:{ordinal}" for ordinal in range(6, 24)
    ] + ["constant:0"]
    assert plan.direct_bindings[0].quote_id == "quote:3"
    assert plan.direct_bindings[0].constant_ordinal == 0


class NumericEnum(IntEnum):
    VALUE = 1


@pytest.mark.parametrize("value", [None, True, NumericEnum.VALUE, dal.CurveJacobianMode.ANALYTIC, 257.0, "257"])
def test_paths_reject_implicit_coercions(value):
    with pytest.raises(TypeError, match="DupireScriptRiskRequest_; num_paths"):
        dal.DupireScriptRiskRequest_(num_paths=value)


@pytest.mark.parametrize("value", [0, -1, 2**31, 10**1000])
def test_paths_reject_nonpositive_or_overflowing_values(value):
    with pytest.raises(RuntimeError, match="num_paths.*1..INT_MAX|num_paths.*positive"):
        dal.DupireScriptRiskRequest_(num_paths=value)


@pytest.mark.parametrize("value", [None, True, NumericEnum.VALUE, dal.CurveJacobianMode.ANALYTIC, 0.0, "0"])
def test_constant_ordinals_reject_implicit_coercions(value):
    with pytest.raises(TypeError, match="DupireQuoteBinding_; constant_ordinal"):
        dal.DupireQuoteBinding_(constant_ordinal=value, quote_id="quote:0")


@pytest.mark.parametrize("value", [-1, 2**100])
def test_constant_ordinals_reject_unsigned_overflow(value):
    with pytest.raises(RuntimeError, match="constant_ordinal"):
        dal.DupireQuoteBinding_(constant_ordinal=value, quote_id="quote:0")


@pytest.mark.parametrize("value", [None, True, 0, NumericEnum.VALUE, dal.CurveJacobianMode.ANALYTIC, "quote:0\0hidden"])
def test_quote_ids_are_checked_strings(value):
    with pytest.raises((TypeError, RuntimeError), match="DupireQuoteBinding_; quote_id"):
        dal.DupireQuoteBinding_(constant_ordinal=0, quote_id=value)


@pytest.mark.parametrize("field,value", [
    ("quotes", {}), ("quotes", dal.RiskRequest_()),
    ("valuation", True), ("valuation", {}), ("simulation", True), ("simulation", {}),
    ("direct", dal.DoubleMatrix_(3, 2)), ("direct", {}),
    ("direct_bindings", {}), ("direct_bindings", "quote:0"),
    ("direct_bindings", iter([])), ("direct_bindings", [(0, "quote:0")]),
    ("direct_bindings", [None]), ("direct_bindings", [True]),
])
def test_optional_values_require_native_types_and_copied_typed_sequences(field, value):
    with pytest.raises(TypeError, match="DupireScriptRiskRequest_; " + field):
        dal.DupireScriptRiskRequest_(num_paths=257, **{field: value})


def test_constructors_require_keywords_and_index_values_are_supported():
    with pytest.raises(TypeError):
        dal.DupireScriptRiskRequest_(257)
    with pytest.raises(TypeError):
        dal.DupireScriptRiskRequest_()
    with pytest.raises(TypeError):
        dal.DupireQuoteBinding_(0, "quote:0")
    class Index:
        def __index__(self):
            return 257
    assert dal.DupireScriptRiskRequest_(num_paths=Index()).num_paths == 257
    assert dal.DupireQuoteBinding_(constant_ordinal=Index(), quote_id=dal.String_("quote:3")).constant_ordinal == 257


def test_request_properties_are_readonly_detached_and_copied():
    bindings = [dal.DupireQuoteBinding_(constant_ordinal=0, quote_id="quote:3")]
    ids, factors = ["quote:3"], [0.01]
    settings = dal.MonteCarloSettings_(enable_aad=True, compiled=True)
    dated = valuation()
    request = dal.DupireScriptRiskRequest_(
        num_paths=257, quotes=dal.CalibrationRiskRequest_(inputs=ids, report_factors=factors),
        direct_bindings=bindings, valuation=dated, simulation=settings,
    )
    ids.clear()
    factors[0] = 999
    bindings.clear()
    settings.compiled = False
    dated.evaluation_date = dal.Date_(2030, 1, 1)
    request.direct_bindings.clear()
    request.quotes.inputs.clear()
    request.simulation.enable_aad = False
    request.valuation.evaluation_date = dal.Date_(2031, 1, 1)
    assert request.quotes.inputs == ["quote:3"]
    assert request.quotes.report_factors == [0.01]
    assert request.simulation.enable_aad and request.simulation.compiled
    assert request.valuation.evaluation_date == dal.Date_(2026, 9, 12)
    assert request.direct_bindings[0].quote_id == "quote:3"
    assert request.direct is None
    for field in ("num_paths", "quotes", "direct_bindings", "direct", "valuation", "simulation"):
        with pytest.raises(AttributeError):
            setattr(request, field, None)
    binding = request.direct_bindings[0]
    with pytest.raises(AttributeError):
        binding.quote_id = "quote:0"
    assert copy.copy(binding).quote_id == copy.deepcopy(binding).quote_id == "quote:3"
    for copied in (copy.copy(request), copy.deepcopy(request)):
        assert copied.num_paths == 257 and copied.direct_bindings[0].quote_id == "quote:3"


@pytest.mark.parametrize("field,value", [
    ("product", None), ("modelData", None), ("calibration", None),
    ("component", True), ("component", "Z_LOCAL\0hidden"), ("request", None),
    ("request", dal.CalibrationRiskRequest_()),
])
def test_required_plan_arguments_identify_the_offending_field(field, value):
    calibration = flat_calibration()
    arguments = dict(product=smooth_product(), modelData=hybrid_model(calibration),
                     calibration=calibration, component="Z_LOCAL",
                     request=dal.DupireScriptRiskRequest_(num_paths=257, valuation=valuation()))
    arguments[field] = value
    with pytest.raises((TypeError, RuntimeError), match="DupireScriptRiskPlan_New; " + field):
        dal.DupireScriptRiskPlan_New(**arguments)
    with pytest.raises(TypeError, match="DupireScriptRiskResult_New; plan"):
        dal.DupireScriptRiskResult_New(value)


@pytest.mark.parametrize("quotes,bindings,simulation,reason", [
    (dal.CalibrationRiskRequest_(inputs=["quote:99"]), [], None, "unknown input"),
    (dal.CalibrationRiskRequest_(inputs=["quote:0", "QUOTE:0"]), [], None, "repeated input"),
    (dal.CalibrationRiskRequest_(inputs=[], report_factors=[0.01]), [], None, "factor count"),
    (dal.CalibrationRiskRequest_(inputs=["quote:0"], report_factors=[0.0]), [], None, "finite and positive"),
    (None, [dal.DupireQuoteBinding_(constant_ordinal=1, quote_id="quote:3")], None, "unknown direct constant ordinal"),
    (None, [dal.DupireQuoteBinding_(constant_ordinal=0, quote_id="quote:99")], None, "unknown direct quote"),
    (None, [dal.DupireQuoteBinding_(constant_ordinal=0, quote_id="quote:3")] * 2, None, "repeated direct constant"),
    (None, [], dal.MonteCarloSettings_(enable_aad=False), "native AAD execution is required"),
    (dal.CalibrationRiskRequest_(numeric_payload_budget_bytes=295), [], None, "CalibrationRiskBudgetExceeded"),
])
def test_semantic_validation_stays_in_native_planning_and_recovers(quotes, bindings, simulation, reason):
    request = dal.DupireScriptRiskRequest_(num_paths=257, quotes=quotes, direct_bindings=bindings,
                                         valuation=valuation(), simulation=simulation)
    with pytest.raises(RuntimeError, match=reason):
        plan_for(request)
    assert plan_for(dal.DupireScriptRiskRequest_(num_paths=257, valuation=valuation())).numeric_payload_bytes == 296


def _check_automatic_native_chain(base_kind, compiled):
    base = (dal.BSModelData_New(100, 0.2, 0.05, 0.02) if base_kind == "flat" else
            dal.MertonIVS_(spot=100, vol=0.2, intensity=0.08, average_jump=-0.1, jump_std=0.15))
    calibration = dal.DupireCalibration_New(base, inputs())
    product, model, dated = smooth_product(), hybrid_model(calibration), valuation()
    simulation = dal.MonteCarloSettings_(enable_aad=True, compiled=compiled)
    manual = dal.MonteCarlo_ValueWithRisk(product, model, 257, valuation=dated, simulation=simulation)
    direct_matrix = dal.DoubleMatrix_(3, 2)
    direct_matrix[1, 1] = manual.jacobian[0, len(manual.input_axis) - 1]
    native = dal.DupireScriptQuoteRisk_New(
        manual, calibration, "Z_LOCAL", direct=dal.DupireDirectQuoteAdjoints_(calibration, direct_matrix),
    )
    for selected, factors in ((None, None), (["quote:3", "quote:0"], [0.01, 0.5]), ([], [])):
        request = dal.DupireScriptRiskRequest_(
            num_paths=257, quotes=dal.CalibrationRiskRequest_(inputs=selected, report_factors=factors, numeric_payload_budget_bytes=304),
            direct_bindings=[dal.DupireQuoteBinding_(constant_ordinal=0, quote_id="quote:3")], valuation=dated, simulation=simulation,
        )
        plan = dal.DupireScriptRiskPlan_New(product, model, calibration, "Z_LOCAL", request)
        result = dal.DupireScriptRiskResult_New(plan)
        assert result.valuation.values == manual.values
        assert result.method == "NativeAADThenNativeAADCalibrationVJP"
        assert result.component == plan.component == "Z_LOCAL"
        assert result.numeric_payload_bytes == 304
        assert result.valuation.provenance.calibration == "fixed"
        manual_columns = {coordinate.id: ordinal for ordinal, coordinate in enumerate(manual.input_axis)}
        assert result.valuation.jacobian.to_rows() == [[
            manual.jacobian[0, manual_columns[coordinate.id]] for coordinate in plan.required_input_axis
        ]]
        for field in ("calibration_adjoints", "direct_adjoints", "total_adjoints"):
            assert getattr(result.quote_risk.quote_risk, field).to_rows() == getattr(native.quote_risk, field).to_rows()
        assert result.quote_risk.quote_risk.direct_adjoints[1, 1] == pytest.approx(3 * math.exp(-calibration.rate), abs=1e-10)
        ordinals = list(range(6)) if selected is None else [int(identifier.split(":")[1]) for identifier in selected]
        raw = [native.quote_risk.total_adjoints[ordinal // 2, ordinal % 2] for ordinal in ordinals]
        assert result.quote_risk.jacobian.to_rows() == [raw]
        factors = [1.0] * len(ordinals) if factors is None else factors
        assert result.quote_risk.reported_jacobian.to_rows() == [[value * factor for value, factor in zip(raw, factors)]]


def run_one_worker(function, *arguments, module=None):
    probe = (
        "import json, runpy, sys; sys.path[:] = json.loads(sys.argv[1]); "
        "runpy.run_path(sys.argv[2])[sys.argv[3]](*json.loads(sys.argv[4]))"
    )
    completed = subprocess.run(
        [sys.executable, "-S", "-c", probe, json.dumps(sys.path), str(Path(module or __file__).resolve()), function, json.dumps(arguments)],
        env={**os.environ, "DAL_NUM_THREADS": "1"}, capture_output=True, text=True, timeout=60, check=False,
    )
    assert completed.returncode == 0, completed.stdout + completed.stderr


@pytest.mark.parametrize("base_kind", ["flat", "merton"])
@pytest.mark.parametrize("compiled", [False, True])
def test_flat_and_merton_requests_match_manual_native_chain_in_one_worker(base_kind, compiled):
    run_one_worker("_check_automatic_native_chain", base_kind, compiled)


def test_external_direct_uses_quote_identity_and_cannot_double_add_bindings():
    calibration = flat_calibration()
    other = dal.DupireCalibration_New(dal.BSModelData_New(100, 0.25, 0.05, 0.02), inputs())
    direct = dal.CalibrationDirectQuoteAdjoints_New(dal.CalibrationPullback_New(other), [[-0.125, 0.25]] * 3)
    request = dal.DupireScriptRiskRequest_(num_paths=257, direct=direct, valuation=valuation())
    plan = plan_for(request, calibration)
    assert plan.numeric_payload_bytes == 296
    result = dal.DupireScriptRiskResult_New(plan)
    assert result.quote_risk.quote_risk.direct_adjoints.to_rows() == [[-0.125, 0.25]] * 3
    request.direct.adjoints[0, 0] = 999
    assert request.direct.adjoints[0, 0] == -0.125
    both = dal.DupireScriptRiskRequest_(
        num_paths=257, direct=direct, valuation=valuation(),
        direct_bindings=[dal.DupireQuoteBinding_(constant_ordinal=0, quote_id="quote:3")],
    )
    with pytest.raises(RuntimeError, match="mutually exclusive"):
        plan_for(both, calibration)
    changed = dal.DupireCalibration_New(dal.BSModelData_New(100, 0.2, 0.05, 0.02), inputs(dal.DoubleMatrix_(3, 2, 0.001)))
    incompatible = dal.DupireScriptRiskRequest_(
        num_paths=257, valuation=valuation(),
        direct=dal.CalibrationDirectQuoteAdjoints_New(dal.CalibrationPullback_New(changed), [[0.0, 0.0]] * 3),
    )
    with pytest.raises(RuntimeError, match="DupireSnapshotMismatch"):
        plan_for(incompatible, calibration)
    assert dal.DupireScriptRiskResult_New(plan).quote_risk.quote_risk.direct_adjoints.to_rows() == [[-0.125, 0.25]] * 3


def test_plan_result_and_settings_are_detached_and_survive_owner_destruction():
    request = dal.DupireScriptRiskRequest_(num_paths=257, valuation=valuation())
    plan = plan_for(request)
    copied_plan = copy.deepcopy(plan)
    result = dal.DupireScriptRiskResult_New(plan)
    copied_result = copy.deepcopy(result)
    raw, price = result.quote_risk.jacobian.to_rows(), result.valuation.values
    plan.required_input_axis.clear()
    plan.complete_input_axis.clear()
    plan.simulation_settings.enable_aad = False
    plan.valuation_settings.evaluation_date = dal.Date_(2030, 1, 1)
    result.quote_risk.jacobian[0, 0] = 999
    result.quote_risk.quote_risk.total_adjoints[0, 0] = 999
    result.valuation.jacobian[0, 0] = 999
    result.valuation.values.clear()
    assert plan.simulation_settings.enable_aad
    assert plan.valuation_settings.evaluation_date == dal.Date_(2026, 9, 12)
    assert result.quote_risk.jacobian.to_rows() == raw
    assert result.valuation.values == price
    for owner, field in ((plan, "quote_plan"), (plan, "num_paths"), (result, "quote_risk"), (result, "valuation")):
        with pytest.raises(AttributeError):
            setattr(owner, field, None)
    assert not hasattr(plan, "product") and not hasattr(plan, "model")
    del plan, result, request
    gc.collect()
    assert copied_result.quote_risk.jacobian.to_rows() == raw
    assert len(copied_plan.required_input_axis) == 18


def test_empty_quotes_keep_smoothed_estimator_and_expired_results():
    calibration = flat_calibration()
    product = dal.Product_New([dal.Date_(2026, 9, 12)], ["if FIX(EQ[LOCAL]) > 100 then pay PAYS 1 else pay PAYS 0 end"])
    plan = plan_for(dal.DupireScriptRiskRequest_(
        num_paths=257, valuation=valuation(), quotes=dal.CalibrationRiskRequest_(inputs=[]),
    ), calibration=calibration, product=product)
    result = dal.DupireScriptRiskResult_New(plan)
    assert result.quote_risk.jacobian.to_rows() == [[]]
    assert len(result.valuation.input_axis) == 18
    assert result.valuation.provenance.method == "NativeAAD"
    native = dal.MonteCarlo_ValueWithRisk(product, hybrid_model(calibration), 257, valuation=valuation())
    passive = dal.MonteCarlo_ValueWithSettings(product, hybrid_model(calibration), 257, valuation=valuation())
    assert result.valuation.values == native.values
    assert result.valuation.values[0] != passive["PV"]
    expired = plan_for(dal.DupireScriptRiskRequest_(
        num_paths=257, valuation=dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2028, 1, 1)),
    ), product=dal.Product_New([dal.Date_(2027, 9, 12)], ["pay PAYS FIX(EQ[LOCAL])"]))
    expired_result = dal.DupireScriptRiskResult_New(expired)
    assert expired_result.method == "ExpiredThenNativeAADCalibrationVJP"
    assert expired_result.valuation.values == [0.0]
    assert expired_result.quote_risk.jacobian.to_rows() == [[0.0] * 6]


def test_execution_releases_gil_and_passive_results_support_concurrent_reads():
    plan = plan_for(dal.DupireScriptRiskRequest_(num_paths=4097, valuation=valuation()))
    result = _run_with_quote_risk_gil_heartbeat(lambda: dal.DupireScriptRiskResult_New(plan), barrier=lambda _: None)
    expected = result.quote_risk.jacobian.to_rows()
    with ThreadPoolExecutor(max_workers=2) as pool:
        copies = list(pool.map(lambda _: copy.deepcopy(result).quote_risk.jacobian.to_rows(), range(4)))
    assert copies == [expected] * 4


def test_planning_captures_global_date_and_explicit_fixing_snapshot():
    original = dal.EvaluationDate_Get()
    try:
        dal.EvaluationDate_Set(dal.Date_(2026, 9, 12))
        request = dal.DupireScriptRiskRequest_(num_paths=257)
        plan = plan_for(request)
        assert request.valuation.evaluation_date is None
        assert plan.valuation_settings.evaluation_date == dal.Date_(2026, 9, 12)
        dal.EvaluationDate_Set(dal.Date_(2028, 1, 1))
        assert not dal.DupireScriptRiskResult_New(plan).valuation.provenance.execution.all_expired
    finally:
        dal.EvaluationDate_Set(original)
    time = dal.DateTime_(dal.Date_(2026, 9, 11), 0, 0)
    fixing = dal.MarketFixingSnapshot_New({"EQ[LOCAL]": {time: 80.0}})
    settings = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 9, 12), fixings=fixing)
    request = dal.DupireScriptRiskRequest_(num_paths=257, valuation=settings)
    product = dal.Product_New([dal.Date_(2026, 9, 11), dal.Date_(2027, 9, 12)],
                              ["x = FIX(EQ[LOCAL])", "pay PAYS x + FIX(EQ[LOCAL])"])
    plan = plan_for(request, product=product)
    settings.fixings = dal.MarketFixingSnapshot_New({"EQ[LOCAL]": {time: 90.0}})
    del request, settings, fixing
    result = dal.DupireScriptRiskResult_New(plan)
    historical = [observation for observation in result.valuation.provenance.execution.observations if observation.historical]
    assert len(historical) == 1 and historical[0].value == 80.0
    assert result.valuation.provenance.execution.fixing_source == "ExplicitSnapshot"


def test_retrained_policy_method_and_surface_owner_validation_remain_native():
    calibration = flat_calibration()
    product = dal.Product_New([dal.Date_(2027, 3, 12), dal.Date_(2027, 9, 12)],
                              ["EXERCISE MAX(100 - FIX(EQ[LOCAL]), 0)"] * 2,
                              settings=dal.ScriptProductSettings_(default_index="EQ[LOCAL]"))
    request = dal.DupireScriptRiskRequest_(
        num_paths=32, valuation=valuation(),
        simulation=dal.MonteCarloSettings_(enable_aad=True, lsmc_training_paths=64, lsmc_policy_risk_mode="RetrainedBump"),
    )
    result = dal.DupireScriptRiskResult_New(plan_for(request, calibration, product))
    assert result.method == "NativeAADWithRetrainedPolicySecantThenNativeAADCalibrationVJP"
    assert result.valuation.provenance.calibration == "fixed"
    incompatible = dal.DupireCalibration_New(dal.BSModelData_New(100, 0.25, 0.05, 0.02), inputs())
    with pytest.raises(RuntimeError, match="DupireSnapshotMismatch"):
        dal.DupireScriptRiskPlan_New(product, hybrid_model(incompatible), calibration, "Z_LOCAL", request)
    assert dal.DupireScriptRiskResult_New(plan_for(request, calibration, product)).method == result.method


def _check_native_execution_on_two_calling_threads():
    plan = plan_for(dal.DupireScriptRiskRequest_(num_paths=257, valuation=valuation()))
    reference = dal.DupireScriptRiskResult_New(plan)
    with ThreadPoolExecutor(max_workers=2) as pool:
        futures = [pool.submit(dal.DupireScriptRiskResult_New, plan) for _ in range(2)]
        results = [future.result(timeout=30) for future in futures]
    for result in results:
        assert result.valuation.values == pytest.approx(reference.valuation.values, rel=1e-10, abs=1e-10)
        assert result.quote_risk.jacobian.to_rows()[0] == pytest.approx(reference.quote_risk.jacobian.to_rows()[0], rel=1e-10, abs=1e-10)


def test_native_execution_can_run_on_two_calling_threads():
    run_one_worker("_check_native_execution_on_two_calling_threads")


def test_parallel_execution_preserves_native_pv_and_required_surface_gradients():
    calibration = flat_calibration()
    product, model, dated = smooth_product(), hybrid_model(calibration), valuation()
    request = dal.DupireScriptRiskRequest_(num_paths=257, valuation=dated)
    reference = dal.MonteCarlo_ValueWithRisk(product, model, 257, valuation=dated)
    plan = dal.DupireScriptRiskPlan_New(product, model, calibration, "Z_LOCAL", request)
    result = dal.DupireScriptRiskResult_New(plan)
    assert result.valuation.values == pytest.approx(reference.values, rel=1e-10, abs=1e-10)
    expected = reference.jacobian.to_rows()[0][6:24]
    assert result.valuation.jacobian.to_rows()[0] == pytest.approx(expected, rel=1e-10, abs=1e-10)
    raw = result.quote_risk.quote_risk
    assert raw.calibration_adjoints.to_rows() == raw.total_adjoints.to_rows()
    assert raw.direct_adjoints.to_rows() == [[0.0, 0.0]] * 3
    assert result.quote_risk.jacobian.to_rows() == [[value for row in raw.total_adjoints.to_rows() for value in row]]
