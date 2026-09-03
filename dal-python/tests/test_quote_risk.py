"""Public Python quote-risk contracts."""

import gc
import math

import dal
import pytest


def _single_quote_risk_inputs(*, compute_inverse=True):
    today = dal.Date_(2025, 6, 20)
    spot = today.AddDays(2)
    maturities = [spot.AddDays(years * 365) for years in (2, 5, 10)]
    fixed_leg = dal.RateLegConvention_New(dal.PeriodLength_New("6M"), dal.DayBasis_New("ACT_365F"))
    float_leg = dal.RateLegConvention_New(dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360"))
    overnight_index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("12M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS()
    )
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_365F"), dal.CollateralType_OIS()
    )
    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = today
    builder.ccy_ = dal.String_("USD")
    builder.curveName_ = dal.String_("python_quote_risk")
    builder.calibrateDiscountCurve_ = True
    builder.initialGuess_ = 0.04
    builder.instruments_ = [
        dal.OISSwap_New(today, spot, maturity, 0.04, fixed_leg, overnight_index, float_leg) for maturity in maturities
    ]
    builder.knotDates_ = maturities
    spec = builder.Build()
    options = dal.CurveCalibrationOptions_()
    options.compute_eff_jacobian_inverse = compute_inverse
    calibrated = dal.CalibrateSingleCurve(spec, options)
    fixings = dal.MarketFixingSnapshot_New({})
    market = dal.RatePricingMarket_(
        valuation_time=dal.DateTime_(today, 9, 0),
        result_currency="USD",
        curve_components={"discount": calibrated.curve_},
        fixings=fixings,
    )
    config = dal.RateQuoteRiskProvenanceConfig_(
        calibration_id="python-calibration",
        component_key_by_parameter_block={"python_quote_risk": "discount"},
    )
    provenance = dal.BuildSingleCurveQuoteRiskProvenance(
        spec=spec,
        result=calibrated,
        options=options,
        bound_market=market,
        config=config,
    )
    terms = dal.DepositTradeTerms_(
        notional=1_000_000.0,
        contract_rate=0.022,
        lend=True,
        index=index,
        discount_component_key="discount",
    )
    trade = dal.RateTradeDefinition_(
        instrument_id="python-deposit",
        instrument_type=dal.RateInstrumentType.DEPOSIT,
        trade_date=today,
        start_date=today,
        maturity_date=maturities[0],
        currency="USD",
        terms=terms,
    )
    return spec, options, calibrated, fixings, market, config, provenance, trade


def _run_with_quote_risk_gil_heartbeat(operation):
    import sys
    import time
    import threading

    started = threading.Event()
    ready = threading.Event()
    stopped = threading.Event()
    heartbeat_count = [0]
    heartbeat_errors = []

    def heartbeat() -> None:
        # Failures inside the worker would otherwise surface only as a zero heartbeat
        # count on the main thread; capture and re-raise them where the test asserts.
        ready.set()
        try:
            assert started.wait(timeout=5.0)  # nosec B101
            while not stopped.is_set():
                heartbeat_count[0] += 1
                time.sleep(0)
        except BaseException as error:
            heartbeat_errors.append(error)

    previous_interval = sys.getswitchinterval()
    sys.setswitchinterval(1.0)
    try:
        # Non-daemon by design: `stopped` is always set in the finally below, so the
        # worker cannot outlive the test even when an assertion aborts the operation.
        thread = threading.Thread(target=heartbeat)
        thread.start()
        assert ready.wait(timeout=5.0)  # nosec B101
        dal._dal._QuoteRiskGilBarrier_EnableForTesting(75)
        started.set()
        result = operation()
    finally:
        stopped.set()
        sys.setswitchinterval(previous_interval)
        thread.join(timeout=5.0)
    assert not thread.is_alive()  # nosec B101
    if heartbeat_errors:
        raise heartbeat_errors[0]
    assert heartbeat_count[0] > 0  # nosec B101
    return result


def test_quote_risk_factories_and_aggregate_are_keyword_only():
    signatures = {
        "BuildSingleCurveQuoteRiskProvenance": ("spec", "result", "options", "bound_market", "config"),
        "BuildJointXccyQuoteRiskProvenance": ("spec", "result", "options", "bound_market", "config"),
        "BuildStagedXccyBasisQuoteRiskProvenance": ("spec", "result", "options", "bound_market", "config"),
        "AggregateRatePortfolioQuoteRisk": ("trades", "market", "provenances"),
    }

    for name, arguments in signatures.items():
        signature = getattr(dal, name).__doc__.splitlines()[0]
        assert signature.startswith(f"{name}(*, ")  # nosec B101
        positions = []
        for argument in arguments:
            assert f"{argument}: " in signature, f"{name} is missing argument {argument}"  # nosec B101
            positions.append(signature.index(f"{argument}: "))
        assert positions == sorted(positions)  # nosec B101

    assert not hasattr(dal, "BuildMultiCurveQuoteRiskProvenance")  # nosec B101
    assert not hasattr(dal, "BuildJointMultiCurveQuoteRiskProvenance")  # nosec B101

    with pytest.raises(TypeError):
        dal.RateQuoteRiskProvenanceConfig_("calibration", {"curve": "discount"})


def test_single_curve_quote_risk_exposes_immutable_provenance_and_unit_bearing_results():
    _, _, _, _, market, config, provenance, trade = _single_quote_risk_inputs()

    assert config.calibration_id == "python-calibration"  # nosec B101
    assert config.component_key_by_parameter_block == {"python_quote_risk": "discount"}  # nosec B101
    assert provenance.kind == "SINGLE_CURVE"  # nosec B101
    assert provenance.available is True  # nosec B101
    assert provenance.reason == ""  # nosec B101
    assert provenance.axis.scheme == dal.RateQuoteRiskAxisFingerprintScheme()  # nosec B101
    assert provenance.state.scheme == dal.RateQuoteRiskStateFingerprintScheme()  # nosec B101
    assert provenance.axis.fingerprint.startswith("sha256:")  # nosec B101
    assert provenance.state.fingerprint.startswith("sha256:")  # nosec B101
    assert len(provenance.axis.parameter_ranges) == 1  # nosec B101
    assert len(provenance.axis.residual_ranges) == 1  # nosec B101
    assert all(quote.unit == "DECIMAL_QUOTE" for quote in provenance.axis.quotes)  # nosec B101

    aggregate = dal.AggregateRatePortfolioQuoteRisk(trades=[trade], market=market, provenances=[provenance])

    assert aggregate.policy == "UnconvertedByActualPvCcy"  # nosec B101
    assert set(aggregate.pv_by_actual_pv_ccy) == {"USD"}  # nosec B101
    assert aggregate.provenance_failures == ()  # nosec B101
    assert len(aggregate.meta) == 1  # nosec B101
    assert aggregate.meta[0].eligible is True  # nosec B101
    assert len(aggregate.buckets) == len(provenance.axis.quotes)  # nosec B101
    for bucket in aggregate.buckets:
        assert bucket.calibration_id == provenance.calibration_id  # nosec B101
        assert bucket.axis_fingerprint == provenance.axis.fingerprint  # nosec B101
        assert bucket.actual_pv_ccy == "USD"  # nosec B101
        assert math.isfinite(bucket.d_pv_d_decimal_quote)  # nosec B101
        assert bucket.dv01 == pytest.approx(bucket.d_pv_d_decimal_quote * 1.0e-4, abs=1.0e-12)  # nosec B101

    original_inverse = provenance.effective_inverse[0, 0]
    detached_inverse = provenance.effective_inverse
    detached_inverse[0, 0] = original_inverse + 1.0
    assert provenance.effective_inverse[0, 0] == original_inverse  # nosec B101
    with pytest.raises(AttributeError):
        config.calibration_id = "changed"
    with pytest.raises(AttributeError):
        provenance.available = False
    with pytest.raises(AttributeError):
        provenance.axis.fingerprint = "changed"
    with pytest.raises(AttributeError):
        aggregate.buckets[0].dv01 = 0.0


def test_single_curve_quote_risk_reports_inverse_unavailable_without_buckets():
    _, _, _, _, market, _, provenance, trade = _single_quote_risk_inputs(compute_inverse=False)

    assert provenance.available is False  # nosec B101
    assert provenance.reason == "QUOTE_RISK_INVERSE_NOT_REQUESTED"  # nosec B101
    assert provenance.effective_inverse.rows() == 0  # nosec B101
    result = dal.AggregateRatePortfolioQuoteRisk(trades=[trade], market=market, provenances=[provenance])
    assert result.buckets == ()  # nosec B101
    assert result.meta == ()  # nosec B101
    assert len(result.provenance_failures) == 1  # nosec B101
    assert result.provenance_failures[0].reason == "QUOTE_RISK_INVERSE_NOT_REQUESTED"  # nosec B101


def test_quote_risk_provenance_survives_intermediate_result_and_market_gc():
    _, _, calibrated, fixings, market, _, provenance, trade = _single_quote_risk_inputs()
    curve = calibrated.curve_
    fingerprint = provenance.state.fingerprint
    del calibrated
    del market
    gc.collect()

    replacement_market = dal.RatePricingMarket_(
        valuation_time=dal.DateTime_(dal.Date_(2025, 6, 20), 9, 0),
        result_currency="USD",
        curve_components={"discount": curve},
        fixings=fixings,
    )
    result = dal.AggregateRatePortfolioQuoteRisk(trades=[trade], market=replacement_market, provenances=[provenance])
    assert provenance.state.fingerprint == fingerprint  # nosec B101
    assert len(result.buckets) == 3  # nosec B101


def test_quote_risk_aggregate_and_single_factory_release_gil_for_the_complete_native_operation():
    spec, options, calibrated, _, market, config, provenance, trade = _single_quote_risk_inputs()
    _run_with_quote_risk_gil_heartbeat(
        lambda: dal.AggregateRatePortfolioQuoteRisk(trades=[trade], market=market, provenances=[provenance])
    )
    _run_with_quote_risk_gil_heartbeat(
        lambda: dal.BuildSingleCurveQuoteRiskProvenance(
            spec=spec,
            result=calibrated,
            options=options,
            bound_market=market,
            config=config,
        )
    )


def test_joint_and_staged_xccy_factories_release_gil_and_expose_the_native_domain_shapes():
    from test_xccy_calibration import _make_xccy_spec, _today as staged_today
    from test_xccy_joint import _joint_spec, _today as joint_today

    joint_spec = _joint_spec()
    joint_options = dal.JointXccyCalibrationOptions_()
    joint_result = dal.CalibrateJointXccyMarket(joint_spec, joint_options)
    joint_curves = {
        "domestic:usd_ois": next(iter(joint_result.domestic_curve_block.discount_curves.values())),
        "foreign:eur_ois": next(iter(joint_result.foreign_curve_block.discount_curves.values())),
        "basis:usd_eur_basis": joint_result.basis_curve,
    }
    joint_bindings = {item.name: f"joint-component-{index}" for index, item in enumerate(joint_result.parameter_ranges)}
    joint_components = {joint_bindings[name]: curve for name, curve in joint_curves.items()}
    joint_fixings = joint_result.fixings
    joint_xccy_market = dal.CrossCurrencyMarket_New(
        domestic_block=joint_result.domestic_curve_block,
        foreign_block=joint_result.foreign_curve_block,
        fx_spot=1.10,
        valuation_time=dal.DateTime_(joint_today(), 0, 0),
        collateral_currency="USD",
        fixings=joint_fixings,
        basis_curve=joint_result.basis_curve,
    )
    joint_market = dal.RatePricingMarket_(
        valuation_time=dal.DateTime_(joint_today(), 0, 0),
        result_currency="USD",
        curve_components=joint_components,
        xccy_market=joint_xccy_market,
        fixings=joint_fixings,
    )
    joint_provenance = _run_with_quote_risk_gil_heartbeat(
        lambda: dal.BuildJointXccyQuoteRiskProvenance(
            spec=joint_spec,
            result=joint_result,
            options=joint_options,
            bound_market=joint_market,
            config=dal.RateQuoteRiskProvenanceConfig_(
                calibration_id="python-joint-xccy",
                component_key_by_parameter_block=joint_bindings,
            ),
        )
    )

    assert joint_provenance.available is True  # nosec B101
    assert joint_provenance.kind == "JOINT_XCCY"  # nosec B101
    assert tuple(item.block_key for item in joint_provenance.axis.parameter_ranges) == tuple(  # nosec B101
        item.name for item in joint_result.parameter_ranges
    )
    assert tuple(item.block_key for item in joint_provenance.axis.residual_ranges) == tuple(  # nosec B101
        item.name for item in joint_result.residual_ranges
    )

    staged_spec, _ = _make_xccy_spec(dal.CurveSolveMode.EXACT, years=(2,))
    staged_options = dal.CrossCurrencyCalibrationOptions_()
    staged_result = dal.CalibrateXccyMarket(staged_spec, staged_options)
    staged_market = dal.RatePricingMarket_(
        valuation_time=dal.DateTime_(staged_today(), 0, 0),
        result_currency="USD",
        curve_components={"staged-basis": staged_result.basis_curve},
        xccy_market=staged_result.market,
    )
    staged_provenance = _run_with_quote_risk_gil_heartbeat(
        lambda: dal.BuildStagedXccyBasisQuoteRiskProvenance(
            spec=staged_spec,
            result=staged_result,
            options=staged_options,
            bound_market=staged_market,
            config=dal.RateQuoteRiskProvenanceConfig_(
                calibration_id="python-staged-xccy",
                component_key_by_parameter_block={"basis:xccy_basis_USD": "staged-basis"},
            ),
        )
    )

    assert staged_provenance.available is True  # nosec B101
    assert staged_provenance.kind == "STAGED_XCCY_BASIS"  # nosec B101
    assert len(staged_provenance.axis.parameter_ranges) == 1  # nosec B101
    assert len(staged_provenance.axis.residual_ranges) == 1  # nosec B101
    assert staged_provenance.component_key_by_parameter_block == {  # nosec B101
        "basis:xccy_basis_USD": "staged-basis"
    }
