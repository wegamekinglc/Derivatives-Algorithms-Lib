import json
import math
import sys
import threading

import dal
from dal import _dal as native
import pytest

D = dal.Date_(2026, 9, 12)
H = dal.Date_(2026, 9, 11)
P = dal.Date_(2026, 9, 22)
INDEX = "EQ[DAL196_TEST]"


def snapshot(value=80.0, date=H, hour=0):
    return dal.MarketFixingSnapshot_New({INDEX: {dal.DateTime_(date, hour): value}})


def model(adapter="bs", rate=0.0, div=0.0):
    if adapter == "bs":
        return dal.BSModelData_New(100.0, 0.0, rate, div)
    return dal.DupireModelData_New(
        100.0,
        rate,
        div,
        [80.0, 120.0],
        [0.0, 1.0],
        dal.DoubleMatrix_([[0.0, 0.0], [0.0, 0.0]]),
    )


def historical_product():
    return dal.Product_New(
        ["SCALE", H, P], ["2", "x = SCALE * FIX(EQ[DAL196_TEST])", "pay PAYS x"]
    )


def assert_pv(result, expected):
    assert abs(result["PV"] - expected) <= 1e-12 * max(1.0, abs(expected))
    assert all(key == "PV" or key.startswith("d_") for key in result)
    assert all(isinstance(value, float) for value in result.values())


@pytest.mark.parametrize("adapter", ["bs", "dupire"])
@pytest.mark.parametrize("method", ["sobol", "mrg32", "irn"])
@pytest.mark.parametrize("use_bb", [False, True])
@pytest.mark.parametrize("compiled", [False, True])
@pytest.mark.parametrize("aad", [False, True])
def test_today_policy_independent_oracles(adapter, method, use_bb, compiled, aad):
    dal.EvaluationDate_Set(dal.Date_(2026, 9, 23))
    product = dal.Product_New([D], ["pay PAYS FIX(EQ[DAL196_TEST])"])
    valuation = dal.ScriptValuationSettings_(
        evaluation_date=D, model_bindings={"spot": INDEX}, fixings=snapshot(date=D)
    )
    execution = dal.MonteCarloSettings_(
        method=method, use_bb=use_bb, compiled=compiled, enable_aad=aad
    )
    data = model(adapter)
    for policy, expected, source in [
        ("Model", 100.0, "Model"),
        (dal.TodayFixingPolicy_.REQUIREHISTORICAL, 80.0, "Historical"),
    ]:
        valuation.today_fixing = policy
        assert_pv(
            dal.MonteCarlo_ValueWithSettings(
                product, data, 257, valuation=valuation, simulation=execution
            ),
            expected,
        )
        request = dal.ScriptValuation_Explain(product, data, valuation=valuation)[
            "requests"
        ][0]
        assert request["source"] == source
        assert request["value"] == (80.0 if source == "Historical" else None)
    valuation.fixings = dal.MarketFixingSnapshot_New({})
    with pytest.raises(RuntimeError, match="MissingFixing"):
        dal.MonteCarlo_ValueWithSettings(
            product, data, 1, valuation=valuation, simulation=execution
        )
    assert dal.EvaluationDate_Get() == dal.Date_(2026, 9, 23)


@pytest.mark.parametrize("compiled", [False, True])
@pytest.mark.parametrize("paths", [1, 257, 8193])
def test_historical_aad_batches_and_repricing(compiled, paths):
    product = historical_product()
    valuation = dal.ScriptValuationSettings_(evaluation_date=D, fixings=snapshot())
    simulation = dal.MonteCarloSettings_(compiled=compiled, enable_aad=True)
    data = model(rate=0.05)
    time = 10.0 / 365.0
    for fixing in [80.0, 90.0, 80.0]:
        valuation.fixings = snapshot(fixing)
        result = dal.MonteCarlo_ValueWithSettings(
            product, data, paths, valuation=valuation, simulation=simulation
        )
        expected = 2 * fixing * math.exp(-0.05 * time)
        assert_pv(result, expected)
        assert abs(result["d_SCALE"] - expected / 2) <= 1e-10
        assert abs(result["d_rate"] + time * expected) <= 1e-10
        assert result["d_spot"] == result["d_vol"] == 0.0
        assert set(result) == {"PV", "d_SCALE", "d_spot", "d_vol", "d_rate", "d_div"}


@pytest.mark.parametrize("adapter", ["bs", "dupire"])
@pytest.mark.parametrize("compiled", [False, True])
@pytest.mark.parametrize("aad", [False, True])
def test_future_observation_and_payment_have_distinct_dates(adapter, compiled, aad):
    product = dal.Product_New([P], ["pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15)"])
    valuation = dal.ScriptValuationSettings_(
        evaluation_date=D,
        model_bindings={"spot": INDEX},
        fixings=snapshot(999.0, date=dal.Date_(2026, 9, 15)),
    )
    simulation = dal.MonteCarloSettings_(compiled=compiled, enable_aad=aad)
    data = model(adapter, rate=0.05, div=0.02)
    expected = 100.0 * math.exp(0.03 * 3 / 365.0 - 0.05 * 10 / 365.0)
    result = dal.MonteCarlo_ValueWithSettings(
        product, data, 257, valuation=valuation, simulation=simulation
    )
    assert_pv(result, expected)
    explanation = dal.ScriptValuation_Explain(product, data, valuation=valuation)
    assert explanation["sample_dates"] == ["2026-09-15", "2026-09-22"]
    assert explanation["requests"][0]["model_slot"] == {"sample_id": 0, "output_id": 0}
    assert explanation["event_to_sample"] == [1]
    repeated = dal.Product_New(
        [P],
        [
            "pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15) - FIX(eq[dal196_test], 2026-09-15)"
        ],
    )
    assert (
        dal.MonteCarlo_ValueWithSettings(
            repeated, data, 257, valuation=valuation, simulation=simulation
        )["PV"]
        == 0.0
    )


@pytest.mark.parametrize("compiled", [False, True])
def test_exact_fuzzy_and_hard_history_have_separate_oracles(compiled):
    valuation = dal.ScriptValuationSettings_(evaluation_date=D, fixings=snapshot())
    data = model()

    def price(strike, aad, past=False):
        action = "x =" if past else "pay PAYS"
        condition = f"IF FIX(EQ[DAL196_TEST], 2026-09-11) > K:0.2 THEN {action} SCALE * 80 ELSE {action} 0 END"
        dates = ["SCALE", "K", H, P] if past else ["SCALE", "K", P]
        events = (
            ["2", str(strike), condition, "pay PAYS x"]
            if past
            else ["2", str(strike), condition]
        )
        product = dal.Product_New(dates, events)
        return dal.MonteCarlo_ValueWithSettings(
            product,
            data,
            257,
            valuation=valuation,
            simulation=dal.MonteCarloSettings_(enable_aad=aad, compiled=compiled),
        )

    assert_pv(price(79.95, False), 160.0)
    fuzzy = price(79.95, True)
    assert_pv(fuzzy, 120.0)
    assert abs(fuzzy["d_SCALE"] - 60.0) <= 1e-10
    assert abs(fuzzy["d_K"] + 800.0) <= 1e-10
    step = 1e-5
    finite_difference = (
        price(79.95 + step, True)["PV"] - price(79.95 - step, True)["PV"]
    ) / (2 * step)
    assert abs(finite_difference + 800.0) < 1e-5
    hard = price(79.95, True, past=True)
    assert_pv(hard, 160.0)
    assert abs(hard["d_SCALE"] - 80.0) <= 1e-10
    assert hard["d_K"] == 0.0


@pytest.mark.parametrize(
    "bindings,identifier,field",
    [
        ({"spot": "EQ[OTHER]"}, "ConflictingModelBinding", "EQ[OTHER]"),
        ({"other": INDEX}, "UnknownModelAsset", "valuation.modelBindings_[0]"),
        ({"": INDEX}, "UnknownModelAsset", "valuation.modelBindings_[0]"),
        ({"spot": ""}, "InvalidIndex", "valuation.modelBindings_[0]"),
        ({"spot": "EQ[A]trailing"}, "InvalidIndex", "valuation.modelBindings_[0]"),
        (
            {"spot": INDEX, "SPOT": INDEX},
            "DuplicateModelBinding",
            "valuation.modelBindings_",
        ),
        (
            {"spot": "FX[EUR/USD]"},
            "UnsupportedModelObservation",
            "valuation.modelBindings_[0]",
        ),
        (
            {"spot": "EQ[DAL196_TEST]@2026-12-31"},
            "UnsupportedModelObservation",
            "valuation.modelBindings_[0]",
        ),
    ],
)
def test_binding_semantics_keep_native_errors(bindings, identifier, field):
    product = dal.Product_New([P], ["pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15)"])
    valuation = dal.ScriptValuationSettings_(evaluation_date=D, model_bindings=bindings)
    for action in (
        lambda: dal.MonteCarlo_ValueWithSettings(
            product, model(), 1, valuation=valuation
        ),
        lambda: dal.ScriptValuation_Explain(product, model(), valuation=valuation),
    ):
        with pytest.raises(RuntimeError) as error:
            action()
        assert identifier in str(error.value)
        assert field in str(error.value)


@pytest.mark.parametrize("compiled", [False, True])
def test_empty_bindings_infer_the_script_index(compiled):
    product = dal.Product_New([P], ["pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15)"])
    valuation = dal.ScriptValuationSettings_(evaluation_date=D)
    simulation = dal.MonteCarloSettings_(compiled=compiled)
    data = model(rate=0.05, div=0.02)
    expected = 100.0 * math.exp(0.03 * 3 / 365.0 - 0.05 * 10 / 365.0)
    assert_pv(
        dal.MonteCarlo_ValueWithSettings(
            product, data, 257, valuation=valuation, simulation=simulation
        ),
        expected,
    )
    explanation = dal.ScriptValuation_Explain(product, data, valuation=valuation)
    assert explanation["model_bindings"] == [
        {"asset": "spot", "index_original": INDEX, "index_canonical": INDEX}
    ]


def test_empty_bindings_reject_ambiguous_future_indices():
    product = dal.Product_New(
        [P],
        ["pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15) + FIX(EQ[OTHER], 2026-09-15)"],
    )
    valuation = dal.ScriptValuationSettings_(evaluation_date=D)
    for action in (
        lambda: dal.MonteCarlo_ValueWithSettings(product, model(), 1, valuation=valuation),
        lambda: dal.ScriptValuation_Explain(product, model(), valuation=valuation),
    ):
        with pytest.raises(RuntimeError) as error:
            action()
        assert "AmbiguousModelBinding" in str(error.value)
        assert INDEX in str(error.value)


def test_missing_history_and_midnight_are_not_silently_replaced():
    product = historical_product()
    for fixings, source in [
        (None, "GlobalSnapshot"),
        (dal.MarketFixingSnapshot_New({}), "ExplicitSnapshot"),
        (snapshot(hour=11), "ExplicitSnapshot"),
    ]:
        valuation = dal.ScriptValuationSettings_(evaluation_date=D, fixings=fixings)
        with pytest.raises(RuntimeError) as error:
            dal.MonteCarlo_ValueWithSettings(product, model(), 1, valuation=valuation)
        assert all(
            part in str(error.value)
            for part in (
                "MissingFixing",
                source,
                INDEX,
                "2026-09-11 00:00:00",
                "row=2",
                "exact historical fixing required",
            )
        )
    source = {INDEX: {dal.DateTime_(H, 0): 80.0}}
    frozen = dal.MarketFixingSnapshot_New(source)
    source[INDEX][dal.DateTime_(H, 0)] = 90.0
    valuation = dal.ScriptValuationSettings_(evaluation_date=D, fixings=frozen)
    assert_pv(
        dal.MonteCarlo_ValueWithSettings(product, model(), 1, valuation=valuation),
        160.0,
    )


def test_diagnostics_keep_actual_ids_and_legacy_schema_boundary():
    product = dal.Product_New(
        [P],
        [
            "pay PAYS FIX(EQ[Z], 2026-09-11) + FIX(EQ[A], 2026-09-15) + FIX(FX[EUR/USD], 2026-09-11) + FIX(eq[z], 2026-09-11)"
        ],
    )
    valuation = dal.ScriptValuationSettings_(
        evaluation_date=D,
        model_bindings={"spot": "EQ[A]"},
        fixings=dal.MarketFixingSnapshot_New(
            {
                "EQ[Z]": {dal.DateTime_(H, 0): 80.0},
                "FX[USD/EUR]": {dal.DateTime_(H, 0): 0.8},
            }
        ),
    )
    data = model()
    description = dal.Product_Describe(product)
    assert description == json.loads(native.Product_Describe(product))
    dal.EvaluationDate_Set(dal.Date_(2026, 9, 23))
    assert dal.Product_Describe(product) == description
    assert "evaluation_date" not in description
    assert all("phase" not in event for event in description["events"])
    explanation = dal.ScriptValuation_Explain(product, data, valuation=valuation)
    assert explanation == json.loads(
        native.ScriptValuation_Explain(product, data, valuation=valuation)
    )
    requests = explanation["requests"]
    assert [request["request_id"] for request in requests] == [0, 1, 2]
    assert [request["history_value_id"] for request in requests] == [0, None, 1]
    assert [request["value"] for request in requests] == [80.0, None, 1.25]
    assert [request["uses"][0]["node_id"] for request in requests] == ["n5", "n6", "n7"]
    assert requests[0]["uses"][1]["node_id"] == "n8"
    assert requests[0]["uses"][1]["index_original"] == "eq[z]"
    assert explanation["event_to_sample"] == [1]
    assert explanation["simulation"] == {
        "rsg": "sobol",
        "use_bb": False,
        "enable_aad": False,
        "smooth": 0.01,
        "compiled": False,
    }
    assert_pv(
        dal.MonteCarlo_ValueWithSettings(product, data, 257, valuation=valuation),
        261.25,
    )
    with pytest.raises(RuntimeError, match="DebugSchemaUnsupported"):
        dal.Product_DebugJson(product)
    bound = dal.Product_New(
        [P], ["pay PAYS 1"], settings=dal.ScriptProductSettings_(default_index=INDEX)
    )
    with pytest.raises(RuntimeError, match="DebugSchemaUnsupported"):
        dal.Product_DebugJson(bound)


def test_legacy_calls_and_default_deduplication():
    dal.EvaluationDate_Set(D)
    data = model()
    legacy = dal.Product_New([D], ["pay PAYS SPOT()"])
    args = [legacy, data, 257, "sobol", False, True, 0.01, True]
    for length in range(3, 9):
        assert_pv(dal.MonteCarlo_Value(*args[:length]), 100.0)
    assert_pv(
        native.MonteCarlo_Value(
            product=legacy,
            modelData=data,
            num_path=257,
            method="sobol",
            use_bb=1,
            enable_aad=0,
            smooth=1,
            compiled=1,
        ),
        100.0,
    )
    assert_pv(
        dal.MonteCarlo_ValueWithSettings(
            product=legacy,
            modelData=data,
            num_path=257,
            valuation=None,
            simulation=None,
        ),
        100.0,
    )
    assert json.loads(dal.Product_DebugJson(legacy))["schema"] == "dal.script-product/1"
    dates, events = [H, P], ["x = SPOT() + FIX(eq[dal196_test])", "pay PAYS x"]
    valuation = dal.ScriptValuationSettings_(evaluation_date=D, fixings=snapshot())
    bound = dal.Product_New(
        dates, events, settings=dal.ScriptProductSettings_(default_index=INDEX)
    )
    assert_pv(
        dal.MonteCarlo_ValueWithSettings(bound, data, 257, valuation=valuation), 160.0
    )
    request = dal.ScriptValuation_Explain(bound, data, valuation=valuation)["requests"]
    assert len(request) == 1
    assert {use["observation_type"] for use in request[0]["uses"]} == {"Spot", "Fix"}
    for date, text, identifier in [
        (H, "pay PAYS SPOT()", "UnboundHistoricalSpot"),
        (P, "pay PAYS SPOT() + FIX(EQ[DAL196_TEST])", "MissingDefaultIndex"),
    ]:
        unbound = dal.Product_New([date, P], [text, "pay PAYS 1"])
        with pytest.raises(RuntimeError, match=identifier):
            dal.MonteCarlo_ValueWithSettings(unbound, data, 1, valuation=valuation)


def test_expired_empty_and_error_recovery():
    valuation = dal.ScriptValuationSettings_(evaluation_date=D)
    data = model()
    expired = dal.Product_New(["SCALE", H], ["2", "pay PAYS SCALE * FIX(EQ[ABSENT])"])
    result = dal.MonteCarlo_ValueWithSettings(
        expired,
        data,
        1,
        valuation=valuation,
        simulation=dal.MonteCarloSettings_(enable_aad=True),
    )
    assert result and all(value == 0 for value in result.values())
    explanation = dal.ScriptValuation_Explain(expired, data, valuation=valuation)
    assert explanation["all_expired"] is True
    assert explanation["requests"][0]["resolution"] == "SkippedExpired"
    for dates, events in [([], []), (["SCALE"], ["2"]), ([P], ["x = 1"])]:
        product = dal.Product_New(dates, events)
        assert dal.Product_Describe(product)["payoff_index"] is None
        with pytest.raises(RuntimeError, match="InvalidScriptStructure"):
            dal.MonteCarlo_ValueWithSettings(product, data, 1, valuation=valuation)
        with pytest.raises(RuntimeError, match="InvalidScriptStructure"):
            dal.ScriptValuation_Explain(product, data, valuation=valuation)
    bad = dal.Product_New([P], ["pay PAYS LOG(-1)"])
    with pytest.raises(RuntimeError):
        dal.MonteCarlo_ValueWithSettings(bad, data, 8193, valuation=valuation)
    assert_pv(
        dal.MonteCarlo_ValueWithSettings(
            dal.Product_New([P], ["pay PAYS 7"]), data, 257, valuation=valuation
        ),
        7.0,
    )


def test_settings_are_copied_before_gil_release():
    product = historical_product()
    data = model()
    valuation = dal.ScriptValuationSettings_(evaluation_date=D, fixings=snapshot())
    simulation = dal.MonteCarloSettings_(enable_aad=True, compiled=True)
    ready, start, progressed = threading.Event(), threading.Event(), threading.Event()

    def mutate():
        ready.set()
        start.wait()
        valuation.evaluation_date = dal.Date_(2026, 9, 23)
        valuation.fixings = dal.MarketFixingSnapshot_New({})
        simulation.enable_aad = False
        simulation.compiled = False
        progressed.set()

    thread = threading.Thread(target=mutate)
    thread.start()
    assert ready.wait(timeout=5)
    previous_interval = sys.getswitchinterval()
    sys.setswitchinterval(100.0)
    try:
        start.set()
        result = dal.MonteCarlo_ValueWithSettings(
            product, data, 2**18, valuation=valuation, simulation=simulation
        )
        assert progressed.is_set()
        assert_pv(result, 160.0)
        assert abs(result["d_SCALE"] - 80.0) <= 1e-10
    finally:
        sys.setswitchinterval(previous_interval)
        start.set()
        thread.join(timeout=5)
