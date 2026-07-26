"""Additive native surfaces required by the Web curve workbench."""

import dal


def _today():
    return dal.Date_(2026, 1, 15)


def test_planner_projects_immutable_origin_aware_snapshots():
    today = _today()
    submitted = [today.AddDays(90), today.AddDays(180)]

    plan = dal.PlanCurveCalibrationKnots(
        today,
        (),
        submitted,
        dal.CurveKnotPolicy.INPUT,
        dal.CurveParameterization.ZERO_RATE,
    )

    assert plan.planner_version == 1  # nosec B101
    assert plan.requested_policy == dal.CurveKnotPolicy.INPUT  # nosec B101
    assert plan.execution_policy == dal.CurveKnotPolicy.INPUT  # nosec B101
    assert isinstance(plan.submitted_knot_dates, tuple)  # nosec B101
    assert isinstance(plan.candidate_trace, tuple)  # nosec B101
    assert isinstance(plan.storage_nodes, tuple)  # nosec B101
    assert plan.anchor_added is True  # nosec B101
    assert plan.storage_nodes[0].origins[0].kind == dal.CurveKnotOriginKind.SYNTHETIC_ANCHOR  # nosec B101
    assert plan.free_parameters[0].component == dal.CurveFreeParameterComponent.ZERO_RATE  # nosec B101


def test_complete_factories_and_dynamic_curve_state_are_read_only():
    today = _today()
    knots = [today.AddDays(30), today.AddDays(365), today.AddDays(730), today.AddDays(1095)]
    right = [0.01, 0.02, 0.03, 0.04]
    left = [0.011, 0.021, 0.031, 0.041]
    base = dal.DiscountPWC_New("base", "USD", knots, right)
    pwlf = dal.DiscountPWLF_New("spread", "USD", knots, left, right, base)

    assert isinstance(base, dal.DiscountPWC_)  # nosec B101
    assert isinstance(pwlf, dal.DiscountPWLF_)  # nosec B101
    assert pwlf.name == "spread"  # nosec B101
    assert pwlf.currency == "USD"  # nosec B101
    assert tuple(pwlf.knot_dates) == tuple(knots)  # nosec B101
    assert pwlf.left_forwards == left  # nosec B101
    assert pwlf.right_forwards == right  # nosec B101
    assert pwlf.base is base  # nosec B101

    node_dates = [today, *knots]
    log_dfs = [0.0, -0.001, -0.02, -0.05, -0.09]
    mapped = dal.DiscountLogDF_New(
        "mapped",
        "USD",
        node_dates,
        log_dfs,
        day_count=dal.DayBasis_New("ACT_360"),
        log_df_scheme=dal.LogDfScheme.MIXED,
        base=base,
    )
    assert isinstance(mapped, dal.DiscountLogDF_)  # nosec B101
    assert tuple(mapped.node_dates) == tuple(node_dates)  # nosec B101
    assert mapped.log_discount_factors == log_dfs  # nosec B101
    assert mapped.day_count == "ACT_360"  # nosec B101
    assert mapped.log_df_scheme == dal.LogDfScheme.MIXED  # nosec B101
    assert mapped.base is base  # nosec B101


def test_options_matrix_materialization_and_basis_scheme_are_exposed():
    options = dal.CurveCalibrationOptions_()
    options.jacobian_mode = dal.CurveJacobianMode.BUMPED
    options.compute_forward_jacobian = False
    options.compute_eff_jacobian_inverse = False
    assert options.jacobianMode_ == dal.CurveJacobianMode.BUMPED  # nosec B101
    assert options.computeForwardJacobian_ is False  # nosec B101
    assert options.computeEffJacobianInverse_ is False  # nosec B101

    matrix = dal.DoubleMatrix_(2, 3, 1.25)
    assert matrix.to_rows() == [[1.25, 1.25, 1.25], [1.25, 1.25, 1.25]]  # nosec B101

    basis = dal.XccyBasisCurveDeclaration_()
    assert basis.log_df_scheme == dal.LogDfScheme.LOG_LINEAR  # nosec B101
    basis.logDfScheme_ = dal.LogDfScheme.MIXED
    assert basis.log_df_scheme == dal.LogDfScheme.MIXED  # nosec B101


def test_structured_analytic_eligibility_is_read_only():
    today = _today()
    maturity = today.AddDays(365)
    fixed_leg = dal.RateLegConvention_New(
        dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_365F")
    )
    float_leg = dal.RateLegConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360")
    )
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("12M"),
        dal.DayBasis_New("ACT_360"),
        dal.CollateralType_OIS(),
    )
    instrument = dal.OISSwap_New(
        today.AddDays(-1), today, maturity, 0.04, fixed_leg, index, float_leg
    )
    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = today
    builder.ccy_ = dal.String_("USD")
    builder.instruments_ = [instrument]
    builder.knotDates_ = [maturity]

    report = dal.ValidateSingleCurveAnalyticEligibility(builder.Build())

    assert report.eligible is False  # nosec B101
    assert isinstance(report.issues, tuple)  # nosec B101
    assert report.issues[0].reason == dal.AnalyticIneligibilityReason.TRADE_DATE_MISMATCH  # nosec B101
    assert report.issues[0].group == "single"  # nosec B101


def test_log_discount_scalar_seed_is_resolved_to_dated_raw_parameters():
    today = dal.Date_(2026, 1, 2)
    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = today
    builder.ccy_ = dal.String_("USD")
    builder.curveName_ = dal.String_("dated-seed")
    builder.liborBasis_ = dal.DayBasis_New("ACT_365F")
    builder.parameterization_ = dal.CurveParameterization.LOG_DISCOUNT
    builder.knotPolicy_ = dal.CurveKnotPolicy.INPUT
    builder.initialGuess_ = 0.03
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("12M"),
        dal.DayBasis_New("ACT_365F"),
        dal.CollateralType_OIS(),
    )
    builder.instruments_ = [
        dal.Deposit_New(
            today,
            today,
            dal.Date_(2028, 1, 2),
            0.03,
            index,
        )
    ]
    builder.knotDates_ = [
        today,
        dal.Date_(2027, 1, 2),
        dal.Date_(2028, 1, 2),
    ]

    resolved = dal.ResolveCurveCalibrationInitialGuess(builder.Build())

    assert resolved == [-0.03, -0.06]  # nosec B101
