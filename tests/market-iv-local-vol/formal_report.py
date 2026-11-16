"""Preserve every registered output and separate N3/N5 error components."""

import argparse
from collections import Counter
import json
from pathlib import Path

import mpmath as mp

from generate import generating_curve
from qualify import fit_rows, fitted_price
from reference import black, carry, ssvi, theta_at
from verify_sampling import verify

PRODUCTS = (
    ("call",) * 5
    + ("put",) * 5
    + ("discount", "discounted_forward")
    + ("parity",) * 5
    + ("forward_mean", "fixed_payment", "delayed_payment")
)


def strikes(case):
    values = list(map(float, case["strikes"]))
    return values + values + [None, None] + values + [None] * 3


def generating_price(fixture, time, strike, call):
    forward, discount = carry(mp.mpf(time), fixture["flat"])
    theta, _ = theta_at(
        mp.mpf(time), *generating_curve(fixture["id"] == "F2_nonlinear")
    )
    variance = (
        theta
        if fixture["strict"]
        else ssvi(theta, mp.mpf("-.35"), mp.mpf(".6"), mp.log(strike / forward))
    )
    return black(forward, discount, strike, variance, call)


def reference_values(fixture, case, snapshot, continuous):
    time = float(case["time"])
    forward, discount = map(float, carry(mp.mpf(time), fixture["flat"]))
    keys = list(map(mp.mpf, case["strikes"]))
    if fixture["strict"]:
        calls = [
            float(
                black(mp.mpf(forward), mp.mpf(discount), strike, mp.mpf(".04") * time)
            )
            for strike in keys
        ]
        puts = [
            float(
                black(
                    mp.mpf(forward),
                    mp.mpf(discount),
                    strike,
                    mp.mpf(".04") * time,
                    False,
                )
            )
            for strike in keys
        ]
    else:
        calls, puts = continuous[:5], continuous[5:10]
    prices = calls + puts + [discount, discount * forward]
    prices += [discount * (forward - float(strike)) for strike in keys]
    prices += [forward, discount, float(carry(mp.mpf("2.25"), fixture["flat"])[1])]
    fit_errors = [
        float(fitted_price(snapshot, mp.mpf(time), strike, fixture["flat"], call))
        - float(generating_price(fixture, time, strike, call))
        for call in (True, False)
        for strike in keys
    ]
    return prices, fit_errors + [0.0] * 10


def statistical_gate(estimate, se, oracle, strict):
    if se / 100 > 1e-4:
        return "INCOMPLETE"
    limit = 5e-5 if strict else 6e-4
    return "PASS" if abs(estimate - oracle) / 100 <= limit + 4.5 * se / 100 else "FAIL"


def gate(estimate, se, oracle, strict, deterministic, reference_status):
    if estimate is None or se is None:
        return "NOT_RUN"
    if deterministic:
        return (
            "PASS" if abs(estimate - oracle) / max(1, abs(oracle)) <= 2e-12 else "FAIL"
        )
    if reference_status != "PASS":
        return "REFERENCE_UNAVAILABLE"
    return statistical_gate(estimate, se, oracle, strict)


def qualified_status(price, grid, pair, deterministic, strict):
    if price != "PASS" or deterministic or strict:
        return price
    if pair != "PASS":
        return pair
    if grid["status"] == "PASS":
        return "PASS"
    certificate = grid.get("reference_self_check")
    return (
        "FAIL"
        if certificate and certificate["status"] == "PASS"
        else "REFERENCE_UNAVAILABLE"
    )


def measurement(mc, location):
    if mc is None:
        return dict(
            estimate=None,
            se=None,
            paired_difference=None,
            paired_se=None,
            source_sampling=None,
            paths_per_batch=None,
        )
    tier = mc["tiers"][-1]
    estimate, se = tier["groups"][3]["estimates"][location]
    pair, pair_se = tier["groups"][6]["estimates"][location]
    return dict(
        estimate=estimate,
        se=se,
        paired_difference=pair,
        paired_se=pair_se,
        source_sampling=mc["sampling"],
        paths_per_batch=tier["paths_per_batch"],
    )


def normalized(value):
    return None if value is None else value / 100


def normalized_difference(value, oracle):
    return None if value is None or oracle is None else (value - oracle) / 100


def final_level(study):
    certification = study.get("reference_self_check")
    key = (
        (6401, 4096, 3)
        if certification and certification["refined"]
        else (3201, 2048, 3)
    )
    return next(
        level["values"]
        for level in study["levels"]
        if (level["nodes"], level["requested_steps"], level["extent"]) == key
    )


def error_components(grid, case):
    values = final_level(grid)
    time = float(case["time"])
    errors = grid["normalized_grid_error"]
    flat = grid["fixture"] == "F1_flat"
    discount = float(carry(mp.mpf(time), flat)[1])
    delayed = float(carry(mp.mpf("2.25"), flat)[1]) / discount
    return (
        values[:10]
        + values[10:12]
        + [values[i] - values[i + 5] for i in range(5)]
        + [values[-1] / discount, values[-2], values[-2] * delayed],
        errors[:10]
        + errors[10:12]
        + [errors[i] - errors[i + 5] for i in range(5)]
        + [errors[-1] / discount, errors[-2], errors[-2] * delayed],
        time,
    )


def result_row(
    fixture, grid, case, index, oracle, fit_error, mc, grid_values, grid_errors
):
    reference = grid.get("reference_self_check")
    reference_status = reference["status"] if reference else "REFERENCE_UNAVAILABLE"
    measured = measurement(mc, 20 * index[0] + index[1])
    product = PRODUCTS[index[1]]
    deterministic = product in ("discount", "fixed_payment", "delayed_payment")
    price = gate(
        measured["estimate"],
        measured["se"],
        oracle,
        fixture["strict"],
        deterministic,
        "PASS",
    )
    pair = paired_gate(measured["paired_difference"], measured["paired_se"])
    status = qualified_status(price, grid, pair, deterministic, fixture["strict"])
    return dict(
        fixture=fixture["id"],
        grid=grid["grid"],
        maturity=float(case["time"]),
        product=product,
        strike=strikes(case)[index[1]],
        **measured,
        oracle=oracle,
        normalized_error=normalized_difference(measured["estimate"], oracle),
        stepping_plus_sampling_normalized=normalized_difference(
            measured["estimate"], grid_values[index[1]]
        ),
        fit_error_normalized=normalized(fit_error),
        grid_pde=grid_values[index[1]],
        grid_error_normalized=grid_errors[index[1]],
        grid_gate=grid["status"],
        grid_reference_status=reference_status,
        paired_gate=pair,
        price_gate=price,
        status=status,
        seed=1024,
        stream_ids=list(range(16)),
        degrees_of_freedom=15,
        thresholds=dict(
            price=5e-5 if fixture["strict"] else 6e-4,
            se=1e-4,
            paired=2.5e-4,
            paired_se=5e-5,
            multiplier=4.5,
        ),
    )


def paired_gate(value, se):
    if value is None or se is None:
        return "NOT_RUN"
    if se / 100 > 5e-5:
        return "INCOMPLETE"
    return "PASS" if abs(value) / 100 <= 2.5e-4 + 4.5 * se / 100 else "FAIL"


def nominal_level(reference):
    return next(
        level["values"]
        for level in reference["levels"]
        if (level["nodes"], level["requested_steps"], level["extent"])
        == (3201, 2048, 3)
    )


def fixture_rows(fixture, snapshot, references, grids, mc, grid_id):
    rows = []
    for ordinal, case in enumerate(fixture["mc_cases"]):
        time = float(case["time"])
        continuous = references[(fixture["id"], time)]
        if continuous["status"] != "PASS":
            raise ValueError("Continuous reference unavailable")
        grid = grids[(fixture["id"], grid_id, time)]
        values, errors, _ = error_components(grid, case)
        oracles, fit_errors = reference_values(
            fixture, case, snapshot, nominal_level(continuous)
        )
        rows.extend(
            result_row(
                fixture,
                grid,
                case,
                (ordinal, output),
                oracles[output],
                fit_errors[output],
                mc,
                values,
                errors,
            )
            for output in range(20)
        )
    return rows


def load_mc(directory, fixture, suffix):
    path = directory / f"{fixture}-{suffix}.json"
    if not path.exists():
        return None
    result = json.loads(path.read_text())
    if result["sampling"] != "FORMAL":
        raise ValueError("Smoke output cannot certify a numerical qualification")
    verify(path)
    return result


def preflight(fixture, observed):
    if not fixture["strict"]:
        return dict(status="NOT_APPLICABLE")
    fit_error = max(row["normalized_error"] for row in fit_rows(fixture, observed))
    grid = next(row for row in observed["grids"] if row["id"] == "tau_quarter")
    vol_error = max(abs(value - 0.2) for value in grid["vols_spot_major"])
    return dict(
        status="PASS" if fit_error <= 1e-6 and vol_error <= 1e-7 else "FAIL",
        fit_max_error=fit_error,
        local_vol_max_error=vol_error,
    )


def all_pass(rows):
    return all(row["status"] == "PASS" for row in rows)


def preflights_pass(rows):
    return all(row["status"] in ("PASS", "NOT_APPLICABLE") for row in rows.values())


def acceptance(rows, grids, preflights):
    selected = [row for row in grids if row["grid"] in ("tau_quarter", "time_tail")]
    if tuple(map(len, (rows, selected, preflights))) != (320, 32, 4):
        return False
    return all_pass(rows) and all_pass(selected) and preflights_pass(preflights)


def require_capture_identity(request, capture):
    if [row["id"] for row in request["fixtures"]] != [
        row["id"] for row in capture["fixtures"]
    ]:
        raise ValueError("Incomplete or mismatched production capture")


def report_summary(rows, grids, preflights):
    return dict(
        schema="dal.market-iv-formal-qualification/1",
        rows=rows,
        counts=dict(Counter(row["status"] for row in rows)),
        grid_counts=dict(Counter(row["status"] for row in grids)),
        preflight=preflights,
        accepted=acceptance(rows, grids, preflights),
        reason="All price, SE, grid and reference admission gates must pass separately",
    )


def apply_preflight(rows, preflight_result):
    return [
        dict(
            row,
            preflight_gate=preflight_result["status"],
            status="FAIL" if preflight_result["status"] == "FAIL" else row["status"],
        )
        for row in rows
    ]


def report(directory):
    request = json.loads((directory / "mc-request.json").read_text())
    capture = json.loads((directory / "capture.json").read_text())
    require_capture_identity(request, capture)
    grids = json.loads((directory / "grid-pde-qualified.json").read_text())
    references = [
        json.loads(line)
        for line in (Path(__file__).parent / "fixtures/pde.jsonl")
        .read_text()
        .splitlines()
    ]
    reference_index = {(row["fixture"], row["maturity"]): row for row in references}
    grid_index = {(row["fixture"], row["grid"], row["maturity"]): row for row in grids}
    rows = []
    preflights = {}
    with mp.workdps(90):
        for fixture, observed in zip(request["fixtures"], capture["fixtures"]):
            preflights[fixture["id"]] = preflight(fixture, observed)
            mc = load_mc(directory, fixture["id"], "formal")
            current = fixture_rows(
                fixture,
                observed["snapshot"],
                reference_index,
                grid_index,
                mc,
                "tau_quarter",
            )
            rows.extend(apply_preflight(current, preflights[fixture["id"]]))
    return report_summary(rows, grids, preflights)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = report(args.directory)
    args.output.write_text(
        json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8"
    )
    print(result["counts"])
    raise SystemExit(0 if result["accepted"] else 2)
