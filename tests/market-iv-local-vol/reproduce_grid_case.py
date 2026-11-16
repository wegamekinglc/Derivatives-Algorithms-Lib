"""One-year ATM counterexample using the unchanged frozen time-tail grid."""

import argparse
import json
from pathlib import Path

from grid_study import coefficient
from pde import solve
from qualify import _finish_study, configurations


def select(rows, **fields):
    return next(
        row for row in rows if all(row[key] == value for key, value in fields.items())
    )


def nominal_level(reference):
    return next(
        level
        for level in reference["levels"]
        if (level["nodes"], level["requested_steps"], level["extent"])
        == (3201, 2048, 3)
    )


def levels(model):
    result = {}
    for setting in configurations():
        value = solve(model, 1.0, [100.5], **setting)
        result[(setting["nodes"], setting["steps"], setting["extent"])] = value
    return result


def reproduce(capture):
    fixture = select(capture["fixtures"], id="F2_linear")
    directory = Path(__file__).parent / "fixtures"
    snapshots = json.loads((directory / "snapshots.json").read_text())
    expected = select(snapshots, fixture="F2_linear")["snapshot"]
    if fixture["snapshot"] != expected:
        raise ValueError("Changed fit requires a fresh continuous reference")
    grid = select(fixture["grids"], id="time_tail")
    model = coefficient(fixture, grid)
    runs = levels(model)
    certificate = _finish_study("F2_linear", model, 1.0, runs)
    references = [
        json.loads(line) for line in (directory / "pde.jsonl").read_text().splitlines()
    ]
    continuous = select(references, fixture="F2_linear", maturity=1.0)
    oracle = nominal_level(continuous)["values"][2]
    key = (6401, 4096, 3) if certificate["refined"] else (3201, 2048, 3)
    value = runs[key]["values"][0]
    error = abs(value - oracle) / 100
    return dict(
        fixture="F2_linear",
        grid="time_tail",
        maturity=1.0,
        strike=100.5,
        grid_reference=certificate,
        grid_price=value,
        continuous_oracle=oracle,
        normalized_error=error,
        limit=2.5e-4,
        status="PASS"
        if certificate["status"] == "PASS" and error <= 2.5e-4
        else "FAIL",
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = reproduce(json.loads(args.capture.read_text()))
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(result["status"], result["normalized_error"])
    raise SystemExit(0 if result["status"] == "PASS" else 2)
