"""Unit tests for the DAL gateway."""

from __future__ import annotations

import hashlib
import inspect
import json
from types import SimpleNamespace
from typing import Any

import dal  # the fake installed by conftest
import pytest

from app.services.dal_gateway import DalGateway, NativeDalCapabilityError, ValuationRequest
from tests.fake_gateway import NativeDalGateway


def make_gateway() -> DalGateway:
    return DalGateway()


def _european_request(**overrides) -> ValuationRequest:
    base: dict[str, object] = dict(
        event_dates=["STRIKE", {"date": "2023-09-15"}],
        events=["100.0", "call pays MAX(spot() - STRIKE, 0.0)"],
        model_kind="BSModelData_",
        model_params={"spot": 100.0, "vol": 0.2, "rate": 0.0, "div": 0.0},
        num_paths=1024,
        enable_aad=True,
        evaluation_date=(2022, 9, 15),
    )
    base.update(overrides)
    return ValuationRequest(**base)  # type: ignore[arg-type]


def test_gateway_backend():
    gw = make_gateway()
    assert gw.is_native is True
    assert gw.backend_name == "dal"


def test_gateway_rejects_fallbacks_without_explicit_test_gateway():
    gateway = NativeDalGateway()

    with pytest.raises(NativeDalCapabilityError, match="synthetic production fallbacks"):
        gateway._require_test_double_fallback("single-curve calibration")


def test_evaluation_date_roundtrip():
    gw = make_gateway()
    gw.set_evaluation_date(2022, 9, 15)
    assert gw.get_evaluation_date() == "2022-09-15"


def test_debug_product_renders_rows():
    gw = make_gateway()
    debug = gw.debug_product(
        [{"date": "2025-09-15"}, "STRIKE"],
        ["call pays MAX(spot() - STRIKE, 0.0)", "120.0"],
    )
    assert "STRIKE" in debug
    assert "2025-09-15" in debug


def test_value_routes_through_monte_carlo():
    """The gateway must price via MonteCarlo_Value.

    The fake ``dal`` records the call, so this asserts pricing routes through
    ``MonteCarlo_Value`` (the symbol exported by
    ``dal-python/src/bindings/value.cpp``) with the right arguments.
    """
    dal.monte_carlo_calls.clear()
    gw = make_gateway()
    res = gw.value(_european_request(num_paths=2048, method="sobol", enable_aad=True))
    assert res["PV"] == 8.0
    assert res["d_spot"] == 0.5
    calls = dal.monte_carlo_calls
    assert len(calls) == 1
    assert calls[0]["num_path"] == 2048
    assert calls[0]["method"] == "sobol"
    assert calls[0]["enable_aad"] is True


def test_value_maps_public_pseudo_method_to_native_mrg32():
    """The web RNG name must resolve to a method accepted by native DAL."""
    dal.monte_carlo_calls.clear()
    gw = make_gateway()

    gw.value(_european_request(method="pseudo"))

    assert len(dal.monte_carlo_calls) == 1
    assert dal.monte_carlo_calls[0]["method"] == "mrg32"


def test_value_rejects_unknown_monte_carlo_method():
    gw = make_gateway()

    with pytest.raises(ValueError, match="Unsupported Monte Carlo method"):
        gw.value(_european_request(method="unknown"))


def test_value_restores_evaluation_date_after_pricing():
    """H5 regression: evaluation_date must be restored after value() returns."""
    import dal

    dal.EvaluationDate_Set(dal.Date_(2020, 1, 1))
    gw = make_gateway()
    gw.value(_european_request(evaluation_date=(2022, 9, 15)))
    assert str(dal.EvaluationDate_Get()) == "2020-01-01"


def test_value_leaves_date_unchanged_when_no_evaluation_date_given():
    """H5 regression: omitting evaluation_date must not mutate global state."""
    import dal

    dal.EvaluationDate_Set(dal.Date_(2020, 1, 1))
    gw = make_gateway()
    gw.value(_european_request(evaluation_date=None))
    assert str(dal.EvaluationDate_Get()) == "2020-01-01"


def test_value_restores_date_even_on_pricing_failure(monkeypatch):
    """H5 regression: date must be restored even if MonteCarlo_Value raises."""
    import dal

    dal.EvaluationDate_Set(dal.Date_(2020, 1, 1))
    gw = make_gateway()

    monkeypatch.setattr(
        dal, "MonteCarlo_Value", lambda *a, **k: (_ for _ in ()).throw(RuntimeError("boom"))
    )

    with pytest.raises(RuntimeError):
        gw.value(_european_request(evaluation_date=(2022, 9, 15)))

    assert str(dal.EvaluationDate_Get()) == "2020-01-01"


def test_gateway_builds_non_flat_dupire_surface():
    gw = make_gateway()
    surface = [[0.24, 0.23], [0.21, 0.20], [0.19, 0.18]]

    model = gw.build_model(
        "DupireModelData_",
        {
            "spot": 100.0,
            "rate": 0.03,
            "repo": 0.01,
            "spots": [90.0, 100.0, 110.0],
            "times": [0.5, 1.0],
            "vols": surface,
        },
    )

    assert model["vols"] == surface  # nosec B101 - pytest assertions are intentional


def test_curve_lab_pricing_uses_the_shared_native_trade_builder() -> None:
    source = inspect.getsource(DalGateway.price_curve_lab_trades)

    assert "self._curve_lab_native_trade_definitions(" in source
    assert "DepositTradeTerms_" not in source
    assert "FraTradeTerms_" not in source
    assert "FutureTradeTerms_" not in source
    assert "FixedFloatTradeTerms_" not in source
    assert "BasisTradeTerms_" not in source
    assert "XccyTradeTerms_" not in source


def test_curve_lab_native_trade_builder_preserves_all_families_and_fixings(
    monkeypatch,
) -> None:
    families = (
        "DEPOSIT",
        "FRA",
        "FUTURE",
        "OIS",
        "IRS",
        "BASIS_SWAP",
        "XCCY",
    )

    def record(kind: str):
        def construct(*args, **kwargs):
            return {"kind": kind, "args": args, **kwargs}

        return construct

    class FixingIdentity:
        index_name = ""
        fixing_hour = 0
        fixing_minute = 0

    class XccyConvention:
        pass

    class HashableRecord:
        def __init__(self, kind: str, *args: object) -> None:
            self.kind = kind
            self.args = args

        def __hash__(self) -> int:
            return hash((self.kind, self.args))

    class XccyBuilder:
        def Build(self):  # noqa: N802 - mirror native builder API
            return {
                "pair": self.pair,
                "domestic_notional": self.domestic_notional,
                "foreign_notional": self.foreign_notional,
                "convention": vars(self.convention),
                "notional_mode": self.notional_mode,
                "domestic_rate_fixing": vars(self.domestic_rate_fixing),
                "foreign_rate_fixing": vars(self.foreign_rate_fixing),
            }

    native = SimpleNamespace(
        Date_=lambda year, month, day: f"{year:04d}-{month:02d}-{day:02d}",
        DateTime_=record("DATETIME"),
        PeriodLength_New=record("PERIOD"),
        DayBasis_New=record("DAY_BASIS"),
        CollateralType_=lambda value: HashableRecord("COLLATERAL", value),
        RateIndexConvention_New=record("INDEX"),
        RateLegConvention_New=record("LEG"),
        FixingIdentity_=FixingIdentity,
        CrossCurrencyConvention_=XccyConvention,
        CrossCurrencySwapConfigBuilder_=XccyBuilder,
        CurrencyPair_New=record("PAIR"),
        XccyNotionalMode=SimpleNamespace(FIXED="FIXED"),
        RateInstrumentType=SimpleNamespace(**dict.fromkeys(families, None)),
        DepositTradeTerms_=record("DEPOSIT_TERMS"),
        FraTradeTerms_=record("FRA_TERMS"),
        FutureTradeTerms_=record("FUTURE_TERMS"),
        FixedFloatTradeTerms_=record("FIXED_FLOAT_TERMS"),
        OisTradeTerms_=record("OIS_TERMS"),
        IrsTradeTerms_=record("IRS_TERMS"),
        BasisTradeTerms_=record("BASIS_TERMS"),
        XccyTradeTerms_=record("XCCY_TERMS"),
        RateTradeDefinition_=record("TRADE"),
    )
    for family in families:
        setattr(native.RateInstrumentType, family, family)

    common = {
        "trade_date": "2026-01-15",
        "start_date": "2026-01-16",
        "maturity_date": "2027-01-15",
        "currency_or_pair": "USD",
    }
    index = {
        "forecast_tenor": "3M",
        "day_basis": "ACT_365F",
        "collateral": "OIS",
        "index_name": "USD-SOFR",
        "fixing_hour": 10,
        "fixing_minute": 15,
    }
    trades = [
        {
            **common,
            "trade_id": f"{position + 1:032x}",
            "instrument_type": family,
            "terms": terms,
        }
        for position, (family, terms) in enumerate(
            (
                (
                    "DEPOSIT",
                    {
                        **index,
                        "notional": "100",
                        "contract_rate": "0.04",
                        "side": "LEND",
                    },
                ),
                (
                    "FRA",
                    {
                        **index,
                        "notional": "100",
                        "contract_rate": "0.04",
                        "side": "RECEIVE_FLOATING",
                    },
                ),
                (
                    "FUTURE",
                    {
                        **index,
                        "contract_count": "2",
                        "reference_price": "95",
                        "contract_value_per_price_point": "1000",
                        "side": "LONG",
                    },
                ),
                (
                    "OIS",
                    {
                        **index,
                        "notional": "100",
                        "contract_rate": "0.04",
                        "side": "PAY_FIXED",
                        "float_index_name": "USD-OIS",
                    },
                ),
                (
                    "IRS",
                    {
                        **index,
                        "notional": "100",
                        "contract_rate": "0.04",
                        "side": "RECEIVE_FIXED",
                        "float_index_name": "USD-IBOR-3M",
                    },
                ),
                (
                    "BASIS_SWAP",
                    {
                        **index,
                        "notional": "100",
                        "contract_spread": "0.001",
                        "side": "RECEIVE_REFERENCE_PAY_SPREAD",
                        "spread_index_name": "USD-IBOR-3M",
                        "reference_index_name": "USD-IBOR-6M",
                    },
                ),
                (
                    "XCCY",
                    {
                        **index,
                        "position_count": "2",
                        "contract_spread": "0.001",
                        "side": "RECEIVE_NON_SPREAD_PAY_SPREAD",
                        "domestic_notional": "100",
                        "foreign_notional": "90",
                        "fx_spot": "1.1",
                        "domestic_index_name": "USD-SOFR",
                        "foreign_index_name": "EUR-ESTR",
                    },
                ),
            )
        )
    ]
    trades[-1]["currency_or_pair"] = "USD-EUR"
    gateway = make_gateway()
    gateway._dal = native

    definitions = gateway._curve_lab_native_trade_definitions(
        trades,
        "discount-default",
    )

    assert [row["instrument_id"] for row in definitions] == [trade["trade_id"] for trade in trades]
    assert [row["instrument_type"] for row in definitions] == list(families)
    assert [row["terms"]["kind"] for row in definitions] == [
        "DEPOSIT_TERMS",
        "FRA_TERMS",
        "FUTURE_TERMS",
        "OIS_TERMS",
        "IRS_TERMS",
        "BASIS_TERMS",
        "XCCY_TERMS",
    ]
    assert definitions[1]["terms"]["fixing_identity"].index_name == "USD-SOFR"
    assert definitions[3]["terms"]["value"]["fixing_identity"].index_name == "USD-OIS"
    assert definitions[5]["terms"]["spread_fixing_identity"].index_name == "USD-IBOR-3M"
    assert definitions[5]["terms"]["reference_fixing_identity"].index_name == "USD-IBOR-6M"
    config = definitions[6]["terms"]["config"]
    assert config["pair"]["args"] == ("USD", "EUR")
    assert config["domestic_rate_fixing"]["index_name"] == "USD-SOFR"
    assert config["foreign_rate_fixing"]["index_name"] == "EUR-ESTR"

    captured: list[list[dict[str, Any]]] = []

    def required_fixings(native_trades, _evaluation_time):
        captured.append(native_trades)
        return []

    native._dal = SimpleNamespace(
        _RequiredHistoricalRateTradeFixings=required_fixings,
    )
    assert (
        gateway.curve_lab_required_historical_fixings(
            trades,
            "2026-01-15T10:30:00Z",
            "discount-default",
        )
        == []
    )

    def plain(value):
        if isinstance(value, dict):
            return {key: plain(item) for key, item in value.items()}
        if isinstance(value, (list, tuple)):
            return [plain(item) for item in value]
        if hasattr(value, "__dict__"):
            return plain(vars(value))
        return value

    def definition_hash(value: object) -> str:
        return hashlib.sha256(
            json.dumps(
                plain(value),
                ensure_ascii=True,
                allow_nan=False,
                separators=(",", ":"),
                sort_keys=True,
            ).encode("ascii")
        ).hexdigest()

    # Canonical fake-native definitions captured from baseline b089b8c9.
    baseline_hash = "4fdaf76f74e59273ab85c83ec9c97d4e05e0b1fdffdb89f576831ea1fdcd3cb6"
    assert definition_hash(definitions) == baseline_hash
    assert len(captured) == 1
    assert definition_hash(captured[0]) == baseline_hash

    priced_definitions: list[list[dict[str, Any]]] = []

    def price_rate_trades(*, trades, market):
        assert market["kind"] == "MARKET"
        priced_definitions.append(trades)
        return [
            SimpleNamespace(
                succeeded=True,
                pv=str(position),
                currency="USD",
                required_historical_fixings=[],
                missing_historical_fixings=[],
                dependency_component_keys=[],
                error="",
            )
            for position in range(len(trades))
        ]

    native.MarketFixingSnapshot_New = record("FIXINGS")
    native.CurveBlock_New = record("CURVE_BLOCK")
    native.CrossCurrencyMarket_New = record("XCCY_MARKET")
    native.RatePricingMarket_ = record("MARKET")
    native.PriceRateTrades = price_rate_trades
    gradients = {
        "discount-default": ["1.25", "2.5"],
        "discount-eur": ["3.75"],
        "basis-usd-eur": ["5"],
    }

    def rate_trade_node_sensitivities(*, trade, market, component_key):
        assert trade["kind"] == "TRADE"
        assert market["kind"] == "MARKET"
        return SimpleNamespace(
            eligible=True,
            gradient=gradients[component_key],
            reason="",
        )

    native.RateTradeNodeSensitivities = rate_trade_node_sensitivities
    parameter_axis = [
        {
            "parameter_id": "usd-late",
            "component_key": "discount-default",
            "node_date": "2028-01-15",
        },
        {
            "parameter_id": "usd-early",
            "component_key": "discount-default",
            "node_date": "2027-01-15",
        },
        {
            "parameter_id": "eur-duplicate-date",
            "component_key": "discount-eur",
            "node_date": "2028-01-15",
        },
        {
            "parameter_id": "basis-duplicate-date",
            "component_key": "basis-usd-eur",
            "node_date": "2028-01-15",
        },
    ]
    document = {
        "mode": "STAGED_XCCY",
        "declarations": [
            {
                "component_key": "discount-default",
                "role": "DISCOUNT",
                "currency": "USD",
            },
            {
                "component_key": "discount-eur",
                "role": "DISCOUNT",
                "currency": "EUR",
            },
            {
                "component_key": "basis-usd-eur",
                "role": "BASIS",
                "currency": "USD",
            },
        ],
    }
    monkeypatch.setattr(
        gateway,
        "_curve_lab_passive_curves",
        lambda *_args, **_kwargs: {
            "discount-default": "USD curve",
            "discount-eur": "EUR curve",
            "basis-usd-eur": "basis curve",
        },
    )

    priced = gateway.price_curve_lab_trades(
        document,
        trades,
        "2026-01-15T10:30:00Z",
        "USD",
        parameter_axis=parameter_axis,
        include_node_sensitivities=True,
    )

    assert [row["trade_id"] for row in priced] == [trade["trade_id"] for trade in trades]
    assert [row["aad_node_gradient"] for row in priced] == [
        ["1.25", "2.5", "3.75", "5"] for _ in trades
    ]
    assert len(priced_definitions) == 1
    assert definition_hash(priced_definitions[0]) == baseline_hash

    document["mode"] = "JOINT_XCCY"
    joint_trades = [trades[6], trades[2], trades[0]]
    priced_definitions.clear()

    joint_priced = gateway.price_curve_lab_trades(
        document,
        joint_trades,
        "2026-01-15T10:30:00Z",
        "USD",
    )

    assert [row["trade_id"] for row in joint_priced] == [
        trade["trade_id"] for trade in joint_trades
    ]
    assert len(priced_definitions) == 1
    # Separate JOINT_XCCY order golden captured from baseline b089b8c9.
    joint_baseline_hash = "a75c68779c083b610fb5aefc5581fe3acddf2336729a6c4cd47c9f4da42a1b88"
    assert definition_hash(priced_definitions[0]) == joint_baseline_hash
