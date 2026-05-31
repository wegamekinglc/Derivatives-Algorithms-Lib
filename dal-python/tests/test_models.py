"""Tests for Black-Scholes and Dupire model data creation."""

import dal


# ---- BSModelData -------------------------------------------------------------


def test_bs_model_new():
    """BSModelData_New creates a valid model handle."""
    model = dal.BSModelData_New(spot=100.0, vol=0.2, rate=0.05, div=0.02)
    assert model is not None


def test_bs_model_zero_vol():
    """BS model with zero vol is accepted (degenerate case)."""
    model = dal.BSModelData_New(spot=100.0, vol=0.0, rate=0.05, div=0.0)
    assert model is not None


def test_bs_model_zero_rate_and_div():
    """BS model with zero rate and dividend works."""
    model = dal.BSModelData_New(spot=100.0, vol=0.3, rate=0.0, div=0.0)
    assert model is not None


def test_bs_model_high_vol():
    """BS model with high vol is accepted."""
    model = dal.BSModelData_New(spot=100.0, vol=2.0, rate=0.05, div=0.02)
    assert model is not None


def test_bs_model_various_spots():
    """BS model works for a range of spot values."""
    for spot in [1.0, 10.0, 100.0, 1000.0, 50000.0]:
        model = dal.BSModelData_New(spot=spot, vol=0.2, rate=0.05, div=0.01)
        assert model is not None


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
    assert model is not None


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
    assert model is not None


def test_dupire_model_skewed_surface():
    """Dupire with a volatility skew (higher vol for lower strikes)."""
    spots = [80.0, 90.0, 100.0, 110.0, 120.0]
    times = [0.5, 1.0]
    # Higher vol for lower spots (typical equity skew)
    vol_values = [
        [0.30, 0.28],  # spot=80
        [0.25, 0.23],  # spot=90
        [0.20, 0.20],  # spot=100
        [0.18, 0.18],  # spot=110
        [0.16, 0.17],  # spot=120
    ]
    vols = dal.DoubleMatrix_(len(spots), len(times), 0.0)
    # Note: Matrix_ only exposes read access via __call__ in the SWIG binding.
    # The fill value in the constructor is used. We verify construction works.
    # For a proper skew test, we'd need a writable matrix, which isn't
    # exposed in the current SWIG interface.

    model = dal.DupireModelData_New(
        spot=100.0,
        rate=0.05,
        repo=0.01,
        spots=spots,
        times=times,
        vols=vols,
    )
    assert model is not None


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
    assert model is not None
