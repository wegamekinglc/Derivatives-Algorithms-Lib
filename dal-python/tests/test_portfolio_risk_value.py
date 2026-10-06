"""Sealed portfolio ownership, strict inputs and detached risk results."""

import copy
import enum
import gc

import dal
import pytest


def portfolio_inputs(distinct=False):
    products = [
        dal.Product_New(
            ["X", dal.Date_(2027, 1, 1)], [str(x), f"a = {s} * SPOT() + X pay PAYS a"]
        )
        for x, s in [(5, 2), (7, 3)]
    ]
    model = dal.BSModelData_New(1, 0, 0, 0)
    models = [model, dal.BSModelData_New(1, 0, 0, 0) if distinct else model]
    portfolio = dal.ScriptPortfolio_New(["A", "B"], products, models)
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 1, 1))
    return portfolio, products, models, valuation


@pytest.mark.parametrize("compiled", [False, True])
def test_weighted_shared_owner_private_columns_and_reported_risk(compiled):
    portfolio, _, _, valuation = portfolio_inputs()
    request = dal.PortfolioWeightedRiskRequest_(
        outputs=["trade:1:payoff", "trade:0:payoff"],
        weights=[-1, 2],
        inputs=["trade:1:constant:0", "model:0:parameter:0", "trade:0:constant:0"],
        report_factors=[0.5, 2, 3],
        numeric_payload_budget_bytes=64,
        scratch_capacity_budget_bytes=64 * 1024 * 1024,
        recording_capacity_budget_bytes=256 * 1024 * 1024,
    )
    result = dal.PortfolioMonteCarlo_ValueWithWeightedRisk(
        portfolio,
        17,
        request=request,
        valuation=valuation,
        simulation=dal.MonteCarloSettings_(enable_aad=True, compiled=compiled),
    )
    assert result.weighted_value == 4
    assert result.component_means == [10, 7]
    assert result.jacobian.to_rows() == [[-1, 1, 2]]
    assert result.reported_jacobian.to_rows() == [[-0.5, 2, 6]]
    assert result.provenance.trade_ids == ["A", "B"]
    assert result.provenance.model_owners == [0, 0]
    assert len(result.complete_input_axis) == 6
    assert result.execution.groups[0].generated_scenarios == 17
    assert result.execution.groups[0].evaluator_calls == 34


@pytest.mark.parametrize("compiled", [False, True])
@pytest.mark.parametrize("native", [False, True])
def test_independent_attribution_rows_aliases_zero_columns_and_work(compiled, native):
    portfolio, _, _, valuation = portfolio_inputs()
    request = dal.PortfolioJacobianRiskRequest_(
        outputs=["trade:1:payoff", "trade:0:output:0", "trade:0:payoff"],
        inputs=(
            ["trade:1:constant:0", "model:0:parameter:0", "trade:0:constant:0"]
            if native
            else []
        ),
        max_block_width=2,
        recording_capacity_budget_bytes=None if native else 0,
    )
    result = dal.PortfolioMonteCarlo_ValueWithJacobianRisk(
        portfolio,
        17,
        request=request,
        valuation=valuation,
        simulation=dal.MonteCarloSettings_(enable_aad=native, compiled=compiled),
    )
    assert result.values == [10, 7, 7]
    assert result.jacobian.Rows() == 3
    assert result.jacobian.Cols() == (3 if native else 0)
    assert result.jacobian.to_rows() == (
        [[1, 3, 0], [0, 2, 1], [0, 2, 1]] if native else [[], [], []]
    )
    group = result.execution.groups[0]
    assert result.execution.requested_max_block_width == 2
    assert group.actual_widths == ([2, 2] if native else [])
    assert group.replay_attempts == (2 if native else 1)
    assert group.generated_scenarios == (34 if native else 17)
    assert group.evaluator_calls == (51 if native else 34)
    assert group.suffix_reversals == (34 if native else 0)
    if not native:
        assert result.execution.peak_recording_bytes == 0


def test_original_owner_identity_and_detached_request_result_lifetimes():
    portfolio, products, models, valuation = portfolio_inputs(distinct=True)
    assert portfolio.model_owners == [0, 1]
    outputs = ["trade:1:payoff"]
    request = dal.PortfolioJacobianRiskRequest_(outputs=outputs)
    outputs.clear()
    result = dal.PortfolioMonteCarlo_ValueWithJacobianRisk(
        portfolio, 17, request=request, valuation=valuation
    )
    del portfolio, products, models
    gc.collect()
    result.values.clear()
    result.jacobian[0, 4] = -999
    result.output_axis.clear()
    result.provenance.trade_ids.clear()
    result.execution.groups.clear()
    request.outputs.clear()
    duplicate = copy.deepcopy(result)
    assert duplicate.values == [10]
    assert duplicate.jacobian[0, 4] == 3
    assert duplicate.provenance.model_owners == [0, 1]
    assert len(duplicate.complete_input_axis) == 10
    assert request.outputs == ["trade:1:payoff"]
    with pytest.raises(AttributeError):
        request.max_block_width = 3
    with pytest.raises(AttributeError):
        result.values = []


@pytest.mark.parametrize("compiled", [False, True])
@pytest.mark.parametrize("native", [False, True])
def test_nonzero_volatility_original_meshes_match_independent_scalar_calls(
    compiled, native
):
    model = dal.BSModelData_New(100, 0.23, 0.02, 0.01)
    products = [
        dal.Product_New(["X", date], [str(x), "pay PAYS MAX(SPOT() - X, 0)"])
        for date, x in [(dal.Date_(2027, 1, 1), 95), (dal.Date_(2027, 6, 3), 110)]
    ]
    portfolio = dal.ScriptPortfolio_New(["A", "B"], products, [model, model])
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 1, 1))
    simulation = dal.MonteCarloSettings_(
        enable_aad=native, compiled=compiled, method="mrg32", use_bb=True
    )
    rows = dal.PortfolioMonteCarlo_ValueWithJacobianRisk(
        portfolio,
        257,
        valuation=valuation,
        simulation=simulation,
        request=dal.PortfolioJacobianRiskRequest_(max_block_width=3),
    )
    weighted = dal.PortfolioMonteCarlo_ValueWithWeightedRisk(
        portfolio,
        257,
        valuation=valuation,
        simulation=simulation,
        request=dal.PortfolioWeightedRiskRequest_(weights=[2, -1]),
    )
    references = [
        dal.MonteCarlo_ValueWithRisk(
            p, model, 257, valuation=valuation, simulation=simulation
        )
        for p in products
    ]
    assert rows.values == pytest.approx(
        [r.values[0] for r in references], abs=1e-10, rel=0
    )
    assert weighted.weighted_value == pytest.approx(
        2 * references[0].values[0] - references[1].values[0], abs=1e-10, rel=0
    )
    assert len(rows.execution.groups) == 2
    matrix = rows.jacobian.to_rows()
    for row, reference in enumerate(references):
        expected = [0.0] * (6 if native else 0)
        if native:
            expected[:4] = reference.jacobian.to_rows()[0][:4]
            expected[4 + row] = reference.jacobian[0, 4]
        assert matrix[row] == pytest.approx(expected, abs=1e-10, rel=0)
    for column in range(rows.jacobian.Cols()):
        assert weighted.jacobian[0, column] == pytest.approx(
            2 * matrix[0][column] - matrix[1][column], abs=1e-10, rel=0
        )


@pytest.mark.parametrize(
    "field,value",
    [
        ("trade_ids", "A"),
        ("trade_ids", [1]),
        ("products", [None]),
        ("modelData", [None]),
    ],
)
def test_constructor_rejects_lossy_or_null_inputs_with_trade_context(field, value):
    _, products, models, _ = portfolio_inputs()
    args = {"trade_ids": ["A"], "products": products[:1], "modelData": models[:1]}
    args[field] = value
    with pytest.raises((TypeError, RuntimeError), match=field):
        dal.ScriptPortfolio_New(**args)


@pytest.mark.parametrize(
    "ids,product_count,model_count", [([], 0, 0), (["A"], 2, 1), (["A", "a"], 2, 2)]
)
def test_constructor_rejects_empty_mismatched_and_duplicate_ids(
    ids, product_count, model_count
):
    _, products, models, _ = portfolio_inputs()
    with pytest.raises(RuntimeError, match="InvalidScriptPortfolio"):
        dal.ScriptPortfolio_New(ids, products[:product_count], models[:model_count])


class InvalidInteger(enum.IntEnum):
    ONE = 1


@pytest.mark.parametrize(
    "request_type", ["PortfolioWeightedRiskRequest_", "PortfolioJacobianRiskRequest_"]
)
@pytest.mark.parametrize(
    "field,value",
    [
        ("inputs", "x"),
        ("outputs", [1]),
        ("report_factors", [True]),
        ("numeric_payload_budget_bytes", True),
        ("scratch_capacity_budget_bytes", -1),
        ("recording_capacity_budget_bytes", InvalidInteger.ONE),
    ],
)
def test_requests_reject_lossy_types(request_type, field, value):
    with pytest.raises((TypeError, RuntimeError), match=field):
        getattr(dal, request_type)(**{field: value})


@pytest.mark.parametrize("width", [0, -1, 1.5, True, InvalidInteger.ONE, 2**64])
def test_width_is_strict_and_bounded(width):
    with pytest.raises((TypeError, RuntimeError), match="max_block_width"):
        dal.PortfolioJacobianRiskRequest_(max_block_width=width)


@pytest.mark.parametrize(
    "function",
    [
        "PortfolioMonteCarlo_ValueWithWeightedRisk",
        "PortfolioMonteCarlo_ValueWithJacobianRisk",
    ],
)
@pytest.mark.parametrize("paths", [True, InvalidInteger.ONE, 0, -1, 1.5, 2**31])
def test_paths_are_strict(function, paths):
    portfolio, _, _, valuation = portfolio_inputs()
    with pytest.raises((TypeError, RuntimeError), match="InvalidPathCount"):
        getattr(dal, function)(portfolio, paths, valuation=valuation)


def test_invalid_request_and_capacity_failures_preserve_valid_results():
    portfolio, _, _, valuation = portfolio_inputs()
    function = dal.PortfolioMonteCarlo_ValueWithJacobianRisk
    prior = function(portfolio, 17, valuation=valuation)
    for request in [
        dal.PortfolioJacobianRiskRequest_(outputs=[]),
        dal.PortfolioJacobianRiskRequest_(inputs=["bad"]),
        dal.PortfolioJacobianRiskRequest_(scratch_capacity_budget_bytes=0),
        dal.PortfolioJacobianRiskRequest_(recording_capacity_budget_bytes=0),
    ]:
        with pytest.raises(RuntimeError):
            function(portfolio, 17, request=request, valuation=valuation)
    with pytest.raises(TypeError, match="request"):
        function(portfolio, 17, request={}, valuation=valuation)
    recovered = function(portfolio, 17, valuation=valuation)
    assert prior.values == recovered.values
    assert prior.jacobian.to_rows() == recovered.jacobian.to_rows()
