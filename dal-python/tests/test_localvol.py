"""Local-volatility surface storage and hybrid model composition."""

import math

import dal
import pytest


def flat_surface(vol=0.0):
    return dal.LocalVolSurfaceData_New(
        "equity_vol",
        [80.0, 120.0],
        [0.0, 1.0],
        dal.DoubleMatrix_([[vol, vol], [vol, vol]]),
    )


def test_surface_json_round_trip_and_bs_composition():
    surface = flat_surface()
    payload = dal._dal._StorableToJson(surface)
    restored = dal._dal._StorableFromJson(payload)
    assert isinstance(restored, dal.LocalVolSurfaceData_)  # nosec B101
    assert dal._dal._StorableToJson(restored) == payload  # nosec B101

    bs = dal.BSModelData_New(100.0, 0.20, 0.05, 0.01)
    model = dal.BSLocalVolModelData_New(
        "local_vol", "EQ[A]", "USD", "W_EQ", bs, restored
    )
    today = dal.Date_(2026, 9, 28)
    dal.EvaluationDate_Set(today)
    product = dal.Product_New([today.AddDays(365)], ["pay PAYS FIX(EQ[A])"])
    result = dal.MonteCarlo_ValueWithSettings(product, model, 16)
    assert result["PV"] == pytest.approx(100.0 * math.exp(-0.01), abs=1e-10)


def test_gsr_and_local_vol_price_equity_and_rate_on_same_path():
    today = dal.Date_(2026, 9, 28)
    expiry = dal.Date_(2027, 9, 28)
    maturity = dal.Date_(2028, 9, 28)
    dal.EvaluationDate_Set(today)
    curve = dal.GSRCurveData_New(
        "curve",
        today,
        "USD",
        [today, expiry, maturity],
        [0.0, -0.03, -0.06],
        [],
        dal.DoubleMatrix_(0, 0),
    )
    rate_vol = dal.GSRVolData_New("rate_vol", [today], [0.02], [today], [1.0])
    components = [
        dal.HybridLocalVolEquityData_New(
            "equity", "EQ[A]", "USD", "W_EQ", 100.0, 0.0, flat_surface()
        ),
        dal.HybridGSRRateData_New("rate", "W_RATE", curve, rate_vol),
    ]
    correlation = dal.HybridConstantCorrelationData_New(
        "corr", ["W_EQ", "W_RATE"], dal.DoubleMatrix_([[1.0, 0.0], [0.0, 1.0]])
    )
    model = dal.HybridModelData_New("gsr_local_vol", "USD", components, correlation)
    product = dal.Product_New(
        [expiry], ["pay PAYS FIX(EQ[A]) + FIX(IR[USD,DF,2028-09-28])"]
    )
    result = dal.MonteCarlo_ValueWithSettings(product, model, 64)
    assert result["PV"] == pytest.approx(100.0 + math.exp(-0.06), abs=0.01)


def test_multi_factor_gsr_rate_component_prices_equity_and_rate():
    today = dal.Date_(2026, 10, 2)
    expiry = today.AddDays(365)
    horizon = today.AddDays(1095)
    dal.EvaluationDate_Set(today)
    curve = dal.GSRCurveData_New(
        "curve",
        today,
        "USD",
        [today, today.AddDays(365), today.AddDays(730), horizon],
        [0.0, -0.03, -0.06, -0.09],
        [],
        dal.DoubleMatrix_(0, 0),
    )
    multi_vol = dal.MultiFactorGSRVolData_New(
        "multi_vol",
        ["level", "slope"],
        [today],
        dal.DoubleMatrix_([[0.02], [0.01]]),
        [today],
        dal.DoubleMatrix_([[1.0], [0.4]]),
        dal.DoubleMatrix_([[1.0, 0.3], [0.3, 1.0]]),
    )
    components = [
        dal.HybridBSEquityData_New("equity", "EQ[A]", "USD", "A_EQ", 100.0, 0.0, 0.0),
        dal.HybridGSRRateDataMulti_New("rate", ["B_LEVEL", "C_SLOPE"], curve, multi_vol),
    ]
    correlation = dal.HybridConstantCorrelationData_New(
        "corr",
        ["A_EQ", "B_LEVEL", "C_SLOPE"],
        dal.DoubleMatrix_([[1.0, 0.0, 0.0], [0.0, 1.0, 0.3], [0.0, 0.3, 1.0]]),
    )
    model = dal.HybridModelData_New("multi_gsr", "USD", components, correlation)
    product = dal.Product_New(
        [expiry], ["pay PAYS FIX(EQ[A]) * FIX(IR[USD,DF,%s])" % str(horizon)]
    )
    result = dal.MonteCarlo_ValueWithSettings(product, model, 512)
    # EQ(t) is a discounted martingale, so the payoff reduces to the forward bond
    # P(0, 3Y) / P(0, 1Y) on the flat curve.
    expected = 100.0 * math.exp(-0.06)
    assert result["PV"] == pytest.approx(expected, rel=5e-3)


def test_gsr_slv_rate_component_prices_equity_and_rate():
    today = dal.Date_(2026, 10, 2)
    expiry = today.AddDays(365)
    horizon = today.AddDays(1095)
    dal.EvaluationDate_Set(today)
    curve = dal.GSRCurveData_New(
        "curve",
        today,
        "USD",
        [today, today.AddDays(365), today.AddDays(730), horizon],
        [0.0, -0.03, -0.06, -0.09],
        [],
        dal.DoubleMatrix_(0, 0),
    )
    multi_vol = dal.MultiFactorGSRVolData_New(
        "multi_vol",
        ["B_RATE"],
        [today],
        dal.DoubleMatrix_([[0.02]]),
        [today],
        dal.DoubleMatrix_([[1.0]]),
        dal.DoubleMatrix_([[1.0]]),
    )
    gaussian = dal.MultiFactorGSRModelData_New("gaussian", curve, multi_vol)
    leverage = dal.GSRLeverageData_New("leverage", [0.0], [0.0], dal.DoubleMatrix_([[1.0]]))
    settings = dal.GSRSLVSettings_()
    settings.kappa = 1.0
    settings.vol_of_vol = 0.0
    settings.variance_correlations = [0.3]
    settings.max_step = 0.5
    slv = dal.GSRSLVModelData_New("smile", gaussian, leverage, settings)
    components = [
        dal.HybridBSEquityData_New("equity", "EQ[A]", "USD", "A_EQ", 100.0, 0.0, 0.0),
        dal.HybridGSRSLVRateData_New("rate", "C_VOL", "D_BRIDGE", slv),
    ]
    correlation = dal.HybridConstantCorrelationData_New(
        "corr",
        ["A_EQ", "B_RATE", "C_VOL", "D_BRIDGE"],
        dal.DoubleMatrix_([[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.3, 0.0], [0.0, 0.3, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]]),
    )
    model = dal.HybridModelData_New("slv_hybrid", "USD", components, correlation)
    product = dal.Product_New(
        [expiry], ["pay PAYS FIX(EQ[A]) * FIX(IR[USD,DF,%s])" % str(horizon)]
    )
    result = dal.MonteCarlo_ValueWithSettings(product, model, 512)
    # With zero vol-of-vol the SLV kernel reduces exactly to its Gaussian core.
    expected = 100.0 * math.exp(-0.06)
    assert result["PV"] == pytest.approx(expected, rel=1e-2)


def test_assembled_correlation_matches_hand_built_matrix():
    today = dal.Date_(2026, 10, 2)
    expiry = today.AddDays(365)
    horizon = today.AddDays(1095)
    dal.EvaluationDate_Set(today)
    curve = dal.GSRCurveData_New(
        "curve", today, "USD",
        [today, today.AddDays(365), today.AddDays(730), horizon],
        [0.0, -0.03, -0.06, -0.09], [], dal.DoubleMatrix_(0, 0),
    )
    rate_vol = dal.GSRVolData_New("rate_vol", [today], [0.02], [today], [1.0])
    product = dal.Product_New(
        [expiry], ["pay PAYS FIX(EQ[A]) * FIX(IR[USD,DF,%s])" % str(horizon)]
    )
    components = [
        dal.HybridBSEquityData_New("equity", "EQ[A]", "USD", "W_EQ", 100.0, 0.0, 0.0),
        dal.HybridGSRRateData_New("rate", "W_RATE", curve, rate_vol),
    ]
    hand = dal.HybridConstantCorrelationData_New(
        "corr", ["W_EQ", "W_RATE"], dal.DoubleMatrix_([[1.0, 0.25], [0.25, 1.0]])
    )
    assembled = dal.HybridCorrelation_Assemble(
        "corr", components, [dal.HybridFactorLink_("W_EQ", "W_RATE", 0.25)]
    )
    no_links = dal.HybridCorrelation_Assemble("corr", components)
    expected = 100.0 * math.exp(-0.06)
    by_hand = dal.MonteCarlo_ValueWithSettings(product, dal.HybridModelData_New("m1", "USD", components, hand), 512)
    by_link = dal.MonteCarlo_ValueWithSettings(product, dal.HybridModelData_New("m2", "USD", components, assembled), 512)
    by_none = dal.MonteCarlo_ValueWithSettings(product, dal.HybridModelData_New("m3", "USD", components, no_links), 512)
    assert by_link["PV"] == pytest.approx(by_hand["PV"], abs=1e-10)  # nosec B101
    assert by_hand["PV"] == pytest.approx(expected, rel=5e-3)  # nosec B101
    assert by_none["PV"] == pytest.approx(expected, rel=5e-3)  # nosec B101
