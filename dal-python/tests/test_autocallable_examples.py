import importlib.util
from pathlib import Path
import sys

import dal
import pytest


EXAMPLES = Path(__file__).resolve().parents[1] / "examples" / "autocallables"
SPEC = importlib.util.spec_from_file_location("autocallable_python_examples", EXAMPLES / "common.py")
examples = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = examples
SPEC.loader.exec_module(examples)


def price_path(case, path, second_path=None):
    dates, events = examples.build_events(case)
    scripts = [events[0]]
    for i, event in enumerate(events[1:]):
        event = event.replace("SPOT()", str(path[i]))
        event = event.replace("FIX(EQ[A])", str(100.0 * path[i]))
        event = event.replace("FIX(EQ[B])", str(120.0 * (second_path or path)[i]))
        scripts.append(event)
    dal.EvaluationDate_Set(dal.Date_(2026, 1, 2))
    product = dal.Product_New([dal.Date_(d.year, d.month, d.day) for d in dates], scripts)
    model = dal.BSModelData_New(1.0, 0.0, 0.0, 0.0)
    return dal.MonteCarlo_Value(product, model, 1, "sobol", False, False)["PV"]


def test_snowball_knockout_and_knockin_recovery():
    case = examples.CASES["classic_snowball"]
    assert price_path(case, [1.0] * 12) == pytest.approx(1.0 + 0.12 * 31 / 365)
    assert price_path(case, [0.9] * 12) == pytest.approx(1.12)
    assert price_path(case, [0.6] + [0.9] * 11) == pytest.approx(0.9)
    assert price_path(case, [0.9] * 11 + [0.6]) == pytest.approx(0.6)


def test_lockout_and_stepdown_change_call_date():
    assert price_path(examples.CASES["lockout_snowball"], [1.0] * 12) == pytest.approx(1.0 + 0.12 * 181 / 365)
    assert price_path(examples.CASES["stepdown_snowball"], [0.99] * 12) == pytest.approx(1.0 + 0.12 * 151 / 365)


def test_phoenix_coupon_memory_and_terminal_barrier():
    path = [0.7] + [0.9] * 11
    assert price_path(examples.CASES["phoenix"], path) == pytest.approx(1.0 + 0.12 * 334 / 365)
    assert price_path(examples.CASES["memory_phoenix"], path) == pytest.approx(1.12)
    assert price_path(examples.CASES["memory_phoenix"], [0.7] * 12) == pytest.approx(1.0)
    assert price_path(examples.CASES["phoenix"], [0.6] + [0.9] * 11) > 1.0
    assert price_path(examples.CASES["phoenix"], [0.6] * 12) == pytest.approx(0.6)


def test_airbag_protection_and_upside_participation():
    assert price_path(examples.CASES["airbag_snowball"], [0.5] * 12) == pytest.approx(0.5 / 0.65)
    assert price_path(examples.CASES["capital_protected"], [0.5] * 12) == pytest.approx(1.04)
    assert price_path(examples.CASES["upside_participation"], [1.1] * 12) == pytest.approx(1.16)


def test_worst_of_uses_both_normalized_assets():
    case = examples.CASES["worst_of_phoenix"]
    assert price_path(case, [1.2] * 12, [0.6] * 12) == pytest.approx(0.6)
    assert price_path(case, [0.6] * 12, [1.2] * 12) == pytest.approx(0.6)
    assert price_path(case, [1.0] * 12, [1.0] * 12) == pytest.approx(1.0 + 0.12 * 31 / 365)


def test_memory_paid_once_on_call_and_forfeited_at_maturity():
    case = examples.CASES["memory_phoenix"]
    assert price_path(case, [0.7, 1.0] + [0.5] * 10) == pytest.approx(1.0 + 0.12 * 59 / 365)
    assert price_path(case, [0.9] + [0.7] * 11) == pytest.approx(1.0 + 0.12 * 31 / 365)


def test_barrier_equalities_and_final_observation():
    assert price_path(examples.CASES["classic_snowball"], [0.65] * 12) == pytest.approx(1.12)
    assert price_path(examples.CASES["phoenix"], [0.8] * 12) == pytest.approx(1.12)
    assert price_path(examples.CASES["phoenix"], [0.65] * 12) == pytest.approx(1.0)
    assert price_path(examples.CASES["classic_snowball"], [0.9] * 11 + [1.0]) == pytest.approx(1.12)
    assert price_path(examples.CASES["memory_phoenix"], [0.7] * 11 + [1.0]) == pytest.approx(1.12)
    assert price_path(examples.CASES["stepdown_memory_phoenix"], [0.99] * 12) == pytest.approx(1.0 + 0.12 * 151 / 365)


@pytest.mark.parametrize("case", examples.CASES.values(), ids=lambda case: case.slug)
def test_ten_examples_run_with_plain_and_aad(case):
    result = examples.value(case, 256)
    assert result["plain"]["PV"] > 0.0
    assert result["aad"]["PV"] > 0.0
