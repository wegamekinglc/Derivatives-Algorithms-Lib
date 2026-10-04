"""Structured scalar native risk, projection and immutable result contracts."""

import json

import dal
import pytest


def product_model():
    product = dal.Product_New(
        ["STRIKE", dal.Date_(2023, 9, 25)],
        ["100", "call PAYS MAX(SPOT() - STRIKE, 0)"],
    )
    model = dal.BSModelData_New(spot=100, vol=0.2, rate=0.05, div=0.02)
    return product, model


@pytest.mark.parametrize("compiled", [False, True])
def test_selected_risk_matches_legacy_and_getters_do_not_mutate(compiled):
    product, model = product_model()
    simulation = dal.MonteCarloSettings_(enable_aad=True, compiled=compiled)
    request = dal.RiskRequest_(
        inputs=["constant:0", "model:1"], report_factors=[0.5, 0.01]
    )
    old = dal.MonteCarlo_ValueWithSettings(product, model, 257, simulation=simulation)
    result = dal.MonteCarlo_ValueWithRisk(
        product, model, 257, request=request, simulation=simulation
    )
    assert result.values == [old["PV"]]
    assert result.jacobian.Rows() == 1
    assert result.jacobian.Cols() == 2
    assert result.jacobian[0, 0] == old["d_STRIKE"]
    assert result.jacobian[0, 1] == old["d_vol"]
    assert result.reported_jacobian[0, 1] == 0.01 * old["d_vol"]
    assert result.legacy_values["d_vol"] == old["d_vol"]
    assert [axis.id for axis in result.input_axis] == ["constant:0", "model:1"]
    matrix = result.jacobian
    matrix[0, 1] = -999
    assert result.jacobian[0, 1] == old["d_vol"]
    with pytest.raises(AttributeError):
        result.input_axis[0].id = "changed"
    simulation.enable_aad = False
    assert result.provenance.execution.simulation.enable_aad is True
    assert result.provenance.method == "NativeAAD"
    assert result.provenance.execution.paths_per_replicate == 257
    assert json.loads(result.provenance.execution.model_snapshot_json)


def test_native_empty_and_passive_omitted_inputs_preserve_zero_column_shape():
    product, model = product_model()
    native = dal.MonteCarlo_ValueWithRisk(
        product, model, 32, request=dal.RiskRequest_(inputs=[], numeric_payload_budget_bytes=8)
    )
    passive = dal.MonteCarlo_ValueWithRisk(
        product, model, 32, simulation=dal.MonteCarloSettings_(enable_aad=False)
    )
    for result in [native, passive]:
        assert result.jacobian.Rows() == 1
        assert result.jacobian.Cols() == 0
        assert result.jacobian.to_rows() == [[]]
    assert native.provenance.method == "NativeAAD"
    assert passive.provenance.method == "PriceOnly"


@pytest.mark.parametrize(
    "kwargs, message",
    [
        ({"inputs": ["bad"]}, "unknown input"),
        ({"inputs": ["model:0", "MODEL:0"]}, "repeated input"),
        ({"outputs": []}, "payoff"),
        ({"outputs": ["other"]}, "payoff"),
        ({"inputs": ["model:0"], "report_factors": [0]}, "report factor"),
        ({"numeric_payload_budget_bytes": 0}, "RiskResultBudgetExceeded"),
    ],
)
def test_bad_request_rejects_and_previous_result_survives(kwargs, message):
    product, model = product_model()
    first = dal.MonteCarlo_ValueWithRisk(product, model, 1)
    saved = first.jacobian.to_rows()
    with pytest.raises(RuntimeError, match=message):
        dal.MonteCarlo_ValueWithRisk(product, model, 1, request=dal.RiskRequest_(**kwargs))
    later = dal.MonteCarlo_ValueWithRisk(product, model, 1)
    assert first.jacobian.to_rows() == saved
    assert later.jacobian.to_rows() == saved


@pytest.mark.parametrize(
    "kwargs",
    [
        {"inputs": "model:0"},
        {"inputs": [1]},
        {"outputs": [True]},
        {"report_factors": [True]},
        {"numeric_payload_budget_bytes": True},
        {"numeric_payload_budget_bytes": 1.5},
    ],
)
def test_request_rejects_implicit_python_type_coercions(kwargs):
    with pytest.raises(TypeError):
        dal.RiskRequest_(**kwargs)


def test_request_is_keyword_only_and_payload_budget_is_integer_checked():
    with pytest.raises(TypeError):
        dal.RiskRequest_(["model:0"])
    with pytest.raises(RuntimeError, match="numeric_payload_budget_bytes"):
        dal.RiskRequest_(numeric_payload_budget_bytes=-1)
    with pytest.raises(RuntimeError, match="numeric_payload_budget_bytes"):
        dal.RiskRequest_(numeric_payload_budget_bytes=2**100)
