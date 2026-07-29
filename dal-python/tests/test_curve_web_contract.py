"""Additive native surfaces required by the Web curve workbench."""

import dal
import pytest


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


def test_curve_archive_hierarchy_and_bytes_bridge_round_trip_recursive_pwc():
    today = _today()
    knots = [today.AddDays(30), today.AddDays(365), today.AddDays(730)]
    base = dal.DiscountPWC_New("base", "USD", knots, [0.01, 0.02, 0.03])
    spread = dal.DiscountPWC_New(
        "spread",
        "USD",
        knots,
        [0.001, 0.002, 0.003],
        base,
    )

    assert issubclass(dal.YCComponent_, dal.Storable_)  # nosec B101
    assert issubclass(dal.DiscountCurve_, dal.YCComponent_)  # nosec B101
    assert issubclass(dal.YieldCurve_, dal.Storable_)  # nosec B101
    assert issubclass(dal.CurveBlock_, dal.YieldCurve_)  # nosec B101
    assert not issubclass(dal.DiscountCurve_, dal.YieldCurve_)  # nosec B101
    assert spread.type == "DiscountCurve"  # nosec B101
    assert spread.name == "spread"  # nosec B101

    payload = dal._dal._StorableToJson(spread)
    restored = dal._dal._StorableFromJson(payload)

    assert isinstance(payload, bytes)  # nosec B101
    assert isinstance(restored, dal.DiscountPWC_)  # nosec B101
    assert restored.base.name == "base"  # nosec B101
    assert dal._dal._StorableToJson(restored) == payload  # nosec B101


def test_archive_bridge_requires_exact_bytes_and_rejects_embedded_nul():
    today = _today()
    curve = dal.DiscountPWC_New(
        "curve",
        "USD",
        [today.AddDays(30)],
        [0.01],
    )
    payload = dal._dal._StorableToJson(curve)

    with pytest.raises(TypeError):
        dal._dal._StorableFromJson(payload.decode("utf-8"))
    with pytest.raises(RuntimeError, match="ARCHIVE_PAYLOAD_NUL"):
        dal._dal._StorableFromJson(payload + b"\0{}")


def test_archive_bridge_rejects_decoded_nul_and_lone_surrogates():
    prefix = b'{"~type":"DiscountPWC_v1","name":"'
    suffix = (
        b'","ccy":"USD","knotDates":["2026-02-14"],"rightVals":[0.01]}'
    )

    with pytest.raises(RuntimeError, match="ARCHIVE_STRING_NUL"):
        dal._dal._StorableFromJson(prefix + b"bad\\u0000name" + suffix)
    with pytest.raises(RuntimeError, match="ARCHIVE_STRING_NUL"):
        dal._dal._StorableFromJson(
            b'{"~type":"DiscountPWC_v1","bad\\u0000key":"value","name":"curve",'
            b'"ccy":"USD","knotDates":["2026-02-14"],"rightVals":[0.01]}'
        )
    with pytest.raises(RuntimeError, match="ARCHIVE_STRING_INVALID_UNICODE"):
        dal._dal._StorableFromJson(prefix + b"bad\\uD800name" + suffix)
    with pytest.raises(RuntimeError, match="ARCHIVE_STRING_INVALID_UNICODE"):
        dal._dal._StorableFromJson(prefix + b"bad\\uDC00name" + suffix)


def test_archive_bridge_preserves_supplementary_unicode_across_reserialization():
    payload = (
        b'{"~type":"DiscountPWC_v1","name":"curve-\\uD83D\\uDE80",'
        b'"ccy":"USD","knotDates":["2026-02-14"],"rightVals":[0.01]}'
    )

    restored = dal._dal._StorableFromJson(payload)
    serialized = dal._dal._StorableToJson(restored)
    round_trip = dal._dal._StorableFromJson(serialized)

    assert restored.name == "curve-\U0001f680"  # nosec B101
    assert round_trip.name == restored.name  # nosec B101
    assert dal._dal._StorableToJson(round_trip) == serialized  # nosec B101


def test_private_bag_bridge_preserves_storable_roots_and_keys():
    today = _today()
    discount = dal.DiscountPWC_New(
        "discount",
        "USD",
        [today.AddDays(30)],
        [0.01],
    )
    projection = dal.DiscountPWC_New(
        "projection",
        "USD",
        [today.AddDays(90)],
        [0.02],
    )

    bag = dal._dal._BagNew(
        "usd-curves",
        {
            "clab/v1/local/discount/USD/OIS": discount,
            "clab/v1/local/projection/USD/3M": projection,
        },
    )
    contents = dal._dal._BagContents(bag)
    restored = dal._dal._StorableFromJson(dal._dal._StorableToJson(bag))

    assert isinstance(bag, dal.Bag_)  # nosec B101
    assert set(contents) == {  # nosec B101
        "clab/v1/local/discount/USD/OIS",
        "clab/v1/local/projection/USD/3M",
    }
    assert all(isinstance(value, dal.DiscountCurve_) for value in contents.values())  # nosec B101
    assert set(dal._dal._BagContents(restored)) == set(contents)  # nosec B101


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


def test_private_web_fixing_preflight_uses_native_cashflow_schedules():
    start = dal.Date_(2025, 1, 2)
    maturity = dal.Date_(2027, 1, 2)
    valuation_time = dal.DateTime_(dal.Date_(2026, 1, 2), 12, 0)
    leg = dal.RateLegConvention_New(
        dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_365F")
    )
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"),
        dal.DayBasis_New("ACT_365F"),
        dal.CollateralType_OIS(),
    )
    config = dal.CrossCurrencySwapConfig_()
    config.pair = dal.CurrencyPair_New("USD", "EUR")
    config.domestic_notional = 100.0
    config.foreign_notional = 90.0
    config.convention.domestic_leg = leg
    config.convention.foreign_leg = leg
    config.convention.domestic_index = index
    config.convention.foreign_index = index
    config.notional_mode = dal.XccyNotionalMode.FIXED
    config.domestic_rate_fixing.index_name = "USD-SOFR"
    config.domestic_rate_fixing.fixing_hour = 11
    config.domestic_rate_fixing.fixing_minute = 0
    config.foreign_rate_fixing.index_name = "EUR-ESTR"
    config.foreign_rate_fixing.fixing_hour = 11
    config.foreign_rate_fixing.fixing_minute = 0
    instrument = dal.CrossCurrencySwap_New(
        start,
        start,
        maturity,
        0.001,
        config,
    )

    required = dal._dal._RequiredHistoricalXccyFixings(
        [instrument], valuation_time
    )

    assert required  # nosec B101
    assert {item[1] for item in required} == {"USD-SOFR", "EUR-ESTR"}  # nosec B101
    assert all(item[0] == 0 for item in required)  # nosec B101


def test_private_rate_trade_fixing_preflight_uses_native_cashflow_schedules():
    start = dal.Date_(2026, 1, 15)
    maturity = dal.Date_(2026, 4, 15)
    valuation_time = dal.DateTime_(start.AddDays(1), 10, 30)
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"),
        dal.DayBasis_New("ACT_365F"),
        dal.CollateralType_OIS(),
    )
    fixing = dal.FixingIdentity_()
    fixing.index_name = "USD-SOFR"
    fixing.fixing_hour = 11
    fixing.fixing_minute = 0
    terms = dal.FraTradeTerms_(
        notional=1_000_000.0,
        contract_rate=0.03,
        receive_floating=True,
        settle_at_start=False,
        index=index,
        fixing_identity=fixing,
        forecast_component_key="forecast",
        discount_component_key="discount",
    )
    trade = dal.RateTradeDefinition_(
        instrument_id="fra-1",
        instrument_type=dal.RateInstrumentType.FRA,
        trade_date=start,
        start_date=start,
        maturity_date=maturity,
        currency="USD",
        terms=terms,
    )

    required = dal._dal._RequiredHistoricalRateTradeFixings(
        [trade], valuation_time
    )

    assert len(required) == 1  # nosec B101
    assert required[0][0] == 0  # nosec B101
    assert required[0][1] == "USD-SOFR"  # nosec B101
    assert repr(required[0][2]) == "2026-01-15 11:00:00"  # nosec B101


def test_solver_budget_exhaustion_has_a_private_typed_exception():
    today = _today()
    maturity = today.AddDays(365)
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("12M"),
        dal.DayBasis_New("ACT_365F"),
        dal.CollateralType_OIS(),
    )
    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = today
    builder.ccy_ = dal.String_("USD")
    builder.curveName_ = dal.String_("non-converging")
    builder.instruments_ = [
        dal.Deposit_New(today, today, maturity, 0.04, index)
    ]
    builder.knotDates_ = [maturity]
    builder.initialGuess_ = 0.50
    builder.maxEvaluations_ = 1

    with pytest.raises(dal._dal._CalibrationConvergenceError):
        dal.CalibrateSingleCurve(builder.Build())
