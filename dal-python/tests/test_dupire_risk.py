"""Frozen Dupire quotes and passive Hybrid risk language contracts."""

import copy
from enum import IntEnum
import gc
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import weakref

import dal
import pytest


def inputs(spreads=None):
    return dal.DupireRiskInputs_(
        quote_strikes=[75.0, 105.0, 135.0],
        quote_maturities=[0.4, 1.2],
        quote_spreads=spreads if spreads is not None else dal.DoubleMatrix_(3, 2),
        inclusion_spots=[60.0, 100.0, 140.0],
        max_spot_spacing=10.0,
        inclusion_times=[0.5, 1.0],
        max_time_spacing=0.5,
    )


def flat_calibration():
    base = dal.BSModelData_New(100.0, 0.2, 0.05, 0.02)
    return dal.DupireCalibration_New(base, inputs(), name="frozen")


def test_flat_calibration_and_quote_getters_are_detached():
    calibration = flat_calibration()
    assert calibration.spot == 100.0
    assert calibration.rate == 0.05
    assert calibration.dividend_yield == 0.02
    assert calibration.surface.name == "frozen"
    assert calibration.spots == [60.0, 70.0, 80.0, 90.0, 100.0, 110.0, 120.0, 130.0, 140.0]
    assert calibration.times == [0.5, 1.0]
    assert calibration.matches(copy.deepcopy(calibration))
    seeds = dal.DupireParameterAdjoints_(calibration, dal.DoubleMatrix_(9, 2, 1.0))
    direct = dal.DupireDirectQuoteAdjoints_(calibration, dal.DoubleMatrix_(3, 2, 0.25))
    result = dal.DupireQuoteRisk_New(calibration, seeds, direct=direct)
    assert result.method == "NativeAADCalibrationVJP"
    assert result.unit == "decimal-vol"
    assert result.boundary == "FixedBaseIVSDeterministicCarryFixedGrids"
    assert sum(map(sum, result.calibration_adjoints.to_rows())) == pytest.approx(18.0, abs=3e-5)
    assert result.direct_adjoints.to_rows() == [[0.25, 0.25]] * 3
    for row in range(3):
        for col in range(2):
            assert result.total_adjoints[row, col] == result.calibration_adjoints[row, col] + 0.25
    original = result.total_adjoints.to_rows()
    result.total_adjoints[0, 0] = -999
    seeds.adjoints[0, 0] = -999
    direct.adjoints[0, 0] = -999
    calibration.inputs.quote_spreads[0, 0] = -999
    calibration.vols[0, 0] = -999
    assert result.total_adjoints.to_rows() == original
    assert seeds.adjoints[0, 0] == 1.0
    assert direct.adjoints[0, 0] == 0.25
    assert calibration.inputs.quote_spreads[0, 0] == 0.0
    assert calibration.vols[0, 0] > 0
    with pytest.raises(AttributeError):
        calibration.spot = 200


def test_custom_ivs_sampling_is_frozen_and_callback_failure_recovers():
    class CustomIVS(dal.IVS_):
        def __init__(self):
            super().__init__(spot=100.0, rate=0.05, dividend_yield=0.02)
            self.volatility = 0.2
            self.calls = 0

        def implied_vol(self, strike, maturity):
            assert strike > 0 and maturity > 0
            self.calls += 1
            return self.volatility

    base = CustomIVS()
    calibration = dal.DupireCalibration_New(base, inputs())
    assert calibration.matches(flat_calibration())
    assert base.calls > 0
    before = base.calls
    base.volatility = 0.3
    seeds = dal.DupireParameterAdjoints_(calibration, dal.DoubleMatrix_(9, 2, 1.0))
    risk = dal.DupireQuoteRisk_New(calibration, seeds).total_adjoints.to_rows()
    assert base.calls == before
    reference = weakref.ref(base)
    del base
    gc.collect()
    assert reference() is None
    assert dal.DupireQuoteRisk_New(calibration, seeds).total_adjoints.to_rows() == risk

    class BrokenIVS(CustomIVS):
        def implied_vol(self, strike, maturity):
            raise ValueError("callback failed exactly here")

    with pytest.raises(ValueError, match="callback failed exactly here"):
        dal.DupireCalibration_New(BrokenIVS(), inputs())
    assert dal.DupireQuoteRisk_New(calibration, seeds).total_adjoints.to_rows() == risk
    assert dal.DupireCalibration_New(CustomIVS(), inputs()).matches(calibration)


class NumericEnum(IntEnum):
    VALUE = 1


@pytest.mark.parametrize("value", [True, NumericEnum.VALUE, "0.2", None, 10**1000])
def test_ivs_numeric_settings_reject_coercion_or_overflow(value):
    with pytest.raises((TypeError, RuntimeError), match="IVS_; spot"):
        dal.IVS_(spot=value)


@pytest.mark.parametrize("value", [True, NumericEnum.VALUE, "0.2", None, float("nan")])
def test_custom_ivs_return_is_a_checked_volatility(value):
    class InvalidIVS(dal.IVS_):
        def implied_vol(self, strike, maturity):
            return value

    with pytest.raises((TypeError, RuntimeError), match="implied_vol"):
        dal.DupireCalibration_New(InvalidIVS(spot=100.0), inputs())


@pytest.mark.parametrize(
    "field,value",
    [
        ("quote_strikes", [75.0, True, 135.0]),
        ("quote_maturities", [0.4, NumericEnum.VALUE]),
        ("inclusion_spots", "60,100,140"),
        ("inclusion_times", [None]),
        ("max_spot_spacing", True),
        ("max_time_spacing", "0.5"),
        ("quote_spreads", [[0, 0]] * 3),
    ],
)
def test_config_fields_reject_implicit_conversions(field, value):
    config = dict(
        quote_strikes=[75, 105, 135], quote_maturities=[0.4, 1.2],
        quote_spreads=dal.DoubleMatrix_(3, 2), inclusion_spots=[60, 100, 140],
        max_spot_spacing=10, inclusion_times=[0.5, 1.0], max_time_spacing=0.5,
    )
    config[field] = value
    with pytest.raises(TypeError, match=field):
        dal.DupireRiskInputs_(**config)


@pytest.mark.parametrize("base", [None, True, "flat"])
def test_required_base_rejects_without_coercion(base):
    with pytest.raises(TypeError, match="base"):
        dal.DupireCalibration_New(base, inputs())


def test_config_and_seed_inputs_are_copied_and_identity_is_checked():
    spread = dal.DoubleMatrix_(3, 2)
    config = inputs(spread)
    spread[0, 0] = 0.1
    assert config.quote_spreads[0, 0] == 0.0
    calibration = dal.DupireCalibration_New(dal.BSModelData_New(100, 0.2, 0.05, 0.02), config)
    numeric = dal.DoubleMatrix_(9, 2, 1)
    seeds = dal.DupireParameterAdjoints_(calibration, numeric)
    numeric[0, 0] = -999
    assert seeds.adjoints[0, 0] == 1
    risk = dal.DupireQuoteRisk_New(calibration, seeds)
    changed = dal.DoubleMatrix_(3, 2, 0.001)
    other = dal.DupireCalibration_New(dal.BSModelData_New(100, 0.2, 0.05, 0.02), inputs(changed))
    assert not calibration.matches(other)
    with pytest.raises(RuntimeError, match="DupireSnapshotMismatch"):
        dal.DupireQuoteRisk_New(other, seeds)
    wrong = dal.DupireParameterAdjoints_(calibration, dal.DoubleMatrix_(2, 9))
    with pytest.raises(RuntimeError, match="seed dimensions disagree; field=parameters"):
        dal.DupireQuoteRisk_New(calibration, wrong)
    assert dal.DupireQuoteRisk_New(calibration, seeds).total_adjoints.to_rows() == risk.total_adjoints.to_rows()


@pytest.mark.parametrize("name", [None, True, NumericEnum.VALUE, "embedded\0NUL"])
def test_calibration_name_is_a_checked_string(name):
    with pytest.raises((TypeError, RuntimeError), match="name"):
        dal.DupireCalibration_New(dal.BSModelData_New(100, 0.2, 0.05, 0.02), inputs(), name=name)


def hybrid_model(calibration):
    components = [
        dal.HybridLocalVolEquityData_New("Z_LOCAL", "EQ[LOCAL]", "USD", "F_LOCAL", 100,
                                       calibration.dividend_yield, calibration.surface, max_step=0.25),
        dal.HybridDeterministicRateData_New("00_RATE", "USD", calibration.rate),
        dal.HybridBSEquityData_New("A_OTHER", "EQ[OTHER]", "USD", "F_OTHER", 120, 0.25, 0.01),
    ]
    correlation = dal.HybridConstantCorrelationData_New(
        "correlation", ["F_LOCAL", "F_OTHER"], dal.DoubleMatrix_([[1, 0], [0, 1]]),
    )
    return dal.HybridModelData_New("unsorted", "USD", components, correlation)


def smooth_product(quote=0.0):
    return dal.Product_New(
        ["QUOTE", dal.Date_(2027, 9, 12)],
        [str(quote), "pay PAYS FIX(EQ[LOCAL]) * FIX(EQ[LOCAL]) / 100 + 0.1 * FIX(EQ[OTHER]) + 3 * QUOTE"],
    )


def _assert_hybrid_mapping(calibration, source, selected, result, direct_matrix):
    seeds = dal.DupireParameterAdjoints_FromRisk(selected, calibration, "Z_LOCAL")
    bare = dal.DupireQuoteRisk_New(calibration, seeds)
    assert result.method == "NativeAADThenNativeAADCalibrationVJP"
    assert result.component == "Z_LOCAL"
    assert result.valuation.values == source.values
    assert result.quote_risk.calibration_adjoints.to_rows() == bare.total_adjoints.to_rows()
    assert result.quote_risk.direct_adjoints.to_rows() == direct_matrix.to_rows()
    assert result.valuation.provenance.execution.model_snapshot_json == selected.provenance.execution.model_snapshot_json
    for row in range(9):
        for col in range(2):
            ordinal = 6 + 2 * row + col
            assert seeds.adjoints[row, col] == source.jacobian[0, ordinal]
    retained = result.quote_risk.total_adjoints.to_rows()
    result.quote_risk.total_adjoints[0, 0] = -999
    result.valuation.jacobian[0, 0] = -999
    assert result.quote_risk.total_adjoints.to_rows() == retained
    assert result.valuation.jacobian.to_rows() == selected.jacobian.to_rows()
    # Remove an actual surface coordinate, irrespective of report ordering.


def _assert_hybrid_errors(calibration, selected, direct, simulation):
    missing_ids = [coordinate.id for coordinate in selected.input_axis if coordinate.id != "model:6"]
    missing = dal.MonteCarlo_ValueWithRisk(
        smooth_product(), hybrid_model(calibration), 257,
        request=dal.RiskRequest_(inputs=missing_ids), simulation=simulation,
    )
    with pytest.raises(RuntimeError, match="missing selected surface risk"):
        dal.DupireScriptQuoteRisk_New(missing, calibration, "Z_LOCAL")
    with pytest.raises(RuntimeError, match="component"):
        dal.DupireParameterAdjoints_FromRisk(selected, calibration, "A_OTHER")
    result = dal.DupireScriptQuoteRisk_New(selected, calibration, "Z_LOCAL", direct=direct)
    assert result.valuation.values == selected.values


def _quote_directions():
    directions = [(row, col) for row in range(3) for col in range(2)] + [None]
    for coordinate in directions:
        direction = [[0.0, 0.0] for _ in range(3)]
        if coordinate is None:
            direction = [[0.3, -0.5], [0.7, 0.2], [-0.4, 0.6]]
        else:
            direction[coordinate[0]][coordinate[1]] = 1.0
        yield coordinate, direction


def _quote_price_difference(base, direction, step, simulation):
    prices = []
    for sign in [-1, 1]:
        spread = dal.DoubleMatrix_([[sign * step * weight for weight in row] for row in direction])
        bumped = dal.DupireCalibration_New(base, inputs(spread))
        prices.append(dal.MonteCarlo_ValueWithSettings(
            smooth_product(spread[0, 0]), hybrid_model(bumped), 257, simulation=simulation,
        )["PV"])
    return (prices[1] - prices[0]) / (2 * step)


def _assert_quote_direction(base, coordinate, direction, result, base_kind, compiled):
    adjoint = sum(result.quote_risk.total_adjoints[row, col] * direction[row][col]
                  for row in range(3) for col in range(2))
    price_simulation = dal.MonteCarloSettings_(enable_aad=False, compiled=compiled)
    passes = []
    for step in [2e-4, 1e-4, 5e-5]:
        difference = _quote_price_difference(base, direction, step, price_simulation)
        tolerance = 1e-3 + 1e-3 * max(abs(adjoint), abs(difference))
        passed = abs(adjoint - difference) <= tolerance
        passes.append(passed)
        print(f"PythonHybridQuoteOracle,{base_kind},{compiled},{coordinate},{step:.17g},{adjoint:.17g},{difference:.17g},{passed}")
    assert (passes[0] and passes[1]) or (passes[1] and passes[2])


def _check_hybrid_quote_chain(base_kind, compiled):
    dal.EvaluationDate_Set(dal.Date_(2026, 9, 12))
    base = (dal.BSModelData_New(100, 0.2, 0.05, 0.02) if base_kind == "flat" else
            dal.MertonIVS_(spot=100, vol=0.2, intensity=0.08, average_jump=-0.1, jump_std=0.15))
    calibration = dal.DupireCalibration_New(base, inputs())
    simulation = dal.MonteCarloSettings_(enable_aad=True, compiled=compiled)
    source = dal.MonteCarlo_ValueWithRisk(smooth_product(), hybrid_model(calibration), 257, simulation=simulation)
    ids = [coordinate.id for coordinate in source.input_axis][::-1]
    selected = dal.MonteCarlo_ValueWithRisk(
        smooth_product(), hybrid_model(calibration), 257,
        request=dal.RiskRequest_(inputs=ids, report_factors=[0.01] * len(ids)), simulation=simulation,
    )
    direct_matrix = dal.DoubleMatrix_(3, 2)
    direct_matrix[0, 0] = 3 * math.exp(-calibration.rate)
    direct = dal.DupireDirectQuoteAdjoints_(calibration, direct_matrix)
    result = dal.DupireScriptQuoteRisk_New(selected, calibration, "Z_LOCAL", direct=direct)
    _assert_hybrid_mapping(calibration, source, selected, result, direct_matrix)
    _assert_hybrid_errors(calibration, selected, direct, simulation)
    for coordinate, direction in _quote_directions():
        _assert_quote_direction(base, coordinate, direction, result, base_kind, compiled)


def test_axes_accept_copied_dal_vectors_and_tuple_inputs():
    config = inputs()
    strikes = dal.DoubleVector([75.0, 105.0, 135.0])
    other = dal.DupireRiskInputs_(
        quote_strikes=strikes, quote_maturities=(0.4, 1.2), quote_spreads=dal.DoubleMatrix_(3, 2),
        inclusion_spots=(60, 100, 140), max_spot_spacing=10, inclusion_times=(0.5, 1.0), max_time_spacing=0.5,
    )
    strikes[0] = 80.0
    assert other.quote_strikes == config.quote_strikes
    assert other.quote_maturities == config.quote_maturities
    assert other.inclusion_spots == config.inclusion_spots
    assert other.inclusion_times == config.inclusion_times


@pytest.mark.parametrize("direct", [True, {}, dal.DoubleMatrix_(3, 2)])
def test_optional_direct_requires_a_typed_seed(direct):
    calibration = flat_calibration()
    seed = dal.DupireParameterAdjoints_(calibration, dal.DoubleMatrix_(9, 2))
    with pytest.raises(TypeError, match="direct"):
        dal.DupireQuoteRisk_New(calibration, seed, direct=direct)


@pytest.mark.parametrize(
    "kwargs,field",
    [({"spot": 0}, "spot"), ({"vol": -0.2}, "vol"), ({"intensity": -0.1}, "intensity"),
     ({"jump_std": -0.2}, "jump_std"), ({"average_jump": float("inf")}, "average_jump")],
)
def test_merton_settings_reject_invalid_domains(kwargs, field):
    config = dict(spot=100, vol=0.2, intensity=0.08, average_jump=-0.1, jump_std=0.15)
    config.update(kwargs)
    with pytest.raises(RuntimeError, match=field):
        dal.MertonIVS_(**config)


def test_required_handles_and_keyword_only_options_are_explicit():
    calibration = flat_calibration()
    seeds = dal.DupireParameterAdjoints_(calibration, dal.DoubleMatrix_(9, 2))
    with pytest.raises(TypeError, match="inputs"):
        dal.DupireCalibration_New(dal.BSModelData_New(100, 0.2, 0.05, 0.02), None)
    with pytest.raises(TypeError, match="calibration"):
        dal.DupireQuoteRisk_New(None, seeds)
    with pytest.raises(TypeError, match="parameter_adjoints"):
        dal.DupireQuoteRisk_New(calibration, None)
    with pytest.raises(TypeError, match="adjoints"):
        dal.DupireParameterAdjoints_(calibration, None)
    with pytest.raises(TypeError):
        dal.DupireQuoteRisk_New(calibration, seeds, None)
    with pytest.raises(TypeError):
        dal.DupireCalibration_New(dal.BSModelData_New(100, 0.2, 0.05, 0.02), inputs(), "positional")
    with pytest.raises(TypeError):
        dal.IVS_(100)


def test_flat_convenience_rejects_a_non_bs_model_and_missing_override():
    calibration = flat_calibration()
    with pytest.raises(RuntimeError, match="base must be BSModelData_ or IVS_"):
        dal.DupireCalibration_New(hybrid_model(calibration), inputs())
    with pytest.raises(TypeError, match="override is required"):
        dal.DupireCalibration_New(dal.IVS_(spot=100), inputs())
    assert flat_calibration().matches(calibration)


@pytest.mark.parametrize("base_kind", ["flat", "merton"])
@pytest.mark.parametrize("compiled", [False, True])
def test_complete_hybrid_quote_chain_in_one_worker(base_kind, compiled):
    probe = (
        "import json, runpy, sys; sys.path[:] = json.loads(sys.argv[1]); "
        "scope = runpy.run_path(sys.argv[2]); "
        "scope['_check_hybrid_quote_chain'](sys.argv[3], sys.argv[4] == 'True')"
    )
    completed = subprocess.run(
        [sys.executable, "-S", "-c", probe, json.dumps(sys.path), str(Path(__file__).resolve()), base_kind, str(compiled)],
        env={**os.environ, "DAL_NUM_THREADS": "1"}, capture_output=True, text=True, timeout=60, check=False,
    )
    print(completed.stdout)
    assert completed.returncode == 0, completed.stdout + completed.stderr
