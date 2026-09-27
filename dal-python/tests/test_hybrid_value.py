import dal
import pytest


def models():
    correlation = dal.DoubleMatrix_([[1.0, 0.0], [0.0, 1.0]])
    correlated = dal.CorrelatedBSModelData_New(
        ["EQ[A]", "EQ[B]"], [100.0, 120.0], [0.0, 0.0], [0.0, 0.0], 0.0, correlation
    )
    components = [
        dal.HybridBSEquityData_New("A", "EQ[A]", "USD", "FA", 100.0, 0.0, 0.0),
        dal.HybridBSEquityData_New("B", "EQ[B]", "USD", "FB", 120.0, 0.0, 0.0),
        dal.HybridDeterministicRateData_New("RATE", "USD", 0.0),
    ]
    provider = dal.HybridConstantCorrelationData_New("corr", ["FA", "FB"], correlation)
    hybrid = dal.HybridModelData_New("hybrid", "USD", components, provider)
    return correlated, hybrid


@pytest.mark.parametrize("compiled", [False, True])
@pytest.mark.parametrize("aad", [False, True])
def test_named_joint_valuation(compiled, aad):
    dal.EvaluationDate_Set(dal.Date_(2026, 9, 27))
    product = dal.Product_New([dal.Date_(2027, 9, 27)], ["pay PAYS FIX(EQ[B]) + 2 * FIX(EQ[A])"])
    simulation = dal.MonteCarloSettings_(compiled=compiled, enable_aad=aad)
    for model in models():
        result = dal.MonteCarlo_ValueWithSettings(product, model, 16, simulation=simulation)
        assert result["PV"] == pytest.approx(320.0)
        if aad:
            assert result["d_spot:EQ[A]"] == pytest.approx(2.0)
            assert result["d_spot:EQ[B]"] == pytest.approx(1.0)


def test_selected_multi_asset_exercise():
    dal.EvaluationDate_Set(dal.Date_(2026, 9, 27))
    dates = [dal.Date_(2027, 3, 27), dal.Date_(2027, 9, 27)]
    events = ["EXERCISE MAX(150 - FIX(EQ[B]), 0)"] * 2
    ambiguous = dal.Product_New(dates, events)
    selected = dal.Product_New(dates, events, settings=dal.ScriptProductSettings_(default_index="EQ[B]"))
    for model in models():
        with pytest.raises(RuntimeError, match="AmbiguousLsmcRegressor"):
            dal.MonteCarlo_ValueWithSettings(ambiguous, model, 64)
        result = dal.MonteCarlo_ValueWithSettings(selected, model, 64)
        assert result["PV"] == pytest.approx(30.0)
