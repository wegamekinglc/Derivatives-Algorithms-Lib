"""Tests for the fixed-plan native Dupire quote Greeks."""

import dal
import pytest


# Rounded from the independent 48-digit fixed-grid benchmark reference.
PRICE = 7.2746480967500065
VEGA = [
    2.809331379657665, 7.654936904806369, -9.319401452963237,
    27.923020206476564, 4.161117546822841, 6.253126918045238,
]
PRODUCTS = [
    [27.28583927224782, 4.798956487024745, 49.77890609287156, -153.37815956069173, 20.623413652396345, 8.865542037076209],
    [-21.01905146494504, -14.584443562660714, -5.42220381477761, 49.77890609287156, -26.803113828978165, -11.976265652365085],
    [31.81812466586441, 12.65575739513499, 44.91394752019262, -165.10134966218048, 31.16537485817902, 12.90058276807370],
]
AXIS = [
    ("quote:0", 75.0, 0.4),
    ("quote:1", 75.0, 1.2),
    ("quote:2", 105.0, 0.4),
    ("quote:3", 105.0, 1.2),
    ("quote:4", 135.0, 0.4),
    ("quote:5", 135.0, 1.2),
]
SPREADS = [0.008, 0.012, 0.0, 0.004, 0.006, 0.009]
DIRECTIONS = [[0, 0, 0, 1, 0, 0], [0, 0, 1, 0, 0, 0], [0.5, -0.25, -0.5, 1.0, 0.25, -0.5]]
POINTWISE = "NativePointwiseSecondOrderAAD"
SHARED = "NativeSharedPrimalSecondOrderAAD"


def make_plan():
    base = dal.BSModelData_New(100.0, 0.20, 0.05, 0.02)
    inputs = dal.DupireRiskInputs_(
        quote_strikes=[75.0, 105.0, 135.0],
        quote_maturities=[0.4, 1.2],
        quote_spreads=dal.DoubleMatrix_([SPREADS[:2], SPREADS[2:4], SPREADS[4:]]),
        inclusion_spots=list(range(60, 141, 10)),
        max_spot_spacing=10.0,
        inclusion_times=[0.5, 1.0],
        max_time_spacing=0.5,
    )
    return dal.DupireQuoteSecondOrderPlan_New(
        dal.DupireCalibration_New(base, inputs),
        strike=105.0, maturity=1.0, underlying_id="EQ", currency="USD", grid=(0.8, 81, 64),
    )


@pytest.fixture
def plan():
    return make_plan()


def assert_common(result):
    assert result.value == pytest.approx(PRICE, rel=0, abs=2e-7)
    assert list(result.gradient) == pytest.approx(VEGA, rel=0, abs=2e-6)
    assert result.input_axis == AXIS
    assert list(result.point) == SPREADS
    assert result.quote_unit == "decimal-vol"


def test_single_and_shared_match_frozen_reference(plan):
    assert plan.input_axis == AXIS
    results = []
    for rows, method in ((1, POINTWISE), (3, SHARED)):
        seeds = DIRECTIONS[:rows]
        result = dal.DupireQuoteSecondOrderResult_New(plan, input_axis=AXIS, directions=dal.DoubleMatrix_(seeds))
        assert_common(result)
        assert result.method == method
        assert result.directions.to_rows() == seeds
        assert (result.hessian_products.Rows(), result.hessian_products.Cols()) == (rows, 6)
        for expected, actual in zip(PRODUCTS, result.hessian_products.to_rows()):
            assert actual == pytest.approx(expected, rel=0, abs=1e-5)
        expected_slopes = [sum(g * v for g, v in zip(VEGA, seed)) for seed in seeds]
        assert list(result.directional_derivatives) == pytest.approx(expected_slopes, rel=0, abs=2e-6)
        results.append(result)
    single, shared = results
    assert single.hessian_products.to_rows()[0] == pytest.approx(shared.hessian_products.to_rows()[0], rel=0, abs=1e-5)


def test_empty_directions_preserve_six_columns_and_payload_budget(plan):
    seeds = dal.DoubleMatrix_(0, 6)
    result = dal.DupireQuoteSecondOrderResult_New(
        plan, input_axis=AXIS, directions=seeds, numeric_payload_budget_bytes=104
    )
    assert_common(result)
    assert result.method == POINTWISE
    assert list(result.directional_derivatives) == []
    for matrix in (result.directions, result.hessian_products):
        assert (matrix.Rows(), matrix.Cols()) == (0, 6)
    with pytest.raises(RuntimeError, match="numeric payload requires 104 bytes with cap 103"):
        dal.DupireQuoteSecondOrderResult_New(plan, input_axis=AXIS, directions=seeds, numeric_payload_budget_bytes=103)
    with pytest.raises(RuntimeError, match=r"recording requires \d+ bytes with cap 0"):
        dal.DupireQuoteSecondOrderResult_New(plan, input_axis=AXIS, directions=seeds, recording_capacity_budget_bytes=0)


def test_wrong_axis_and_direction_columns_are_rejected(plan):
    seeds = dal.DoubleMatrix_(DIRECTIONS)
    for mismatched in ([AXIS[1], AXIS[0], *AXIS[2:]], [("Quote:0", 75.0, 0.4), *AXIS[1:]]):
        with pytest.raises(RuntimeError, match="axis differs at column 0"):
            dal.DupireQuoteSecondOrderResult_New(plan, input_axis=mismatched, directions=seeds)
    with pytest.raises(RuntimeError, match="axis length and direction column count"):
        dal.DupireQuoteSecondOrderResult_New(plan, input_axis=AXIS, directions=dal.DoubleMatrix_(1, 5))


def test_result_owns_inputs_and_returns_copies():
    def evaluate(local_plan):
        axis = local_plan.input_axis
        seeds = dal.DoubleMatrix_(DIRECTIONS)
        result = dal.DupireQuoteSecondOrderResult_New(local_plan, input_axis=axis, directions=seeds)
        axis.clear()
        seeds[0, 3] = 99.0
        assert local_plan.input_axis == AXIS
        return result

    result = evaluate(make_plan())
    result.input_axis.clear()
    direction_copy = result.directions
    direction_copy[0, 3] = 0.0
    hvp_copy = result.hessian_products
    hvp_copy[0, 3] = 0.0
    assert_common(result)
    assert result.directions.to_rows() == DIRECTIONS
    assert result.hessian_products[0, 3] == pytest.approx(PRODUCTS[0][3], rel=0, abs=1e-5)
