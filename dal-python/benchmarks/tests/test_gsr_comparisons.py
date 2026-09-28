"""GSR scenario parity across the available comparison backends."""

import pytest

from dal_comparisons import gsr_quantlib
from dal_comparisons.scenarios import cases, expected, unsupported_reason
from dal_comparisons.suite import prepare


def test_gsr_inventory_has_coupon_swap_and_one_factor_swaption():
    inventory = {case["name"]: case for case in cases()}
    swap = inventory["gsr_swap_static_pv_32"]
    swaption = inventory["gsr_swaption_price_65536"]
    assert swap["size"] == 32
    assert swaption["size"] == 65536
    assert expected(swap) == pytest.approx([-0.00457023717969] * 32, abs=1e-11)
    assert expected(swaption) == pytest.approx([0.00569126499597], abs=1e-10)
    assert unsupported_reason("rateslib", swap) is None
    assert "GSR" in unsupported_reason("rateslib", swaption)


@pytest.mark.parametrize(
    "backend,name",
    [
        ("dal", "gsr_swap_static_pv_32"),
        ("quantlib", "gsr_swap_static_pv_32"),
        ("rateslib", "gsr_swap_static_pv_32"),
        ("dal", "gsr_swaption_price_65536"),
        ("quantlib", "gsr_swaption_price_65536"),
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


def test_quantlib_swaption_rejects_a_price_outside_mc_accuracy():
    case = next(
        case for case in cases(smoke=True) if case["name"] == "gsr_swaption_price_65536"
    )
    work = prepare("quantlib", case)
    price = work.run()[0]
    with pytest.raises(ValueError, match="numerical mismatch"):
        work.validate([price + 3e-4])
