"""Common owning calibration pullbacks and detached Python projections."""

import copy
from concurrent.futures import ThreadPoolExecutor
from enum import IntEnum
import gc
import hashlib
import json
import math
import pickle
import weakref

import dal
import pytest

from test_dupire_risk import flat_calibration, inputs as dupire_inputs
from test_joint_quote_risk import joint_inputs
from test_quote_risk import _run_with_quote_risk_gil_heartbeat, _single_quote_risk_inputs


def captured_single():
    spec, options, result, _, market, config, default, trade = _single_quote_risk_inputs()
    captured_config = dal.RateQuoteRiskProvenanceConfig_(
        calibration_id=config.calibration_id,
        component_key_by_parameter_block=config.component_key_by_parameter_block,
        retain_calibration_record=True,
    )
    captured = dal.BuildSingleCurveQuoteRiskProvenance(
        spec=spec, result=result, options=options, bound_market=market, config=captured_config,
    )
    return config, captured_config, default, captured, market, trade


def test_curve_record_capture_is_owned_optional_and_preserves_original_identity():
    config, captured_config, default, captured, market, trade = captured_single()
    assert config.retain_calibration_record is False
    assert captured_config.retain_calibration_record is True
    assert default.calibration_record == ""
    record = captured.calibration_record
    assert record and json.loads(record)
    assert "sha256:" + hashlib.sha256(record.encode("utf-8")).hexdigest() == captured.state.fingerprint
    assert captured.state.fingerprint == default.state.fingerprint
    assert captured.axis.fingerprint == default.axis.fingerprint
    assert captured.effective_inverse.to_rows() == default.effective_inverse.to_rows()
    assert captured.tolerance == default.tolerance
    boundary = dal.CalibrationPullback_New(captured)
    del config, captured_config, default, captured, market, trade
    gc.collect()
    assert isinstance(boundary.source, dal.RateQuoteRiskProvenance_)
    assert boundary.source.calibration_record == record
    assert boundary.domain == "RATE_CURVE"


def test_common_dupire_matches_typed_result_and_owns_numeric_projections():
    calibration = flat_calibration()
    boundary = dal.CalibrationPullback_New(calibration)
    nodes = dal.DoubleMatrix_(9, 2, -0.25)
    quotes = dal.DoubleMatrix_(3, 2, 0.125)
    parameters = dal.CalibrationParameterAdjoints_New(boundary, nodes)
    direct = dal.CalibrationDirectQuoteAdjoints_New(boundary, quotes)
    reference = dal.DupireQuoteRisk_New(
        calibration, dal.DupireParameterAdjoints_(calibration, nodes),
        direct=dal.DupireDirectQuoteAdjoints_(calibration, quotes),
    )
    nodes[0, 0] = quotes[0, 0] = -999.0
    result = dal.PullbackCalibration(boundary, parameters, direct=direct)
    assert boundary.domain == "DUPIRE"
    assert (boundary.parameter_rows, boundary.parameter_cols) == (9, 2)
    assert (boundary.quote_rows, boundary.quote_cols) == (3, 2)
    assert isinstance(boundary.source, dal.DupireCalibrationSnapshot_)
    assert boundary.source.matches(calibration)
    assert boundary.matches(copy.deepcopy(boundary))
    assert result.calibration.matches(boundary)
    assert result.method == reference.method
    assert result.unit == reference.unit
    assert result.boundary == reference.boundary
    for contribution in ("calibration_adjoints", "direct_adjoints", "total_adjoints"):
        assert getattr(result, contribution).to_rows() == getattr(reference, contribution).to_rows()
    before = result.total_adjoints.to_rows()
    result.total_adjoints[0, 0] = -1000.0
    parameters.adjoints[0, 0] = -1000.0
    direct.adjoints[0, 0] = -1000.0
    boundary.source.inputs.quote_spreads[0, 0] = -1000.0
    assert result.total_adjoints.to_rows() == before
    assert parameters.adjoints[0, 0] == -0.25
    assert direct.adjoints[0, 0] == 0.125
    assert dal.PullbackCalibration(boundary, parameters, direct=direct).total_adjoints.to_rows() == before


def test_sequence_seeds_are_rectangular_owned_numeric_matrices():
    boundary = dal.CalibrationPullback_New(flat_calibration())
    for factory, rows, cols in (
        (dal.CalibrationParameterAdjoints_New, 9, 2),
        (dal.CalibrationDirectQuoteAdjoints_New, 3, 2),
    ):
        values = [[-0.25] * cols for _ in range(rows)]
        seed = factory(boundary, values)
        tuples = factory(boundary, tuple(tuple(row) for row in values))
        values[0][0] = 1000.0
        assert seed.adjoints.to_rows() == tuples.adjoints.to_rows() == [[-0.25] * cols for _ in range(rows)]


class NumericEnum(IntEnum):
    VALUE = 1


@pytest.mark.parametrize("value", [None, 0, 1, 1.0, "true", NumericEnum.VALUE, [], {}])
def test_capture_option_rejects_boolean_coercion(value):
    with pytest.raises(TypeError, match="retain_calibration_record"):
        dal.RateQuoteRiskProvenanceConfig_(
            calibration_id="checked", component_key_by_parameter_block={"curve": "discount"},
            retain_calibration_record=value,
        )


def test_explicit_false_capture_keeps_defaults_and_config_is_readonly():
    config = dal.RateQuoteRiskProvenanceConfig_(
        calibration_id="checked", component_key_by_parameter_block={"curve": "discount"},
        retain_calibration_record=False,
    )
    assert config.retain_calibration_record is False
    with pytest.raises(AttributeError):
        config.retain_calibration_record = True


def captured_generic(*, layered=False, mode=None, inverse=True, calibration_id=None):
    spec, options, result, market, config, trade = joint_inputs(layered=layered, mode=mode, inverse=inverse)
    captured = dal.BuildJointMultiCurveQuoteRiskProvenance(
        spec=spec, result=result, options=options, bound_market=market,
        config=dal.RateQuoteRiskProvenanceConfig_(
            calibration_id=calibration_id or config.calibration_id,
            component_key_by_parameter_block=config.component_key_by_parameter_block,
            retain_calibration_record=True,
        ),
    )
    return captured, market, trade


def captured_xccy(kind, mode):
    from test_xccy_calibration import _make_xccy_spec, _today as staged_today
    from test_xccy_joint import _joint_spec, _today as joint_today

    if kind == "JOINT_XCCY":
        spec = _joint_spec()
        options = dal.JointXccyCalibrationOptions_()
        options.jacobian_mode = mode
        result = dal.CalibrateJointXccyMarket(spec, options)
        curves = {
            "domestic:usd_ois": next(iter(result.domestic_curve_block.discount_curves.values())),
            "foreign:eur_ois": next(iter(result.foreign_curve_block.discount_curves.values())),
            "basis:usd_eur_basis": result.basis_curve,
        }
        bindings = {item.name: f"joint-component-{index}" for index, item in enumerate(result.parameter_ranges)}
        fixings = result.fixings
        xccy = dal.CrossCurrencyMarket_New(
            domestic_block=result.domestic_curve_block, foreign_block=result.foreign_curve_block,
            fx_spot=1.10, valuation_time=dal.DateTime_(joint_today(), 0), collateral_currency="USD",
            fixings=fixings, basis_curve=result.basis_curve,
        )
        market = dal.RatePricingMarket_(
            valuation_time=dal.DateTime_(joint_today(), 0), result_currency="USD",
            curve_components={bindings[name]: curve for name, curve in curves.items()}, xccy_market=xccy, fixings=fixings,
        )
        build = dal.BuildJointXccyQuoteRiskProvenance
    else:
        spec, _ = _make_xccy_spec(dal.CurveSolveMode.EXACT, years=(2,))
        options = dal.CrossCurrencyCalibrationOptions_()
        options.jacobian_mode = mode
        result = dal.CalibrateXccyMarket(spec, options)
        market = dal.RatePricingMarket_(
            valuation_time=dal.DateTime_(staged_today(), 0), result_currency="USD",
            curve_components={"staged-basis": result.basis_curve}, xccy_market=result.market,
        )
        bindings = {"basis:xccy_basis_USD": "staged-basis"}
        build = dal.BuildStagedXccyBasisQuoteRiskProvenance
    return build(
        spec=spec, result=result, options=options, bound_market=market,
        config=dal.RateQuoteRiskProvenanceConfig_(
            calibration_id="python-" + kind, component_key_by_parameter_block=bindings, retain_calibration_record=True,
        ),
    )


def assert_curve_map(provenance):
    boundary = dal.CalibrationPullback_New(provenance)
    parameters = dal.CalibrationParameterAdjoints_New(boundary, [[math.sin(0.3 + row)] for row in range(boundary.parameter_rows)])
    direct = dal.CalibrationDirectQuoteAdjoints_New(boundary, [[-0.125]] * boundary.quote_rows)
    result = dal.PullbackCalibration(boundary, parameters, direct=direct)
    assert boundary.domain == "RATE_CURVE"
    assert (boundary.parameter_cols, boundary.quote_cols) == (1, 1)
    assert boundary.parameter_rows == len(provenance.axis.parameters)
    assert boundary.quote_rows == len(provenance.axis.quotes)
    assert result.method == "RetainedCurveEffectiveInverse"
    assert result.unit == "DECIMAL_QUOTE"
    assert result.boundary == "FrozenCalibrationEffectiveInverse"
    inverse = provenance.effective_inverse
    values = parameters.adjoints
    for quote in range(boundary.quote_rows):
        expected = sum(values[row, 0] * inverse[row, quote] for row in range(boundary.parameter_rows)) / provenance.tolerance
        assert result.calibration_adjoints[quote, 0] == pytest.approx(expected, rel=1e-10, abs=1e-10)
        assert result.direct_adjoints[quote, 0] == -0.125
        assert result.total_adjoints[quote, 0] == result.calibration_adjoints[quote, 0] - 0.125
    assert boundary.source.calibration_record == provenance.calibration_record
    assert result.calibration.source.state.fingerprint == provenance.state.fingerprint


@pytest.mark.parametrize("mode", [dal.CurveJacobianMode.ANALYTIC, dal.CurveJacobianMode.BUMPED])
@pytest.mark.parametrize("layered", [False, True])
def test_generic_curve_mapping_preserves_coupled_axes_and_record(mode, layered):
    assert_curve_map(captured_generic(layered=layered, mode=mode)[0])


@pytest.mark.parametrize("mode", [dal.CurveJacobianMode.ANALYTIC, dal.CurveJacobianMode.BUMPED])
@pytest.mark.parametrize("kind", ["JOINT_XCCY", "STAGED_XCCY_BASIS"])
def test_joint_and_staged_capture_and_mapping_keep_native_coordinates(kind, mode):
    provenance = captured_xccy(kind, mode)
    assert provenance.kind == kind
    assert "sha256:" + hashlib.sha256(provenance.calibration_record.encode()).hexdigest() == provenance.state.fingerprint
    assert_curve_map(provenance)


def test_single_common_mapping_matches_actual_legacy_portfolio_risk():
    _, _, _, provenance, market, trade = captured_single()
    boundary = dal.CalibrationPullback_New(provenance)
    cells = dal.RateTradeNodeSensitivitiesBatch(trades=[trade], market=market, component_keys=["discount"])
    assert len(cells) == 1 and cells[0].result.eligible
    gradient = [[value] for value in cells[0].result.gradient]
    result = dal.PullbackCalibration(boundary, dal.CalibrationParameterAdjoints_New(boundary, gradient))
    legacy = dal.AggregateRatePortfolioQuoteRisk(trades=[trade], market=market, provenances=[provenance])
    assert not legacy.provenance_failures
    assert result.total_adjoints.to_rows() == [[bucket.d_pv_d_decimal_quote] for bucket in legacy.buckets]
    assert [row[0] * 1e-4 for row in result.total_adjoints.to_rows()] == [bucket.dv01 for bucket in legacy.buckets]


@pytest.mark.parametrize("factory,rows", [("CalibrationParameterAdjoints_New", 9), ("CalibrationDirectQuoteAdjoints_New", 3)])
@pytest.mark.parametrize("value", [True, NumericEnum.VALUE, dal.CurveJacobianMode.ANALYTIC, "0.2", None, float("nan"), float("inf"), 10**1000])
def test_numeric_sequence_cells_reject_coercion_nonfinite_and_overflow(factory, rows, value):
    boundary = dal.CalibrationPullback_New(flat_calibration())
    cells = [[0.0, 0.0] for _ in range(rows)]
    cells[1][1] = value
    with pytest.raises((TypeError, RuntimeError, ValueError), match="adjoints; row=1; column=1"):
        getattr(dal, factory)(boundary, cells)
    good = getattr(dal, factory)(boundary, [[0.0, 0.0] for _ in range(rows)])
    assert good.adjoints.to_rows() == [[0.0, 0.0] for _ in range(rows)]


@pytest.mark.parametrize("value", [None, True, "matrix", 1, [], [0.0] * 9, [[0.0, 0.0]] * 8, [[0.0]] * 9])
def test_matrix_container_and_shape_failures_are_explicit(value):
    boundary = dal.CalibrationPullback_New(flat_calibration())
    with pytest.raises((TypeError, RuntimeError), match="adjoints"):
        dal.CalibrationParameterAdjoints_New(boundary, value)


def test_common_input_types_identity_and_direct_domain_failures_recover():
    calibration = flat_calibration()
    boundary = dal.CalibrationPullback_New(calibration)
    seeds = dal.CalibrationParameterAdjoints_New(boundary, dal.DoubleMatrix_(9, 2, 0.25))
    before = dal.PullbackCalibration(boundary, seeds).total_adjoints.to_rows()
    for value in (None, True, "source", calibration):
        with pytest.raises(TypeError, match="calibration"):
            dal.PullbackCalibration(value, seeds)
    for value in (None, True, "seeds", dal.DupireParameterAdjoints_(calibration, dal.DoubleMatrix_(9, 2))):
        with pytest.raises(TypeError, match="parameter_adjoints"):
            dal.PullbackCalibration(boundary, value)
    for value in (True, "direct", seeds):
        with pytest.raises(TypeError, match="direct"):
            dal.PullbackCalibration(boundary, seeds, direct=value)
    curve = dal.CalibrationPullback_New(captured_generic()[0])
    direct = dal.CalibrationDirectQuoteAdjoints_New(curve, [[0.0]] * curve.quote_rows)
    with pytest.raises(RuntimeError, match="CalibrationSnapshotMismatch"):
        dal.PullbackCalibration(boundary, seeds, direct=direct)
    assert dal.PullbackCalibration(boundary, seeds).total_adjoints.to_rows() == before


def test_curve_full_identity_rejects_case_only_id_even_with_equal_hashes_and_record():
    original = captured_generic()[0]
    changed = captured_generic(calibration_id=original.calibration_id.upper())[0]
    assert original.state.fingerprint == changed.state.fingerprint
    assert original.axis.fingerprint == changed.axis.fingerprint
    assert original.calibration_record == changed.calibration_record
    boundary, other = dal.CalibrationPullback_New(original), dal.CalibrationPullback_New(changed)
    assert not boundary.matches(other)
    seed = dal.CalibrationParameterAdjoints_New(other, [[0.0]] * other.parameter_rows)
    with pytest.raises(RuntimeError, match="CalibrationSnapshotMismatch"):
        dal.PullbackCalibration(boundary, seed)
    good = dal.CalibrationParameterAdjoints_New(boundary, [[0.0]] * boundary.parameter_rows)
    direct = dal.CalibrationDirectQuoteAdjoints_New(other, [[0.0]] * other.quote_rows)
    with pytest.raises(RuntimeError, match="CalibrationSnapshotMismatch"):
        dal.PullbackCalibration(boundary, good, direct=direct)


def test_missing_record_and_unavailable_inverse_keep_original_reasons():
    default = _single_quote_risk_inputs()[6]
    with pytest.raises(RuntimeError, match="QUOTE_RISK_CALIBRATION_RECORD_NOT_RETAINED"):
        dal.CalibrationPullback_New(default)
    unavailable = captured_generic(inverse=False)[0]
    assert not unavailable.available
    with pytest.raises(RuntimeError, match=unavailable.reason):
        dal.CalibrationPullback_New(unavailable)


def test_dupire_direct_identity_survives_an_equal_quote_axis_under_different_base():
    original = flat_calibration()
    changed = dal.DupireCalibration_New(dal.BSModelData_New(100, 0.25, 0.05, 0.02), dupire_inputs())
    boundary, other = dal.CalibrationPullback_New(original), dal.CalibrationPullback_New(changed)
    assert not boundary.matches(other)
    seeds = dal.CalibrationParameterAdjoints_New(boundary, [[0.25, 0.25]] * 9)
    direct = dal.CalibrationDirectQuoteAdjoints_New(other, [[-0.125, -0.125]] * 3)
    combined = dal.PullbackCalibration(boundary, seeds, direct=direct)
    reference = dal.DupireQuoteRisk_New(
        original, dal.DupireParameterAdjoints_(original, seeds.adjoints),
        direct=dal.DupireDirectQuoteAdjoints_(changed, direct.adjoints),
    )
    assert combined.total_adjoints.to_rows() == reference.total_adjoints.to_rows()
    with pytest.raises(RuntimeError, match="CalibrationSnapshotMismatch"):
        dal.PullbackCalibration(boundary, dal.CalibrationParameterAdjoints_New(other, seeds.adjoints))


def test_common_values_copy_detach_and_reject_mutation_and_archive_serialization():
    boundary = dal.CalibrationPullback_New(flat_calibration())
    seed = dal.CalibrationParameterAdjoints_New(boundary, [[0.25, 0.25]] * 9)
    direct = dal.CalibrationDirectQuoteAdjoints_New(boundary, [[-0.125, -0.125]] * 3)
    result = dal.PullbackCalibration(boundary, seed, direct=direct)
    for value, field in ((boundary, "domain"), (seed, "adjoints"), (direct, "calibration"), (result, "total_adjoints")):
        assert type(copy.copy(value)) is type(value)
        assert type(copy.deepcopy(value)) is type(value)
        with pytest.raises(AttributeError):
            setattr(value, field, None)
        with pytest.raises(TypeError):
            pickle.dumps(value)
        with pytest.raises(TypeError, match="Storable"):
            dal._dal._StorableToJson(value)


@pytest.mark.parametrize("mode", [dal.CurveJacobianMode.ANALYTIC, dal.CurveJacobianMode.BUMPED])
def test_plain_generic_common_mapping_matches_actual_trade_risk(mode):
    provenance, market, trade = captured_generic(mode=mode)
    boundary = dal.CalibrationPullback_New(provenance)
    cells = dal.RateTradeNodeSensitivitiesBatch(
        trades=[trade], market=market, component_keys=["discount", "forward"],
    )
    assert len(cells) == 2 and all(cell.result.eligible for cell in cells)
    gradients = {cell.component_key: cell.result.gradient for cell in cells}
    seeds = [[gradients[provenance.component_key_by_parameter_block[p.block_key]][p.block_ordinal]]
             for p in provenance.axis.parameters]
    common = dal.PullbackCalibration(boundary, dal.CalibrationParameterAdjoints_New(boundary, seeds))
    legacy = dal.AggregateRatePortfolioQuoteRisk(trades=[trade], market=market, provenances=[provenance])
    assert not legacy.provenance_failures
    assert common.total_adjoints.to_rows() == [[bucket.d_pv_d_decimal_quote] for bucket in legacy.buckets]


@pytest.mark.parametrize("curve", [False, True])
def test_zero_and_direct_only_contributions_preserve_each_quote(curve):
    boundary = dal.CalibrationPullback_New(captured_single()[3] if curve else flat_calibration())
    seeds = dal.CalibrationParameterAdjoints_New(
        boundary, [[0.0] * boundary.parameter_cols for _ in range(boundary.parameter_rows)],
    )
    quotes = [[(-1.0)**(row + col) * (1 + row + col) / 8
               for col in range(boundary.quote_cols)] for row in range(boundary.quote_rows)]
    zero = dal.PullbackCalibration(boundary, seeds)
    assert zero.total_adjoints.to_rows() == [[0.0] * boundary.quote_cols for _ in range(boundary.quote_rows)]
    direct = dal.CalibrationDirectQuoteAdjoints_New(boundary, quotes)
    result = dal.PullbackCalibration(boundary, seeds, direct=direct)
    assert result.calibration_adjoints.to_rows() == zero.total_adjoints.to_rows()
    assert result.direct_adjoints.to_rows() == result.total_adjoints.to_rows() == quotes


@pytest.mark.parametrize("curve", [False, True])
def test_finite_seeds_that_overflow_native_mapping_fail_and_recover(curve):
    boundary = dal.CalibrationPullback_New(captured_single()[3] if curve else flat_calibration())
    bad = dal.CalibrationParameterAdjoints_New(
        boundary, [[1e308] * boundary.parameter_cols for _ in range(boundary.parameter_rows)],
    )
    with pytest.raises(RuntimeError, match="InvalidCalibrationPullback"):
        dal.PullbackCalibration(boundary, bad)
    good = dal.CalibrationParameterAdjoints_New(
        boundary, [[0.25] * boundary.parameter_cols for _ in range(boundary.parameter_rows)],
    )
    assert all(math.isfinite(value) for row in dal.PullbackCalibration(boundary, good).total_adjoints.to_rows() for value in row)


@pytest.mark.parametrize("factory,rows,role", [
    ("CalibrationParameterAdjoints_New", 9, "parameters"),
    ("CalibrationDirectQuoteAdjoints_New", 3, "directQuotes"),
])
def test_typed_matrix_shapes_and_nonfinite_cells_keep_native_context(factory, rows, role):
    boundary = dal.CalibrationPullback_New(flat_calibration())
    for matrix in (dal.DoubleMatrix_(rows - 1, 2), dal.DoubleMatrix_(rows, 1)):
        with pytest.raises(RuntimeError, match="field=" + role):
            getattr(dal, factory)(boundary, matrix)
    for value in (float("nan"), float("inf"), -float("inf")):
        matrix = dal.DoubleMatrix_(rows, 2)
        matrix[1, 1] = value
        with pytest.raises(RuntimeError, match="field=" + role + "; row=1; column=1"):
            getattr(dal, factory)(boundary, matrix)
    ragged = [[0.0, 0.0] for _ in range(rows)]
    ragged[1].pop()
    with pytest.raises(RuntimeError, match="adjoints; row=1"):
        getattr(dal, factory)(boundary, ragged)


def test_required_factory_arguments_and_direct_keyword_are_explicit():
    boundary = dal.CalibrationPullback_New(flat_calibration())
    seed = dal.CalibrationParameterAdjoints_New(boundary, [[0.0, 0.0]] * 9)
    for value in (None, True, "record", boundary, dal.DoubleMatrix_(9, 2)):
        with pytest.raises(TypeError, match="CalibrationPullback_New; calibration"):
            dal.CalibrationPullback_New(value)
    for factory in (dal.CalibrationParameterAdjoints_New, dal.CalibrationDirectQuoteAdjoints_New):
        with pytest.raises(TypeError, match="calibration"):
            factory(None, [[0.0]])
        with pytest.raises(TypeError):
            factory(boundary)
    with pytest.raises(TypeError):
        dal.CalibrationPullback_New()
    with pytest.raises(TypeError):
        dal.PullbackCalibration(boundary, seed, None)


def test_python_ivs_is_frozen_before_mapping_and_can_be_collected():
    class CustomIVS(dal.IVS_):
        def __init__(self):
            super().__init__(spot=100.0, rate=0.05, dividend_yield=0.02)
            self.calls = 0
            self.fail = False

        def implied_vol(self, strike, maturity):
            self.calls += 1
            if self.fail:
                raise ValueError("common mapping invoked the frozen callback")
            return 0.2

    base = CustomIVS()
    calibration = dal.DupireCalibration_New(base, dupire_inputs())
    boundary = dal.CalibrationPullback_New(calibration)
    seeds = dal.CalibrationParameterAdjoints_New(boundary, [[0.25, 0.25]] * 9)
    before = base.calls
    assert before > 0
    base.fail = True
    expected = dal.PullbackCalibration(boundary, seeds).total_adjoints.to_rows()
    assert base.calls == before
    reference = weakref.ref(base)
    del calibration, base
    gc.collect()
    assert reference() is None
    assert dal.PullbackCalibration(boundary, seeds).total_adjoints.to_rows() == expected


def test_concurrent_mappings_own_sources_and_keep_thread_local_recordings():
    boundaries = [dal.CalibrationPullback_New(flat_calibration()), dal.CalibrationPullback_New(captured_generic()[0])]
    seeds = [dal.CalibrationParameterAdjoints_New(
        boundary, [[0.25] * boundary.parameter_cols for _ in range(boundary.parameter_rows)],
    ) for boundary in boundaries]
    expected = [dal.PullbackCalibration(boundary, seed).total_adjoints.to_rows() for boundary, seed in zip(boundaries, seeds)]

    def operation(index):
        domain = index % 2
        return dal.PullbackCalibration(boundaries[domain], seeds[domain]).total_adjoints.to_rows()

    with ThreadPoolExecutor(max_workers=4) as pool:
        actual = list(pool.map(operation, range(24)))
    assert actual == [expected[index % 2] for index in range(24)]


def test_native_mapping_releases_gil_without_a_test_barrier():
    settings = dal.DupireRiskInputs_(
        quote_strikes=[75.0, 105.0, 135.0], quote_maturities=[0.4, 1.2],
        quote_spreads=dal.DoubleMatrix_(3, 2), inclusion_spots=[60.0, 100.0, 140.0],
        max_spot_spacing=1.0, inclusion_times=[ordinal / 10 for ordinal in range(1, 49)], max_time_spacing=0.1,
    )
    boundary = dal.CalibrationPullback_New(dal.DupireCalibration_New(dal.BSModelData_New(100, 0.2, 0.05, 0.02), settings))
    seeds = dal.CalibrationParameterAdjoints_New(
        boundary, dal.DoubleMatrix_(boundary.parameter_rows, boundary.parameter_cols, 0.25),
    )
    expected = dal.PullbackCalibration(boundary, seeds).total_adjoints.to_rows()
    result = _run_with_quote_risk_gil_heartbeat(lambda: dal.PullbackCalibration(boundary, seeds), barrier=lambda _: None)
    assert result.total_adjoints.to_rows() == expected
