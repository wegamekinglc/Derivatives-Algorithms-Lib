"""Persisted Curve Lab PV, DV01, and Key Rate DV01 execution."""

from __future__ import annotations

import hashlib
import json
from copy import deepcopy
from decimal import Decimal
from typing import TYPE_CHECKING
from uuid import uuid4

from app.schemas.curve_lab import RiskRunRequestV2
from app.services.curve_lab_lifecycle import (
    CurveLabLifecycleError,
    _audit,
    _now,
    get_version,
)
from app.services.quote_canonicalization import canonicalize_quote
from app.services.store import NotFoundError

if TYPE_CHECKING:
    from app.services.dal_gateway import DalGateway
    from app.services.store import StoreProtocol

_UINT64_MAX = (1 << 64) - 1
_LIMITS = {
    "T": 1_000,
    "P": 500,
    "Q": 500,
    "price_evaluations": 100_000,
    "calibration_solves": 1_002,
    "aad_recordings": 1_000,
    "estimated_wall_millis": 900_000,
}
_COSTS = {
    "context_build_millis": 1,
    "price_evaluation_millis": 1,
    "calibration_solve_millis": 10,
    "aad_recording_overhead_millis": 1,
}


def _canonical_bytes(value: object) -> bytes:
    return json.dumps(
        value,
        ensure_ascii=True,
        allow_nan=False,
        separators=(",", ":"),
        sort_keys=True,
    ).encode("ascii")


def _hash(value: object) -> str:
    return hashlib.sha256(_canonical_bytes(value)).hexdigest()


def _decimal_text(value: Decimal | str | int | float) -> str:
    decimal_value = value if isinstance(value, Decimal) else Decimal(str(value))
    if decimal_value == 0:
        return "0"
    text = format(decimal_value, "f")
    if "." in text:
        text = text.rstrip("0").rstrip(".")
    return text


def _checked_add(lhs: int, rhs: int) -> tuple[int, bool]:
    if lhs > _UINT64_MAX - rhs:
        return _UINT64_MAX, True
    return lhs + rhs, False


def _checked_multiply(lhs: int, rhs: int) -> tuple[int, bool]:
    if lhs and rhs > _UINT64_MAX // lhs:
        return _UINT64_MAX, True
    return lhs * rhs, False


def estimate_work(
    *,
    trades: int,
    aad_eligible_trades: int,
    parameters: int,
    quotes: int,
    measures: tuple[str, ...],
    sensitivity_layers: tuple[str, ...],
) -> dict[str, int | bool]:
    node = int(
        bool(
            {
                "TRADE_TO_NODE",
                "COMPOSED_QUOTE_DIAGNOSTIC",
            }
            & set(sensitivity_layers)
        )
    )
    n_param, overflow = _checked_multiply(2 * node, parameters)
    n_aad, term_overflow = _checked_multiply(node, aad_eligible_trades)
    overflow |= term_overflow
    if "KEY_RATE_DV01" in measures:
        n_quote, term_overflow = _checked_add(quotes, 1)
    elif "DV01" in measures:
        n_quote, term_overflow = 1, False
    else:
        n_quote, term_overflow = 0, False
    overflow |= term_overflow
    needs_jacobian = bool(
        {
            "CALIBRATION_JACOBIAN",
            "COMPOSED_QUOTE_DIAGNOSTIC",
        }
        & set(sensitivity_layers)
    )
    n_jac, term_overflow = _checked_multiply(2, quotes)
    n_jac = n_jac if needs_jacobian else 0
    overflow |= term_overflow and needs_jacobian

    parameter_prices, term_overflow = _checked_multiply(n_param, trades)
    overflow |= term_overflow
    aad_prices = n_aad
    quote_prices, term_overflow = _checked_multiply(n_quote, trades)
    overflow |= term_overflow

    contexts = 1
    for term in (n_param, n_aad, n_quote, n_jac):
        contexts, term_overflow = _checked_add(contexts, term)
        overflow |= term_overflow
    price_evaluations = trades
    for term in (parameter_prices, aad_prices, quote_prices):
        price_evaluations, term_overflow = _checked_add(price_evaluations, term)
        overflow |= term_overflow
    calibration_solves, term_overflow = _checked_add(n_quote, n_jac)
    overflow |= term_overflow

    contributions = []
    for count, coefficient in (
        (contexts, _COSTS["context_build_millis"]),
        (price_evaluations, _COSTS["price_evaluation_millis"]),
        (calibration_solves, _COSTS["calibration_solve_millis"]),
        (n_aad, _COSTS["aad_recording_overhead_millis"]),
    ):
        contribution, term_overflow = _checked_multiply(count, coefficient)
        overflow |= term_overflow
        contributions.append(contribution)
    wall = 0
    for contribution in contributions:
        wall, term_overflow = _checked_add(wall, contribution)
        overflow |= term_overflow

    return {
        "T": trades,
        "T_aad": aad_eligible_trades,
        "P": parameters,
        "Q": quotes,
        "I_node": node,
        "N_param": n_param,
        "N_aad": n_aad,
        "N_quote": n_quote,
        "N_jac": n_jac,
        "parameter_bump_price_evaluations": parameter_prices,
        "aad_price_evaluations": aad_prices,
        "quote_bump_price_evaluations": quote_prices,
        "contexts": contexts,
        "price_evaluations": price_evaluations,
        "calibration_solves": calibration_solves,
        "aad_recordings": n_aad,
        "estimated_wall_millis": wall,
        "overflow": overflow,
    }


def _admit_work(estimate: dict[str, int | bool]) -> None:
    for field in (
        "T",
        "P",
        "Q",
        "price_evaluations",
        "calibration_solves",
        "aad_recordings",
        "estimated_wall_millis",
    ):
        value = int(estimate[field])
        if not estimate["overflow"] and value <= _LIMITS[field]:
            continue
        code = (
            "RISK_DEADLINE_BUDGET_EXCEEDED"
            if field == "estimated_wall_millis"
            else "RISK_WORK_LIMIT_EXCEEDED"
        )
        raise CurveLabLifecycleError(
            422,
            code,
            "Curve Lab risk work exceeds the configured admission budget.",
            f"estimated_work.{field}",
            value,
            constraint=f"must be <= configured limit {_LIMITS[field]}",
            overflow=estimate["overflow"],
            estimate=estimate,
        )


def _bumped_document(
    document: dict,
    quote_axis: list[dict],
    quote_index: int | None,
    direction: int = 1,
) -> dict:
    result = deepcopy(document)
    by_id = {item["instrument_id"]: item for item in result["instruments"]}
    selected = quote_axis if quote_index is None else [quote_axis[quote_index]]
    for axis in selected:
        instrument = by_id[axis["instrument_id"]]
        raw = Decimal(axis["raw_quote"]) + (Decimal(axis["exact_risk_raw_bump"]) * direction)
        canonical = canonicalize_quote(
            instrument["instrument_type"],
            _decimal_text(raw),
            axis["canonical_raw_unit"],
        )
        instrument.update(
            {
                "raw_quote": canonical.raw_quote,
                "normalized_quote": canonical.normalized_quote,
                "quote_coordinate_kind": canonical.quote_coordinate_kind,
                "canonical_raw_unit": canonical.canonical_raw_unit,
                "exact_risk_raw_bump": canonical.exact_risk_raw_bump,
                "normalized_risk_bump": canonical.normalized_risk_bump,
            }
        )
    return result


def _fixing_keys(values: object) -> list[dict]:
    result: list[dict] = []
    for value in values or []:
        if isinstance(value, dict):
            result.append(value)
        else:
            index_name, fixing_time = value
            result.append(
                {
                    "index_name": str(index_name),
                    "fixing_time": str(fixing_time),
                }
            )
    return result


def _pricing_result(trade: dict, native: dict) -> dict:
    common = {
        "trade_id": trade["trade_id"],
        "instrument_type": trade["instrument_type"],
        "required_historical_fixing_keys": _fixing_keys(native.get("required_historical_fixings")),
        "dependency_component_keys": list(native.get("dependency_component_keys", [])),
    }
    if native.get("succeeded"):
        return {
            **common,
            "status": "SUCCEEDED",
            "pv": _decimal_text(native["pv"]),
            "currency": str(native["currency"]),
            "normalized_plan_hash": _hash(trade),
        }
    return {
        **common,
        "status": "FAILED",
        "error": {
            "code": (
                "MISSING_HISTORICAL_FIXING"
                if native.get("missing_historical_fixings")
                else "PRICING_FAILED"
            ),
            "message": str(native.get("error") or "Native trade pricing failed."),
            "field": f"trades[{trade['trade_id']}]",
            "value": None,
            "resource_id": trade["trade_id"],
            "details": {},
        },
        "missing_historical_fixing_keys": _fixing_keys(native.get("missing_historical_fixings")),
    }


def _native_by_trade(trades: list[dict], rows: list[dict]) -> dict[str, dict]:
    if len(rows) != len(trades):
        raise ValueError("native pricing result cardinality mismatch")
    return {trade["trade_id"]: row for trade, row in zip(trades, rows, strict=True)}


def _differences(
    trades: list[dict],
    base: dict[str, dict],
    bumped: dict[str, dict],
) -> list[dict] | None:
    result: list[dict] = []
    for trade in trades:
        trade_id = trade["trade_id"]
        base_row = base[trade_id]
        bumped_row = bumped[trade_id]
        if not base_row.get("succeeded") or not bumped_row.get("succeeded"):
            return None
        value = Decimal(str(bumped_row["pv"])) - Decimal(str(base_row["pv"]))
        result.append({"trade_id": trade_id, "value": _decimal_text(value)})
    return result


def _central_price_matrix(
    gateway: DalGateway,
    document: dict,
    trades: list[dict],
    parameter_axis: list[dict],
    evaluation_time: str,
    base_currency: str,
) -> list[list[str]] | None:
    epsilon = Decimal("0.000001")
    columns: list[list[str]] = []
    for axis in parameter_axis:
        plus = _native_by_trade(
            trades,
            gateway.price_curve_lab_parameter_bump(
                document,
                trades,
                evaluation_time,
                base_currency,
                axis,
                float(epsilon),
            ),
        )
        minus = _native_by_trade(
            trades,
            gateway.price_curve_lab_parameter_bump(
                document,
                trades,
                evaluation_time,
                base_currency,
                axis,
                -float(epsilon),
            ),
        )
        column: list[str] = []
        for trade in trades:
            trade_id = trade["trade_id"]
            if not plus[trade_id].get("succeeded") or not minus[trade_id].get("succeeded"):
                return None
            derivative = (
                Decimal(str(plus[trade_id]["pv"])) - Decimal(str(minus[trade_id]["pv"]))
            ) / (2 * epsilon)
            column.append(_decimal_text(derivative))
        columns.append(column)
    return [[columns[column][row] for column in range(len(columns))] for row in range(len(trades))]


def _central_calibration_jacobian(
    gateway: DalGateway,
    document: dict,
    quote_axis: list[dict],
    parameter_axis: list[dict],
) -> list[list[str]]:
    columns: list[list[str]] = []
    for quote_index, quote in enumerate(quote_axis):
        plus = gateway.curve_lab_parameter_values(
            _bumped_document(document, quote_axis, quote_index, 1),
            parameter_axis,
        )
        minus = gateway.curve_lab_parameter_values(
            _bumped_document(document, quote_axis, quote_index, -1),
            parameter_axis,
        )
        denominator = Decimal(quote["normalized_risk_bump"]) * 2
        columns.append(
            [
                _decimal_text((Decimal(plus[index]) - Decimal(minus[index])) / denominator)
                for index in range(len(parameter_axis))
            ]
        )
    return [
        [columns[column][row] for column in range(len(columns))]
        for row in range(len(parameter_axis))
    ]


def _multiply_matrices(
    lhs: list[list[str]],
    rhs: list[list[str]],
) -> list[list[str]]:
    if not lhs:
        return []
    columns = len(rhs[0]) if rhs else 0
    shared = len(rhs)
    return [
        [
            _decimal_text(
                sum(
                    (
                        Decimal(lhs[row][inner]) * Decimal(rhs[inner][column])
                        for inner in range(shared)
                    ),
                    Decimal(0),
                )
            )
            for column in range(columns)
        ]
        for row in range(len(lhs))
    ]


def _failed_matrix(
    *,
    matrix_id: str,
    mathematical_name: str,
    orientation: str,
    row_axis_ref: str,
    column_axis_ref: str,
    rows: int,
    columns: int,
    method: str,
    reason_code: str,
    reason: str,
    input_unit: str,
    output_unit: str,
) -> dict:
    return {
        "matrix_id": matrix_id,
        "mathematical_name": mathematical_name,
        "orientation": orientation,
        "row_axis_ref": row_axis_ref,
        "column_axis_ref": column_axis_ref,
        "rows": rows,
        "columns": columns,
        "availability": "FAILED",
        "availability_reason_code": reason_code,
        "availability_reason": reason,
        "method": method,
        "bump_target": None,
        "bump_size": None,
        "input_unit": input_unit,
        "output_unit": output_unit,
        "failure": {
            "code": reason_code,
            "message": reason,
            "field": "sensitivity_layers",
            "value": None,
            "resource_id": None,
            "details": {},
        },
    }


def create_risk_run(
    store: StoreProtocol,
    gateway: DalGateway,
    request: RiskRunRequestV2,
) -> dict:
    version = get_version(store, request.curve_version_id)
    quote_risk = bool({"DV01", "KEY_RATE_DV01"} & set(request.measures))
    if version["source_kind"] == "IMPORT" and (
        quote_risk
        or {
            "CALIBRATION_JACOBIAN",
            "COMPOSED_QUOTE_DIAGNOSTIC",
        }
        & set(request.sensitivity_layers)
    ):
        raise CurveLabLifecycleError(
            409,
            "CALIBRATION_LINEAGE_REQUIRED",
            "Quote risk requires verified calibration lineage.",
            "measures",
            next(
                (measure for measure in request.measures if measure in {"DV01", "KEY_RATE_DV01"}),
                None,
            ),
            resource_id=request.curve_version_id,
            constraint=("quote risk requires a built or replay-verified calibration manifest"),
        )

    build = (
        store.get_curve_lab_build_run(version["build_run_id"])
        if version["source_kind"] == "BUILD"
        else None
    )
    document = (
        deepcopy(build["request"])
        if build is not None
        else deepcopy(version["verification"].get("document"))
    )
    if not isinstance(document, dict):
        raise CurveLabLifecycleError(
            409,
            "RISK_RUNTIME_CONTEXT_REQUIRED",
            "Curve version does not carry reconstructable runtime context.",
            "curve_version_id",
            request.curve_version_id,
        )
    quote_axis = list(build["quote_axis"]) if build is not None else []
    parameter_axis = list(build["parameter_axis"]) if build is not None else []
    trades = list(request.target.model_dump(mode="json")["trades"])
    estimate = estimate_work(
        trades=len(trades),
        aad_eligible_trades=0,
        parameters=len(parameter_axis),
        quotes=len(quote_axis),
        measures=request.measures,
        sensitivity_layers=request.sensitivity_layers,
    )
    _admit_work(estimate)

    request_json = request.model_dump(mode="json")
    base_rows = gateway.price_curve_lab_trades(
        document,
        trades,
        request_json["evaluation_time"],
        request.base_currency,
    )
    base = _native_by_trade(trades, base_rows)
    pricing = [_pricing_result(trade, base[trade["trade_id"]]) for trade in trades]
    result: dict[str, object] = {"pricing": pricing}
    matrices: list[dict] = []
    requested_layers = set(request.sensitivity_layers)
    needs_trade_to_node = bool({"TRADE_TO_NODE", "COMPOSED_QUOTE_DIAGNOSTIC"} & requested_layers)
    needs_jacobian = bool(
        {
            "CALIBRATION_JACOBIAN",
            "COMPOSED_QUOTE_DIAGNOSTIC",
        }
        & requested_layers
    )
    trade_to_node: list[list[str]] | None = None
    jacobian: list[list[str]] | None = None

    if needs_trade_to_node:
        try:
            trade_to_node = _central_price_matrix(
                gateway,
                document,
                trades,
                parameter_axis,
                request_json["evaluation_time"],
                request.base_currency,
            )
        except Exception:  # noqa: BLE001 - matrix failure is a persisted outcome
            trade_to_node = None
        if trade_to_node is not None:
            matrices.append(
                {
                    "matrix_id": "trade-to-node",
                    "mathematical_name": "trade_to_node_pv_gradient",
                    "orientation": "TRADE_X_PARAMETER",
                    "row_axis_ref": "request.target.trades",
                    "column_axis_ref": "parameter_axis",
                    "rows": len(trades),
                    "columns": len(parameter_axis),
                    "availability": "AVAILABLE",
                    "availability_reason_code": None,
                    "availability_reason": None,
                    "method": "CENTRAL_NATIVE_PARAMETER_BUMP",
                    "bump_target": "NATIVE_PARAMETER",
                    "bump_size": "0.000001",
                    "input_unit": "NATIVE_PARAMETER_UNIT",
                    "output_unit": f"{request.base_currency}_PV_PER_PARAMETER_UNIT",
                    "values": trade_to_node,
                    "failure": None,
                }
            )
        else:
            matrices.append(
                _failed_matrix(
                    matrix_id="trade-to-node",
                    mathematical_name="trade_to_node_pv_gradient",
                    orientation="TRADE_X_PARAMETER",
                    row_axis_ref="request.target.trades",
                    column_axis_ref="parameter_axis",
                    rows=len(trades),
                    columns=len(parameter_axis),
                    method="CENTRAL_NATIVE_PARAMETER_BUMP",
                    reason_code="PARAMETER_BUMP_FAILED",
                    reason="A required native parameter bump failed.",
                    input_unit="NATIVE_PARAMETER_UNIT",
                    output_unit=(f"{request.base_currency}_PV_PER_PARAMETER_UNIT"),
                )
            )

    if needs_jacobian:
        try:
            jacobian = _central_calibration_jacobian(
                gateway,
                document,
                quote_axis,
                parameter_axis,
            )
        except Exception:  # noqa: BLE001 - matrix failure is a persisted outcome
            jacobian = None
        if jacobian is not None:
            matrices.append(
                {
                    "matrix_id": "calibration-jacobian",
                    "mathematical_name": "d_parameter_d_normalized_quote",
                    "orientation": "PARAMETER_X_QUOTE",
                    "row_axis_ref": "parameter_axis",
                    "column_axis_ref": "quote_axis",
                    "rows": len(parameter_axis),
                    "columns": len(quote_axis),
                    "availability": "AVAILABLE",
                    "availability_reason_code": None,
                    "availability_reason": None,
                    "method": "CENTRAL_FULL_RECALIBRATION",
                    "bump_target": "NORMALIZED_QUOTE",
                    "bump_size": "PER_QUOTE_AXIS",
                    "input_unit": "DECIMAL_RATE",
                    "output_unit": "NATIVE_PARAMETER_UNIT_PER_DECIMAL_RATE",
                    "values": jacobian,
                    "failure": None,
                }
            )
        else:
            matrices.append(
                _failed_matrix(
                    matrix_id="calibration-jacobian",
                    mathematical_name="d_parameter_d_normalized_quote",
                    orientation="PARAMETER_X_QUOTE",
                    row_axis_ref="parameter_axis",
                    column_axis_ref="quote_axis",
                    rows=len(parameter_axis),
                    columns=len(quote_axis),
                    method="CENTRAL_FULL_RECALIBRATION",
                    reason_code="JACOBIAN_REPLAY_FAILED",
                    reason="A required calibration replay failed.",
                    input_unit="DECIMAL_RATE",
                    output_unit="NATIVE_PARAMETER_UNIT_PER_DECIMAL_RATE",
                )
            )

    if "COMPOSED_QUOTE_DIAGNOSTIC" in requested_layers:
        if trade_to_node is not None and jacobian is not None:
            composed = _multiply_matrices(trade_to_node, jacobian)
            matrices.append(
                {
                    "matrix_id": "composed-quote-diagnostic",
                    "mathematical_name": "trade_to_node_times_calibration_jacobian",
                    "orientation": "TRADE_X_QUOTE",
                    "row_axis_ref": "request.target.trades",
                    "column_axis_ref": "quote_axis",
                    "rows": len(trades),
                    "columns": len(quote_axis),
                    "availability": "AVAILABLE",
                    "availability_reason_code": None,
                    "availability_reason": None,
                    "method": "MATRIX_COMPOSITION",
                    "bump_target": None,
                    "bump_size": None,
                    "input_unit": "DECIMAL_RATE",
                    "output_unit": f"{request.base_currency}_PV_PER_DECIMAL_RATE",
                    "values": composed,
                    "failure": None,
                }
            )
        else:
            matrices.append(
                _failed_matrix(
                    matrix_id="composed-quote-diagnostic",
                    mathematical_name=("trade_to_node_times_calibration_jacobian"),
                    orientation="TRADE_X_QUOTE",
                    row_axis_ref="request.target.trades",
                    column_axis_ref="quote_axis",
                    rows=len(trades),
                    columns=len(quote_axis),
                    method="MATRIX_COMPOSITION",
                    reason_code="COMPOSED_INPUT_UNAVAILABLE",
                    reason=("Trade-to-node or calibration-Jacobian input is unavailable."),
                    input_unit="DECIMAL_RATE",
                    output_unit=(f"{request.base_currency}_PV_PER_DECIMAL_RATE"),
                )
            )

    if requested_layers:
        result["sensitivity_matrices"] = [
            {
                "matrix_id": matrix["matrix_id"],
                "availability": matrix["availability"],
                "method": matrix["method"],
            }
            for matrix in matrices
            if matrix["matrix_id"] != "key-rate-dv01"
        ]

    parallel: list[dict] | None = None
    if quote_risk:
        parallel_document = _bumped_document(document, quote_axis, None)
        parallel_rows = gateway.price_curve_lab_trades(
            parallel_document,
            trades,
            request_json["evaluation_time"],
            request.base_currency,
        )
        parallel = _differences(
            trades,
            base,
            _native_by_trade(trades, parallel_rows),
        )
        if parallel is not None:
            result["dv01"] = parallel

    if "KEY_RATE_DV01" in request.measures:
        columns: list[list[str]] = []
        bump_rows: list[dict] = []
        failed = False
        for quote_index, axis in enumerate(quote_axis):
            bumped_rows = gateway.price_curve_lab_trades(
                _bumped_document(document, quote_axis, quote_index),
                trades,
                request_json["evaluation_time"],
                request.base_currency,
            )
            differences = _differences(
                trades,
                base,
                _native_by_trade(trades, bumped_rows),
            )
            status = "SUCCEEDED" if differences is not None else "FAILED"
            bump_rows.append(
                {
                    "bump_id": f"key-rate-{quote_index}",
                    "kind": "KEY_RATE",
                    "quote_id": axis["quote_id"],
                    "status": status,
                    "raw_bump": axis["exact_risk_raw_bump"],
                    "normalized_bump": axis["normalized_risk_bump"],
                    "calibration_status": status,
                    "pricing_status": status,
                    "error": None,
                }
            )
            if differences is None:
                failed = True
            else:
                columns.append([row["value"] for row in differences])
        if parallel is not None:
            bump_rows.insert(
                0,
                {
                    "bump_id": "parallel",
                    "kind": "PARALLEL",
                    "quote_id": None,
                    "status": "SUCCEEDED",
                    "raw_bump": None,
                    "normalized_bump": "0.0001",
                    "calibration_status": "SUCCEEDED",
                    "pricing_status": "SUCCEEDED",
                    "error": None,
                },
            )
        result["quote_bumps"] = bump_rows
        if not failed and parallel is not None:
            values = [
                [columns[column][row] for column in range(len(columns))]
                for row in range(len(trades))
            ]
            sums = [sum((Decimal(value) for value in row), Decimal(0)) for row in values]
            result["key_rate_sum"] = [
                {
                    "trade_id": trade["trade_id"],
                    "value": _decimal_text(sums[index]),
                }
                for index, trade in enumerate(trades)
            ]
            result["nonlinear_reconciliation"] = [
                {
                    "trade_id": trade["trade_id"],
                    "value": _decimal_text(Decimal(parallel[index]["value"]) - sums[index]),
                }
                for index, trade in enumerate(trades)
            ]
            matrices.append(
                {
                    "matrix_id": "key-rate-dv01",
                    "mathematical_name": "key_rate_dv01",
                    "orientation": "TRADE_X_QUOTE",
                    "row_axis_ref": "request.target.trades",
                    "column_axis_ref": "quote_axis",
                    "rows": len(trades),
                    "columns": len(quote_axis),
                    "availability": "AVAILABLE",
                    "availability_reason_code": None,
                    "availability_reason": None,
                    "method": "FULL_RECALIBRATION",
                    "bump_target": "NORMALIZED_QUOTE",
                    "bump_size": "0.0001",
                    "input_unit": "DECIMAL_RATE",
                    "output_unit": request.base_currency,
                    "values": values,
                    "failure": None,
                }
            )
        else:
            matrices.append(
                {
                    "matrix_id": "key-rate-dv01",
                    "mathematical_name": "key_rate_dv01",
                    "orientation": "TRADE_X_QUOTE",
                    "row_axis_ref": "request.target.trades",
                    "column_axis_ref": "quote_axis",
                    "rows": len(trades),
                    "columns": len(quote_axis),
                    "availability": "FAILED",
                    "availability_reason_code": "QUOTE_BUMP_FAILED",
                    "availability_reason": "A required quote bump failed.",
                    "method": "FULL_RECALIBRATION",
                    "bump_target": "NORMALIZED_QUOTE",
                    "bump_size": "0.0001",
                    "input_unit": "DECIMAL_RATE",
                    "output_unit": request.base_currency,
                    "failure": {
                        "code": "QUOTE_BUMP_FAILED",
                        "message": "A required quote bump failed.",
                        "field": "quote_axis",
                        "value": None,
                        "resource_id": None,
                        "details": {},
                    },
                }
            )

    now = _now()
    record = {
        "id": uuid4().hex,
        "curve_version_id": request.curve_version_id,
        "calibration_run_id": version.get("build_run_id"),
        "import_job_id": version.get("import_job_id"),
        "source_kind": ("BUILD_VERSION" if version["source_kind"] == "BUILD" else "IMPORT_VERSION"),
        "request": request_json,
        "target_fingerprint": _hash(request_json["target"]),
        "quote_axis": quote_axis if build is not None else None,
        "parameter_axis": parameter_axis,
        "estimated_work": estimate,
        "state": "SUCCEEDED",
        "result": result,
        "error": None,
        "created_at": now,
        "finished_at": now,
    }
    stored = store.publish_curve_lab_risk_run(record, matrices)
    _audit(
        store,
        "RISK_RUN_SUCCEEDED",
        "curve_risk_run",
        stored["id"],
        request_json,
        estimated_work=estimate,
    )
    return stored


def get_risk_run(store: StoreProtocol, run_id: str) -> dict:
    try:
        return store.get_curve_lab_risk_run(run_id)
    except NotFoundError as exc:
        raise CurveLabLifecycleError(
            404,
            "CURVE_RISK_RUN_NOT_FOUND",
            "Curve risk run was not found.",
            "risk_run_id",
            run_id,
        ) from exc


def get_matrix(store: StoreProtocol, run_id: str, matrix_id: str) -> dict:
    get_risk_run(store, run_id)
    try:
        return store.get_curve_lab_matrix(run_id, matrix_id)
    except NotFoundError as exc:
        raise CurveLabLifecycleError(
            404,
            "CURVE_MATRIX_NOT_FOUND",
            "Curve risk matrix was not found.",
            "matrix_id",
            matrix_id,
            resource_id=run_id,
        ) from exc
