"""Tests for curve instrument factories."""

import dal


def _today():
    return dal.Date_(2025, 6, 20)


def _spot():
    return _today().AddDays(2)


def _fixed_6m():
    return dal.RateLegConvention_New(dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_365F"))


def _float_3m():
    return dal.RateLegConvention_New(dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360"))


def _libor_3m():
    return dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS()
    )


def _overnight_index():
    return dal.RateIndexConvention_New(
        dal.PeriodLength_New("12M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS()
    )


# ---- Deposit ----

def test_deposit_new():
    """DepositNew constructs a deposit instrument."""
    start = _spot()
    maturity = start.AddDays(90)
    inst = dal.DepositNew(_today(), start, maturity, 0.05, _libor_3m())
    assert inst is not None  # nosec B101 - pytest assertions are intentional


def test_deposit_new_various_maturities():
    """DepositNew works for various maturities."""
    for days in [30, 90, 180, 365]:
        start = _spot()
        maturity = start.AddDays(days)
        inst = dal.DepositNew(_today(), start, maturity, 0.04, _libor_3m())
        assert inst is not None  # nosec B101 - pytest assertions are intentional


# ---- FRA ----

def test_fra_new():
    """FRANew constructs a FRA instrument."""
    start = _spot().AddDays(180)
    maturity = start.AddDays(90)
    inst = dal.FRANew(_today(), start, maturity, 0.045, _libor_3m())
    assert inst is not None  # nosec B101 - pytest assertions are intentional


# ---- Future ----

def test_future_new():
    """FutureNew constructs a future instrument."""
    start = _spot().AddDays(90)
    maturity = start.AddDays(90)
    inst = dal.FutureNew(_today(), start, maturity, 0.045, _libor_3m())
    assert inst is not None  # nosec B101 - pytest assertions are intentional


def test_future_new_with_convexity():
    """FutureNew accepts a convexity adjustment."""
    start = _spot().AddDays(90)
    maturity = start.AddDays(90)
    inst = dal.FutureNew(_today(), start, maturity, 0.045, _libor_3m(), convexity_adjustment=0.0005)
    assert inst is not None  # nosec B101 - pytest assertions are intentional


# ---- Vanilla Swap ----

def test_swap_new():
    """SwapNew constructs a vanilla Libor swap."""
    start = _spot()
    maturity = start.AddDays(1825)  # 5 years
    inst = dal.SwapNew(_today(), start, maturity, 0.04, _fixed_6m(), _libor_3m(), _float_3m())
    assert inst is not None  # nosec B101 - pytest assertions are intentional


def test_swap_new_various_maturities():
    """SwapNew works for various maturities."""
    for years in [1, 2, 5, 10, 30]:
        start = _spot()
        maturity = start.AddDays(years * 365)
        inst = dal.SwapNew(_today(), start, maturity, 0.04, _fixed_6m(), _libor_3m(), _float_3m())
        assert inst is not None  # nosec B101 - pytest assertions are intentional


# ---- OIS Swap ----

def test_ois_swap_new():
    """OISSwapNew constructs an OIS swap."""
    start = _spot()
    maturity = start.AddDays(1825)
    inst = dal.OISSwapNew(_today(), start, maturity, 0.035, _fixed_6m(), _overnight_index(), _float_3m())
    assert inst is not None  # nosec B101 - pytest assertions are intentional


# ---- Basis Swap ----

def test_basis_swap_new():
    """BasisSwapNew constructs a basis swap."""
    start = _spot()
    maturity = start.AddDays(3650)  # 10 years
    inst = dal.BasisSwapNew(
        _today(), start, maturity, 0.0025,
        _libor_3m(), _float_3m(),         # spread leg
        _overnight_index(), _float_3m(),   # ref leg
    )
    assert inst is not None  # nosec B101 - pytest assertions are intentional


# ---- Cross-Currency Swap ----

def test_cross_currency_swap_new():
    """CrossCurrencySwapNew constructs a cross-currency swap with explicit conventions."""
    start = _spot()
    maturity = start.AddDays(3650)
    currencies = dal.CurrencyPair_New("USD", "EUR")
    domestic_leg = dal.RateLegConvention_New(dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_365F"))
    domestic_index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS()
    )
    foreign_leg = dal.RateLegConvention_New(dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_360"))
    foreign_index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS()
    )
    inst = dal.CrossCurrencySwapNew(
        _today(), start, maturity, 0.001,
        currencies,
        domestic_notional=100.0, foreign_notional=90.0,
        domestic_leg=domestic_leg, domestic_index=domestic_index,
        foreign_leg=foreign_leg, foreign_index=foreign_index,
    )
    assert inst is not None  # nosec B101 - pytest assertions are intentional


def test_cross_currency_swap_new_defaults():
    """CrossCurrencySwapNew works with default leg/index conventions."""
    start = _spot()
    maturity = start.AddDays(3650)
    currencies = dal.CurrencyPair_New("USD", "EUR")
    inst = dal.CrossCurrencySwapNew(_today(), start, maturity, 0.001, currencies)
    assert inst is not None  # nosec B101 - pytest assertions are intentional
