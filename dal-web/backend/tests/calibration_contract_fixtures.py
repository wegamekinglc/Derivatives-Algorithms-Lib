from __future__ import annotations

import copy
import time
from collections.abc import Mapping
from datetime import date, timedelta

from fastapi.testclient import TestClient


def future_knots(count: int) -> list[str]:
    return [
        date(
            2026 + month_index // 12,
            month_index % 12 + 1,
            2,
        ).isoformat()
        for month_index in range(1, count + 1)
    ]


def rate_index() -> dict[str, object]:
    return {
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


def rate_leg() -> dict[str, object]:
    return {
        "payment_frequency": "P6M",
        "day_basis": "ACT_365F",
        "payment_lag": 0,
        "business_day_convention": "Following",
        "payment_convention": "Following",
        "accrual_holidays": "",
        "payment_holidays": "",
        "end_of_month": False,
    }


def xccy_config() -> dict[str, object]:
    return {
        "pair": {"domestic": "USD", "foreign": "EUR"},
        "domestic_notional": 110.0,
        "foreign_notional": 100.0,
        "convention": {
            "initial_notional_exchange": True,
            "final_notional_exchange": True,
            "spread_on_foreign_leg": True,
            "domestic_index": rate_index(),
            "domestic_leg": rate_leg(),
            "foreign_index": rate_index(),
            "foreign_leg": rate_leg(),
        },
        "notional_mode": "FIXED",
        "fx_reset": {
            "fixing_lag": -1,
            "fixing_holidays": "",
            "fixing_convention": "Preceding",
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


def solver(*, tolerance: float = 1.0e-8) -> dict[str, object]:
    return {
        "solve_mode": "EXACT",
        "smoothing_weight": 1.0,
        "tolerance": tolerance,
        "fit_tolerance": 1.0e-6,
        "initial_guess": 0.0,
        "max_evaluations": 200,
        "max_restarts": 20,
    }


def options(*, matrices: bool = True) -> dict[str, object]:
    return {
        "jacobian_mode": "ANALYTIC",
        "include_jacobian": matrices,
        "include_effective_inverse": matrices,
    }


def deposit(currency: str, rate: float) -> dict[str, object]:
    return {
        "kind": "DEPOSIT",
        "label": f"{currency} DEP 1Y",
        "trade_date": "2026-01-02",
        "start": "2026-01-02",
        "maturity": "2027-01-02",
        "market_rate": rate,
        "index": rate_index(),
    }


def single_request(currency: str, rate: float) -> dict[str, object]:
    return {
        "schema_version": 1,
        "name": f"{currency.lower()}_ois_2026_01_02",
        "today": "2026-01-02",
        "currency": currency,
        "declaration": {
            "curve_name": f"{currency.lower()}_ois",
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
            "initial_guess_per_node": [rate],
        },
        "instruments": [deposit(currency, rate)],
        "solver": solver(),
        "options": options(matrices=False),
    }


def xccy_swap() -> dict[str, object]:
    return {
        "kind": "XCCY_SWAP",
        "label": "USD/EUR XCCY 1Y",
        "trade_date": "2026-01-02",
        "start": "2026-01-02",
        "maturity": "2027-01-02",
        "market_rate": -0.001,
        "config": xccy_config(),
    }


def staged_request(
    domestic_curve_id: str, foreign_curve_id: str
) -> dict[str, object]:
    return {
        "schema_version": 1,
        "name": "usd_eur_basis_2026_01_02",
        "valuation_time": "2026-01-02T10:00:00",
        "pair": {"domestic": "USD", "foreign": "EUR"},
        "collateral_currency": "USD",
        "fx_spot": 1.10,
        "fx_forward_collateral": "OIS",
        "domestic_curve_block": {
            "name": "usd_market",
            "currency": "USD",
            "libor_basis": "ACT_365F",
            "discount_curve_ids": {"OIS": domestic_curve_id},
            "forward_curve_ids": {},
        },
        "foreign_curve_block": {
            "name": "eur_market",
            "currency": "EUR",
            "libor_basis": "ACT_365F",
            "discount_curve_ids": {"OIS": foreign_curve_id},
            "forward_curve_ids": {},
        },
        "basis": {
            "curve_name": "usd_eur_basis",
            "knot_dates": ["2027-01-02"],
            "instruments": [xccy_swap()],
            "initial_guess_per_node": [0.0],
        },
        "fixings": [],
        "solver": solver(tolerance=1.0e-10),
        "options": options(),
    }


def joint_request() -> dict[str, object]:
    declaration = {
        "calibrate_discount_curve": True,
        "target_collateral": "OIS",
        "target_tenor": None,
        "base_layered_over_discount": False,
        "parameterization": "PIECEWISE_CONSTANT_FWD",
        "log_df_scheme": None,
        "knot_dates": ["2027-01-02"],
        "smoothing_weight": 1.0,
    }
    domestic = copy.deepcopy(declaration)
    domestic.update(
        {
            "curve_name": "usd_ois",
            "instruments": [deposit("USD", 0.04)],
            "initial_guess_per_node": [0.04],
        }
    )
    foreign = copy.deepcopy(declaration)
    foreign.update(
        {
            "curve_name": "eur_ois",
            "instruments": [deposit("EUR", 0.03)],
            "initial_guess_per_node": [0.03],
        }
    )
    return {
        "schema_version": 1,
        "name": "usd_eur_joint_2026_01_02",
        "valuation_time": "2026-01-02T10:00:00",
        "pair": {"domestic": "USD", "foreign": "EUR"},
        "collateral_currency": "USD",
        "fx_spot": 1.10,
        "domestic": {
            "currency": "USD",
            "libor_basis": "ACT_365F",
            "declarations": [domestic],
        },
        "foreign": {
            "currency": "EUR",
            "libor_basis": "ACT_365F",
            "declarations": [foreign],
        },
        "basis": {
            "curve_name": "usd_eur_basis",
            "parameterization": "PIECEWISE_CONSTANT_FWD",
            "log_df_scheme": None,
            "knot_dates": ["2027-01-02"],
            "instruments": [xccy_swap()],
            "smoothing_weight": 1.0,
            "initial_guess_per_node": [0.0],
        },
        "fixings": [],
        "solver": solver(),
        "options": options(),
    }


def matrix_metadata_request(
    *,
    include_jacobian: bool,
    include_effective_inverse: bool,
) -> dict[str, object]:
    request = single_request("USD", 0.02)
    knots = future_knots(100)
    request["declaration"].update(
        {
            "parameterization": "PIECEWISE_LINEAR_FWD",
            "knot_dates": knots,
            "initial_guess_per_node": [0.02] * 200,
        }
    )
    request["instruments"] = [
        {
            **deposit("USD", 0.02 + index * 0.0001),
            "label": f"USD DEP {index + 1}",
            "maturity": knot,
        }
        for index, knot in enumerate(knots)
    ]
    request["options"] = {
        "jacobian_mode": "ANALYTIC",
        "include_jacobian": include_jacobian,
        "include_effective_inverse": include_effective_inverse,
    }
    return request


def joint_capacity_request(total: int) -> dict[str, object]:
    if total not in {200, 201, 202}:
        raise ValueError("joint capacity fixture supports only 200/201/202")
    request = joint_request()
    request["solver"]["solve_mode"] = "APPROXIMATE"
    request["options"] = options(matrices=False)
    domestic = request["domestic"]["declarations"][0]
    foreign = request["foreign"]["declarations"][0]
    basis = request["basis"]
    domestic_knots = 100 if total == 202 else 99
    domestic.update(
        {
            "parameterization": "PIECEWISE_LINEAR_FWD",
            "knot_dates": future_knots(domestic_knots),
            "initial_guess_per_node": [0.02] * (2 * domestic_knots),
        }
    )
    foreign["knot_dates"] = future_knots(1)
    foreign["initial_guess_per_node"] = [0.03]
    basis_knots = 2 if total == 201 else 1
    basis["knot_dates"] = future_knots(basis_knots)
    basis["initial_guess_per_node"] = [0.0] * basis_knots
    return request


def policy_resolution_request(policy: str) -> dict[str, object]:
    """D-15 fixed traversal input for INPUT/INSTRUMENTS/AUGMENTED."""
    if policy not in {"INPUT", "INSTRUMENTS", "AUGMENTED"}:
        raise ValueError("unsupported policy-resolution fixture")
    request = single_request("USD", 0.04)
    request["declaration"].update(
        {
            "knot_policy": policy,
            "knot_dates": (
                [] if policy == "INSTRUMENTS" else ["2026-03-02", "2026-05-02"]
            ),
            "initial_guess_per_node": [],
        }
    )
    first = deposit("USD", 0.04)
    first.update(
        {
            "label": "i0",
            "start": "2026-01-02",
            "maturity": "2026-03-02",
        }
    )
    second = deposit("USD", 0.041)
    second.update(
        {
            "label": "i1",
            "start": "2026-04-02",
            "maturity": "2026-05-02",
        }
    )
    request["instruments"] = [first, second]
    request["solver"]["solve_mode"] = "APPROXIMATE"
    return request


def first_offender_request(source: str) -> tuple[dict[str, object], dict[str, object]]:
    """D-17 fixed 101st-storage-node traversal cases for API-11."""
    request = single_request("USD", 0.02)
    request["declaration"]["initial_guess_per_node"] = []
    request["solver"]["solve_mode"] = "APPROXIMATE"
    first_date = date(2027, 1, 2)

    if source == "input":
        knots = [(first_date + timedelta(days=index)).isoformat() for index in range(100)]
        request["declaration"].update(
            {
                "parameterization": "ZERO_RATE",
                "log_df_scheme": "LOG_LINEAR",
                "knot_dates": knots,
            }
        )
        request["instruments"][0]["maturity"] = knots[-1]
        return request, {
            "location": ["body", "declaration", "knot_dates", 99],
            "candidate_ordinal": 99,
            "candidate_date": knots[99],
            "origin": {"kind": "INPUT", "input_knot_index": 99},
        }

    if source in {"start", "maturity"}:
        count = 51 if source == "start" else 50
        instruments = []
        for index in range(count):
            start = first_date + timedelta(days=2 * index)
            maturity = start + timedelta(days=1)
            instrument = deposit("USD", 0.02 + index * 0.00001)
            instrument.update(
                {
                    "label": f"i{index}",
                    "start": start.isoformat(),
                    "maturity": maturity.isoformat(),
                }
            )
            instruments.append(instrument)
        request["declaration"].update(
            {
                "knot_policy": "INSTRUMENTS",
                "knot_dates": [],
                "parameterization": (
                    "PIECEWISE_CONSTANT_FWD"
                    if source == "start"
                    else "ZERO_RATE"
                ),
                "log_df_scheme": None if source == "start" else "LOG_LINEAR",
            }
        )
        request["instruments"] = instruments
        instrument_index = 50 if source == "start" else 49
        candidate_ordinal = 100 if source == "start" else 99
        field = source
        return request, {
            "location": ["body", "instruments", instrument_index, field],
            "candidate_ordinal": candidate_ordinal,
            "candidate_date": instruments[instrument_index][field],
            "origin": {
                "kind": (
                    "INSTRUMENT_START"
                    if source == "start"
                    else "INSTRUMENT_END"
                ),
                "instrument_input_index": instrument_index,
            },
        }

    if source == "augmented":
        knots = [(first_date + timedelta(days=index)).isoformat() for index in range(99)]
        first = deposit("USD", 0.02)
        first.update(
            {
                "label": "filtered-and-duplicate",
                "start": "2026-01-02",
                "maturity": knots[0],
            }
        )
        second = deposit("USD", 0.021)
        second.update(
            {
                "label": "first-augmented-offender",
                "start": (first_date + timedelta(days=99)).isoformat(),
                "maturity": (first_date + timedelta(days=100)).isoformat(),
            }
        )
        request["declaration"].update(
            {
                "knot_policy": "AUGMENTED",
                "knot_dates": knots,
            }
        )
        request["instruments"] = [first, second]
        return request, {
            "location": ["body", "instruments", 1, "maturity"],
            "candidate_ordinal": 102,
            "candidate_date": second["maturity"],
            "origin": {
                "kind": "INSTRUMENT_END",
                "instrument_input_index": 1,
            },
        }

    raise ValueError("unsupported first-offender fixture")


def wait_for_terminal(
    client: TestClient, submitted: Mapping[str, object]
) -> dict[str, object]:
    location = str(submitted["location"])
    for _ in range(200):
        response = client.get(location)
        body = response.json()
        if body["status"] != "running":
            return body
        time.sleep(0.005)
    raise AssertionError(f"calibration remained running at {location}")


def submit_and_wait(
    client: TestClient, path: str, payload: dict[str, object]
) -> dict[str, object]:
    response = client.post(path, json=payload)
    assert response.status_code == 202, response.text
    return wait_for_terminal(
        client,
        {
            "location": response.headers["location"],
        },
    )
