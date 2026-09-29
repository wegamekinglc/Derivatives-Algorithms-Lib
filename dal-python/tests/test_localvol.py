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
