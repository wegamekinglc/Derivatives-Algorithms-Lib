"""Tests for Black-Scholes and Dupire model data creation."""

import dal
import math
import pytest


# ---- BSModelData -------------------------------------------------------------


def test_bs_model_new():
    """BSModelData_New creates a valid model handle."""
    model = dal.BSModelData_New(spot=100.0, vol=0.2, rate=0.05, div=0.02)
    assert model is not None  # nosec B101 - pytest assertions are intentional


def test_bs_model_zero_vol():
    """BS model with zero vol is accepted (degenerate case)."""
    model = dal.BSModelData_New(spot=100.0, vol=0.0, rate=0.05, div=0.0)
    assert model is not None  # nosec B101 - pytest assertions are intentional


def test_bs_model_zero_rate_and_div():
    """BS model with zero rate and dividend works."""
    model = dal.BSModelData_New(spot=100.0, vol=0.3, rate=0.0, div=0.0)
    assert model is not None  # nosec B101 - pytest assertions are intentional


def test_bs_model_high_vol():
    """BS model with high vol is accepted."""
    model = dal.BSModelData_New(spot=100.0, vol=2.0, rate=0.05, div=0.02)
    assert model is not None  # nosec B101 - pytest assertions are intentional


def test_bs_model_various_spots():
    """BS model works for a range of spot values."""
    for spot in [1.0, 10.0, 100.0, 1000.0, 50000.0]:
        model = dal.BSModelData_New(spot=spot, vol=0.2, rate=0.05, div=0.01)
        assert model is not None  # nosec B101 - pytest assertions are intentional


# ---- DupireModelData ---------------------------------------------------------


def test_dupire_model_new():
    """DupireModelData_New creates a valid model handle."""
    spots = [80.0, 90.0, 100.0, 110.0, 120.0]
    times = [0.5, 1.0, 2.0]
    vols = dal.DoubleMatrix_(len(spots), len(times), 0.2)

    model = dal.DupireModelData_New(
        spot=100.0,
        rate=0.05,
        repo=0.01,
        spots=spots,
        times=times,
        vols=vols,
    )
    assert model is not None  # nosec B101 - pytest assertions are intentional


def test_dupire_model_flat_surface():
    """Dupire with a flat vol surface (constant across strikes and times)."""
    spots = [90.0, 100.0, 110.0]
    times = [0.25, 0.5, 1.0]
    vols = dal.DoubleMatrix_(len(spots), len(times), 0.15)

    model = dal.DupireModelData_New(
        spot=100.0,
        rate=0.03,
        repo=0.0,
        spots=spots,
        times=times,
        vols=vols,
    )
    assert model is not None  # nosec B101 - pytest assertions are intentional


def test_dupire_model_skewed_surface():
    """Dupire accepts a non-flat volatility surface from nested Python rows."""
    spots = [80.0, 90.0, 100.0, 110.0, 120.0]
    times = [0.5, 1.0]
    vols = dal.DoubleMatrix_(
        [
            [0.25, 0.24],
            [0.23, 0.22],
            [0.21, 0.20],
            [0.20, 0.19],
            [0.19, 0.18],
        ]
    )

    model = dal.DupireModelData_New(
        spot=100.0,
        rate=0.05,
        repo=0.01,
        spots=spots,
        times=times,
        vols=vols,
    )
    assert model is not None  # nosec B101 - pytest assertions are intentional


def test_dupire_model_single_spot_single_time():
    """Dupire with minimal surface (1 spot, 1 time)."""
    spots = [100.0]
    times = [1.0]
    vols = dal.DoubleMatrix_(1, 1, 0.2)

    model = dal.DupireModelData_New(
        spot=100.0,
        rate=0.05,
        repo=0.0,
        spots=spots,
        times=times,
        vols=vols,
    )
    assert model is not None  # nosec B101 - pytest assertions are intentional


def test_gsr_curve_vol_and_model_price_zero_vol_bond():
    today = dal.Date_(2026, 9, 28)
    exercise = dal.Date_(2027, 9, 28)
    maturity = dal.Date_(2028, 9, 28)
    curve = dal.GSRCurveData_New(
        "curve", today, "USD", [today, exercise, maturity], [0.0, -0.03, -0.06], [], dal.DoubleMatrix_(0, 0)
    )
    vol = dal.GSRVolData_New("vol", [today], [0.0], [today], [1.0])
    model = dal.GSRModelData_New("gsr", curve, vol)
    dal.EvaluationDate_Set(today)
    product = dal.Product_New([exercise], ["pay PAYS FIX(IR[USD,DF,2028-09-28])"])
    result = dal.MonteCarlo_ValueWithSettings(product, model, 16, simulation=dal.MonteCarloSettings_(enable_aad=True))
    assert result["PV"] == pytest.approx(math.exp(-0.06), abs=1e-10)
    assert result["d_logdf:OIS:2028-09-28"] == pytest.approx(math.exp(-0.06), abs=1e-10)


def test_gsr_aad_matches_second_bond_moment_and_model_parameter_risks():
    today = dal.Date_(2026, 9, 28)
    exercise = dal.Date_(2027, 9, 28)
    maturity = dal.Date_(2028, 9, 28)
    curve = dal.GSRCurveData_New(
        "curve", today, "USD", [today, exercise, maturity], [0.0, -0.03, -0.06], [], dal.DoubleMatrix_(0, 0)
    )
    g = 0.05
    h = 1.2
    vol = dal.GSRVolData_New("vol", [today], [g], [today], [h])
    model = dal.GSRModelData_New("gsr", curve, vol)
    dal.EvaluationDate_Set(today)
    product = dal.Product_New(
        [exercise],
        ["pay PAYS FIX(IR[USD,DF,2028-09-28]) * FIX(IR[USD,DF,2028-09-28])"],
    )
    result = dal.MonteCarlo_ValueWithSettings(product, model, 1048576, simulation=dal.MonteCarloSettings_(enable_aad=True))
    tenor = 366.0 / 365.0
    variance = g * g
    bond_loading = h * tenor
    expected = math.exp(-0.09 + bond_loading * bond_loading * variance)
    assert result["PV"] == pytest.approx(expected, abs=2e-5)
    assert result["d_logdf:OIS:2027-09-28"] == pytest.approx(-expected, abs=2e-5)
    assert result["d_logdf:OIS:2028-09-28"] == pytest.approx(2.0 * expected, abs=2e-5)
    assert result["d_g:2026-09-28"] == pytest.approx(expected * 2.0 * bond_loading * bond_loading * g, abs=2e-5)
    assert result["d_H:2026-09-28"] == pytest.approx(expected * 2.0 * h * tenor * tenor * variance, abs=2e-5)


def test_gsr_projection_node_risk_matches_central_difference():
    today = dal.Date_(2026, 9, 28)
    fixing = dal.Date_(2027, 9, 28)
    horizon = dal.Date_(2028, 9, 28)
    dal.EvaluationDate_Set(today)
    product = dal.Product_New([fixing], ["pay PAYS FIX(IR[USD,LIBOR_3M_LCH])"])
    vol = dal.GSRVolData_New("vol", [today], [0.0], [today], [1.0])

    def value(last_projection_node, enable_aad):
        curve = dal.GSRCurveData_New(
            "curve", today, "USD", [today, fixing, horizon], [0.0, -0.03, -0.06],
            ["3M"], dal.DoubleMatrix_([[0.0, -0.04, last_projection_node]]),
        )
        model = dal.GSRModelData_New("gsr", curve, vol)
        return dal.MonteCarlo_ValueWithSettings(
            product, model, 16, simulation=dal.MonteCarloSettings_(enable_aad=enable_aad)
        )

    aad = value(-0.08, True)
    bump = 1e-5
    difference = (value(-0.08 + bump, False)["PV"] - value(-0.08 - bump, False)["PV"]) / (2.0 * bump)
    assert aad["d_logdf:3M:2028-09-28"] == pytest.approx(difference, abs=1e-7)
    assert math.isfinite(aad["d_g:2026-09-28"])
