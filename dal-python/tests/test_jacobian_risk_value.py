"""Owning blocked script Jacobians and strict request inputs."""

import copy
import enum
import gc

import dal
import pytest


def jacobian_inputs():
    product = dal.Product_New(
        ["X", "Y", dal.Date_(2027, 1, 1)],
        ["2", "3", "a = X * Y b = X + Y alias = a pay PAYS 5"],
    )
    model = dal.BSModelData_New(spot=100, vol=0.2, rate=0, div=0)
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 1, 1))
    return product, model, valuation


@pytest.mark.parametrize("compiled", [False, True])
def test_ordered_jacobian_rows_columns_reporting_and_replay(compiled):
    product, model, valuation = jacobian_inputs()
    request = dal.JacobianRiskRequest_(
        outputs=["output:2", "output:1", "payoff", "output:0"],
        inputs=["constant:1", "constant:0"],
        report_factors=[0.5, 2],
        max_block_width=3,
        numeric_payload_budget_bytes=96,
    )
    result = dal.MonteCarlo_ValueWithJacobianRisk(
        product,
        model,
        17,
        request=request,
        valuation=valuation,
        simulation=dal.MonteCarloSettings_(enable_aad=True, compiled=compiled),
    )
    assert result.values == [6, 5, 5, 6]
    assert result.jacobian.Rows() == 4
    assert result.jacobian.Cols() == 2
    assert result.jacobian.to_rows() == [[2, 3], [1, 1], [0, 0], [2, 3]]
    assert result.reported_jacobian.to_rows() == [[1, 6], [0.5, 2], [0, 0], [1, 6]]
    assert [c.id for c in result.output_axis] == request.outputs
    assert len(result.complete_output_axis) == 4
    assert [c.id for c in result.input_axis] == request.inputs
    assert len(result.complete_input_axis) == 6
    assert result.execution.actual_widths == [3, 3]
    assert result.execution.replay_attempts == 2
    assert result.execution.executed_paths == 34
    assert result.execution.peak_recording_bytes > 0
    assert result.execution.peak_scratch_bytes > 0
    assert result.provenance.method == "NativeAAD"
    assert result.provenance.execution.paths_per_replicate == 17


def test_request_and_result_are_detached_and_survive_other_calls():
    product, model, valuation = jacobian_inputs()
    outputs = ["output:0"]
    factors = [2]
    request = dal.JacobianRiskRequest_(
        outputs=outputs, inputs=["constant:1"], report_factors=factors
    )
    outputs.clear()
    factors.clear()
    result = dal.MonteCarlo_ValueWithJacobianRisk(
        product, model, 17, request=request, valuation=valuation
    )
    request.outputs.clear()
    request.inputs.clear()
    request.report_factors.clear()
    result.values.clear()
    result.output_axis.clear()
    result.complete_output_axis.clear()
    result.input_axis.clear()
    result.complete_input_axis.clear()
    result.execution.actual_widths.clear()
    result.jacobian[0, 0] = -999
    result.reported_jacobian[0, 0] = -999
    with pytest.raises(AttributeError):
        request.max_block_width = 3
    with pytest.raises(AttributeError):
        result.execution.executed_paths = 99
    assert request.outputs == ["output:0"]
    assert result.values == [6]
    assert result.jacobian.to_rows() == [[2]]
    assert result.reported_jacobian.to_rows() == [[4]]
    with pytest.raises(RuntimeError, match="budget"):
        dal.MonteCarlo_ValueWithJacobianRisk(
            product,
            model,
            17,
            request=dal.JacobianRiskRequest_(scratch_capacity_budget_bytes=0),
            valuation=valuation,
        )
    saved = copy.deepcopy(result)
    del product, model, request, result
    gc.collect()
    assert saved.values == [6]
    assert saved.jacobian.to_rows() == [[2]]
    assert saved.execution.actual_widths == [1]


@pytest.mark.parametrize("compiled", [False, True])
@pytest.mark.parametrize("native", [False, True])
def test_empty_columns_preserve_estimator_and_two_dimensional_shape(compiled, native):
    product = dal.Product_New(
        [dal.Date_(2026, 1, 1)],
        ["IF SPOT() > 100 THEN a = 1 ELSE a = 0 END b = a pay PAYS 0"],
    )
    model = dal.BSModelData_New(spot=100, vol=0, rate=0, div=0)
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 1, 1))
    result = dal.MonteCarlo_ValueWithJacobianRisk(
        product,
        model,
        17,
        request=dal.JacobianRiskRequest_(outputs=["output:1", "output:0"], inputs=[]),
        valuation=valuation,
        simulation=dal.MonteCarloSettings_(enable_aad=native, compiled=compiled),
    )
    assert result.values == ([0.5, 0.5] if native else [0, 0])
    assert result.jacobian.Rows() == 2
    assert result.jacobian.Cols() == 0
    assert result.provenance.method == ("NativeAAD" if native else "PriceOnly")
    assert result.execution.actual_widths == ([1, 1] if native else [])
    assert result.execution.executed_paths == (34 if native else 17)


class NumericEnum(enum.IntEnum):
    ONE = 1


@pytest.mark.parametrize(
    "field",
    [
        "max_block_width",
        "numeric_payload_budget_bytes",
        "recording_capacity_budget_bytes",
        "scratch_capacity_budget_bytes",
    ],
)
@pytest.mark.parametrize("value", [True, 1.5, "1", NumericEnum.ONE])
def test_width_and_budgets_reject_implicit_integer_conversions(field, value):
    with pytest.raises(TypeError, match=field):
        dal.JacobianRiskRequest_(**{field: value})


@pytest.mark.parametrize(
    "kwargs",
    [
        {"inputs": "model:0"},
        {"outputs": [True]},
        {"report_factors": [True]},
        {"max_block_width": 0},
        {"max_block_width": -1},
        {"max_block_width": 2**100},
        {"numeric_payload_budget_bytes": -1},
        {"recording_capacity_budget_bytes": 2**100},
        {"scratch_capacity_budget_bytes": -1},
    ],
)
def test_malformed_requests_fail(kwargs):
    with pytest.raises((TypeError, RuntimeError)):
        dal.JacobianRiskRequest_(**kwargs)


@pytest.mark.parametrize("paths", [True, 1.5, 0, -1, 2**100])
def test_path_count_and_request_type_are_strict(paths):
    product, model, valuation = jacobian_inputs()
    with pytest.raises((TypeError, RuntimeError)):
        dal.MonteCarlo_ValueWithJacobianRisk(product, model, paths, valuation=valuation)


def test_keyword_only_defaults_and_wrong_settings_types():
    product, model, valuation = jacobian_inputs()
    with pytest.raises(TypeError):
        dal.JacobianRiskRequest_(["model:0"])
    with pytest.raises(TypeError):
        dal.MonteCarlo_ValueWithJacobianRisk(
            product, model, 1, dal.JacobianRiskRequest_()
        )
    with pytest.raises(TypeError, match="JacobianRiskRequest_"):
        dal.MonteCarlo_ValueWithJacobianRisk(
            product, model, 1, request=dal.RiskRequest_(), valuation=valuation
        )
    result = dal.MonteCarlo_ValueWithJacobianRisk(
        product, model, 17, valuation=valuation
    )
    assert result.values == [5]
    assert result.jacobian.Rows() == 1
    assert result.jacobian.Cols() == 6


def test_exact_result_budget_and_invalid_ordered_axes():
    product, model, valuation = jacobian_inputs()
    for kwargs in (
        {"outputs": []},
        {"outputs": ["payoff", "payoff"]},
        {"outputs": ["unknown"]},
        {"inputs": ["model:0", "model:0"]},
        {"inputs": ["constant:99"]},
        {"numeric_payload_budget_bytes": 55},
    ):
        with pytest.raises(RuntimeError):
            dal.MonteCarlo_ValueWithJacobianRisk(
                product,
                model,
                17,
                request=dal.JacobianRiskRequest_(**kwargs),
                valuation=valuation,
            )
