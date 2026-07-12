"""Tests for curve data factories."""

import dal
import pytest


def _today():
    return dal.Date_(2025, 6, 20)


def _spot():
    return _today().AddDays(2)


# ---- DiscountPWLF_New ----

def test_discount_pwlf_new():
    """DiscountPWLF_New constructs a flat-forward discount curve."""
    knot_dates = [_spot(), _spot().AddDays(1825)]  # spot + 5 years
    fwd_rates = [0.04, 0.04]
    curve = dal.DiscountPWLF_New("flat", "USD", knot_dates, fwd_rates)
    assert curve is not None  # nosec B101 - pytest assertions are intentional


def test_discount_pwlf_new_multiple_knots():
    """DiscountPWLF_New works with multiple knot points."""
    knot_dates = []
    fwd_rates = []
    for y in [0, 1, 2, 5, 10]:
        knot_dates.append(_spot().AddDays(y * 365))
        fwd_rates.append(0.05)
    curve = dal.DiscountPWLF_New("multi", "USD", knot_dates, fwd_rates)
    assert curve is not None  # nosec B101 - pytest assertions are intentional


def test_discount_pwlf_new_with_base():
    """DiscountPWLF_New accepts an optional base discount curve."""
    knot_dates = [_spot(), _spot().AddDays(365)]
    fwd_rates = [0.03, 0.03]
    base = dal.DiscountPWLF_New("base", "USD", knot_dates, fwd_rates)
    assert base is not None  # nosec B101 - pytest assertions are intentional

    knot_dates2 = [_spot(), _spot().AddDays(1825)]
    fwd_rates2 = [0.04, 0.04]
    curve = dal.DiscountPWLF_New("bootstrapped", "USD", knot_dates2, fwd_rates2, base=base)
    assert curve is not None  # nosec B101 - pytest assertions are intentional


# ---- DiscountZeroRate_New ----

def test_discount_zero_rate_new_defaults():
    """DiscountZeroRate_New supplies ACT/365F and log-linear defaults."""
    curve = dal.DiscountZeroRate_New(
        "zero_default",
        "USD",
        _today(),
        [_today().AddDays(365), _today().AddDays(730)],
        [0.02, 0.025],
    )

    assert curve is not None  # nosec B101 - pytest assertions are intentional


@pytest.mark.parametrize(
    "scheme",
    [
        dal.LogDfScheme.LOG_LINEAR,
        dal.LogDfScheme.LOG_CUBIC_NATURAL,
        dal.LogDfScheme.MIXED,
    ],
)
def test_discount_zero_rate_new_explicit_options(scheme):
    """DiscountZeroRate_New accepts an explicit day count and every log-DF scheme."""
    curve = dal.DiscountZeroRate_New(
        "zero_explicit",
        "USD",
        _today(),
        [_today().AddDays(360), _today().AddDays(720), _today().AddDays(1080)],
        [0.02, 0.025, 0.03],
        day_count=dal.DayBasis_New("ACT_360"),
        log_df_scheme=scheme,
    )

    assert curve is not None  # nosec B101 - pytest assertions are intentional


def test_discount_zero_rate_new_with_base():
    """DiscountZeroRate_New accepts a base curve for multiplicative layering."""
    node_dates = [_today().AddDays(365), _today().AddDays(730)]
    base = dal.DiscountZeroRate_New("base", "USD", _today(), node_dates, [0.01, 0.01])
    curve = dal.DiscountZeroRate_New(
        "zero_spread",
        "USD",
        _today(),
        node_dates,
        [0.02, 0.02],
        base=base,
    )

    assert curve is not None  # nosec B101 - pytest assertions are intentional


@pytest.mark.parametrize(
    "node_dates,zero_rates",
    [
        (lambda today: [today], [0.02]),
        (lambda today: [today.AddDays(365)], []),
        (lambda today: [today.AddDays(365)], [float("nan")]),
    ],
)
def test_discount_zero_rate_new_rejects_invalid_inputs(node_dates, zero_rates):
    """Native ZERO_RATE validation is translated to Python exceptions."""
    today = _today()

    with pytest.raises(RuntimeError, match="zero-rate discount curve"):
        dal.DiscountZeroRate_New(
            "invalid_zero",
            "USD",
            today,
            node_dates(today),
            zero_rates,
        )


# ---- CurveBlock_New (simple) ----

def test_curve_block_new_simple():
    """CurveBlock_New constructs a CurveBlock_ from a single discount curve."""
    knot_dates = [_spot(), _spot().AddDays(1825)]
    fwd_rates = [0.04, 0.04]
    dc = dal.DiscountPWLF_New("ois", "USD", knot_dates, fwd_rates)

    block = dal.CurveBlock_New(dc)
    assert block is not None  # nosec B101 - pytest assertions are intentional


def test_curve_block_new_simple_with_basis():
    """CurveBlock_New accepts an optional Libor day basis."""
    knot_dates = [_spot(), _spot().AddDays(1825)]
    fwd_rates = [0.04, 0.04]
    dc = dal.DiscountPWLF_New("ois", "USD", knot_dates, fwd_rates)

    block = dal.CurveBlock_New(dc, libor_basis=dal.DayBasis_New("ACT_360"))
    assert block is not None  # nosec B101 - pytest assertions are intentional


# ---- CurveBlock_New (full) ----

def test_curve_block_new_full():
    """CurveBlock_New constructs a CurveBlock_ from discount and forward curve maps."""
    knot_dates = [_spot(), _spot().AddDays(1825)]

    ois_curve = dal.DiscountPWLF_New("ois", "USD", knot_dates, [0.04, 0.04])
    libor_curve = dal.DiscountPWLF_New("libor", "USD", knot_dates, [0.045, 0.045])

    discounts = {dal.CollateralType_OIS(): ois_curve}
    forwards = {dal.PeriodLength_New("3M"): libor_curve}

    block = dal.CurveBlock_New("usd", "USD", discounts, forwards, dal.DayBasis_New("ACT_365F"))
    assert block is not None  # nosec B101 - pytest assertions are intentional
