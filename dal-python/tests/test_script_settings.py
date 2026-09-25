import copy
import datetime
import enum
import json
import math

import dal
from dal import _dal as native
import pytest


def test_product_settings_copy_and_diagnostic_layers():
    settings = dal.ScriptProductSettings_(default_index="EQ[DAL196_TEST]")
    product = dal.Product_New(
        events_dates=[dal.Cell_(dal.Date_(2026, 9, 22))],
        events=["pay PAYS SPOT()"],
        settings=settings,
    )
    settings.default_index = "EQ[OTHER]"
    description = dal.Product_Describe(product)
    assert description == json.loads(native.Product_Describe(product))
    assert description["schema"] == "dal.script-product/2"
    assert description["default_index"] == {
        "original": "EQ[DAL196_TEST]",
        "canonical": "EQ[DAL196_TEST]",
    }


def test_historical_settings_value_preserves_parameter_risk(historical_fix):
    product = historical_fix.product()
    valuation = dal.ScriptValuationSettings_(
        evaluation_date=historical_fix.today, fixings=historical_fix.snapshot()
    )
    model = dal.BSModelData_New(100.0, 0.0, 0.05, 0.0)
    simulation = dal.MonteCarloSettings_(enable_aad=True, compiled=True)
    result = dal.MonteCarlo_ValueWithSettings(
        product, model, 257, valuation=valuation, simulation=simulation
    )
    time = (historical_fix.payment - historical_fix.today) / 365.0
    expected = 160.0 * math.exp(-0.05 * time)
    assert abs(result["PV"] - expected) <= 1e-12 * expected
    assert abs(result["d_SCALE"] - expected / 2.0) <= 1e-10
    assert abs(result["d_rate"] + time * expected) <= 1e-10
    assert result["d_spot"] == result["d_vol"] == 0.0
    explanation = dal.ScriptValuation_Explain(product, model, valuation=valuation)
    assert explanation == json.loads(
        native.ScriptValuation_Explain(product, model, valuation=valuation)
    )
    assert explanation["schema"] == "dal.script-valuation/1"
    assert explanation["requests"][0]["value"] == 80.0


def test_settings_values_and_copy_protocol():
    assert dal.ScriptProductSettings_ is native.ScriptProductSettings_
    assert dal.TodayFixingPolicy_ is native.TodayFixingPolicy_
    assert set(dal.TodayFixingPolicy_.__members__) == {"MODEL", "REQUIREHISTORICAL"}
    product = dal.ScriptProductSettings_(default_index=dal.String_("eq[MiXeD]"))
    assert product.default_index == "eq[MiXeD]"
    date = dal.Date_(2026, 9, 12)
    snapshot = dal.MarketFixingSnapshot_New({})
    valuation = dal.ScriptValuationSettings_(
        evaluation_date=date,
        today_fixing=dal.String_("RequireHistorical"),
        fixings=snapshot,
    )
    assert valuation.evaluation_date == date
    assert valuation.evaluation_date is not date
    assert valuation.today_fixing == dal.TodayFixingPolicy_.REQUIREHISTORICAL
    simulation = dal.MonteCarloSettings_(
        method=dal.String_("SoBoL"),
        use_bb=True,
        enable_aad=True,
        smooth=2,
        compiled=False,
    )
    assert (
        simulation.method,
        simulation.use_bb,
        simulation.enable_aad,
        simulation.smooth,
        simulation.compiled,
    ) == (
        "SoBoL",
        True,
        True,
        2.0,
        False,
    )
    for source, field, replacement in [
        (product, "default_index", "EQ[OTHER]"),
        (valuation, "today_fixing", dal.TodayFixingPolicy_.MODEL),
        (simulation, "smooth", 0.02),
    ]:
        for copier in (copy.copy, copy.deepcopy):
            clone = copier(source)
            assert clone is not source
            assert getattr(clone, field) == getattr(source, field)
            setattr(clone, field, replacement)
            assert getattr(clone, field) != getattr(source, field)
    for copier in (copy.copy, copy.deepcopy):
        assert copier(valuation).fixings is snapshot
    defaults = dal.ScriptValuationSettings_()
    assert defaults.evaluation_date is defaults.fixings is None
    assert defaults.today_fixing == dal.TodayFixingPolicy_.MODEL
    execution = dal.MonteCarloSettings_()
    assert (
        execution.method,
        execution.use_bb,
        execution.enable_aad,
        execution.smooth,
        execution.compiled,
    ) == (
        "sobol",
        False,
        False,
        0.01,
        None,
    )


class IntChoice(enum.IntEnum):
    ONE = 1


class ForeignSetting(str, enum.Enum):
    MODEL = "Model"
    HISTORY = "RequireHistorical"
    INDEX = "EQ[A]"
    METHOD = "sobol"
    ASSET = "spot"
    EVENT = "pay PAYS FIX(EQ[A])"


class SettingText(str):
    pass


@pytest.mark.parametrize("construct", [True, False], ids=["constructor", "setter"])
@pytest.mark.parametrize(
    "class_name,field,bad,previous",
    [
        (
            "ScriptValuationSettings_",
            "today_fixing",
            ForeignSetting.MODEL,
            dal.TodayFixingPolicy_.REQUIREHISTORICAL,
        ),
        (
            "ScriptValuationSettings_",
            "today_fixing",
            ForeignSetting.HISTORY,
            dal.TodayFixingPolicy_.MODEL,
        ),
        ("ScriptProductSettings_", "default_index", ForeignSetting.INDEX, "EQ[OLD]"),
        ("MonteCarloSettings_", "method", ForeignSetting.METHOD, "mrg32"),
    ],
)
def test_settings_reject_foreign_string_enums(
    class_name, field, bad, previous, construct
):
    cls = getattr(dal, class_name)
    settings = cls(**{field: previous})
    with pytest.raises(TypeError) as error:
        if construct:
            cls(**{field: bad})
        else:
            setattr(settings, field, bad)
    assert getattr(settings, field) == previous
    message = str(error.value)
    assert all(
        part in message
        for part in ("InvalidSetting", class_name, field, bad.value, "expected")
    )
    if field == "today_fixing":
        assert all(
            part in message
            for part in ("InvalidTodayFixingPolicy", "Model", "RequireHistorical")
        )


@pytest.mark.parametrize("construct", [True, False], ids=["constructor", "setter"])
@pytest.mark.parametrize("text_type", [str, SettingText, dal.String_])
@pytest.mark.parametrize(
    "class_name,field,text,expected",
    [
        (
            "ScriptValuationSettings_",
            "today_fixing",
            "Model",
            dal.TodayFixingPolicy_.MODEL,
        ),
        (
            "ScriptValuationSettings_",
            "today_fixing",
            "RequireHistorical",
            dal.TodayFixingPolicy_.REQUIREHISTORICAL,
        ),
        ("ScriptProductSettings_", "default_index", "eq[A]", "eq[A]"),
        ("MonteCarloSettings_", "method", "SoBoL", "SoBoL"),
    ],
)
def test_settings_accept_supported_text(
    class_name, field, text, expected, text_type, construct
):
    cls = getattr(dal, class_name)
    settings = cls()
    if construct:
        settings = cls(**{field: text_type(text)})
    else:
        setattr(settings, field, text_type(text))
    assert getattr(settings, field) == expected


@pytest.mark.parametrize("construct", [True, False], ids=["constructor", "setter"])
@pytest.mark.parametrize(
    "policy",
    [dal.TodayFixingPolicy_.MODEL, dal.TodayFixingPolicy_.REQUIREHISTORICAL],
)
def test_settings_accept_native_today_policy(policy, construct):
    settings = dal.ScriptValuationSettings_()
    if construct:
        settings = dal.ScriptValuationSettings_(today_fixing=policy)
    else:
        settings.today_fixing = policy
    assert settings.today_fixing == policy


@pytest.mark.parametrize("layer", [dal, native])
@pytest.mark.parametrize("text_type", [str, SettingText, dal.String_, ForeignSetting])
def test_event_text_remains_compatible(layer, text_type):
    today = dal.Date_(2026, 9, 12)
    valuation = layer.ScriptValuationSettings_(evaluation_date=today)
    product = layer.Product_New([dal.Cell_(today)], [text_type("pay PAYS FIX(EQ[A])")])
    model = dal.BSModelData_New(100.0, 0.0, 0.0, 0.0)
    result = layer.MonteCarlo_ValueWithSettings(product, model, 1, valuation=valuation)
    assert result == {"PV": 100.0}


class IndexCount:
    def __index__(self):
        return 1


@pytest.mark.parametrize("entry", ["MonteCarlo_Value", "MonteCarlo_ValueWithSettings", "ScriptSimulation_Explain"])
@pytest.mark.parametrize(
    "value",
    [True, False, 1.0, 1.5, None, "2", float("nan"), float("inf"), IntChoice.ONE],
)
def test_path_type_errors_keep_context(entry, value):
    with pytest.raises(TypeError) as error:
        getattr(native, entry)(None, None, value)
    message = str(error.value)
    assert all(
        part in message
        for part in (
            "InvalidPathCount",
            entry,
            "num_path",
            "positive integer",
            "2147483647",
        )
    )


@pytest.mark.parametrize("entry", ["MonteCarlo_Value", "MonteCarlo_ValueWithSettings", "ScriptSimulation_Explain"])
@pytest.mark.parametrize("value", [0, -1, 2**31, 2**100])
def test_path_range_errors_keep_context(entry, value):
    with pytest.raises(RuntimeError) as error:
        getattr(native, entry)(None, None, value)
    assert all(
        part in str(error.value)
        for part in ("InvalidPathCount", "num_path", str(value), "2147483647")
    )


@pytest.mark.parametrize("entry", ["MonteCarlo_Value", "MonteCarlo_ValueWithSettings", "ScriptSimulation_Explain"])
@pytest.mark.parametrize("value", [1, IndexCount(), 2**31 - 1])
def test_path_conversion_accepts_integer_protocol_and_upper_bound(entry, value):
    # The next native precondition observes successful conversion without allocating paths.
    with pytest.raises(RuntimeError, match="product=null; expected a non-null product"):
        getattr(native, entry)(None, None, value)


@pytest.mark.parametrize(
    "class_name,field,bad,kind,identifier",
    [
        ("ScriptProductSettings_", "default_index", None, TypeError, "InvalidSetting"),
        (
            "ScriptProductSettings_",
            "default_index",
            b"EQ[A]",
            TypeError,
            "InvalidSetting",
        ),
        (
            "ScriptProductSettings_",
            "default_index",
            "EQ[A]\0tail",
            RuntimeError,
            "InvalidSetting",
        ),
        (
            "ScriptValuationSettings_",
            "evaluation_date",
            "2026-09-12",
            TypeError,
            "InvalidSetting",
        ),
        (
            "ScriptValuationSettings_",
            "evaluation_date",
            datetime.date(2026, 9, 12),
            TypeError,
            "InvalidSetting",
        ),
        (
            "ScriptValuationSettings_",
            "today_fixing",
            None,
            TypeError,
            "InvalidTodayFixingPolicy",
        ),
        (
            "ScriptValuationSettings_",
            "today_fixing",
            1,
            TypeError,
            "InvalidTodayFixingPolicy",
        ),
        (
            "ScriptValuationSettings_",
            "today_fixing",
            "model",
            RuntimeError,
            "InvalidTodayFixingPolicy",
        ),
        (
            "ScriptValuationSettings_",
            "today_fixing",
            "Model ",
            RuntimeError,
            "InvalidTodayFixingPolicy",
        ),
        (
            "ScriptValuationSettings_",
            "today_fixing",
            "MODEL",
            RuntimeError,
            "InvalidTodayFixingPolicy",
        ),
        (
            "ScriptValuationSettings_",
            "today_fixing",
            "Model\0",
            RuntimeError,
            "InvalidTodayFixingPolicy",
        ),
        ("ScriptValuationSettings_", "fixings", {}, TypeError, "InvalidSetting"),
        ("MonteCarloSettings_", "method", b"sobol", TypeError, "InvalidSetting"),
        ("MonteCarloSettings_", "method", "sobol ", RuntimeError, "InvalidSetting"),
        ("MonteCarloSettings_", "method", "sobol\0", RuntimeError, "InvalidSetting"),
        ("MonteCarloSettings_", "use_bb", 1, TypeError, "InvalidSetting"),
        ("MonteCarloSettings_", "enable_aad", None, TypeError, "InvalidSetting"),
        ("MonteCarloSettings_", "compiled", 0, TypeError, "InvalidSetting"),
        ("MonteCarloSettings_", "smooth", True, TypeError, "InvalidSmoothing"),
        ("MonteCarloSettings_", "smooth", "0.1", TypeError, "InvalidSmoothing"),
        ("MonteCarloSettings_", "smooth", 0, RuntimeError, "InvalidSmoothing"),
        ("MonteCarloSettings_", "smooth", -1, RuntimeError, "InvalidSmoothing"),
        (
            "MonteCarloSettings_",
            "smooth",
            float("nan"),
            RuntimeError,
            "InvalidSmoothing",
        ),
        (
            "MonteCarloSettings_",
            "smooth",
            float("inf"),
            RuntimeError,
            "InvalidSmoothing",
        ),
        ("MonteCarloSettings_", "smooth", 10**400, RuntimeError, "InvalidSmoothing"),
    ],
)
def test_settings_constructor_and_transactional_setter_errors(
    class_name, field, bad, kind, identifier
):
    cls = getattr(dal, class_name)
    for construct in (True, False):
        instance = cls()
        previous = getattr(instance, field)
        with pytest.raises(kind) as error:
            if construct:
                cls(**{field: bad})
            else:
                setattr(instance, field, bad)
        assert identifier in str(error.value)
        assert field in str(error.value)
        assert "expected" in str(error.value)
        assert getattr(instance, field) == previous


def test_invalid_native_date_is_rejected_without_formatting_it():
    invalid = dal.Date_(1900, 1, 1)
    with pytest.raises(
        RuntimeError, match=r"InvalidSetting.*valuation.evaluationDate_.*valid date"
    ):
        dal.ScriptValuationSettings_(evaluation_date=invalid)


@pytest.mark.parametrize(
    "class_name",
    ["ScriptProductSettings_", "ScriptValuationSettings_", "MonteCarloSettings_"],
)
def test_settings_reject_unknown_fields_and_positional_construction(class_name):
    cls = getattr(dal, class_name)
    with pytest.raises(TypeError):
        cls(None)
    with pytest.raises(TypeError, match="unknown"):
        cls(unknown=True)
    with pytest.raises(AttributeError):
        cls().unknown = True


@pytest.mark.parametrize("layer", [dal, native])
@pytest.mark.parametrize(
    "event,kind",
    [
        (dal.String_("pay PAYS 1"), None),
        (None, TypeError),
        (b"pay PAYS 1", TypeError),
        ("pay PAYS 1\0ignored", RuntimeError),
    ],
)
def test_product_event_conversion_and_nul(layer, event, kind):
    dates = [dal.Cell_(dal.Date_(2026, 9, 22))]
    if kind is None:
        product = layer.Product_New(dates, [event])
        assert dal.Product_Describe(product)["input_rows"][0]["text"] == "pay PAYS 1"
    else:
        with pytest.raises(kind) as error:
            layer.Product_New(dates, [event])
        assert all(
            part in str(error.value)
            for part in ("InvalidSetting", "Product_New", "events", "row=1", "expected")
        )


def test_product_date_cell_conversion_context():
    with pytest.raises(TypeError) as error:
        dal.Product_New([object()], ["pay PAYS 1"])
    assert all(
        part in str(error.value)
        for part in (
            "InvalidSetting",
            "Product_New",
            "events_dates",
            "row=1",
            "expected",
        )
    )
    with pytest.raises(TypeError) as error:
        native.Product_New([dal.Date_(2026, 9, 22)], ["pay PAYS 1"])
    assert all(
        part in str(error.value)
        for part in ("InvalidSetting", "Product_New", "dates", "row=1", "expected")
    )
    with pytest.raises(RuntimeError, match="NUL"):
        dal.Product_New(
            ["SCALE\0suffix", dal.Date_(2026, 9, 22)], ["2", "pay PAYS SCALE"]
        )


@pytest.mark.parametrize("layer", [dal, native])
def test_api_keyword_boundaries_and_settings_types(layer):
    dates = [dal.Cell_(dal.Date_(2026, 9, 22))]
    events = ["pay PAYS 1"]
    date_keyword = "events_dates" if layer is dal else "dates"
    product = layer.Product_New(
        **{date_keyword: dates, "events": events, "settings": None}
    )
    data = dal.BSModelData_New(100.0, 0.0, 0.0, 0.0)
    with pytest.raises(TypeError):
        layer.Product_New(dates, events, None)
    wrong_keyword = "dates" if layer is dal else "events_dates"
    with pytest.raises(TypeError, match=wrong_keyword):
        layer.Product_New(**{wrong_keyword: dates, "events": events})
    with pytest.raises(TypeError):
        layer.Product_New(dates, events, events=events)
    with pytest.raises(TypeError, match="unexpected"):
        layer.Product_New(dates, events, unexpected=True)
    for value in [{}, "Model", dal.MonteCarloSettings_()]:
        with pytest.raises(TypeError, match="InvalidSetting.*settings"):
            layer.Product_New(dates, events, settings=value)
    for entry, field in [
        ("MonteCarlo_ValueWithSettings", "valuation"),
        ("MonteCarlo_ValueWithSettings", "simulation"),
        ("ScriptValuation_Explain", "valuation"),
        ("ScriptSimulation_Explain", "valuation"),
        ("ScriptSimulation_Explain", "simulation"),
    ]:
        args = [product, data] + ([] if entry == "ScriptValuation_Explain" else [1])
        with pytest.raises(TypeError):
            getattr(layer, entry)(*args, None)
        for value in [{}, "Model", dal.ScriptProductSettings_()]:
            with pytest.raises(TypeError, match=f"InvalidSetting.*{field}"):
                getattr(layer, entry)(*args, **{field: value})
        with pytest.raises(TypeError, match="method"):
            getattr(layer, entry)(*args, method="sobol")
    for field in ("settings", "valuation", "simulation"):
        with pytest.raises(TypeError, match=field):
            layer.MonteCarlo_Value(product, data, 1, **{field: None})
    with pytest.raises(TypeError):
        layer.MonteCarlo_Value(product, data, 1, num_path=1)


def test_today_policy_errors_list_values_and_reject_unnamed_enum():
    for value in ["", "requirehistorical", "Model\0tail", dal.TodayFixingPolicy_(2)]:
        with pytest.raises(RuntimeError) as error:
            dal.ScriptValuationSettings_(today_fixing=value)
        assert all(
            part in str(error.value)
            for part in (
                "InvalidSetting",
                "InvalidTodayFixingPolicy",
                "today_fixing",
                "Model",
                "RequireHistorical",
            )
        )
    with pytest.raises(TypeError, match="InvalidPathCount"):
        dal.MonteCarlo_Value(None, None, dal.TodayFixingPolicy_.REQUIREHISTORICAL)


def test_legacy_smoothing_rejects_invalid_values_in_both_layers():
    product = dal.Product_New([dal.Date_(2026, 9, 22)], ["pay PAYS 1"])
    model = dal.BSModelData_New(100.0, 0.0, 0.0, 0.0)
    for layer in (dal, native):
        for value in [0.0, -1.0, float("inf"), float("nan")]:
            with pytest.raises(RuntimeError) as error:
                layer.MonteCarlo_Value(product, model, 1, smooth=value)
            assert all(
                part in str(error.value)
                for part in (
                    "InvalidSetting",
                    "InvalidSmoothing",
                    "simulation.smooth_",
                    "finite positive",
                )
            )


@pytest.mark.parametrize("field", ["use_bb", "enable_aad", "compiled"])
@pytest.mark.parametrize(
    "bad", [0, 1, "False", {}, IntChoice.ONE, dal.TodayFixingPolicy_.MODEL]
)
def test_every_new_boolean_field_is_strict(field, bad):
    for setter in [False, True]:
        settings = dal.MonteCarloSettings_()
        previous = getattr(settings, field)
        with pytest.raises(TypeError, match=f"InvalidSetting.*{field}.*expected bool"):
            if setter:
                setattr(settings, field, bad)
            else:
                dal.MonteCarloSettings_(**{field: bad})
        assert getattr(settings, field) == previous


def test_lsmc_training_paths_accepts_optional_positive_integers():
    assert dal.MonteCarloSettings_().lsmc_training_paths is None
    for count in [None, 1, 16384, 2**31 - 1, IndexCount()]:
        expected = None if count is None else int(count)
        settings = dal.MonteCarloSettings_(lsmc_training_paths=count)
        assert settings.lsmc_training_paths == expected
        assert copy.deepcopy(settings).lsmc_training_paths == expected
        settings.lsmc_training_paths = 17
        settings.lsmc_training_paths = count
        assert settings.lsmc_training_paths == expected


@pytest.mark.parametrize("construct", [True, False], ids=["constructor", "setter"])
@pytest.mark.parametrize(
    "bad",
    [True, False, 1.5, 3.0, 0, -1, 2**31, 2**100, "3", {}, IntChoice.ONE, dal.TodayFixingPolicy_.MODEL],
)
def test_lsmc_training_paths_rejects_invalid_counts(bad, construct):
    settings = dal.MonteCarloSettings_(lsmc_training_paths=128)
    with pytest.raises((TypeError, RuntimeError), match="InvalidLsmcTrainingPaths") as error:
        if construct:
            dal.MonteCarloSettings_(lsmc_training_paths=bad)
        else:
            settings.lsmc_training_paths = bad
    assert all(
        part in str(error.value)
        for part in ("InvalidSetting", "MonteCarloSettings_", "lsmc_training_paths", "expected", "positive")
    )
    assert settings.lsmc_training_paths == 128


def test_lsmc_validation_paths_accepts_optional_positive_integers():
    assert dal.MonteCarloSettings_().lsmc_validation_paths is None
    for count in [None, 1, 1024, 2**31 - 1, IndexCount()]:
        expected = None if count is None else int(count)
        settings = dal.MonteCarloSettings_(lsmc_validation_paths=count)
        assert settings.lsmc_validation_paths == expected
        assert copy.deepcopy(settings).lsmc_validation_paths == expected
        settings.lsmc_validation_paths = count
        assert settings.lsmc_validation_paths == expected


@pytest.mark.parametrize("construct", [True, False], ids=["constructor", "setter"])
@pytest.mark.parametrize("bad", [True, 0, -1, 1.5, 2**31, "3", IntChoice.ONE])
def test_lsmc_validation_paths_rejects_invalid_counts(bad, construct):
    settings = dal.MonteCarloSettings_(lsmc_validation_paths=128)
    with pytest.raises((TypeError, RuntimeError), match="InvalidLsmcValidationPaths"):
        if construct:
            dal.MonteCarloSettings_(lsmc_validation_paths=bad)
        else:
            settings.lsmc_validation_paths = bad
    assert settings.lsmc_validation_paths == 128


def test_lsmc_rqmc_settings_round_trip():
    settings = dal.MonteCarloSettings_(lsmc_rqmc_replicates=8, lsmc_training_seed=17, lsmc_pricing_seed=29)
    assert settings.lsmc_rqmc_replicates == 8
    assert settings.lsmc_training_seed == 17
    assert settings.lsmc_pricing_seed == 29
    copied = copy.deepcopy(settings)
    assert copied.lsmc_rqmc_replicates == 8
    assert copied.lsmc_training_seed == 17
    assert copied.lsmc_pricing_seed == 29
    settings.lsmc_rqmc_replicates = 4
    settings.lsmc_training_seed = 0
    settings.lsmc_pricing_seed = None
    assert (settings.lsmc_rqmc_replicates, settings.lsmc_training_seed, settings.lsmc_pricing_seed) == (4, 0, None)


@pytest.mark.parametrize("field,bad", [("lsmc_rqmc_replicates", 1), ("lsmc_training_seed", -1), ("lsmc_pricing_seed", -1)])
@pytest.mark.parametrize("construct", [True, False], ids=["constructor", "setter"])
def test_lsmc_rqmc_settings_reject_invalid_values(field, bad, construct):
    settings = dal.MonteCarloSettings_(lsmc_rqmc_replicates=4, lsmc_training_seed=17, lsmc_pricing_seed=29)
    previous = getattr(settings, field)
    with pytest.raises(RuntimeError, match="InvalidLsmc"):
        if construct:
            dal.MonteCarloSettings_(**{field: bad})
        else:
            setattr(settings, field, bad)
    assert getattr(settings, field) == previous


@pytest.mark.parametrize("field", ["lsmc_rqmc_replicates", "lsmc_training_seed", "lsmc_pricing_seed"])
def test_lsmc_rqmc_settings_reject_boolean(field):
    with pytest.raises(TypeError, match=f"InvalidSetting.*{field}"):
        dal.MonteCarloSettings_(**{field: True})


def test_lsmc_basis_degree_accepts_integers_in_range():
    assert dal.MonteCarloSettings_().lsmc_basis_degree == 3
    for degree in [1, 3, 8, IndexCount()]:
        expected = int(degree)
        settings = dal.MonteCarloSettings_(lsmc_basis_degree=degree)
        assert settings.lsmc_basis_degree == expected
        settings.lsmc_basis_degree = 3
        settings.lsmc_basis_degree = degree
        assert settings.lsmc_basis_degree == expected


@pytest.mark.parametrize("construct", [True, False], ids=["constructor", "setter"])
@pytest.mark.parametrize(
    "bad",
    [
        True,
        False,
        1.5,
        3.0,
        0,
        -1,
        9,
        2**40,
        "3",
        None,
        {},
        IntChoice.ONE,
        dal.TodayFixingPolicy_.MODEL,
    ],
)
def test_lsmc_basis_degree_rejects_non_integers_and_out_of_range(bad, construct):
    settings = dal.MonteCarloSettings_()
    with pytest.raises((TypeError, RuntimeError), match="InvalidLsmcBasisDegree") as error:
        if construct:
            dal.MonteCarloSettings_(lsmc_basis_degree=bad)
        else:
            settings.lsmc_basis_degree = bad
    message = str(error.value)
    assert all(
        part in message
        for part in ("InvalidSetting", "MonteCarloSettings_", "lsmc_basis_degree", "expected", "1", "8")
    )
    assert settings.lsmc_basis_degree == 3


@pytest.mark.parametrize("compiled", [False, True])
def test_script_simulation_explain_reports_exercise_events(compiled):
    today = dal.Date_(2026, 9, 12)
    first = dal.Date_(2026, 12, 12)
    second = dal.Date_(2027, 3, 12)
    product = dal.Product_New(
        [first, second],
        ["EXERCISE MAX(100.0 - spot(), 0.0)", "EXERCISE MAX(100.0 - spot(), 0.0)"],
    )
    model = dal.BSModelData_New(100.0, 0.2, 0.05, 0.0)
    valuation = dal.ScriptValuationSettings_(evaluation_date=today)
    simulation = dal.MonteCarloSettings_(lsmc_basis_degree=5, compiled=compiled, lsmc_training_paths=4096)
    diagnostic = dal.ScriptSimulation_Explain(
        product, model, 4096, valuation=valuation, simulation=simulation
    )
    assert diagnostic == json.loads(
        native.ScriptSimulation_Explain(
            product, model, 4096, valuation=valuation, simulation=simulation
        )
    )
    assert diagnostic["schema"] == "dal.script-simulation/1"
    assert diagnostic["evaluation_date"] == "2026-09-12"
    assert diagnostic["simulation"]["lsmc_basis_degree"] == 5
    assert diagnostic["simulation"]["lsmc_training_paths"] == 4096
    assert diagnostic["simulation"]["lsmc_validation_paths"] is None
    assert diagnostic["simulation"]["compiled"] is compiled
    assert diagnostic["n_paths"] == 4096
    events = diagnostic["exercise_events"]
    assert [event["event_id"] for event in events] == [0, 1]
    assert [event["date"] for event in events] == ["2026-12-12", "2027-03-12"]
    assert all(event["basis_degree"] == 5 for event in events)
    assert all(event["num_coefficients"] == 6 for event in events)
    assert all(len(event["coefficients"]) == 6 for event in events)
    assert all(event["regressor_index"] is None for event in events)
    # in-the-money condition-true paths enter the regression: exact sobol counts on
    # this ATM put, identical in the tree-walk and compiled engines (the first date
    # matches the dal-public pin in test_script_diagnostics.cpp)
    assert [event["num_cond_true_paths"] for event in events] == [1926, 1891]
    assert all(event["degenerate"] is False for event in events)
    assert all(event["degenerate_reason"] is None for event in events)
    assert all(0.0 < event["exercise_rate"] <= 1.0 for event in events)
    more_pricing = dal.ScriptSimulation_Explain(
        product, model, 8193, valuation=valuation, simulation=simulation
    )
    assert more_pricing["n_paths"] == 8193
    for trained, repriced in zip(events, more_pricing["exercise_events"]):
        assert trained["coefficients"] == repriced["coefficients"]
        assert trained["num_cond_true_paths"] == repriced["num_cond_true_paths"]
    plain = dal.Product_New([second], ["pay PAYS 1.0"])
    without_exercise = dal.ScriptSimulation_Explain(plain, model, 1024)
    assert without_exercise["exercise_events"] == []
    assert without_exercise["n_paths"] == 1024
    aad = dal.MonteCarloSettings_(enable_aad=True)
    with pytest.raises(RuntimeError, match="UnsupportedExecutionMode.*enable_aad"):
        dal.ScriptSimulation_Explain(product, model, 4096, simulation=aad)
    with pytest.raises(RuntimeError, match="InvalidPathCount"):
        dal.ScriptSimulation_Explain(product, model, 0)
    for value in [{}, "Model", dal.ScriptProductSettings_()]:
        with pytest.raises(TypeError, match="InvalidSetting.*simulation"):
            dal.ScriptSimulation_Explain(product, model, 4096, simulation=value)


def test_lsmc_rqmc_exposes_conditional_error_and_prices_replicates():
    product = dal.Product_New(
        [dal.Date_(2026, 12, 12), dal.Date_(2027, 3, 12)],
        ["EXERCISE MAX(100.0 - spot(), 0.0)"] * 2,
    )
    model = dal.BSModelData_New(100.0, 0.2, 0.05, 0.0)
    valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 9, 12))
    simulation = dal.MonteCarloSettings_(
        lsmc_training_paths=1024,
        lsmc_rqmc_replicates=4,
        lsmc_training_seed=17,
        lsmc_pricing_seed=29,
        smooth=1e-10,
    )
    diagnostic = dal.ScriptSimulation_Explain(product, model, 257, valuation=valuation, simulation=simulation)
    uncertainty = diagnostic["uncertainty"]
    assert uncertainty["mode"] == "rqmc_conditional_policy"
    assert uncertainty["replicate_count"] == 4
    assert uncertainty["training_seed"] == 17
    assert uncertainty["pricing_seed"] == 29
    assert uncertainty["pricing_paths_total"] == 1028
    assert len(uncertainty["replicate_means"]) == 4
    assert uncertainty["replicate_mean_se"] > 0
    hard = dal.MonteCarlo_ValueWithSettings(product, model, 257, valuation=valuation, simulation=simulation)
    assert math.isclose(hard["PV"], sum(uncertainty["replicate_means"]) / 4, abs_tol=1e-10)
    simulation.enable_aad = True
    fuzzy = dal.MonteCarlo_ValueWithSettings(product, model, 257, valuation=valuation, simulation=simulation)
    assert math.isclose(fuzzy["PV"], hard["PV"], abs_tol=1e-7)


@pytest.mark.parametrize(
    "bad",
    [
        20260912,
        1.5,
        datetime.datetime(2026, 9, 12),
        dal.DateTime_(dal.Date_(2026, 9, 12), 0),
        dal.Cell_(dal.Date_(2026, 9, 12)),
        dal.String_("2026-09-12"),
        dal.TodayFixingPolicy_.MODEL,
        {},
    ],
)
def test_evaluation_date_requires_dal_date(bad):
    for setter in [False, True]:
        settings = dal.ScriptValuationSettings_()
        with pytest.raises(
            TypeError, match="InvalidSetting.*evaluation_date.*valid date"
        ):
            if setter:
                settings.evaluation_date = bad
            else:
                dal.ScriptValuationSettings_(evaluation_date=bad)
        assert settings.evaluation_date is None


def test_field_reset_and_unicode_copy():
    settings = dal.ScriptValuationSettings_(
        evaluation_date=dal.Date_(2026, 9, 12),
        fixings=dal.MarketFixingSnapshot_New({}),
    )
    settings.evaluation_date = None
    settings.fixings = None
    settings.today_fixing = dal.TodayFixingPolicy_.MODEL
    assert settings.evaluation_date is settings.fixings is None
    execution = dal.MonteCarloSettings_(compiled=True)
    execution.compiled = None
    assert execution.compiled is None
    product_settings = dal.ScriptProductSettings_(default_index="eq[测试]")
    product = dal.Product_New(
        [dal.Date_(2026, 9, 22)], ["pay PAYS SPOT()"], settings=product_settings
    )
    assert dal.Product_Describe(product)["default_index"]["original"] == "eq[测试]"
    assert copy.deepcopy(product_settings).default_index == "eq[测试]"


def test_invalid_product_dates_and_default_are_native_validation():
    with pytest.raises(RuntimeError, match=r"InvalidSetting.*dates.size.*events.size"):
        dal.Product_New([dal.Date_(2026, 9, 22)], [])
    invalid = dal.Product_New([dal.Date_(1900, 1, 1)], ["pay PAYS 1"])
    with pytest.raises(RuntimeError) as error:
        dal.Product_Describe(invalid)
    assert all(
        part in str(error.value)
        for part in ("InvalidFixingDate", "row=1", "dates/events")
    )
    settings = dal.ScriptProductSettings_(default_index="EQ[A]trailing")
    product = dal.Product_New(
        [dal.Date_(2026, 9, 22)], ["pay PAYS 1"], settings=settings
    )
    with pytest.raises(RuntimeError, match="(?s)InvalidIndex.*product.defaultIndex_"):
        dal.Product_Describe(product)


@pytest.mark.parametrize(
    "text,identifier",
    [
        ("pay PAYS FIX(EQ[A], 2026-02-30)", "InvalidFixingDate"),
        ("pay PAYS FIX(EQ[A], 2026-09-23)", "LookAheadObservation"),
        ("pay PAYS FIX(EQ[A], 2026-09-11 11:00:00)", "InvalidFixingDate"),
    ],
)
def test_fix_date_errors_preserve_source(text, identifier):
    product = dal.Product_New([dal.Date_(2026, 9, 22)], [text])
    with pytest.raises(RuntimeError) as error:
        dal.Product_Describe(product)
    assert all(
        part in str(error.value) for part in (identifier, "row=1", "line=", "column=")
    )


@pytest.mark.parametrize("entry", ["MonteCarlo_Value", "MonteCarlo_ValueWithSettings", "ScriptSimulation_Explain"])
def test_huge_path_integer_is_rejected_even_beyond_python_repr_limit(entry):
    with pytest.raises(RuntimeError, match="InvalidPathCount.*num_path.*2147483647"):
        getattr(native, entry)(None, None, 10**5000)


def test_legacy_smooth_overflow_keeps_field_context_and_valid_float_protocol():
    class Width:
        def __float__(self):
            return 0.02

    product = dal.Product_New([dal.Date_(2026, 9, 22)], ["pay PAYS 1"])
    model = dal.BSModelData_New(100.0, 0.0, 0.0, 0.0)
    assert dal.MonteCarlo_Value(product, model, 1, smooth=Width())["PV"] == 1.0
    assert dal.MonteCarlo_Value(product, model, 1, smooth=True)["PV"] == 1.0
    with pytest.raises(RuntimeError, match="InvalidSmoothing.*smooth.*finite positive"):
        native.MonteCarlo_Value(product, model, 1, smooth=10**400)


def test_lsmc_policy_risk_settings_validate_exact_mode_and_bump():
    defaults = dal.MonteCarloSettings_()
    assert defaults.lsmc_policy_risk_mode == "Frozen"
    assert defaults.lsmc_policy_bump_relative == 1e-3
    settings = dal.MonteCarloSettings_(
        enable_aad=True,
        lsmc_policy_risk_mode="RetrainedBump",
        lsmc_policy_bump_relative=2e-3,
    )
    assert copy.deepcopy(settings).lsmc_policy_risk_mode == "RetrainedBump"
    assert settings.lsmc_policy_bump_relative == 2e-3
    for bad in ["retrainedbump", "Frozen ", "Other", None, 2]:
        with pytest.raises((TypeError, RuntimeError), match="InvalidLsmcPolicyRiskMode"):
            dal.MonteCarloSettings_(lsmc_policy_risk_mode=bad)
    for bad in [True, 0.0, -0.1, 0.11, float("nan"), float("inf"), "0.01"]:
        with pytest.raises((TypeError, RuntimeError), match="InvalidLsmcPolicyBumpRelative"):
            dal.MonteCarloSettings_(lsmc_policy_bump_relative=bad)
