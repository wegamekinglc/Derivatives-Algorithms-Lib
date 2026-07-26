"""Tests for cross-currency calibration."""

import dal


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


def test_xccy_builder_per_node_guess_has_legacy_and_snake_case_names():
    builder = dal.CrossCurrencyCalibrationSpecBuilder_()
    builder.initial_guess_per_node = [0.0125, 0.0175]

    assert builder.initialGuessPerNode_ == [0.0125, 0.0175]  # nosec B101
    assert builder.Build().initial_guess_per_node == [0.0125, 0.0175]  # nosec B101


def test_xccy_builder_new_fields_have_legacy_and_snake_case_names():
    """Reset-aware fields do not replace the existing underscore surface."""
    builder = dal.CrossCurrencyCalibrationSpecBuilder_()
    valuation_time = dal.DateTime_(_today(), 9, 45)
    snapshot = dal.MarketFixingSnapshot_New({})

    builder.valuation_time = valuation_time
    builder.collateralCurrency_ = dal.Ccy_("USD")
    builder.fixings = snapshot
    builder.fx_spot = 1.10

    assert builder.valuationTime_ is not None  # nosec B101 - pytest assertions are intentional
    assert builder.collateral_currency is not None  # nosec B101 - pytest assertions are intentional
    assert builder.fixings_ is snapshot  # nosec B101 - pytest assertions are intentional
    assert builder.fxSpot_ == 1.10  # nosec B101 - pytest assertions are intentional


def test_legacy_xccy_constructor_accepts_every_original_positional_argument():
    """The config overload leaves the original positional order untouched."""
    currencies = dal.CurrencyPair_New("USD", "EUR")
    domestic_leg = dal.RateLegConvention_New(dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_365F"))
    domestic_index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS()
    )
    foreign_leg = dal.RateLegConvention_New(dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_360"))
    foreign_index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS()
    )

    instrument = dal.CrossCurrencySwap_New(
        _today(),
        _spot(),
        _spot().AddDays(365),
        0.001,
        currencies,
        125.0,
        113.5,
        domestic_leg,
        domestic_index,
        foreign_leg,
        foreign_index,
    )
    assert instrument is not None  # nosec B101 - pytest assertions are intentional


# ---- Cross-currency calibration ----

def _make_xccy_instruments(fx_spot, years=(2, 5, 10)):
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
    for y in years:
        maturity = _spot().AddDays(y * 365)
        knot_dates.append(maturity)
        inst = dal.CrossCurrencySwap_New(
            _today(), _spot(), maturity, 0.01, currencies,
            domestic_notional=100.0, foreign_notional=100.0 / fx_spot,
            domestic_leg=usd_leg, domestic_index=usd_index,
            foreign_leg=eur_leg, foreign_index=eur_index,
        )
        instruments.append(inst)
    return instruments, knot_dates


def _make_xccy_spec(solve_mode, years=(2, 5, 10)):
    usd_block, eur_block = _make_baseline_curves()
    instruments, knot_dates = _make_xccy_instruments(fx_spot=1.10, years=years)

    builder = dal.CrossCurrencyCalibrationSpecBuilder_()
    builder.today_ = _today()
    builder.basisPair_ = dal.CurrencyPair_New("USD", "EUR")
    builder.domesticCurveBlock_ = usd_block
    builder.foreignCurveBlock_ = eur_block
    builder.fxSpot_ = 1.10
    builder.tolerance_ = 1.0e-8
    builder.solveMode_ = solve_mode
    builder.initialGuess_ = 0.01
    builder.instruments_ = instruments
    builder.knotDates_ = knot_dates
    return builder.Build(), knot_dates


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


def test_staged_xccy_options_overload_preserves_default_results_and_aliases():
    """The additive overload retains the legacy call and exposes both naming styles."""
    spec, knot_dates = _make_xccy_spec(dal.CurveSolveMode.APPROXIMATE)
    legacy = dal.CalibrateXccyMarket(spec)
    options = dal.CrossCurrencyCalibrationOptions_()
    explicit = dal.CalibrateXccyMarket(spec, options)

    assert options.jacobianMode_ == dal.CurveJacobianMode.ANALYTIC  # nosec B101
    options.jacobian_mode = dal.CurveJacobianMode.BUMPED
    options.computeEffJacobianInverse_ = False
    options.compute_forward_jacobian = False
    assert options.jacobianMode_ == dal.CurveJacobianMode.BUMPED  # nosec B101
    assert not options.compute_eff_jacobian_inverse  # nosec B101
    assert not options.computeForwardJacobian_  # nosec B101

    assert legacy.diagnostics.market_rates == explicit.diagnostics_.marketRates_  # nosec B101
    assert legacy.diagnostics.model_rates == explicit.diagnostics_.modelRates_  # nosec B101
    assert legacy.diagnostics.residuals == explicit.diagnostics_.residuals_  # nosec B101
    assert legacy.fx_forward_curve.forwards == explicit.fxForwardCurve_.forwards_  # nosec B101
    assert explicit.diagnostics.parameter_knot_dates == knot_dates  # nosec B101
    assert explicit.diagnostics.parameterKnotDates_ == explicit.diagnostics.parameter_knot_dates  # nosec B101
    assert explicit.diagnostics.instrumentNames_ == explicit.diagnostics.instrument_names  # nosec B101
    assert explicit.diagnostics.residualTolerance_ == explicit.diagnostics.residual_tolerance  # nosec B101
    assert explicit.diagnostics.jacobianScaling_ == "unscaled"  # nosec B101
    assert explicit.diagnostics.jacobian_scaling == "unscaled"  # nosec B101
    assert explicit.diagnostics.effJacobianInverseScaling_ == "solver_scaled"  # nosec B101
    assert explicit.diagnostics.eff_jacobian_inverse_scaling == "solver_scaled"  # nosec B101
    assert explicit.diagnostics.jacobianAvailability_ == "not_available_for_mode"  # nosec B101
    assert explicit.diagnostics.eff_jacobian_inverse_availability == "not_available_for_mode"  # nosec B101
    assert explicit.diagnostics.jacobian_.rows() == 0  # nosec B101
    assert explicit.diagnostics.jacobian.cols() == 0  # nosec B101
    assert explicit.diagnostics.effJacobianInverse_.rows() == 0  # nosec B101
    assert explicit.diagnostics.eff_jacobian_inverse.cols() == 0  # nosec B101


def test_staged_xccy_exact_matrices_keep_diagnostics_ownership_and_axes():
    """Exact analytic matrices remain on staged diagnostics with reversed axes."""
    spec, knot_dates = _make_xccy_spec(dal.CurveSolveMode.EXACT, years=(2,))
    result = dal.CalibrateXccyMarket(spec, dal.CrossCurrencyCalibrationOptions_())
    diagnostics = result.diagnostics

    assert diagnostics.jacobian_availability == "available"  # nosec B101
    assert diagnostics.effJacobianInverseAvailability_ == "available"  # nosec B101
    assert diagnostics.jacobian.rows() == len(diagnostics.instrument_names)  # nosec B101
    assert diagnostics.jacobian_.cols() == len(knot_dates)  # nosec B101
    assert diagnostics.eff_jacobian_inverse.rows() == len(knot_dates)  # nosec B101
    assert diagnostics.effJacobianInverse_.cols() == len(diagnostics.instrumentNames_)  # nosec B101
    assert not hasattr(result, "jacobian_at_solution")  # nosec B101
