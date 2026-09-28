"""GSR scenario parity across the available comparison backends."""

import pytest

from dal_comparisons import gsr_dal, gsr_quantlib
from dal_comparisons.scenarios import cases, expected, method, unsupported_reason
from dal_comparisons.suite import prepare


def test_gsr_inventory_retains_both_swap_boundaries_and_option_methods():
    inventory = {case["name"]: case for case in cases()}
    for name in ("gsr_swap_reused_cashflows_32", "gsr_swap_fresh_cashflows_32"):
        swap = inventory[name]
        assert swap["size"] == 32
        assert expected(swap) == pytest.approx([-0.00457023717969] * 32, abs=1e-11)
        assert unsupported_reason("rateslib", swap) is None
    for name in ("gsr_swaption_price_65536", "gsr_swaption_gaussian1d_128"):
        swaption = inventory[name]
        assert swaption["size"] == 65536
        assert expected(swaption) == pytest.approx([0.00569126499597], abs=1e-10)
        assert "GSR" in unsupported_reason("rateslib", swaption)
    assert inventory["gsr_swaption_gaussian1d_128"]["integration_points"] == 128
    assert "Sobol" in method("quantlib", inventory["gsr_swaption_price_65536"])
    assert "Gaussian1d" in method("quantlib", inventory["gsr_swaption_gaussian1d_128"])


@pytest.mark.parametrize(
    "backend,name",
    [
        ("dal", "gsr_swap_reused_cashflows_32"),
        ("quantlib", "gsr_swap_reused_cashflows_32"),
        ("rateslib", "gsr_swap_reused_cashflows_32"),
        ("dal", "gsr_swap_fresh_cashflows_32"),
        ("quantlib", "gsr_swap_fresh_cashflows_32"),
        ("rateslib", "gsr_swap_fresh_cashflows_32"),
        ("dal", "gsr_swaption_price_65536"),
        ("quantlib", "gsr_swaption_price_65536"),
        ("dal", "gsr_swaption_gaussian1d_128"),
        ("quantlib", "gsr_swaption_gaussian1d_128"),
    ],
)
def test_gsr_backend_matches_independent_curve_oracle(backend, name):
    case = next(case for case in cases(smoke=True) if case["name"] == name)
    work = prepare(backend, case)
    work.validate(work.run())


def test_quantlib_swaption_uses_fresh_sobol_paths(monkeypatch):
    case = next(
        case for case in cases(smoke=True) if case["name"] == "gsr_swaption_price_65536"
    )
    original = gsr_quantlib.ql.GaussianSobolPathGenerator
    counts = []

    class CountedGenerator:
        def __init__(self, *args):
            self.generator = original(*args)
            counts.append(0)

        def next(self):
            counts[-1] += 1
            return self.generator.next()

    monkeypatch.setattr(gsr_quantlib.ql, "GaussianSobolPathGenerator", CountedGenerator)
    work = prepare("quantlib", case)
    first = work.run()
    second = work.run()
    assert counts == [case["size"], case["size"]]
    assert second == first
    work.validate(first)


def test_swap_cashflow_reuse_modes_keep_their_timing_boundaries(monkeypatch):
    inventory = {case["name"]: case for case in cases(smoke=True)}
    quantlib_calls = []
    original_swap = gsr_quantlib.swap

    def counted_swap(handle):
        quantlib_calls.append(handle)
        return original_swap(handle)

    monkeypatch.setattr(gsr_quantlib, "swap", counted_swap)
    reused = prepare("quantlib", inventory["gsr_swap_reused_cashflows_32"])
    assert len(quantlib_calls) == 4
    reused.validate(reused.run())
    assert len(quantlib_calls) == 4
    fresh = prepare("quantlib", inventory["gsr_swap_fresh_cashflows_32"])
    assert len(quantlib_calls) == 4
    fresh.validate(fresh.run())
    assert len(quantlib_calls) == 8

    prepared_calls = []
    original_prepare = gsr_dal.dal.PreparedRateTrades_New

    def counted_prepare(*, trades):
        prepared_calls.append(len(trades))
        return original_prepare(trades=trades)

    monkeypatch.setattr(gsr_dal.dal, "PreparedRateTrades_New", counted_prepare)
    reused = prepare("dal", inventory["gsr_swap_reused_cashflows_32"])
    assert prepared_calls == [4]
    reused.validate(reused.run())
    assert prepared_calls == [4]
    fresh = prepare("dal", inventory["gsr_swap_fresh_cashflows_32"])
    fresh.validate(fresh.run())
    assert prepared_calls == [4]


def test_quantlib_gaussian1d_engine_is_retained(monkeypatch):
    case = next(
        case
        for case in cases(smoke=True)
        if case["name"] == "gsr_swaption_gaussian1d_128"
    )
    original = gsr_quantlib.ql.Gaussian1dSwaptionEngine
    integration_points = []

    def counted_engine(*args):
        integration_points.append(args[1])
        return original(*args)

    monkeypatch.setattr(gsr_quantlib.ql, "Gaussian1dSwaptionEngine", counted_engine)
    work = prepare("quantlib", case)
    assert integration_points == [128]
    first = work.run()
    work.validate(first)
    assert work.run() == first
    with pytest.raises(ValueError, match="numerical mismatch"):
        work.validate([first[0] + 2e-5])


def test_quantlib_swaption_rejects_a_price_outside_mc_accuracy():
    case = next(
        case for case in cases(smoke=True) if case["name"] == "gsr_swaption_price_65536"
    )
    work = prepare("quantlib", case)
    price = work.run()[0]
    with pytest.raises(ValueError, match="numerical mismatch"):
        work.validate([price + 3e-4])
