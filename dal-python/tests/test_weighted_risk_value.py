"""Native weighted script objectives, passive ownership and strict Python inputs."""

import copy
import enum
import gc

import dal
import pytest


def weighted_inputs():
    product = dal.Product_New(
        ["X", "Y", dal.Date_(2027, 1, 1)],
        ["2", "3", "a = X * Y b = X + Y pay PAYS 5"],
    )
    model = dal.BSModelData_New(spot=100, vol=0.2, rate=0, div=0)
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 1, 1))
    return product, model, valuation


@pytest.mark.parametrize("compiled", [False, True])
def test_analytic_weighted_objective_and_reported_gradient(compiled):
    product, model, valuation = weighted_inputs()
    request = dal.WeightedRiskRequest_(
        outputs=["output:0", "output:1", "payoff"],
        weights=[2, -1, 0.5],
        inputs=["constant:0", "constant:1"],
        report_factors=[0.5, 2],
        numeric_payload_budget_bytes=72,
    )
    result = dal.MonteCarlo_ValueWithWeightedRisk(
        product,
        model,
        257,
        request=request,
        valuation=valuation,
        simulation=dal.MonteCarloSettings_(enable_aad=True, compiled=compiled),
    )
    assert result.weighted_value == 9.5
    assert result.component_means == [6, 5, 5]
    assert result.weights == [2, -1, 0.5]
    assert [(c.id, c.label, c.slot) for c in result.output_axis] == [
        ("output:0", "a", 0),
        ("output:1", "b", 1),
        ("payoff", "pay", 2),
    ]
    assert result.jacobian.Rows() == 1
    assert result.jacobian.Cols() == 2
    assert result.jacobian.to_rows()[0] == pytest.approx([5, 3], rel=1e-10, abs=1e-10)
    assert result.reported_jacobian.to_rows()[0] == pytest.approx(
        [2.5, 6], rel=1e-10, abs=1e-10
    )
    assert result.provenance.method == "NativeAAD"
    assert result.provenance.execution.paths_per_replicate == 257


def test_output_axis_query_and_default_native_request():
    product, model, valuation = weighted_inputs()
    axis = dal.Product_Get_RiskOutputs(product)
    assert [(c.id, c.label, c.slot) for c in axis] == [
        ("output:0", "a", 0),
        ("output:1", "b", 1),
        ("payoff", "pay", 2),
    ]
    axis.clear()
    assert len(dal.Product_Get_RiskOutputs(product)) == 3
    with pytest.raises(AttributeError):
        dal.Product_Get_RiskOutputs(product)[0].id = "changed"
    with pytest.raises(RuntimeError, match="product"):
        dal.Product_Get_RiskOutputs(None)
    result = dal.MonteCarlo_ValueWithWeightedRisk(
        product, model, 17, valuation=valuation
    )
    assert result.weighted_value == 5
    assert result.component_means == [5]
    assert result.weights == [1]
    assert result.output_axis[0].id == "payoff"
    assert result.jacobian.Rows() == 1
    assert result.jacobian.Cols() == 6
    assert len(result.complete_input_axis) == 6
    assert not hasattr(result, "legacy_values")
    assert not hasattr(result, "output_ids")


def test_request_and_result_getters_are_owning_copies():
    product, model, valuation = weighted_inputs()
    weights = [2, -1, 0.5]
    outputs = ["output:0", "output:1", "payoff"]
    factors = [0.5, 2]
    request = dal.WeightedRiskRequest_(
        outputs=outputs,
        weights=weights,
        inputs=["constant:0", "constant:1"],
        report_factors=factors,
    )
    weights.clear()
    outputs.clear()
    factors.clear()
    simulation = dal.MonteCarloSettings_(enable_aad=True)
    result = dal.MonteCarlo_ValueWithWeightedRisk(
        product, model, 257, request=request, valuation=valuation, simulation=simulation
    )
    simulation.enable_aad = False
    request.weights.clear()
    request.outputs.clear()
    request.report_factors.clear()
    result.weights.clear()
    result.component_means.clear()
    result.output_axis.clear()
    result.input_axis.clear()
    result.complete_input_axis.clear()
    raw = result.jacobian
    raw[0, 0] = -999
    reported = result.reported_jacobian
    reported[0, 0] = -999
    with pytest.raises(AttributeError):
        result.output_axis[0].slot = 99
    with pytest.raises(AttributeError):
        result.weights = [99]
    assert request.weights == [2, -1, 0.5]
    assert result.weights == [2, -1, 0.5]
    assert result.component_means == [6, 5, 5]
    assert result.jacobian[0, 0] == pytest.approx(5, rel=1e-10, abs=1e-10)
    assert result.reported_jacobian[0, 0] == pytest.approx(2.5, rel=1e-10, abs=1e-10)
    assert result.provenance.execution.simulation.enable_aad is True
    copied = copy.deepcopy(result)
    del product, model, request, result
    gc.collect()
    assert copied.weighted_value == 9.5
    assert copied.component_means == [6, 5, 5]
    assert copied.jacobian.to_rows()[0] == pytest.approx([5, 3], rel=1e-10, abs=1e-10)


class NumericEnum(enum.IntEnum):
    ONE = 1


@pytest.mark.parametrize(
    "kwargs",
    [
        {"inputs": "model:0"},
        {"inputs": [1]},
        {"outputs": [True]},
        {"weights": "1"},
        {"weights": [True]},
        {"weights": [NumericEnum.ONE]},
        {"weights": ["1"]},
        {"weights": [[1]]},
        {"weights": [object()]},
        {"report_factors": [True]},
        {"numeric_payload_budget_bytes": True},
        {"numeric_payload_budget_bytes": 1.5},
    ],
)
def test_request_rejects_implicit_types_and_wrong_weight_rank(kwargs):
    with pytest.raises(TypeError):
        dal.WeightedRiskRequest_(**kwargs)


def test_keyword_only_arguments_and_integer_overflow():
    product, model, valuation = weighted_inputs()
    with pytest.raises(TypeError):
        dal.WeightedRiskRequest_(["model:0"])
    with pytest.raises(TypeError):
        dal.MonteCarlo_ValueWithWeightedRisk(
            product, model, 1, dal.WeightedRiskRequest_()
        )
    for value in (-1, 2**100):
        with pytest.raises(RuntimeError, match="numeric_payload_budget_bytes"):
            dal.WeightedRiskRequest_(numeric_payload_budget_bytes=value)
    with pytest.raises(RuntimeError, match="weights.*representable"):
        dal.WeightedRiskRequest_(weights=[2**5000])
    with pytest.raises(TypeError, match="WeightedRiskRequest_"):
        dal.MonteCarlo_ValueWithWeightedRisk(
            product, model, 1, request=dal.RiskRequest_(), valuation=valuation
        )


@pytest.mark.parametrize(
    "kwargs, message",
    [
        ({"outputs": []}, "nonempty"),
        ({"outputs": ["unknown"]}, "unknown scalar output"),
        ({"outputs": ["payoff", "PAYOFF"]}, "repeated output"),
        ({"outputs": ["vector:0"]}, "unknown scalar output"),
        ({"weights": []}, "weight count"),
        ({"weights": [1, 2]}, "weight count"),
        ({"weights": [float("nan")]}, "weight must be finite"),
        ({"weights": [float("inf")]}, "weight must be finite"),
        ({"inputs": ["weight:0"]}, "unknown input"),
        ({"inputs": ["model:0", "MODEL:0"]}, "repeated input"),
        ({"inputs": ["model:0"], "report_factors": [0]}, "report factor"),
        ({"numeric_payload_budget_bytes": 0}, "RiskResultBudgetExceeded"),
    ],
)
def test_native_preflight_rejects_before_missing_history(kwargs, message):
    product = dal.Product_New(
        [dal.Date_(2025, 1, 1), dal.Date_(2027, 1, 1)],
        ["a = FIX(EQ[WEIGHTED_PY_MISSING])", "pay PAYS a"],
    )
    _, model, valuation = weighted_inputs()
    with pytest.raises(RuntimeError, match=message):
        dal.MonteCarlo_ValueWithWeightedRisk(
            product,
            model,
            17,
            request=dal.WeightedRiskRequest_(**kwargs),
            valuation=valuation,
        )


@pytest.mark.parametrize("compiled", [False, True])
def test_native_empty_and_explicit_price_only_keep_two_dimensional_shape(compiled):
    product = dal.Product_New(
        [dal.Date_(2026, 1, 1)], ["IF SPOT() > 100 THEN pay PAYS 1 ELSE pay PAYS 0 END"]
    )
    model = dal.BSModelData_New(spot=100, vol=0, rate=0, div=0)
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 1, 1))
    request = dal.WeightedRiskRequest_(inputs=[], numeric_payload_budget_bytes=24)
    native = dal.MonteCarlo_ValueWithWeightedRisk(
        product,
        model,
        17,
        request=request,
        valuation=valuation,
        simulation=dal.MonteCarloSettings_(enable_aad=True, compiled=compiled),
    )
    passive = dal.MonteCarlo_ValueWithWeightedRisk(
        product,
        model,
        17,
        valuation=valuation,
        simulation=dal.MonteCarloSettings_(enable_aad=False, compiled=compiled),
    )
    assert native.weighted_value == pytest.approx(0.5, rel=1e-10, abs=1e-10)
    assert passive.weighted_value == 0
    assert native.provenance.method == "NativeAAD"
    assert passive.provenance.method == "PriceOnly"
    for result in (native, passive):
        assert result.jacobian.Rows() == 1
        assert result.jacobian.Cols() == 0
        assert result.jacobian.to_rows() == [[]]
        assert result.reported_jacobian.to_rows() == [[]]
    with pytest.raises(RuntimeError, match="price-only"):
        dal.MonteCarlo_ValueWithWeightedRisk(
            product,
            model,
            17,
            request=dal.WeightedRiskRequest_(inputs=["model:0"]),
            valuation=valuation,
            simulation=dal.MonteCarloSettings_(enable_aad=False),
        )


@pytest.mark.parametrize("compiled", [False, True])
def test_aliases_negative_zero_weights_permutations_and_exact_budget(compiled):
    product = dal.Product_New(
        ["X", "Y", dal.Date_(2025, 1, 1), dal.Date_(2027, 1, 1)],
        ["2", "3", "a = X * Y b = a direct = X literal = 5", "pay PAYS 0"],
    )
    _, model, valuation = weighted_inputs()
    simulation = dal.MonteCarloSettings_(enable_aad=True, compiled=compiled)
    for weights, value, gradient in [
        ([2, 3, 0, 0], 30, [10, 15]),
        ([-1, -1, 0, 0], -12, [-4, -6]),
        ([0, 0, 4, -2], -2, [0, 4]),
        ([0, 0, 0, 0], 0, [0, 0]),
    ]:
        request = dal.WeightedRiskRequest_(
            outputs=["output:0", "output:1", "output:2", "output:3"],
            weights=weights,
            inputs=["constant:1", "constant:0"],
            numeric_payload_budget_bytes=88,
        )
        result = dal.MonteCarlo_ValueWithWeightedRisk(
            product,
            model,
            257,
            request=request,
            valuation=valuation,
            simulation=simulation,
        )
        assert result.weighted_value == value
        assert result.component_means == [6, 6, 2, 5]
        assert result.jacobian.to_rows()[0] == pytest.approx(
            gradient, rel=1e-10, abs=1e-10
        )
    too_short = dal.WeightedRiskRequest_(
        outputs=["output:0", "output:1", "output:2", "output:3"],
        inputs=["constant:1", "constant:0"],
        numeric_payload_budget_bytes=87,
    )
    with pytest.raises(RuntimeError, match="RiskResultBudgetExceeded"):
        dal.MonteCarlo_ValueWithWeightedRisk(
            product, model, 257, request=too_short, valuation=valuation
        )


@pytest.mark.parametrize("compiled", [False, True])
def test_frozen_common_path_spot_finite_difference(compiled):
    product = dal.Product_New(
        [dal.Date_(2027, 1, 1)], ["a = SPOT() b = a * a / 100 pay PAYS a + b"]
    )
    _, model, valuation = weighted_inputs()
    selection = dict(outputs=["output:0", "output:1", "payoff"], weights=[2, -1, 0.5])
    native = dal.MonteCarlo_ValueWithWeightedRisk(
        product,
        model,
        4096,
        request=dal.WeightedRiskRequest_(inputs=["model:0"], **selection),
        valuation=valuation,
        simulation=dal.MonteCarloSettings_(enable_aad=True, compiled=compiled),
    )
    prices = []
    for spot in (99.9, 100.1):
        bumped = dal.BSModelData_New(spot=spot, vol=0.2, rate=0, div=0)
        prices.append(
            dal.MonteCarlo_ValueWithWeightedRisk(
                product,
                bumped,
                4096,
                request=dal.WeightedRiskRequest_(inputs=[], **selection),
                valuation=valuation,
                simulation=dal.MonteCarloSettings_(enable_aad=False, compiled=compiled),
            ).weighted_value
        )
    finite_difference = (prices[1] - prices[0]) / 0.2
    assert native.jacobian[0, 0] == pytest.approx(
        finite_difference, rel=1e-10, abs=1e-10
    )


@pytest.mark.parametrize("compiled", [False, True])
def test_zero_weight_nonfinite_component_raises_and_next_valuation_recovers(compiled):
    _, model, valuation = weighted_inputs()
    request = dal.WeightedRiskRequest_(outputs=["output:0", "payoff"], weights=[0, 1])
    for aad in (False, True):
        product = dal.Product_New([dal.Date_(2027, 1, 1)], ["a = EXP(1000) pay PAYS 1"])
        simulation = dal.MonteCarloSettings_(enable_aad=aad, compiled=compiled)
        with pytest.raises(RuntimeError, match="finite"):
            dal.MonteCarlo_ValueWithWeightedRisk(
                product,
                model,
                257,
                request=request,
                valuation=valuation,
                simulation=simulation,
            )
        product = dal.Product_New([dal.Date_(2027, 1, 1)], ["a = 100 pay PAYS 1"])
        recovered = dal.MonteCarlo_ValueWithWeightedRisk(
            product,
            model,
            257,
            request=request,
            valuation=valuation,
            simulation=simulation,
        )
        assert recovered.weighted_value == 1
        assert recovered.component_means == [100, 1]


def test_native_weighted_valuation_releases_gil_during_real_work():
    from test_quote_risk import _run_with_quote_risk_gil_heartbeat

    product = dal.Product_New([dal.Date_(2027, 1, 1)], ["a = SPOT() pay PAYS 2 * a"])
    _, model, valuation = weighted_inputs()
    request = dal.WeightedRiskRequest_(
        outputs=["output:0", "payoff"], weights=[2, -0.5]
    )
    result = _run_with_quote_risk_gil_heartbeat(
        lambda: dal.MonteCarlo_ValueWithWeightedRisk(
            product, model, 65536, request=request, valuation=valuation
        ),
        barrier=lambda _: None,
    )
    assert result.weighted_value == pytest.approx(
        2 * result.component_means[0] - 0.5 * result.component_means[1],
        rel=1e-10,
        abs=1e-10,
    )
