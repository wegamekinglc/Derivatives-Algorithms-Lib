import copy
import os
import pickle
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import pytest
import dal


def grid():
    today = dal.Date_(2026, 11, 16)
    expiry = today.AddDays(365)
    market = dal.MarketIvMarket_(today, 100., [dal.MarketIvTerm_(expiry, 102., .98)])
    quote = dal.MarketIvQuote_(id="atm", expiry=expiry, strike=102., option_type="CALL", quote_type="DECIMAL_IV", mid=.2)
    fit = dal.MarketIvFitResult_New(dal.MarketIvFitRequest_(market, [quote]))
    assert fit.is_success, fit.failure
    return dal.MarketIvDupireResult_New(fit.snapshot, dal.MarketIvDupireRequest_([80., 100., 120.], [.1, 1.]))


def settings(**kwargs):
    return dal.MarketIvLocalVolModelSettings_("EQ[A]", "USD", "W_EQ", 1.25, **kwargs)


def test_owning_factory_and_detached_reports():
    source = grid()
    model = dal.MarketIvLocalVolModelData_New("market", source, settings())
    assert isinstance(model, dal.ModelData_)
    assert model.configuration.max_step == 1 / 52
    assert model.summary["schema"] == "dal.market-iv-local-vol-model-summary/1"
    assert model.summary["spot"] == 100.
    model.summary.clear()
    model.carry_terms[-1]["forward"] = 0.
    model.warnings.clear()
    assert model.carry_terms[-1]["forward"] > 100.
    assert model.capabilities["passive_price"] and not model.capabilities["aad"]
    assert model.construction_execution["numeric_payload_bytes"] > 0
    identity = model.model_identity
    del source
    for value in (copy.copy(model), copy.deepcopy(model)):
        assert value.model_identity == identity
    for value in (model, settings()):
        with pytest.raises(RuntimeError, match="PickleUnsupported"):
            pickle.dumps(value)
    with pytest.raises(dal.MarketIvLocalVolError, match="ArchiveUnsupported") as failure:
        dal._dal._StorableToJson(model)
    assert failure.value.code == "ArchiveUnsupported"
    assert failure.value.field == "/model"
    assert failure.value.stage and failure.value.expected and failure.value.actual


def test_passive_pricing_inspection_and_capability_errors():
    model = dal.MarketIvLocalVolModelData_New("market", grid(), settings())
    today = dal.Date_(2026, 11, 16)
    product = dal.Product_New([today.AddDays(365)], ["pay PAYS 1"])
    valuation = dal.ScriptValuationSettings_(evaluation_date=today)
    assert dal.MonteCarlo_ValueWithSettings(product, model, 16, valuation=valuation)["PV"] == pytest.approx(.98, abs=2e-12)
    report = dal.MarketIvLocalVolModelData_Get_Execution(product, model, valuation=valuation)
    assert report["model_identity"] == model.model_identity
    assert report["prepared_columns"] == report["internal_steps"]
    assert report["per_path_market_queries"] == 0
    with pytest.raises(dal.MarketIvLocalVolError, match="DerivativeUnavailable"):
        dal.MonteCarlo_ValueWithSettings(product, model, 16, valuation=valuation, simulation=dal.MonteCarloSettings_(enable_aad=True))
    with pytest.raises(RuntimeError, match="InvalidModelEvaluationDate"):
        dal.MonteCarlo_ValueWithSettings(product, model, 16, valuation=dal.ScriptValuationSettings_(evaluation_date=today.AddDays(1)))
    late = dal.Product_New([today.AddDays(500)], ["pay PAYS 1"])
    with pytest.raises(dal.MarketIvLocalVolError, match="HorizonExceeded"):
        dal.MonteCarlo_ValueWithSettings(late, model, 16, valuation=valuation)
    exercise = dal.Product_New([today.AddDays(365)], ["EXERCISE MAX(spot() - 100, 0)"])
    with pytest.raises(dal.MarketIvLocalVolError, match="UnsupportedMarketIvLocalVolExercise"):
        dal.MonteCarlo_ValueWithSettings(exercise, model, 16, valuation=valuation)
    for text in ("pay PAYS FIX(EQ[B])", "pay PAYS FIX(IR[EUR,DF,2027-11-16])"):
        foreign = dal.Product_New([today.AddDays(365)], [text])
        with pytest.raises(RuntimeError):
            dal.MonteCarlo_ValueWithSettings(foreign, model, 16, valuation=valuation)
    with ThreadPoolExecutor(max_workers=4) as pool:
        reports = list(pool.map(lambda _: dal.MarketIvLocalVolModelData_Get_Execution(product, model, valuation=valuation), range(4)))
    assert all(row == report for row in reports)
    vanilla = dal.Product_New([today.AddDays(365)], ["pay PAYS MAX(FIX(EQ[A]) - 102, 0)"])
    simulation = dal.MonteCarloSettings_(method="mrg32")
    pv = dal.MonteCarlo_ValueWithSettings(vanilla, model, 64, valuation=valuation, simulation=simulation)["PV"]
    print("DAL543_ALIGNMENT", model.source_identity, model.model_identity, format(pv, ".17g"))


@pytest.mark.parametrize("key,value,error", [
    ("max_step", True, TypeError), ("max_step", "0.1", TypeError),
    ("max_step", float("nan"), ValueError), ("max_step", 0., ValueError),
    ("max_internal_steps", True, TypeError), ("max_internal_steps", 1.5, TypeError),
    ("max_internal_steps", -1, ValueError), ("max_internal_steps", 2**100, ValueError),
    ("unknown", 1, ValueError),
])
def test_strict_settings(key, value, error):
    with pytest.raises(error, match="InvalidMarketIvLocalVolRequest"):
        settings(**{key: value})


def test_request_identity_v2_matches_native_noon_golden():
    code = """
import dal
from test_market_iv_local_vol import grid, settings

today = dal.Date_(2026, 11, 16)
model = dal.MarketIvLocalVolModelData_New("market", grid(), settings())
product = dal.Product_New([today.AddDays(365)], ["pay PAYS 1"])
snapshot = dal.MarketFixingSnapshot_New({"EQ[A]": {dal.DateTime_(today.AddDays(-1), 12): 99.}})
for fixings, golden in (
    (None, "sha256:0f0aa577def22e9db4b2bab9ab3ccc4237b6fc00cec12eb2f444b85345a1b5e2"),
    (snapshot, "sha256:04f36f435aec453a561fbcd494742776ea35fa821be56291492de3eeb19d2339"),
):
    valuation = dal.ScriptValuationSettings_(evaluation_date=today, fixings=fixings)
    report = dal.MarketIvLocalVolModelData_Get_Execution(product, model, valuation=valuation)
    assert report["workers"] == 1
    assert report["request_identity"] == golden
    assert report["schema"] == "dal.market-iv-local-vol-execution/1"
    assert len(report) == 23
    assert report["source_identity"] == model.source_identity
    assert report["model_identity"] == model.model_identity
    pv = dal.MonteCarlo_ValueWithSettings(product, model, 16, valuation=valuation)["PV"]
    assert abs(pv - 0.98) <= 2e-12, pv
"""
    package = str(Path(dal.__file__).resolve().parent.parent)
    tests = str(Path(__file__).resolve().parent)
    result = subprocess.run(
        [sys.executable, "-c", code],
        env={**os.environ, "DAL_NUM_THREADS": "1", "PYTHONPATH": os.pathsep.join((package, tests))},
        capture_output=True, text=True, timeout=60, check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr


def test_factory_releases_gil():
    source, config = grid(), settings()
    with ThreadPoolExecutor(max_workers=1) as pool:
        def build():
            dal._dal._MarketIvLocalVolGilBarrier_ArmForTesting()
            return dal.MarketIvLocalVolModelData_New("concurrent", source, config)
        future = pool.submit(build)
        try:
            assert dal._dal._MarketIvLocalVolGilBarrier_AwaitEntryForTesting()
        finally:
            released = dal._dal._MarketIvLocalVolGilBarrier_ReleaseForTesting()
        assert released
        assert future.result().summary["spot"] == 100.


def test_bad_source_budget_and_readonly_settings():
    source, config = grid(), settings()
    with pytest.raises(AttributeError):
        config.horizon = 2.
    with pytest.raises(TypeError):
        dal.MarketIvLocalVolModelData_New("bad", source.surface, config)
    failed = dal.MarketIvDupireResult_New(source.snapshot, dal.MarketIvDupireRequest_([80., 100., 120.], [1e-12, 1.]))
    assert not failed.is_success
    with pytest.raises(dal.MarketIvLocalVolError, match="MarketIvDupireUnavailable"):
        dal.MarketIvLocalVolModelData_New("failed", failed, config)
    with pytest.raises(dal.MarketIvLocalVolError, match="BudgetExceeded"):
        dal.MarketIvLocalVolModelData_New("budget", source, settings(numeric_payload_budget_bytes=0))
