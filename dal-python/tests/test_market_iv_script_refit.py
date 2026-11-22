import gc
import json
import math
import statistics
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import dal
import pytest

FIXTURES = json.loads((Path(__file__).resolve().parents[2] / "tests/market-iv-local-vol/fixtures/script_refit.json").read_text())


def request(case, iv_shift=0., currency="USD", **overrides):
    market_case, quote_ids, steps = FIXTURES["cases"][case]
    data = FIXTURES[market_case]
    today = dal.Date_(2026, 11, 16)
    expiry = today.AddDays(365)
    market = dal.MarketIvMarket_(today, 100., [dal.MarketIvTerm_(expiry, data["forward"], .96)])
    quotes = [dal.MarketIvQuote_(id=name, expiry=expiry, strike=strike, option_type=option, quote_type=kind,
                                 mid=mid + iv_shift, bid=bid, ask=ask, weight=weight)
              for name, strike, option, kind, mid, bid, ask, weight in data["quotes"]]
    fit = dal.MarketIvFitResult_New(dal.MarketIvFitRequest_(market, quotes))
    grid = dal.MarketIvDupireRequest_(data["spots"], data["times"])
    settings = dal.MarketIvLocalVolModelSettings_("EQ[A]", currency, "W_EQ", 1., max_step=data["max_step"])
    product = dal.Product_New([expiry], [f"pay PAYS MAX(spot() - {data['strike']}, 0)"])
    options = dict(steps=steps, blocks=16, paths_per_block=257, valuation=dal.ScriptValuationSettings_(evaluation_date=today))
    return dal.MarketIvScriptRefitRequest_(fit.snapshot, grid, settings, product, quote_ids, **(options | overrides))


@pytest.mark.parametrize("case", FIXTURES["cases"])
def test_owned_rows(case):
    source = request(case)
    result = dal.MarketIvScriptRefitRiskResult_New(source)
    del source
    gc.collect()
    rows = result.rows
    names = {"all_buckets": ["left", "atm", "right"], "partial": ["atm", "right"]}
    assert [row["id"] for row in rows] == names.get(case, ["atm"])
    for row in rows:
        assert row["pv_currency"] == "USD" and math.isfinite(row["base_pv"])
        if case == "partial" and row["id"] == "atm":
            assert (row["input_unit"], row["step"], row["status"]) == ("premium", .01, "SIDE_FAILED")
            assert (row["plus_failure_stage"], row["plus_fit_status"]) == ("FIT", "INCONSISTENT_QUOTES")
            assert row["plus_pv"] is row["slope"] is row["standard_error"] is None
            assert math.isfinite(row["minus_pv"])
        else:
            assert row["input_unit"] == "decimal_iv" and row["status"] in ("SUCCESS", "REGIME_CHANGED")
            assert row["step"] == (5e-5 if case == "half" else 1e-4)
            assert row["slope"] == pytest.approx((row["plus_pv"] - row["minus_pv"]) / (2 * row["step"]), abs=1e-8)
            assert row["standard_error"] >= 0
    print("REFIT", case, json.dumps(rows, sort_keys=True))
    assert result.execution["total_paths"] == result.execution["scenarios"] * 16 * 257
    rows.clear()
    assert result.rows


def evaluate_while_mutating(source, mutate):
    barrier = dal._dal

    def evaluate():
        barrier._MarketIvLocalVolGilBarrier_ArmForTesting()
        return dal.MarketIvScriptRefitRiskResult_New(source)

    with ThreadPoolExecutor(max_workers=1) as pool:
        pending = pool.submit(evaluate)
        assert barrier._MarketIvLocalVolGilBarrier_AwaitEntryForTesting()
        try:
            mutate()
        finally:
            assert barrier._MarketIvLocalVolGilBarrier_ReleaseForTesting()
        return pending.result(timeout=10)


def test_gil_snapshot():
    source = request("half")
    baseline = dal.MarketIvScriptRefitRiskResult_New(source).rows

    def mutate():
        source.quote_ids = ["right"]
        source.steps = [.01]
        source.valuation.evaluation_date = dal.Date_(2026, 11, 17)
        source.simulation.enable_aad = True

    assert evaluate_while_mutating(source, mutate).rows == baseline
    with pytest.raises(RuntimeError):
        dal.MarketIvScriptRefitRiskResult_New(source)


def test_refit_budget_and_steps():
    budget = dal.MarketIvScriptRefitBudget_()
    budget.max_total_paths = 3 * 16 * 257 - 1
    with pytest.raises(RuntimeError, match="/budget/max_total_paths"):
        dal.MarketIvScriptRefitRiskResult_New(request("full", budget=budget))
    with pytest.raises(TypeError, match="steps"):
        request("full", steps=[True])
    with pytest.raises(TypeError, match="blocks"):
        request("full", blocks=2.5)


def test_result_currency_uses_model_configuration():
    row, = dal.MarketIvScriptRefitRiskResult_New(request("half", currency="US_D")).rows
    assert row["pv_currency"] == "USD"
    print("CURRENCY US_D", row["pv_currency"])


def test_gil_snapshot_of_default_valuation_date():
    original_date = dal.EvaluationDate_Get()
    try:
        dal.EvaluationDate_Set(dal.Date_(2026, 11, 16))
        source = request("half", valuation=None)
        expected = dal.MarketIvScriptRefitRiskResult_New(source).rows

        captured = evaluate_while_mutating(source, lambda: dal.EvaluationDate_Set(dal.Date_(2026, 11, 17)))
        assert captured.rows == expected
        print("DEFAULT_DATE 2026-11-16", dal.EvaluationDate_Get(), captured.rows[0]["base_pv"])
        with pytest.raises(RuntimeError, match="InvalidModelEvaluationDate"):
            dal.MarketIvScriptRefitRiskResult_New(source)
    finally:
        dal.EvaluationDate_Set(original_date)


def test_parallel_flat_iv_against_black():
    h = 1e-4
    plus, minus = (dal.MarketIvScriptRefitRiskResult_New(request("full", iv_shift=s, paths_per_block=131072)) for s in (h, -h))
    assert len(plus.base_block_means) == len(minus.base_block_means) == 16
    slopes = [(p - m) / (2 * h) for p, m in zip(plus.base_block_means, minus.base_block_means)]
    slope, se = statistics.mean(slopes), statistics.stdev(slopes) / 4
    black = .96 * 105 * (math.erf((.2 + h) / math.sqrt(8)) - math.erf((.2 - h) / math.sqrt(8))) / (2 * h)
    assert se / 100 <= .001
    assert abs(slope - black) / 100 <= .001 + 4.5 * se / 100
    print("BLACK", plus.rows[0]["base_pv"], minus.rows[0]["base_pv"], slope, se, black)
