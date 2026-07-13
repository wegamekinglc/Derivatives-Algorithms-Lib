"""Python coverage for joint domestic, foreign, and XCCY calibration."""

import dal


def _today():
    return dal.Date_(2025, 1, 16)


def _joint_curve(name, ccy, market_rate):
    maturity = _today().AddDays(365)
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_365F"), dal.CollateralType_OIS()
    )
    declaration = dal.JointCurveDeclaration_()
    declaration.curveName_ = name
    declaration.instruments = [dal.Deposit_New(_today(), _today(), maturity, market_rate, index)]
    declaration.knot_dates = [maturity]
    declaration.targetCollateral_ = dal.CollateralType_OIS()
    declaration.calibrate_discount_curve = True
    declaration.parameterization_ = dal.CurveParameterization.PIECEWISE_CONSTANT_FWD
    declaration.log_df_scheme = dal.LogDfScheme.LOG_LINEAR

    result = dal.JointCurrencyCurveSpec_()
    result.ccy = dal.Ccy_(ccy)
    result.liborBasis_ = dal.DayBasis_New("ACT_365F")
    result.curves = [declaration]
    return result


def _basis_instrument():
    maturity = _today().AddDays(365)
    pair = dal.CurrencyPair_New("USD", "EUR")
    domestic_leg = dal.RateLegConvention_New(dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_365F"))
    foreign_leg = dal.RateLegConvention_New(dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_360"))
    domestic_index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_365F"), dal.CollateralType_OIS()
    )
    foreign_index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_365F"), dal.CollateralType_OIS()
    )
    return dal.CrossCurrencySwap_New(
        _today(),
        _today(),
        maturity,
        0.001,
        pair,
        110.0,
        100.0,
        domestic_leg,
        domestic_index,
        foreign_leg,
        foreign_index,
    )


def _joint_spec():
    maturity = _today().AddDays(365)
    basis = dal.XccyBasisCurveDeclaration_()
    basis.curve_name = "usd_eur_basis"
    basis.instruments_ = [_basis_instrument()]
    basis.knot_dates = [maturity]
    basis.parameterization = dal.CurveParameterization.PIECEWISE_CONSTANT_FWD
    basis.smoothingWeight_ = 1.0
    basis.initial_guess_per_node = [0.001]

    builder = dal.JointXccyCalibrationSpecBuilder_()
    builder.valuationTime_ = dal.DateTime_(_today(), 0, 0)
    builder.pair = dal.CurrencyPair_New("USD", "EUR")
    builder.collateral_currency = dal.Ccy_("USD")
    builder.fxSpot_ = 1.10
    builder.domestic = _joint_curve("usd_ois", "USD", 0.04)
    builder.foreign_ = _joint_curve("eur_ois", "EUR", 0.03)
    builder.basis = basis
    builder.fixings_ = dal.MarketFixingSnapshot_New({})
    builder.solver_options.initialGuess_ = 0.01
    builder.solverOptions_.tolerance_ = 1.0e-9
    builder.solver_options.max_evaluations = 400
    return builder.Build()


def _assert_result_layout(result, expect_jacobian=True):
    assert result.converged_  # nosec B101
    assert result.converged  # nosec B101
    assert len(result.marketRates_) == len(result.model_rates)  # nosec B101
    assert len(result.residuals) == len(result.market_rates)  # nosec B101
    assert sum(r.size_ for r in result.residualRanges_) == len(result.residuals_)  # nosec B101
    assert sum(r.size for r in result.parameter_ranges) > 0  # nosec B101
    assert result.parameterRanges_[0].name_ == result.parameter_ranges[0].name  # nosec B101
    if expect_jacobian:
        assert result.jacobianAtSolution_.rows() == sum(r.size_ for r in result.residualRanges_)  # nosec B101
        assert result.jacobian_at_solution.cols() == sum(r.size for r in result.parameter_ranges)  # nosec B101
    else:
        assert result.jacobian_at_solution.rows() == 0  # nosec B101
        assert result.jacobianAtSolution_.cols() == 0  # nosec B101
    assert result.domesticCurveBlock_ is not None  # nosec B101
    assert result.foreign_curve_block is not None  # nosec B101
    assert result.basisCurve_ is not None  # nosec B101
    assert result.fx_forward_curve is not None  # nosec B101


def test_joint_builder_defaults_and_default_calibration_result_layout():
    """The default overload exposes every joint result block and range."""
    defaults = dal.JointXccyCalibrationSpecBuilder_()
    assert defaults.solverOptions_.initialGuess_ == 0.0  # nosec B101
    assert defaults.solver_options.initial_guess == 0.0  # nosec B101

    _assert_result_layout(dal.CalibrateJointXccyMarket(_joint_spec()))


def test_joint_options_overload_and_aliases():
    """The options overload accepts the Python enum and matrix toggles."""
    options = dal.JointXccyCalibrationOptions_()
    options.jacobian_mode = dal.CurveJacobianMode.BUMPED
    options.computeEffJacobianInverse_ = False
    options.compute_forward_jacobian = False

    assert options.jacobianMode_ == dal.CurveJacobianMode.BUMPED  # nosec B101
    assert not options.compute_eff_jacobian_inverse  # nosec B101
    assert not options.computeForwardJacobian_  # nosec B101

    result = dal.CalibrateJointXccyMarket(_joint_spec(), options)
    _assert_result_layout(result, expect_jacobian=False)
    assert result.effJacobianInverse_.rows() == 0  # nosec B101
    assert result.eff_jacobian_inverse.cols() == 0  # nosec B101
