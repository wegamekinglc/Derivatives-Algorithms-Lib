"""Compiled DAL smoke checks for the Web calibration gateway."""

from __future__ import annotations

import sys
from pathlib import Path


def _single_payload() -> dict[str, object]:
    index = {
        "spot_lag": 0,
        "fixing_lag": 0,
        "use_projection_curve": False,
        "forecast_tenor": "P12M",
        "day_basis": "ACT_365F",
        "business_day_convention": "Following",
        "fixing_holidays": "",
        "accrual_holidays": "",
        "end_of_month": False,
        "collateral": "OIS",
    }
    return {
        "schema_version": 1,
        "name": "compiled_single_smoke",
        "today": "2026-01-02",
        "currency": "USD",
        "declaration": {
            "curve_name": "usd_ois_native",
            "target_collateral": "OIS",
            "target_tenor": None,
            "calibrate_discount_curve": True,
            "libor_basis": "ACT_365F",
            "parameterization": "PIECEWISE_CONSTANT_FWD",
            "log_df_scheme": None,
            "knot_policy": "INPUT",
            "knot_dates": ["2027-01-02"],
            "base_curve_id": None,
            "discount_curve_ids": {},
            "forward_curve_ids": {},
            "initial_guess_per_node": [0.01],
        },
        "instruments": [
            {
                "kind": "DEPOSIT",
                "label": "USD DEP 1Y",
                "trade_date": "2026-01-02",
                "start": "2026-01-02",
                "maturity": "2027-01-02",
                "market_rate": 0.04,
                "index": index,
            }
        ],
        "solver": {
            "solve_mode": "EXACT",
            "smoothing_weight": 1.0,
            "tolerance": 1e-9,
            "fit_tolerance": 1e-7,
            "initial_guess": 0.01,
            "max_evaluations": 200,
            "max_restarts": 20,
        },
        "options": {
            "jacobian_mode": "ANALYTIC",
            "include_jacobian": True,
            "include_effective_inverse": True,
        },
    }


def _rate_leg(day_basis: str) -> dict[str, object]:
    return {
        "payment_frequency": "P6M",
        "day_basis": day_basis,
        "payment_lag": 0,
        "business_day_convention": "Following",
        "payment_convention": "Following",
        "accrual_holidays": "",
        "payment_holidays": "",
        "end_of_month": False,
    }


def _rate_index(day_basis: str) -> dict[str, object]:
    return {
        "spot_lag": 0,
        "fixing_lag": 0,
        "use_projection_curve": False,
        "forecast_tenor": "P3M",
        "day_basis": day_basis,
        "business_day_convention": "Following",
        "fixing_holidays": "",
        "accrual_holidays": "",
        "end_of_month": False,
        "collateral": "OIS",
    }


def _xccy_config() -> dict[str, object]:
    return {
        "pair": {"domestic": "USD", "foreign": "EUR"},
        "domestic_notional": 110.0,
        "foreign_notional": 100.0,
        "convention": {
            "initial_notional_exchange": True,
            "final_notional_exchange": True,
            "spread_on_foreign_leg": True,
            "domestic_index": _rate_index("ACT_365F"),
            "domestic_leg": _rate_leg("ACT_365F"),
            "foreign_index": _rate_index("ACT_360"),
            "foreign_leg": _rate_leg("ACT_360"),
        },
        "notional_mode": "FIXED",
        "fx_reset": {
            "fixing_lag": -1,
            "fixing_holidays": "",
            "fixing_convention": "Following",
            "fixing_hour": -1,
            "fixing_minute": -1,
        },
        "domestic_rate_fixing": {
            "index_name": "",
            "fixing_hour": -1,
            "fixing_minute": -1,
        },
        "foreign_rate_fixing": {
            "index_name": "",
            "fixing_hour": -1,
            "fixing_minute": -1,
        },
    }


def _curve_payload(
    curve_id: str, source_run_id: str, name: str, currency: str, rate: float
) -> dict[str, object]:
    return {
        "dto_version": 1,
        "id": curve_id,
        "name": name,
        "currency": currency,
        "role": "discount",
        "target": {"collateral": "OIS", "tenor": None},
        "parameterization": "PIECEWISE_CONSTANT_FWD",
        "anchor_date": "2026-01-02",
        "day_count": "ACT_365F",
        "log_df_scheme": None,
        "node_dates": ["2026-01-03", "2036-01-02"],
        "parameters": {"right_forwards": [rate, rate]},
        "base_curve_id": None,
        "base": None,
        "source_run_id": source_run_id,
    }


def _staged_payload(usd_curve_id: str, eur_curve_id: str) -> dict[str, object]:
    return {
        "schema_version": 1,
        "name": "compiled_staged_smoke",
        "valuation_time": "2026-01-02T00:00:00",
        "pair": {"domestic": "USD", "foreign": "EUR"},
        "collateral_currency": "USD",
        "fx_spot": 1.10,
        "fx_forward_collateral": "OIS",
        "domestic_curve_block": {
            "name": "usd",
            "currency": "USD",
            "libor_basis": "ACT_365F",
            "discount_curve_ids": {"OIS": usd_curve_id},
            "forward_curve_ids": {},
        },
        "foreign_curve_block": {
            "name": "eur",
            "currency": "EUR",
            "libor_basis": "ACT_360",
            "discount_curve_ids": {"OIS": eur_curve_id},
            "forward_curve_ids": {},
        },
        "basis": {
            "curve_name": "usd_eur_basis",
            "knot_dates": ["2027-01-02"],
            "instruments": [
                {
                    "kind": "XCCY_SWAP",
                    "label": "USD EUR XCCY 1Y",
                    "trade_date": "2026-01-02",
                    "start": "2026-01-02",
                    "maturity": "2027-01-02",
                    "market_rate": 0.001,
                    "config": _xccy_config(),
                }
            ],
            "initial_guess_per_node": [0.02],
        },
        "fixings": [],
        "solver": {
            "solve_mode": "EXACT",
            "smoothing_weight": 1.0,
            "tolerance": 1e-9,
            "fit_tolerance": 1e-7,
            "initial_guess": 0.02,
            "max_evaluations": 200,
            "max_restarts": 20,
        },
        "options": {
            "jacobian_mode": "ANALYTIC",
            "include_jacobian": True,
            "include_effective_inverse": True,
        },
    }


def _joint_payload() -> dict[str, object]:
    def declaration(name: str, market_rate: float) -> dict[str, object]:
        return {
            "curve_name": name,
            "calibrate_discount_curve": True,
            "target_collateral": "OIS",
            "target_tenor": None,
            "base_layered_over_discount": False,
            "parameterization": "PIECEWISE_CONSTANT_FWD",
            "log_df_scheme": None,
            "knot_dates": ["2027-01-02"],
            "instruments": [
                {
                    "kind": "DEPOSIT",
                    "label": f"{name} DEP 1Y",
                    "trade_date": "2026-01-02",
                    "start": "2026-01-02",
                    "maturity": "2027-01-02",
                    "market_rate": market_rate,
                    "index": _rate_index("ACT_365F"),
                }
            ],
            "smoothing_weight": 1.0,
            "initial_guess_per_node": [0.01],
        }

    config = _xccy_config()
    config["convention"]["foreign_index"] = _rate_index("ACT_365F")
    config["convention"]["foreign_leg"] = _rate_leg("ACT_365F")
    return {
        "schema_version": 1,
        "name": "compiled_joint_smoke",
        "valuation_time": "2026-01-02T00:00:00",
        "pair": {"domestic": "USD", "foreign": "EUR"},
        "collateral_currency": "USD",
        "fx_spot": 1.10,
        "domestic": {
            "currency": "USD",
            "libor_basis": "ACT_365F",
            "declarations": [declaration("usd_ois", 0.04)],
        },
        "foreign": {
            "currency": "EUR",
            "libor_basis": "ACT_365F",
            "declarations": [declaration("eur_ois", 0.03)],
        },
        "basis": {
            "curve_name": "usd_eur_basis",
            "parameterization": "PIECEWISE_CONSTANT_FWD",
            "log_df_scheme": None,
            "knot_dates": ["2027-01-02"],
            "instruments": [
                {
                    "kind": "XCCY_SWAP",
                    "label": "USD EUR XCCY 1Y",
                    "trade_date": "2026-01-02",
                    "start": "2026-01-02",
                    "maturity": "2027-01-02",
                    "market_rate": 0.001,
                    "config": config,
                }
            ],
            "smoothing_weight": 1.0,
            "initial_guess_per_node": [0.001],
        },
        "fixings": [],
        "solver": {
            "solve_mode": "EXACT",
            "smoothing_weight": 1.0,
            "tolerance": 1e-9,
            "fit_tolerance": 1e-7,
            "initial_guess": 0.01,
            "max_evaluations": 400,
            "max_restarts": 20,
        },
        "options": {
            "jacobian_mode": "ANALYTIC",
            "include_jacobian": True,
            "include_effective_inverse": True,
        },
    }


def _run_single_smoke(
    gateway,
    request_type,
    admission_request_type,
    pre_lock_request_type,
    evidence_type,
    project_expected_identity,
    canonical_hash,
) -> None:
    request = request_type.model_validate(_single_payload())
    observed_plans = []
    admission = gateway.plan_single_admission(
        admission_request_type(request, {}),
        observed_plans.append,
    )
    if observed_plans != [admission.resolved_knot_plan]:
        raise AssertionError("compiled planner was not observed exactly once")
    plan = admission.resolved_knot_plan.to_bounded_dto()
    expected = project_expected_identity(request, plan)
    evidence = evidence_type(
        plan,
        canonical_hash(plan),
        expected,
        canonical_hash(expected),
    )
    inspected = []
    result = gateway.calibrate_single(
        pre_lock_request_type(request, {}),
        lambda _at: None,
        lambda _pre_lock: evidence,
        inspected.append,
    )
    if inspected != [expected]:
        raise AssertionError("compiled execution identity differs from admission")
    rate = result.curves[0]["parameters"]["right_forwards"][0]
    if abs(rate - 0.01) < 1e-5:
        raise AssertionError("gateway returned the initial guess instead of a native solve")
    if result.solver_diagnostics.max_abs_residual > request.solver.fit_tolerance:
        raise AssertionError("compiled single residual exceeded fit tolerance")
    if result.jacobian.availability != "available":
        raise AssertionError("compiled analytic Jacobian was not materialized")
    if result.effective_inverse.availability != "available":
        raise AssertionError("compiled effective inverse was not materialized")
    print(
        "single compiled smoke: "
        f"rate={rate:.12g} residual={result.solver_diagnostics.max_abs_residual:.3g}"
    )


def _referenced_curves(type_adapter_type, curve_type) -> dict[str, object]:
    usd_curve_id = "b" * 32
    eur_curve_id = "c" * 32
    source_run_id = "d" * 32
    curve_adapter = type_adapter_type(curve_type)
    return {
        usd_curve_id: curve_adapter.validate_python(
            _curve_payload(usd_curve_id, source_run_id, "usd_ois", "USD", 0.04)
        ),
        eur_curve_id: curve_adapter.validate_python(
            _curve_payload(eur_curve_id, source_run_id, "eur_ois", "EUR", 0.03)
        ),
    }


def _run_staged_smoke(gateway, request_type, gateway_request_type, referenced: dict[str, object]) -> None:
    usd_curve_id = "b" * 32
    eur_curve_id = "c" * 32
    staged_request = request_type.model_validate(
        _staged_payload(usd_curve_id, eur_curve_id)
    )
    staged = gateway.calibrate_staged_xccy(
        gateway_request_type(staged_request, referenced),
        lambda _at: None,
    )
    basis_rate = staged.curves[0]["parameters"]["right_forwards"][0]
    if abs(basis_rate - 0.02) < 1e-5:
        raise AssertionError(
            "staged gateway returned the initial guess instead of a native solve"
        )
    if staged.solver_diagnostics.max_abs_residual > staged_request.solver.fit_tolerance:
        raise AssertionError("compiled staged residual exceeded fit tolerance")
    print(
        "staged compiled smoke: "
        f"rate={basis_rate:.12g} "
        f"residual={staged.solver_diagnostics.max_abs_residual:.3g}"
    )


def _run_joint_smoke(gateway, request_type, gateway_request_type) -> None:
    joint_request = request_type.model_validate(_joint_payload())
    joint = gateway.calibrate_joint_xccy(
        gateway_request_type(joint_request),
        lambda _at: None,
    )
    if len(joint.curves) != 3:
        raise AssertionError("compiled joint gateway did not return every curve")
    if joint.solver_diagnostics.max_abs_residual > joint_request.solver.fit_tolerance:
        raise AssertionError("compiled joint residual exceeded fit tolerance")
    if joint.jacobian.availability != "available":
        raise AssertionError("compiled joint Jacobian was not materialized")
    if joint.effective_inverse.availability != "available":
        raise AssertionError("compiled joint inverse was not materialized")
    print(
        "joint compiled smoke: "
        f"curves={len(joint.curves)} "
        f"residual={joint.solver_diagnostics.max_abs_residual:.3g}"
    )


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    backend = root / "dal-web" / "backend"
    sys.path.insert(0, str(backend))

    from pydantic import TypeAdapter

    from app.schemas.calibrations import (
        CurveReconstructionDTO,
        JointXccyCalibrationRequest,
        SingleCalibrationRequest,
        StagedXccyCalibrationRequest,
    )
    from app.services.calibrations import (
        SingleGatewayPreLockRequest,
        VerifiedSingleWorkerAdmissionEvidence,
        _project_expected_identity,
        canonical_model_hash,
    )
    from app.services.dal_gateway import (
        DalGateway,
        JointXccyGatewayRequest,
        SingleGatewayAdmissionRequest,
        StagedXccyGatewayRequest,
    )

    gateway = DalGateway()
    _run_single_smoke(
        gateway,
        SingleCalibrationRequest,
        SingleGatewayAdmissionRequest,
        SingleGatewayPreLockRequest,
        VerifiedSingleWorkerAdmissionEvidence,
        _project_expected_identity,
        canonical_model_hash,
    )
    referenced = _referenced_curves(TypeAdapter, CurveReconstructionDTO)
    _run_staged_smoke(
        gateway,
        StagedXccyCalibrationRequest,
        StagedXccyGatewayRequest,
        referenced,
    )
    _run_joint_smoke(gateway, JointXccyCalibrationRequest, JointXccyGatewayRequest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
