import copy
import math

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


@pytest.mark.parametrize("compiled", [False, True])
@pytest.mark.parametrize("aad", [False, True])
def test_two_state_multi_asset_exercise(compiled, aad):
    dal.EvaluationDate_Set(dal.Date_(2026, 9, 27))
    settings = dal.ScriptProductSettings_(regression_features=["EQ[A]", "EQ[B]"])
    assert settings.regression_features == ["EQ[A]", "EQ[B]"]
    assert copy.deepcopy(settings).regression_features == ["EQ[A]", "EQ[B]"]
    product = dal.Product_New(
        [dal.Date_(2027, 3, 27), dal.Date_(2027, 9, 27)],
        ["EXERCISE MAX(FIX(EQ[A]) - FIX(EQ[B]), 0)", "EXERCISE MAX(FIX(EQ[B]) - FIX(EQ[A]), 0)"],
        settings=settings,
    )
    assert dal.Product_Describe(product)["regression_features"] == ["EQ[A]", "EQ[B]"]
    simulation = dal.MonteCarloSettings_(compiled=compiled, enable_aad=aad, lsmc_training_paths=128)
    for model in models():
        result = dal.MonteCarlo_ValueWithSettings(product, model, 128, simulation=simulation)
        assert result["PV"] == pytest.approx(20.0)


def test_regression_features_reject_non_text_elements():
    with pytest.raises((TypeError, ValueError)):
        dal.ScriptProductSettings_(regression_features=["EQ[A]", 3])


@pytest.mark.parametrize("compiled", [False, True])
def test_logdf_rate_factory_prices_and_differentiates(compiled):
    dal.EvaluationDate_Set(dal.Date_(2026, 9, 27))
    correlation = dal.DoubleMatrix_([[1.0]])
    components = [
        dal.HybridBSEquityData_New("A", "EQ[A]", "USD", "FA", 100.0, 0.0, 0.0),
        dal.HybridLogDfRateData_New("RATE", "USD", [0.0, 1.0], [0.0, -0.08]),
    ]
    provider = dal.HybridConstantCorrelationData_New("corr", ["FA"], correlation)
    model = dal.HybridModelData_New("hybrid_curve", "USD", components, provider)
    product = dal.Product_New([dal.Date_(2027, 9, 27)], ["pay PAYS FIX(EQ[A]) + 25"])
    simulation = dal.MonteCarloSettings_(compiled=compiled, enable_aad=True)
    result = dal.MonteCarlo_ValueWithSettings(product, model, 16, simulation=simulation)
    assert result["PV"] == pytest.approx(100.0 + 25.0 * math.exp(-0.08), abs=1e-10)
    assert result["d_logdf:USD:1"] == pytest.approx(25.0 * math.exp(-0.08), abs=1e-10)


def test_logdf_rate_factory_rejects_bad_grid():
    with pytest.raises(RuntimeError, match="InvalidHybridCurve"):
        dal.HybridLogDfRateData_New("RATE", "USD", [0.0, 1.0, 1.0], [0.0, -0.02, -0.03])


def test_logdf_rate_snapshot_uses_curve_dates_and_currency():
    today = dal.Date_(2026, 9, 27)
    first = dal.Date_(2027, 9, 27)
    second = dal.Date_(2028, 9, 27)
    curve = dal.DiscountLogDF_New("calibrated", "USD", [today, first, second], [0.0, -0.03, -0.08])
    rate = dal.HybridLogDfRateDataFromCurve_New("RATE", curve, today, [today, first, second])
    equity = dal.HybridBSEquityData_New("A", "EQ[A]", "USD", "FA", 100.0, 0.0, 0.0)
    provider = dal.HybridConstantCorrelationData_New("corr", ["FA"], dal.DoubleMatrix_([[1.0]]))
    model = dal.HybridModelData_New("hybrid_curve", "USD", [equity, rate], provider)
    dal.EvaluationDate_Set(today)
    product = dal.Product_New([second], ["pay PAYS 25"])
    result = dal.MonteCarlo_ValueWithSettings(product, model, 16)
    assert result["PV"] == pytest.approx(25.0 * math.exp(-0.08), abs=1e-10)
