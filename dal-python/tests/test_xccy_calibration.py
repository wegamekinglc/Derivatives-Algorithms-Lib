"""Tests for cross-currency calibration."""

import dal

S = dal.String_


def _today():
    return dal.Date_(2025, 6, 20)


def _spot():
    return _today().AddDays(2)


def _make_baseline_curves():
    """Build flat 4% OIS curves for USD and EUR."""
    knot_dates = [_spot(), _spot().AddDays(3650)]
    ois_rates = [0.04, 0.04]

    usd_ois = dal.DiscountPWLF_New("usd_ois", "USD", knot_dates, ois_rates)
    eur_ois = dal.DiscountPWLF_New("eur_ois", "EUR", knot_dates, ois_rates)

    usd_block = dal.CurveBlock_New(usd_ois, libor_basis=dal.DayBasis_New("ACT_365F"))
    eur_block = dal.CurveBlock_New(eur_ois, libor_basis=dal.DayBasis_New("ACT_360"))

    return usd_block, eur_block


# ---- CrossCurrencyCalibrationSpecBuilder_ ----

def test_xccy_builder_defaults():
    """CrossCurrencyCalibrationSpecBuilder_ has sensible defaults."""
    builder = dal.CrossCurrencyCalibrationSpecBuilder_()
    assert builder.tolerance_ == 1.0e-10  # nosec B101 - pytest assertions are intentional
    assert builder.fitTolerance_ == 1.0e-6  # nosec B101 - pytest assertions are intentional
    assert builder.maxEvaluations_ == 200  # nosec B101 - pytest assertions are intentional
    assert builder.maxRestarts_ == 20  # nosec B101 - pytest assertions are intentional
    assert builder.initialGuess_ == 0.0  # nosec B101 - pytest assertions are intentional
    assert builder.smoothingWeight_ == 1.0  # nosec B101 - pytest assertions are intentional
    assert builder.fxSpot_ == 0.0  # nosec B101 - pytest assertions are intentional


def test_xccy_builder_can_set_fields():
    """CrossCurrencyCalibrationSpecBuilder_ fields can be set."""
    builder = dal.CrossCurrencyCalibrationSpecBuilder_()
    builder.today_ = _today()
    builder.basisPair_ = dal.CurrencyPair_New("USD", "EUR")
    builder.fxSpot_ = 1.10
    builder.tolerance_ = 1.0e-8
    builder.solveMode_ = dal.CurveSolveMode.APPROXIMATE
    assert builder.fxSpot_ == 1.10  # nosec B101 - pytest assertions are intentional
    assert builder.tolerance_ == 1.0e-8  # nosec B101 - pytest assertions are intentional


# ---- Cross-currency calibration ----

def _make_xccy_instruments(fx_spot):
    """Build cross-currency swap instruments for calibration."""
    currencies = dal.CurrencyPair_New("USD", "EUR")
    usd_leg = dal.RateLegConvention_New(dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_365F"))
    usd_index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS()
    )
    eur_leg = dal.RateLegConvention_New(dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_360"))
    eur_index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS()
    )

    instruments = []
    knot_dates = []
    for y in [2, 5, 10]:
        maturity = _spot().AddDays(y * 365)
        knot_dates.append(maturity)
        inst = dal.CrossCurrencySwapNew(
            _today(), _spot(), maturity, 0.01, currencies,
            domestic_notional=100.0, foreign_notional=100.0 / fx_spot,
            domestic_leg=usd_leg, domestic_index=usd_index,
            foreign_leg=eur_leg, foreign_index=eur_index,
        )
        instruments.append(inst)
    return instruments, knot_dates


def test_calibrate_xccy_market():
    """CalibrateXccyMarket calibrates a cross-currency basis curve."""
    usd_block, eur_block = _make_baseline_curves()
    instruments, knot_dates = _make_xccy_instruments(fx_spot=1.10)

    builder = dal.CrossCurrencyCalibrationSpecBuilder_()
    builder.today_ = _today()
    builder.basisPair_ = dal.CurrencyPair_New("USD", "EUR")
    builder.domesticCurveBlock_ = usd_block
    builder.foreignCurveBlock_ = eur_block
    builder.fxSpot_ = 1.10
    builder.tolerance_ = 1.0e-8
    builder.solveMode_ = dal.CurveSolveMode.APPROXIMATE
    builder.initialGuess_ = 0.01
    builder.instruments_ = instruments
    builder.knotDates_ = knot_dates

    spec = builder.Build()
    result = dal.CalibrateXccyMarket(spec)

    diag = result.diagnostics_
    assert len(diag.marketRates_) > 0  # nosec B101 - pytest assertions are intentional
    assert len(diag.marketRates_) == len(diag.modelRates_)  # nosec B101 - pytest assertions are intentional
    assert diag.maxAbsResidual_ < 1.0e-4  # nosec B101 - pytest assertions are intentional
    assert diag.rmsResidual_ < 1.0e-4  # nosec B101 - pytest assertions are intentional

    # FX forward curve should be populated
    fwd = result.fxForwardCurve_
    assert len(fwd.dates_) > 0  # nosec B101 - pytest assertions are intentional
    assert len(fwd.dates_) == len(fwd.forwards_)  # nosec B101 - pytest assertions are intentional


def test_calibrate_xccy_market_result_has_market():
    """CalibrateXccyMarket result contains a market and FX forward curve."""
    usd_block, eur_block = _make_baseline_curves()
    instruments, knot_dates = _make_xccy_instruments(fx_spot=1.10)

    builder = dal.CrossCurrencyCalibrationSpecBuilder_()
    builder.today_ = _today()
    builder.basisPair_ = dal.CurrencyPair_New("USD", "EUR")
    builder.domesticCurveBlock_ = usd_block
    builder.foreignCurveBlock_ = eur_block
    builder.fxSpot_ = 1.10
    builder.tolerance_ = 1.0e-8
    builder.solveMode_ = dal.CurveSolveMode.APPROXIMATE
    builder.initialGuess_ = 0.01
    builder.instruments_ = instruments
    builder.knotDates_ = knot_dates

    spec = builder.Build()
    result = dal.CalibrateXccyMarket(spec)

    assert result.market_ is not None  # nosec B101 - pytest assertions are intentional
    assert result.fxForwardCurve_ is not None  # nosec B101 - pytest assertions are intentional
    assert result.diagnostics_ is not None  # nosec B101 - pytest assertions are intentional
