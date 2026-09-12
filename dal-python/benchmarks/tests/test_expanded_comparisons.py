"""Option, calibration and explicit capability contracts for comparisons."""

from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from dal_comparisons.scenarios import cases, unsupported_reason


def test_discrete_barrier_oracle_converges_without_mc_sampling():
    from dal_comparisons.option_scenarios import barrier_price

    assert barrier_price(nodes=256) == pytest.approx(barrier_price(nodes=384), abs=1e-8)


def test_new_families_preserve_historical_inventory_and_cover_both_sizes():
    inventory = cases()
    assert len(inventory) == 31
    assert [case["name"] for case in inventory[:13]][-1] == "irs_cold_pv_256"


def test_mc_inventory_covers_both_products_and_operations():
    inventory = cases()
    options = [case for case in inventory if case["operation"].startswith("mc_")]
    assert len(options) == 8
    assert {case["kind"] for case in options} == {"vanilla", "barrier"}
    assert {case["operation"] for case in options} == {"mc_price", "mc_greeks"}


def test_calibration_inventory_covers_solve_structures_and_sizes():
    inventory = cases()
    calibration = [case for case in inventory if case["operation"] == "calibration"]
    assert {case["kind"] for case in calibration} == {
        "single",
        "multi_staged",
        "multi_joint",
        "xccy_staged",
        "xccy_joint",
    }
    assert {case["size"] for case in calibration} == {5, 15}


def test_only_declared_unsupported_cases_can_be_skipped():
    from dal_comparisons.scenarios import unsupported_reason
    from dal_comparisons.worker import measured_case
    from dal_comparisons.evidence import check_row

    case = next(c for c in cases(True) if c["operation"] == "mc_price")
    row = measured_case("rateslib", case)
    assert row["status"] == "unsupported"
    assert row["reason"] == unsupported_reason("rateslib", case)
    assert "samples_ns" not in row and "values" not in row
    check_row(row, case, "rateslib")
    with pytest.raises(ValueError):
        check_row(row, case, "dal")
    row["reason"] = "dependency import failed"
    with pytest.raises(ValueError):
        check_row(row, case, "rateslib")


@pytest.mark.parametrize(
    "kind", ["single", "multi_staged", "multi_joint", "xccy_staged", "xccy_joint"]
)
def test_dal_calibration_matches_all_independent_nodes_and_midpoints(kind):
    from dal_comparisons.suite import prepare

    case = {"name": kind, "operation": "calibration", "kind": kind, "size": 2}
    work = prepare("dal", case)
    result = work.run()
    work.validate(result)
    work.validate(work.run())
    result[-1] *= 1.001
    with pytest.raises(ValueError, match="mismatch"):
        work.validate(result)


@pytest.mark.parametrize("backend", ["quantlib", "rateslib"])
@pytest.mark.parametrize("kind", ["single", "multi_staged", "xccy_staged"])
def test_third_party_calibration_matches_independent_nonflat_curves(backend, kind):
    from dal_comparisons.suite import prepare

    work = prepare(
        backend, {"name": kind, "operation": "calibration", "kind": kind, "size": 2}
    )
    work.validate(work.run())
    work.validate(work.run())


@pytest.mark.parametrize(
    "backend,case",
    [
        (backend, case)
        for case in cases()[13:]
        for backend in ("dal", "quantlib", "rateslib")
        if not unsupported_reason(backend, case)
    ],
    ids=lambda value: value["name"] if isinstance(value, dict) else value,
)
def test_full_option_and_calibration_profiles_validate(backend, case):
    from dal_comparisons.suite import prepare

    work = prepare(backend, case)
    work.validate(work.run())


@pytest.mark.parametrize("backend", ["dal", "quantlib"])
def test_mc_greeks_restart_generators_and_reject_bad_components(backend):
    from dal_comparisons.suite import prepare

    for kind in ("vanilla", "barrier"):
        case = next(
            c
            for c in cases(True)
            if c.get("kind") == kind and c["operation"] == "mc_greeks"
        )
        work = prepare(backend, case)
        values = work.run()
        assert work.run() == pytest.approx(values, abs=1e-12, rel=1e-12)
        for i in range(4):
            bad = values[:]
            bad[i] += 1000
            with pytest.raises(ValueError, match="mismatch"):
                work.validate(bad)


def test_calibration_queries_are_midnight_and_include_all_intervals():
    from dal_comparisons.calibration_scenarios import dates, queries

    nodes, grid = dates(15), queries(15)
    assert len(grid) == 30
    assert all(d.hour == d.minute == d.second == 0 for d in grid)
    assert all(
        left < middle < right
        for left, middle, right in zip(nodes, grid[15:], nodes[1:])
    )


@pytest.mark.parametrize(
    "backend,module_name,entrypoint",
    [
        ("dal", "dal", "CalibrateSingleCurve"),
        ("quantlib", "QuantLib", "PiecewiseLogLinearDiscount"),
        ("rateslib", "rateslib", "Solver"),
    ],
)
def test_staged_calibration_solves_fresh_inside_each_run(
    backend, module_name, entrypoint, monkeypatch
):
    import importlib
    from dal_comparisons.suite import prepare

    module = importlib.import_module(module_name)
    original = getattr(module, entrypoint)
    calls = []

    def counted(*args, **kwargs):
        calls.append(None)
        return original(*args, **kwargs)

    monkeypatch.setattr(module, entrypoint, counted)
    work = prepare(
        backend,
        {
            "name": "fresh",
            "operation": "calibration",
            "kind": "multi_staged",
            "size": 2,
        },
    )
    assert not calls
    work.validate(work.run())
    assert len(calls) == 2
    work.validate(work.run())
    assert len(calls) == 4
