"""Owning common quote requests and native report projections."""

import copy
from concurrent.futures import ThreadPoolExecutor
from enum import IntEnum
import gc
import math
import pickle

import dal
import pytest

from test_calibration_risk import captured_generic, captured_xccy
from test_dupire_risk import flat_calibration, inputs as dupire_inputs
from test_quote_risk import _run_with_quote_risk_gil_heartbeat, _single_quote_risk_inputs


def test_common_request_selects_native_quote_ordinals_and_reports_once():
    boundary = dal.CalibrationPullback_New(flat_calibration())
    request = dal.CalibrationRiskRequest_(
        inputs=["quote:3", "quote:0"], report_factors=[0.01, 0.5],
        numeric_payload_budget_bytes=144,
    )
    plan = dal.CalibrationRiskPlan_New(boundary, request=request)
    seed = dal.CalibrationParameterAdjoints_New(boundary, [[-0.25, 0.5]] * 9)
    direct = dal.CalibrationDirectQuoteAdjoints_New(boundary, [[0.125, -0.25]] * 3)
    result = dal.CalibrationRiskResult_New(plan, seed, direct=direct)
    reference = dal.PullbackCalibration(boundary, seed, direct=direct)
    assert plan.selected_ordinals == [3, 0]
    assert plan.numeric_payload_bytes == 144
    assert [coordinate.id for coordinate in plan.input_axis] == ["quote:3", "quote:0"]
    assert [coordinate.id for coordinate in plan.complete_input_axis] == [f"quote:{ordinal}" for ordinal in range(6)]
    assert result.jacobian.to_rows() == [[reference.total_adjoints[1, 1], reference.total_adjoints[0, 0]]]
    assert result.reported_jacobian.to_rows() == [[reference.total_adjoints[1, 1] * 0.01, reference.total_adjoints[0, 0] * 0.5]]
    for field in ("calibration_adjoints", "direct_adjoints", "total_adjoints"):
        assert getattr(result.quote_risk, field).to_rows() == getattr(reference, field).to_rows()


def curve_source(provider, mode):
    if provider in ("generic", "layered"):
        return captured_generic(layered=provider == "layered", mode=mode)[0]
    if provider in ("JOINT_XCCY", "STAGED_XCCY_BASIS"):
        return captured_xccy(provider, mode)
    spec, options, _, fixings, _, config, _, _ = _single_quote_risk_inputs()
    options.jacobian_mode = mode
    calibrated = dal.CalibrateSingleCurve(spec, options)
    market = dal.RatePricingMarket_(
        valuation_time=dal.DateTime_(dal.Date_(2025, 6, 20), 9, 0), result_currency="USD",
        curve_components={"discount": calibrated.curve_}, fixings=fixings,
    )
    return dal.BuildSingleCurveQuoteRiskProvenance(
        spec=spec, result=calibrated, options=options, bound_market=market,
        config=dal.RateQuoteRiskProvenanceConfig_(
            calibration_id=config.calibration_id,
            component_key_by_parameter_block=config.component_key_by_parameter_block,
            retain_calibration_record=True,
        ),
    )


def check_requested_mapping(source):
    boundary = dal.CalibrationPullback_New(source)
    count = boundary.quote_rows * boundary.quote_cols
    bytes_required = 24 * count
    seed = dal.CalibrationParameterAdjoints_New(boundary, [
        [math.sin(0.3 + row + col) / 4 for col in range(boundary.parameter_cols)]
        for row in range(boundary.parameter_rows)
    ])
    direct = dal.CalibrationDirectQuoteAdjoints_New(boundary, [
        [(-1.0)**(row + col) * (1 + row + col) / 8 for col in range(boundary.quote_cols)]
        for row in range(boundary.quote_rows)
    ])
    reference = dal.PullbackCalibration(boundary, seed, direct=direct)
    selected_ids = [f"quote:{count - 1}"] + (["QUOTE:0"] if count > 1 else [])
    for ids, factors in ((None, None), (selected_ids, [0.01, 1e-4][:len(selected_ids)]), ([], [])):
        request = dal.CalibrationRiskRequest_(
            inputs=ids, report_factors=factors, numeric_payload_budget_bytes=bytes_required,
        )
        plan = dal.CalibrationRiskPlan_New(boundary, request=request)
        result = dal.CalibrationRiskResult_New(plan, seed, direct=direct)
        assert plan.numeric_payload_bytes == bytes_required
        assert plan.calibration.matches(boundary)
        assert result.plan.calibration.matches(boundary)
        ordinals = list(range(count)) if ids is None else [int(identifier.split(":")[1]) for identifier in ids]
        assert plan.selected_ordinals == ordinals
        assert [c.id for c in plan.input_axis] == [f"quote:{ordinal}" for ordinal in ordinals]
        assert len(plan.complete_input_axis) == count
        for ordinal, coordinate in enumerate(plan.complete_input_axis):
            assert coordinate.ordinal == ordinal
            assert coordinate.id == f"quote:{ordinal}"
            assert (coordinate.row, coordinate.column) == divmod(ordinal, boundary.quote_cols)
            assert coordinate.report_scale == 1.0
            if boundary.domain == "DUPIRE":
                assert coordinate.label == f"spread:{coordinate.row}:{coordinate.column}"
                assert coordinate.native_unit == "decimal-vol"
                assert coordinate.value == source.inputs.quote_spreads[coordinate.row, coordinate.column]
                assert coordinate.strike == source.inputs.quote_strikes[coordinate.row]
                assert coordinate.maturity == source.inputs.quote_maturities[coordinate.column]
                assert coordinate.block_key is coordinate.block_ordinal is None
            else:
                quote = source.axis.quotes[ordinal]
                assert coordinate.label == quote.display_name
                assert coordinate.native_unit == quote.unit
                assert coordinate.block_key == quote.block_key
                assert coordinate.block_ordinal == quote.block_ordinal
                assert coordinate.value is coordinate.strike is coordinate.maturity is None
        for field, projection in (
            ("total_adjoints", "jacobian"), ("calibration_adjoints", "calibration_jacobian"),
            ("direct_adjoints", "direct_jacobian"),
        ):
            matrix = getattr(reference, field)
            assert getattr(result.quote_risk, field).to_rows() == matrix.to_rows()
            selected = [matrix[ordinal // boundary.quote_cols, ordinal % boundary.quote_cols] for ordinal in ordinals]
            assert getattr(result, projection).to_rows() == [selected]
        scales = [1.0] * len(ordinals) if factors is None else factors
        assert result.reported_jacobian.to_rows() == [[value * factor for value, factor in zip(result.jacobian.to_rows()[0], scales)]]
        assert (result.quote_risk.method, result.quote_risk.unit, result.quote_risk.boundary) == (
            reference.method, reference.unit, reference.boundary,
        )
        short = dal.CalibrationRiskRequest_(
            inputs=ids, report_factors=factors, numeric_payload_budget_bytes=bytes_required - 1,
        )
        with pytest.raises(RuntimeError, match="CalibrationRiskBudgetExceeded"):
            dal.CalibrationRiskPlan_New(boundary, request=short)


def test_dupire_complete_subset_and_empty_requests_keep_full_payload_and_metadata():
    check_requested_mapping(flat_calibration())


@pytest.mark.parametrize("mode", [dal.CurveJacobianMode.ANALYTIC, dal.CurveJacobianMode.BUMPED])
@pytest.mark.parametrize("provider", ["single", "generic", "layered", "JOINT_XCCY", "STAGED_XCCY_BASIS"])
def test_all_captured_curve_providers_keep_native_axes_and_inverse_modes(provider, mode):
    check_requested_mapping(curve_source(provider, mode))


class NumericEnum(IntEnum):
    VALUE = 1


@pytest.mark.parametrize("field,value", [
    ("inputs", "quote:0"), ("inputs", {"quote:0"}), ("inputs", {"quote:0": 1}),
    ("inputs", iter(["quote:0"])), ("inputs", [True]), ("inputs", [1]),
    ("inputs", [NumericEnum.VALUE]), ("inputs", [dal.CurveJacobianMode.ANALYTIC]),
    ("report_factors", "0.1"), ("report_factors", {0.1}), ("report_factors", [True]),
    ("report_factors", [NumericEnum.VALUE]), ("report_factors", [dal.CurveJacobianMode.ANALYTIC]),
    ("report_factors", ["0.1"]), ("report_factors", [None]),
    ("numeric_payload_budget_bytes", True), ("numeric_payload_budget_bytes", NumericEnum.VALUE),
    ("numeric_payload_budget_bytes", dal.CurveJacobianMode.ANALYTIC),
    ("numeric_payload_budget_bytes", 144.0), ("numeric_payload_budget_bytes", "144"),
])
def test_request_fields_reject_container_and_numeric_coercion(field, value):
    with pytest.raises(TypeError, match="CalibrationRiskRequest_; " + field):
        dal.CalibrationRiskRequest_(**{field: value})


@pytest.mark.parametrize("field,value", [
    ("inputs", ["quote:0\0hidden"]), ("report_factors", [10**1000]),
    ("numeric_payload_budget_bytes", -1), ("numeric_payload_budget_bytes", 2**100),
])
def test_request_nul_or_unrepresentable_inputs_identify_the_field(field, value):
    with pytest.raises(RuntimeError, match=field):
        dal.CalibrationRiskRequest_(**{field: value})


@pytest.mark.parametrize("configuration,reason", [
    ({"inputs": ["quote:6"]}, "unknown input"),
    ({"inputs": ["model:0"]}, "unknown input"),
    ({"inputs": ["quote:0", "QUOTE:0"]}, "repeated input"),
    ({"inputs": ["quote:0"], "report_factors": []}, "factor count"),
    ({"inputs": [], "report_factors": [1.0]}, "factor count"),
    ({"inputs": ["quote:0"], "report_factors": [0.0]}, "finite and positive"),
    ({"inputs": ["quote:0"], "report_factors": [-1.0]}, "finite and positive"),
    ({"inputs": ["quote:0"], "report_factors": [float("nan")]}, "finite and positive"),
    ({"inputs": ["quote:0"], "report_factors": [float("inf")]}, "finite and positive"),
])
def test_semantic_errors_wait_for_native_planning_and_valid_plans_recover(configuration, reason):
    request = dal.CalibrationRiskRequest_(**configuration)
    boundary = dal.CalibrationPullback_New(flat_calibration())
    with pytest.raises(RuntimeError, match=reason):
        dal.CalibrationRiskPlan_New(boundary, request=request)
    assert dal.CalibrationRiskPlan_New(boundary).selected_ordinals == list(range(6))


def test_request_is_keyword_only_copied_readonly_and_preserves_none_and_empty():
    with pytest.raises(TypeError):
        dal.CalibrationRiskRequest_(["quote:0"])
    default = dal.CalibrationRiskRequest_()
    assert default.inputs is default.report_factors is default.numeric_payload_budget_bytes is None
    ids, factors = ["quote:3", "quote:0"], [0.01, 0.5]
    request = dal.CalibrationRiskRequest_(inputs=ids, report_factors=factors, numeric_payload_budget_bytes=144)
    ids.clear()
    factors[0] = 999
    request.inputs.clear()
    request.report_factors[0] = 999
    assert request.inputs == ["quote:3", "quote:0"]
    assert request.report_factors == [0.01, 0.5]
    assert dal.CalibrationRiskRequest_(inputs=(), report_factors=()).inputs == []
    class Index:
        def __index__(self):
            return 144
    assert dal.CalibrationRiskRequest_(numeric_payload_budget_bytes=Index()).numeric_payload_budget_bytes == 144
    for field in ("inputs", "report_factors", "numeric_payload_budget_bytes"):
        with pytest.raises(AttributeError):
            setattr(request, field, None)
    assert copy.copy(request).inputs == copy.deepcopy(request).inputs == ["quote:3", "quote:0"]


def test_plan_result_and_coordinate_copies_survive_owner_destruction_and_detach_every_projection():
    boundary = dal.CalibrationPullback_New(flat_calibration())
    plan = dal.CalibrationRiskPlan_New(boundary)
    seed = dal.CalibrationParameterAdjoints_New(boundary, [[-0.25, 0.5]] * 9)
    result = dal.CalibrationRiskResult_New(plan, seed)
    matrices = {field: getattr(result, field).to_rows() for field in (
        "jacobian", "calibration_jacobian", "direct_jacobian", "reported_jacobian",
    )}
    for field in matrices:
        getattr(result, field)[0, 0] = -999
        assert getattr(result, field).to_rows() == matrices[field]
    result.quote_risk.total_adjoints[0, 0] = -999
    plan.input_axis.clear()
    plan.selected_ordinals.clear()
    plan.calibration.source.inputs.quote_spreads[0, 0] = -999
    coordinate = plan.input_axis[0]
    for value, field in ((plan, "calibration"), (result, "jacobian"), (coordinate, "id")):
        assert type(copy.copy(value)) is type(value)
        assert type(copy.deepcopy(value)) is type(value)
        with pytest.raises(AttributeError):
            setattr(value, field, None)
        with pytest.raises(TypeError):
            pickle.dumps(value)
        with pytest.raises(TypeError, match="Storable"):
            dal._dal._StorableToJson(value)
    for field in ("id", "label", "ordinal", "row", "column", "native_unit", "report_scale", "value", "strike", "maturity", "block_key", "block_ordinal"):
        with pytest.raises(AttributeError):
            setattr(coordinate, field, None)
    del plan, seed, boundary
    gc.collect()
    assert result.jacobian.to_rows() == matrices["jacobian"]
    assert result.plan.selected_ordinals == list(range(6))
    assert result.quote_risk.total_adjoints[0, 0] == matrices["jacobian"][0][0]


def test_required_native_handles_and_optional_types_have_context():
    boundary = dal.CalibrationPullback_New(flat_calibration())
    plan = dal.CalibrationRiskPlan_New(boundary)
    seed = dal.CalibrationParameterAdjoints_New(boundary, [[0.0, 0.0]] * 9)
    for value in (None, True, "source", flat_calibration()):
        with pytest.raises(TypeError, match="CalibrationRiskPlan_New; calibration"):
            dal.CalibrationRiskPlan_New(value)
    for value in (True, {}, seed, dal.RiskRequest_()):
        with pytest.raises(TypeError, match="CalibrationRiskPlan_New; request"):
            dal.CalibrationRiskPlan_New(boundary, request=value)
    for value in (None, True, "plan", boundary):
        with pytest.raises(TypeError, match="CalibrationRiskResult_New; plan"):
            dal.CalibrationRiskResult_New(value, seed)
    for value in (None, True, "seed", [[0.0, 0.0]] * 9):
        with pytest.raises(TypeError, match="CalibrationRiskResult_New; parameter_adjoints"):
            dal.CalibrationRiskResult_New(plan, value)
    for value in (True, "direct", seed):
        with pytest.raises(TypeError, match="CalibrationRiskResult_New; direct"):
            dal.CalibrationRiskResult_New(plan, seed, direct=value)
    with pytest.raises(TypeError):
        dal.CalibrationRiskPlan_New(boundary, dal.CalibrationRiskRequest_())
    with pytest.raises(TypeError):
        dal.CalibrationRiskResult_New(plan, seed, None)
    for factory in (dal.CalibrationRiskPlan_New, dal.CalibrationRiskResult_New):
        with pytest.raises(TypeError):
            factory()


def test_parameter_full_identity_and_dupire_direct_quote_only_identity_recover():
    boundary = dal.CalibrationPullback_New(flat_calibration())
    other = dal.CalibrationPullback_New(dal.DupireCalibration_New(dal.BSModelData_New(100, 0.25, 0.05, 0.02), dupire_inputs()))
    curve = dal.CalibrationPullback_New(captured_generic()[0])
    plan = dal.CalibrationRiskPlan_New(boundary)
    zero = dal.CalibrationParameterAdjoints_New(boundary, [[0.0, 0.0]] * 9)
    wrong = dal.CalibrationParameterAdjoints_New(other, [[0.0, 0.0]] * 9)
    direct = dal.CalibrationDirectQuoteAdjoints_New(other, [[-0.125, 0.25]] * 3)
    result = dal.CalibrationRiskResult_New(plan, zero, direct=direct)
    assert result.quote_risk.total_adjoints.to_rows() == [[-0.125, 0.25]] * 3
    with pytest.raises(RuntimeError, match="CalibrationSnapshotMismatch"):
        dal.CalibrationRiskResult_New(plan, wrong)
    with pytest.raises(RuntimeError, match="CalibrationSnapshotMismatch"):
        dal.CalibrationRiskResult_New(plan, zero, direct=dal.CalibrationDirectQuoteAdjoints_New(curve, [[0.0]] * curve.quote_rows))
    assert dal.CalibrationRiskResult_New(plan, zero).jacobian.to_rows() == [[0.0] * 6]
    assert result.jacobian.to_rows() == [[-0.125, 0.25] * 3]


def test_report_overflow_keeps_earlier_results_and_later_execution_valid():
    boundary = dal.CalibrationPullback_New(flat_calibration())
    zero = dal.CalibrationParameterAdjoints_New(boundary, [[0.0, 0.0]] * 9)
    direct = dal.CalibrationDirectQuoteAdjoints_New(boundary, [[2.0, 2.0]] * 3)
    full = dal.CalibrationRiskPlan_New(boundary)
    earlier = dal.CalibrationRiskResult_New(full, zero, direct=direct)
    bad = dal.CalibrationRiskPlan_New(boundary, request=dal.CalibrationRiskRequest_(inputs=["quote:0"], report_factors=[1e308]))
    with pytest.raises(RuntimeError, match="non-finite reported contribution"):
        dal.CalibrationRiskResult_New(bad, zero, direct=direct)
    assert earlier.jacobian.to_rows() == [[2.0] * 6]
    assert dal.CalibrationRiskResult_New(full, zero, direct=direct).jacobian.to_rows() == [[2.0] * 6]


def test_concurrent_owned_requests_keep_exact_native_results():
    boundaries = [dal.CalibrationPullback_New(flat_calibration()), dal.CalibrationPullback_New(captured_generic()[0])]
    plans = [dal.CalibrationRiskPlan_New(boundary) for boundary in boundaries]
    seeds = [dal.CalibrationParameterAdjoints_New(boundary, [[0.25] * boundary.parameter_cols] * boundary.parameter_rows) for boundary in boundaries]
    expected = [dal.CalibrationRiskResult_New(plan, seed).jacobian.to_rows() for plan, seed in zip(plans, seeds)]
    def operation(index):
        domain = index % 2
        result = dal.CalibrationRiskResult_New(plans[domain], seeds[domain])
        return result.jacobian.to_rows()
    with ThreadPoolExecutor(max_workers=4) as pool:
        actual = list(pool.map(operation, range(24)))
    assert actual == [expected[index % 2] for index in range(24)]


def test_native_requested_mapping_releases_gil_without_a_test_barrier():
    settings = dal.DupireRiskInputs_(
        quote_strikes=[75.0, 105.0, 135.0], quote_maturities=[0.4, 1.2],
        quote_spreads=dal.DoubleMatrix_(3, 2), inclusion_spots=[60.0, 100.0, 140.0],
        max_spot_spacing=1.0, inclusion_times=[ordinal / 10 for ordinal in range(1, 49)], max_time_spacing=0.1,
    )
    boundary = dal.CalibrationPullback_New(dal.DupireCalibration_New(dal.BSModelData_New(100, 0.2, 0.05, 0.02), settings))
    plan = dal.CalibrationRiskPlan_New(boundary, request=dal.CalibrationRiskRequest_(inputs=[]))
    seed = dal.CalibrationParameterAdjoints_New(boundary, dal.DoubleMatrix_(boundary.parameter_rows, boundary.parameter_cols, 0.25))
    reference = dal.PullbackCalibration(boundary, seed).total_adjoints.to_rows()
    result = _run_with_quote_risk_gil_heartbeat(lambda: dal.CalibrationRiskResult_New(plan, seed), barrier=lambda _: None)
    assert result.quote_risk.total_adjoints.to_rows() == reference
    assert result.jacobian.to_rows() == [[]]
