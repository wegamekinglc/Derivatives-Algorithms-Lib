"""Matched exercise grids, held-out LSM calibration and independent references."""

import math

import pytest

from dal_comparisons import exercise_scenarios as inputs


def test_exercise_profiles_keep_training_budget_separate():
    full, smoke = inputs.cases(), inputs.cases(True)
    assert len(full) == 8
    assert {case["kind"] for case in full} == {"bermudan", "american"}
    assert {case["size"] for case in full} == {16384, 65536}
    assert {case["training_paths"] for case in full} == {16384}
    assert {case["training_paths"] for case in smoke} == {2048}
    assert {case["size"] for case in smoke} == {4096}
    assert [case["name"] for case in full] == [case["name"] for case in smoke]


def test_exercise_dates_are_nested_and_include_maturity():
    quarterly = inputs.exercise_days("bermudan")
    weekly = inputs.exercise_days("american")
    assert quarterly == tuple(range(91, 547, 91))
    assert weekly == tuple(range(7, 547, 7))
    assert set(quarterly) <= set(weekly)


def test_lattice_european_limit_and_early_exercise_premium():
    t = 546 / 365
    sd = 0.2 * math.sqrt(t)
    d1 = (0.05 * t + sd * sd / 2) / sd
    cdf = lambda x: math.erfc(-x / math.sqrt(2)) / 2
    european = 100 * (math.exp(-0.05 * t) * cdf(sd - d1) - cdf(-d1))
    assert inputs.tree_price((546,)) == pytest.approx(european, abs=0.002)
    bermudan = inputs.tree_price(inputs.exercise_days("bermudan"))
    american = inputs.tree_price(inputs.exercise_days("american"))
    assert european + 0.5 < bermudan < american


@pytest.mark.parametrize("kind", ["bermudan", "american"])
@pytest.mark.parametrize(
    "spot,vol,rate",
    [
        (100, 0.2, 0.05),
        (99, 0.2, 0.05),
        (101, 0.2, 0.05),
        (100, 0.19, 0.05),
        (100, 0.21, 0.05),
        (100, 0.2, 0.04),
        (100, 0.2, 0.06),
    ],
)
def test_lattice_converges_at_every_greek_bump(kind, spot, vol, rate):
    days = inputs.exercise_days(kind)
    assert inputs.tree_price(days, spot, vol, rate) == pytest.approx(
        inputs.tree_price(days, spot, vol, rate, steps_per_day=16), abs=0.003
    )


@pytest.mark.parametrize("backend", ["dal", "quantlib"])
@pytest.mark.parametrize("kind", ["bermudan", "american"])
def test_lsmc_prices_and_greeks_are_reproducible_and_discriminating(backend, kind):
    from dal_comparisons.suite import prepare

    case = next(
        c
        for c in inputs.cases(True)
        if c["kind"] == kind and c["operation"] == "mc_greeks"
    )
    work = prepare(backend, case)
    values = work.run()
    work.validate(values)
    assert work.run() == pytest.approx(values, abs=1e-10, rel=0)
    for index, bound in enumerate(inputs.tolerance(case)):
        bad = inputs.expected(case)[:]
        bad[index] += bound * 1.1
        with pytest.raises(ValueError, match="mismatch"):
            work.validate(bad)


@pytest.mark.parametrize("kind", ["bermudan", "american"])
def test_quantlib_uses_disjoint_pseudorandom_streams_and_fresh_training(
    kind, monkeypatch
):
    import QuantLib as ql
    from dal_comparisons.exercise_quantlib import runner

    original = ql.MCAmericanEngine
    calls = []

    def counted(process, rng, **settings):
        calls.append((rng, settings))
        return original(process, rng, **settings)

    monkeypatch.setattr(ql, "MCAmericanEngine", counted)
    case = next(
        c
        for c in inputs.cases(True)
        if c["kind"] == kind and c["operation"] == "mc_price"
    )
    run = runner(dict(case, training_paths=3072))
    assert not calls
    run()
    run()
    assert len(calls) == 2
    for rng, settings in calls:
        assert rng == "pseudorandom"
        assert settings["seed"] != settings["seedCalibration"]
        assert settings["nCalibrationSamples"] == 3072
        assert settings["requiredSamples"] == 4096
        assert settings["timeSteps"] == len(inputs.exercise_days(kind))
        assert settings["polynomOrder"] == 3


@pytest.mark.parametrize("kind", ["bermudan", "american"])
def test_quantlib_grid_has_no_extra_bermudan_exercise_times(kind):
    import QuantLib as ql

    times = [day / 365 for day in inputs.exercise_days(kind)]
    required = times if kind == "bermudan" else times[-1:]
    assert list(ql.TimeGrid(required, len(times)))[1:] == pytest.approx(
        times, abs=1e-14
    )


def test_dal_passes_independent_budget_and_refits_every_greek_bump(monkeypatch):
    import dal
    from dal_comparisons.exercise_dal import runner

    original = dal.MonteCarlo_ValueWithSettings
    calls = []

    def counted(product, model, paths, **settings):
        calls.append((product, paths, settings["simulation"]))
        return original(product, model, paths, **settings)

    monkeypatch.setattr(dal, "MonteCarlo_ValueWithSettings", counted)
    case = next(c for c in inputs.cases(True) if c["operation"] == "mc_greeks")
    run = runner(dict(case, training_paths=3072))
    assert not calls
    run()
    assert len(calls) == 7
    assert len({id(product) for product, _, _ in calls}) == 7
    for _, paths, simulation in calls:
        assert paths == 4096
        assert simulation.lsmc_training_paths == 3072
        assert simulation.lsmc_basis_degree == 3
        assert not simulation.enable_aad


@pytest.mark.parametrize("case", inputs.cases(), ids=lambda c: c["name"])
def test_european_substitute_cannot_pass_early_exercise_price_bound(case):
    from dal_comparisons.scenarios import validate

    with pytest.raises(ValueError, match="mismatch"):
        validate(
            [inputs.tree_price((546,))],
            inputs.expected(case)[:1],
            abs_tol=inputs.tolerance(case)[:1],
        )
