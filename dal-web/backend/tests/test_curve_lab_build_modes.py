"""Curve Lab V2 build-mode routing regressions from DAL-23."""

from __future__ import annotations

import hashlib
import json
from copy import deepcopy
from types import SimpleNamespace

import pytest

DISCOUNT_KEY = "clab/v1/local/discount/USD/OIS"
PROJECTION_KEY = "clab/v1/local/projection/USD/3M"


def _instrument(
    component_key: str,
    *,
    instrument_id: str,
    maturity: str,
    quote: str,
) -> dict[str, object]:
    return {
        "instrument_id": instrument_id,
        "instrument_type": "DEPOSIT",
        "trade_date": "2026-01-15",
        "start_date": "2026-01-16",
        "maturity_date": maturity,
        "currency_or_pair": "USD",
        "normalized_quote": quote,
        "included": True,
        "terms": {
            "component_key": component_key,
            "forecast_tenor": "3M",
            "day_basis": "ACT_365F",
            "collateral": "OIS",
        },
    }


def _multi_document() -> dict[str, object]:
    return {
        "mode": "MULTI_CURVE",
        "as_of_date": "2026-01-15",
        "declarations": [
            {
                "component_key": DISCOUNT_KEY,
                "role": "DISCOUNT",
                "currency": "USD",
                "parameterization": "PIECEWISE_CONSTANT_FWD",
            },
            {
                "component_key": PROJECTION_KEY,
                "role": "PROJECTION",
                "currency": "USD",
                "parameterization": "PIECEWISE_CONSTANT_FWD",
            },
        ],
        "instruments": [
            _instrument(
                DISCOUNT_KEY,
                instrument_id="1" * 32,
                maturity="2027-01-15",
                quote="0.01",
            ),
            _instrument(
                PROJECTION_KEY,
                instrument_id="2" * 32,
                maturity="2028-01-15",
                quote="0.02",
            ),
        ],
        "solver": {
            "solve_mode": "EXACT",
            "smoothing_weight": 1.0,
            "tolerance": 1.0e-8,
            "fit_tolerance": 1.0e-6,
            "max_evaluations": 200,
            "max_restarts": 20,
            "initial_guess": 0.05,
        },
    }


def test_declaration_local_fallback_helper_never_merges_instruments() -> None:
    """The single-mode test helper must retain declaration ownership."""
    from app.services.dal_gateway import DalGateway

    gateway = DalGateway()

    curves = gateway._curve_lab_fallback_curves(
        _multi_document(),
        _multi_document()["declarations"],
    )

    assert curves[DISCOUNT_KEY]["dates"] == ["2027-01-15"]
    assert curves[DISCOUNT_KEY]["values"] == [0.01]
    assert curves[PROJECTION_KEY]["dates"] == ["2028-01-15"]
    assert curves[PROJECTION_KEY]["values"] == [0.02]


def test_multi_curve_fails_closed_without_native_bundle_surface() -> None:
    """Non-single builds must never silently substitute passive curves."""
    from app.services.dal_gateway import DalGateway

    gateway = DalGateway()

    with pytest.raises(RuntimeError, match="MULTI_CURVE native calibration is unavailable"):
        gateway._curve_lab_passive_curves(_multi_document())


class _Token:
    def __init__(self, value: str) -> None:
        self.value = value

    def __hash__(self) -> int:
        return hash(self.value)

    def __eq__(self, other: object) -> bool:
        return isinstance(other, _Token) and self.value == other.value


class _CurveCalibrationSpecBuilder:
    def Build(self):  # noqa: N802 - mirrors native DAL
        return self


class _MultiCurveCalibrationSpec:
    def __init__(self) -> None:
        self.stages_: list[object] = []


class _RecordingMultiDal:
    CurveCalibrationSpecBuilder_ = _CurveCalibrationSpecBuilder
    MultiCurveCalibrationSpec_ = _MultiCurveCalibrationSpec
    CurveParameterization = SimpleNamespace(PIECEWISE_CONSTANT_FWD="PWC")
    CurveSolveMode = SimpleNamespace(EXACT="EXACT")
    CurveKnotPolicy = SimpleNamespace(INPUT="INPUT")
    LogDfScheme = SimpleNamespace(LOG_LINEAR="LOG_LINEAR")

    def __init__(self) -> None:
        self.stages: list[object] = []

    @staticmethod
    def Date_(year: int, month: int, day: int):  # noqa: N802
        return f"{year:04d}-{month:02d}-{day:02d}"

    @staticmethod
    def String_(value: str):  # noqa: N802
        return value

    @staticmethod
    def CollateralType_(value: str):  # noqa: N802
        return _Token(value)

    @staticmethod
    def PeriodLength_New(value: str):  # noqa: N802
        return _Token(value)

    @staticmethod
    def DayBasis_New(value: str):  # noqa: N802
        return value

    @staticmethod
    def RateIndexConvention_New(*values):  # noqa: N802
        return values

    @staticmethod
    def Deposit_New(  # noqa: N802
        _trade_date,
        _start,
        maturity,
        market_rate,
        _index,
    ):
        return {"maturity": maturity, "market_rate": market_rate}

    def CalibrateMultiCurveBundle(self, spec):  # noqa: N802
        self.stages = list(spec.stages_)
        return SimpleNamespace(
            discountCurves_={_Token("OIS"): {"native": "discount"}},
            forwardCurves_={_Token("3M"): {"native": "projection"}},
        )

    @staticmethod
    def EvaluationDate_Get():  # noqa: N802
        return "2026-01-15"


class _DynamicBuilder:
    def Build(self):  # noqa: N802 - mirrors native DAL
        return self


class _JointBuilder(_DynamicBuilder):
    def __init__(self) -> None:
        self.solver_options = SimpleNamespace()


class _RecordingXccyDal:
    CrossCurrencyCalibrationSpecBuilder_ = _DynamicBuilder
    JointXccyCalibrationSpecBuilder_ = _JointBuilder
    XccyBasisCurveDeclaration_ = SimpleNamespace
    CurveParameterization = SimpleNamespace(PIECEWISE_CONSTANT_FWD="PWC")
    LogDfScheme = SimpleNamespace(LOG_LINEAR="LOG_LINEAR")
    CurveSolveMode = SimpleNamespace(EXACT="EXACT")

    def __init__(self) -> None:
        self.staged_specs: list[object] = []
        self.joint_specs: list[object] = []

    @staticmethod
    def Date_(year: int, month: int, day: int):  # noqa: N802
        return f"{year:04d}-{month:02d}-{day:02d}"

    @staticmethod
    def DateTime_(day, hour, minute):  # noqa: N802
        return (day, hour, minute)

    @staticmethod
    def Ccy_(value):  # noqa: N802
        return value

    @staticmethod
    def CurrencyPair_New(domestic, foreign):  # noqa: N802
        return (domestic, foreign)

    @staticmethod
    def MarketFixingSnapshot_New(values):  # noqa: N802
        return values

    @staticmethod
    def CollateralType_(value):  # noqa: N802
        return value

    def CalibrateXccyMarket(self, spec):  # noqa: N802
        self.staged_specs.append(spec)
        return SimpleNamespace(basis_curve={"native": "staged-basis"})

    def CalibrateJointXccyMarket(self, spec):  # noqa: N802
        self.joint_specs.append(spec)
        return SimpleNamespace(
            domestic_curve_block={"block": "USD"},
            foreign_curve_block={"block": "EUR"},
            basis_curve={"native": "joint-basis"},
        )


def _xccy_document(mode: str) -> dict[str, object]:
    domestic = "clab/v1/local/discount/USD/OIS"
    foreign = "clab/v1/local/discount/EUR/OIS"
    basis = "clab/v1/xccy/basis/USD-EUR/3M"
    return {
        "mode": mode,
        "as_of_date": "2026-01-15",
        "declarations": [
            {
                "component_key": domestic,
                "role": "DISCOUNT",
                "currency": "USD",
                "parameterization": "PIECEWISE_CONSTANT_FWD",
            },
            {
                "component_key": foreign,
                "role": "DISCOUNT",
                "currency": "EUR",
                "parameterization": "PIECEWISE_CONSTANT_FWD",
            },
            {
                "component_key": basis,
                "role": "BASIS",
                "currency": "USD",
                "parameterization": "PIECEWISE_CONSTANT_FWD",
            },
        ],
        "instruments": [
            {
                "instrument_id": "3" * 32,
                "instrument_type": "XCCY",
                "trade_date": "2026-01-15",
                "start_date": "2026-01-16",
                "maturity_date": "2027-01-15",
                "currency_or_pair": "USD-EUR",
                "normalized_quote": "0.001",
                "included": True,
                "terms": {
                    "component_key": basis,
                    "fx_spot": 1.1,
                    "fx_forward_collateral": "OIS",
                },
            }
        ],
        "solver": {
            "solve_mode": "EXACT",
            "smoothing_weight": 1.0,
            "tolerance": 1.0e-8,
            "fit_tolerance": 1.0e-6,
            "max_evaluations": 200,
            "max_restarts": 20,
            "initial_guess": 0.01,
        },
    }


def _reversed_staged_document() -> dict[str, object]:
    document = _xccy_document("STAGED_XCCY")
    usd, eur, basis = document["declarations"]
    usd["component_key"] = "curve-z"
    eur["component_key"] = "curve-a"
    basis["component_key"] = "basis-q"
    document["declarations"] = [basis, eur, usd]
    document["instruments"] = [
        {
            **_instrument(
                "basis-q",
                instrument_id="3" * 32,
                maturity="2029-01-15",
                quote="0.001",
            ),
            "instrument_type": "XCCY",
            "currency_or_pair": "USD-EUR",
            "terms": {
                "component_key": "basis-q",
                "fx_spot": 1.1,
            },
        },
        _instrument(
            "curve-a",
            instrument_id="2" * 32,
            maturity="2028-01-15",
            quote="0.02",
        ),
        _instrument(
            "curve-z",
            instrument_id="1" * 32,
            maturity="2027-01-15",
            quote="0.01",
        ),
    ]
    for item in document["instruments"]:
        item.update(
            {
                "quote_coordinate_kind": (
                    "SPREAD" if item["instrument_type"] == "XCCY" else "RATE"
                ),
                "canonical_raw_unit": "DECIMAL",
                "raw_quote": item["normalized_quote"],
                "exact_risk_raw_bump": "0.0001",
                "normalized_risk_bump": "0.0001",
            }
        )
    return document


def test_reversed_staged_axes_follow_pair_and_native_layout_not_keys() -> None:
    from app.services.curve_lab_lifecycle import quote_axis
    from app.services.dal_gateway import DalGateway

    document = _reversed_staged_document()
    quotes = quote_axis(document)
    gateway = DalGateway()
    parameters = gateway._curve_lab_parameter_axis_from_curves(
        document,
        {
            "curve-z": {"knot_dates": ["2027-01-15"]},
            "curve-a": {"knot_dates": ["2028-01-15"]},
            "basis-q": {"knot_dates": ["2029-01-15"]},
        },
    )

    assert [(item["component_key"], item["stage_id"]) for item in quotes] == [
        ("curve-z", "stage-0"),
        ("curve-a", "stage-1"),
        ("basis-q", "stage-2"),
    ]
    assert [
        (
            item["component_key"],
            item["stage_id"],
            item["component_local_parameter_index"],
        )
        for item in parameters
    ] == [
        ("curve-z", "stage-0", 0),
        ("curve-a", "stage-1", 0),
        ("basis-q", "stage-2", 0),
    ]


def test_multi_curve_calls_native_bundle_with_declaration_local_stages() -> None:
    """Replacing native multi calibration with passive PWCs breaks this test."""
    from app.services.dal_gateway import DalGateway

    gateway = DalGateway()
    recording = _RecordingMultiDal()
    gateway._dal = recording

    curves = gateway._curve_lab_multi_curves(_multi_document(), {})

    assert [
        [(item["maturity"], item["market_rate"]) for item in stage.instruments_]
        for stage in recording.stages
    ] == [
        [("2027-01-15", 0.01)],
        [("2028-01-15", 0.02)],
    ]
    assert curves == {
        DISCOUNT_KEY: {"native": "discount"},
        PROJECTION_KEY: {"native": "projection"},
    }


def test_xccy_modes_call_native_entry_points_with_mode_specific_specs(
    monkeypatch,
) -> None:
    from app.services.dal_gateway import DalGateway

    gateway = DalGateway()
    recording = _RecordingXccyDal()
    gateway._dal = recording
    local_curves = {
        "clab/v1/local/discount/USD/OIS": {"curve": "USD"},
        "clab/v1/local/discount/EUR/OIS": {"curve": "EUR"},
    }
    monkeypatch.setattr(
        gateway,
        "_curve_lab_local_currency_curves",
        lambda *_args: dict(local_curves),
    )
    monkeypatch.setattr(
        gateway,
        "_curve_lab_native_curve_block",
        lambda currency, *_args: {"block": currency},
    )
    monkeypatch.setattr(
        gateway,
        "_curve_lab_xccy_instrument",
        lambda instrument: {"instrument_id": instrument["instrument_id"]},
    )
    monkeypatch.setattr(
        gateway,
        "_curve_lab_joint_currency_spec",
        lambda _document, currency, _declarations: {"currency": currency},
    )
    monkeypatch.setattr(
        gateway,
        "_curve_lab_curves_from_blocks",
        lambda _declarations, _blocks: dict(local_curves),
    )

    staged = gateway._curve_lab_staged_xccy_curves(
        _xccy_document("STAGED_XCCY"),
        {},
    )
    joint = gateway._curve_lab_joint_xccy_curves(
        _xccy_document("JOINT_XCCY"),
        {},
    )

    assert len(recording.staged_specs) == 1
    staged_spec = recording.staged_specs[0]
    assert staged_spec.basis_pair == ("USD", "EUR")
    assert staged_spec.fx_spot == 1.1
    assert staged_spec.domestic_curve_block == {"block": "USD"}
    assert staged_spec.foreign_curve_block == {"block": "EUR"}
    assert staged_spec.instruments == [{"instrument_id": "3" * 32}]
    assert staged["clab/v1/xccy/basis/USD-EUR/3M"] == {"native": "staged-basis"}

    assert len(recording.joint_specs) == 1
    joint_spec = recording.joint_specs[0]
    assert joint_spec.pair == ("USD", "EUR")
    assert joint_spec.fx_spot == 1.1
    assert joint_spec.domestic == {"currency": "USD"}
    assert joint_spec.foreign == {"currency": "EUR"}
    assert joint_spec.basis.curve_name == "clab/v1/xccy/basis/USD-EUR/3M"
    assert joint_spec.basis.instruments == [{"instrument_id": "3" * 32}]
    assert joint["clab/v1/xccy/basis/USD-EUR/3M"] == {"native": "joint-basis"}


def test_staged_and_joint_xccy_modes_use_dedicated_calibration_paths(
    monkeypatch,
) -> None:
    """A generic non-single PWC branch must not swallow either XCCY mode."""
    from app.services.dal_gateway import DalGateway

    gateway = DalGateway()
    calls: list[str] = []
    monkeypatch.setattr(
        gateway,
        "_curve_lab_staged_xccy_curves",
        lambda _document, _dependencies, _fixings: calls.append("staged") or {"staged": object()},
        raising=False,
    )
    monkeypatch.setattr(
        gateway,
        "_curve_lab_joint_xccy_curves",
        lambda _document, _dependencies, _fixings: calls.append("joint") or {"joint": object()},
        raising=False,
    )
    document = _multi_document()

    staged = deepcopy(document)
    staged["mode"] = "STAGED_XCCY"
    joint = deepcopy(document)
    joint["mode"] = "JOINT_XCCY"

    assert set(gateway._curve_lab_passive_curves(staged)) == {"staged"}
    assert set(gateway._curve_lab_passive_curves(joint)) == {"joint"}
    assert calls == ["staged", "joint"]


def _archive_curve(name: str, rate: float) -> bytes:
    return json.dumps(
        {
            "~type": "DiscountPWC_v1",
            "name": name,
            "ccy": "USD",
            "knotDates": ["2027-01-15"],
            "rightVals": [rate],
        },
        separators=(",", ":"),
    ).encode()


def test_dependencies_restore_exact_admitted_archives_without_recalibration() -> None:
    """Dependency documents are identity metadata, never a rebuild source."""
    from app.services.dal_gateway import DalGateway

    first_payload = _archive_curve(DISCOUNT_KEY, 0.01)
    second_payload = _archive_curve(PROJECTION_KEY, 0.02)
    restored = {
        first_payload: SimpleNamespace(type="DiscountCurve", marker="discount"),
        second_payload: SimpleNamespace(type="DiscountCurve", marker="projection"),
    }
    reads: list[bytes] = []

    class Extension:
        @staticmethod
        def _StorableFromJson(payload):  # noqa: N802
            reads.append(payload)
            return restored[payload]

    gateway = DalGateway()
    gateway._dal = SimpleNamespace(_dal=Extension())
    dependencies = [
        {
            "id": "1" * 32,
            "native_payload": first_payload,
            "native_payload_hash": hashlib.sha256(first_payload).hexdigest(),
            "root_kind": "DISCOUNT_CURVE",
            "verification": {
                "document": {
                    "declarations": [{"component_key": DISCOUNT_KEY}],
                    "dependency_version_ids": ["upstream-not-needed-for-restore"],
                }
            },
        },
        {
            "id": "2" * 32,
            "native_payload": second_payload,
            "native_payload_hash": hashlib.sha256(second_payload).hexdigest(),
            "root_kind": "DISCOUNT_CURVE",
            "verification": {
                "document": {
                    "declarations": [{"component_key": PROJECTION_KEY}],
                    "instruments": [],
                }
            },
        },
    ]

    curves = gateway._curve_lab_dependency_curves(dependencies)

    assert reads == [first_payload, second_payload]
    assert curves == {
        DISCOUNT_KEY: restored[first_payload],
        PROJECTION_KEY: restored[second_payload],
    }


def test_dependency_archive_hash_is_verified_before_native_read() -> None:
    from app.services.dal_gateway import DalGateway

    payload = _archive_curve(DISCOUNT_KEY, 0.01)
    called = False

    class Extension:
        @staticmethod
        def _StorableFromJson(_payload):  # noqa: N802
            nonlocal called
            called = True
            raise AssertionError("corrupt dependency reached native reader")

    gateway = DalGateway()
    gateway._dal = SimpleNamespace(_dal=Extension())
    version = {
        "id": "1" * 32,
        "native_payload": payload,
        "native_payload_hash": "0" * 64,
        "root_kind": "DISCOUNT_CURVE",
        "verification": {
            "document": {
                "declarations": [{"component_key": DISCOUNT_KEY}],
            }
        },
    }

    with pytest.raises(ValueError, match="dependency archive hash mismatch"):
        gateway._curve_lab_dependency_curves([version])

    assert called is False
