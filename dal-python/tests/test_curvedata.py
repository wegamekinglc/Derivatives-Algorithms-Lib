"""Tests for curve data factories."""

import dal


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
