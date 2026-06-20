"""Tests for curve protocol type factories."""

import dal


# ---- CollateralType_ ----

def test_collateral_type_ois():
    """CollateralType_OIS returns a valid CollateralType_."""
    ct = dal.CollateralType_OIS()
    assert ct is not None  # nosec B101 - pytest assertions are intentional
    r = repr(ct)
    assert isinstance(r, str) and len(r) > 0  # nosec B101 - pytest assertions are intentional


def test_collateral_type_libor():
    """CollateralType_Libor accepts a tenor and returns a valid CollateralType_."""
    tenor = dal.PeriodLength_New("3M")
    ct = dal.CollateralType_Libor(tenor)
    assert ct is not None  # nosec B101 - pytest assertions are intentional
    r = repr(ct)
    assert isinstance(r, str) and len(r) > 0  # nosec B101 - pytest assertions are intentional


def test_collateral_type_construct_from_string():
    """CollateralType_ can be constructed from a string name."""
    ct = dal.CollateralType_("OIS")
    assert ct is not None  # nosec B101 - pytest assertions are intentional


# ---- PeriodLength_ ----

def test_period_length_new():
    """PeriodLength_New creates a PeriodLength_ from an ISO string."""
    pl = dal.PeriodLength_New("3M")
    assert pl is not None  # nosec B101 - pytest assertions are intentional
    r = repr(pl)
    assert isinstance(r, str) and len(r) > 0  # nosec B101 - pytest assertions are intentional


def test_period_length_new_various():
    """PeriodLength_New works for standard tenor strings."""
    for iso in ["1M", "3M", "6M", "12M"]:
        pl = dal.PeriodLength_New(iso)
        r = repr(pl)
        assert isinstance(r, str) and len(r) > 0  # nosec B101 - pytest assertions are intentional


def test_period_length_construct():
    """PeriodLength_ can be constructed directly from an ISO string."""
    pl = dal.PeriodLength_("6M")
    assert pl is not None  # nosec B101 - pytest assertions are intentional


# ---- DayBasis_ ----

def test_day_basis_new():
    """DayBasis_New creates a DayBasis_ from a name string."""
    db = dal.DayBasis_New("ACT_365F")
    assert db is not None  # nosec B101 - pytest assertions are intentional
    r = repr(db)
    assert isinstance(r, str) and len(r) > 0  # nosec B101 - pytest assertions are intentional


def test_day_basis_new_various():
    """DayBasis_New works for standard day count conventions."""
    for name in ["ACT_365F", "ACT_360", "30_360"]:
        db = dal.DayBasis_New(name)
        r = repr(db)
        assert isinstance(r, str) and len(r) > 0  # nosec B101 - pytest assertions are intentional


def test_day_basis_construct():
    """DayBasis_ can be constructed directly from a name string."""
    db = dal.DayBasis_("ACT_360")
    assert db is not None  # nosec B101 - pytest assertions are intentional


# ---- RateLegConvention_ ----

def test_rate_leg_convention_new():
    """RateLegConvention_New creates a convention from frequency and day basis."""
    freq = dal.PeriodLength_New("6M")
    basis = dal.DayBasis_New("ACT_365F")
    rlc = dal.RateLegConvention_New(freq, basis)
    assert rlc is not None  # nosec B101 - pytest assertions are intentional


def test_rate_leg_convention_construct():
    """RateLegConvention_ has a default constructor."""
    rlc = dal.RateLegConvention_()
    assert rlc is not None  # nosec B101 - pytest assertions are intentional


# ---- RateIndexConvention_ ----

def test_rate_index_convention_new():
    """RateIndexConvention_New creates a convention from tenor, basis, collateral."""
    tenor = dal.PeriodLength_New("3M")
    basis = dal.DayBasis_New("ACT_360")
    collateral = dal.CollateralType_OIS()
    ric = dal.RateIndexConvention_New(tenor, basis, collateral)
    assert ric is not None  # nosec B101 - pytest assertions are intentional


def test_rate_index_convention_new_with_projection():
    """RateIndexConvention_New accepts use_projection_curve flag."""
    tenor = dal.PeriodLength_New("3M")
    basis = dal.DayBasis_New("ACT_360")
    collateral = dal.CollateralType_OIS()
    ric = dal.RateIndexConvention_New(tenor, basis, collateral, use_projection_curve=True)
    assert ric is not None  # nosec B101 - pytest assertions are intentional


def test_rate_index_convention_default_construct():
    """RateIndexConvention_ has a default constructor."""
    ric = dal.RateIndexConvention_()
    assert ric is not None  # nosec B101 - pytest assertions are intentional


# ---- CurrencyPair_ ----

def test_currency_pair_new():
    """CurrencyPair_New creates a pair from two ISO currency codes."""
    pair = dal.CurrencyPair_New("USD", "EUR")
    assert pair is not None  # nosec B101 - pytest assertions are intentional


def test_currency_pair_new_various():
    """CurrencyPair_New works for various currency pairs."""
    for dom, frn in [("USD", "EUR"), ("GBP", "JPY"), ("EUR", "CHF")]:
        pair = dal.CurrencyPair_New(dom, frn)
        assert pair is not None  # nosec B101 - pytest assertions are intentional


# ---- Enums ----

def test_curve_solve_mode_enum():
    """CurveSolveMode enum has EXACT and APPROXIMATE values."""
    assert dal.CurveSolveMode.EXACT is not None  # nosec B101 - pytest assertions are intentional
    assert dal.CurveSolveMode.APPROXIMATE is not None  # nosec B101 - pytest assertions are intentional
    assert dal.CurveSolveMode.EXACT != dal.CurveSolveMode.APPROXIMATE  # nosec B101 - pytest assertions are intentional


def test_curve_parameterization_enum():
    """CurveParameterization enum has expected values."""
    assert dal.CurveParameterization.PIECEWISE_LINEAR_FWD is not None  # nosec B101 - pytest assertions are intentional
    assert dal.CurveParameterization.PIECEWISE_CONSTANT_FWD is not None  # nosec B101 - pytest assertions are intentional
    assert dal.CurveParameterization.ZERO_RATE is not None  # nosec B101 - pytest assertions are intentional
    assert dal.CurveParameterization.LOG_DISCOUNT is not None  # nosec B101 - pytest assertions are intentional


def test_curve_jacobian_mode_enum():
    """CurveJacobianMode enum has BUMPED and ANALYTIC values."""
    assert dal.CurveJacobianMode.BUMPED is not None  # nosec B101 - pytest assertions are intentional
    assert dal.CurveJacobianMode.ANALYTIC is not None  # nosec B101 - pytest assertions are intentional


def test_log_df_scheme_enum():
    """LogDfScheme enum has expected values."""
    assert dal.LogDfScheme.LOG_LINEAR is not None  # nosec B101 - pytest assertions are intentional
    assert dal.LogDfScheme.LOG_CUBIC_NATURAL is not None  # nosec B101 - pytest assertions are intentional
    assert dal.LogDfScheme.MIXED is not None  # nosec B101 - pytest assertions are intentional
