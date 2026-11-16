"""Qualify captured discrete grids against the accepted continuous references."""

import argparse
from concurrent.futures import ProcessPoolExecutor
import json
from pathlib import Path

import numpy as np

from pde import GridCoefficients, solve
from qualify import MATURITIES, coefficients, configurations, _finish_study
from reference import RATIOS, carry


class CertifiedGrid(GridCoefficients):
    @property
    def flat(self):
        return self.continuous.flat


def coefficient(fixture, grid):
    continuous = coefficients(fixture["snapshot"], fixture["id"] == "F1_flat")
    return CertifiedGrid(
        continuous,
        np.log(np.array(grid["spots"]) / 100),
        np.array(grid["times"]),
        np.array(grid["vols_spot_major"]).reshape(
            grid["spot_nodes"], grid["time_nodes"]
        ),
    )


def task(args):
    name, grid_id, maturity, model, settings = args
    forward = float(carry(maturity, model.continuous.flat)[0])
    result = solve(
        model, maturity, [float(ratio) * forward for ratio in RATIOS], **settings
    )
    return name, grid_id, maturity, settings, result


def tasks(capture):
    result = []
    for fixture in capture["fixtures"]:
        for grid in fixture["grids"]:
            if not grid["success"]:
                raise ValueError("Production grid unavailable: " + grid["id"])
            model = coefficient(fixture, grid)
            settings = (
                configurations()
                if grid["id"] in ("tau_quarter", "time_tail")
                else [dict(nodes=3201, steps=2048, extent=3)]
            )
            for maturity in MATURITIES:
                result.extend(
                    (fixture["id"], grid["id"], maturity, model, setting)
                    for setting in settings
                )
    return result


def certify_grid(fixture, grid, maturity, runs):
    if grid["id"] not in ("tau_quarter", "time_tail"):
        return None
    return _finish_study(fixture["id"], coefficient(fixture, grid), maturity, runs)


def admitted_status(reference, certification, delta):
    if (
        certification is None
        or certification["status"] != "PASS"
        or reference["status"] != "PASS"
    ):
        return "INCOMPLETE"
    return "PASS" if np.max(np.abs(delta)) <= 2.5e-4 else "FAIL"


def comparison(fixture, grid, maturity, reference, runs):
    baseline = next(
        level
        for level in reference["levels"]
        if (level["nodes"], level["requested_steps"], level["extent"])
        == (3201, 2048, 3)
    )
    certification = certify_grid(fixture, grid, maturity, runs)
    final = (
        (6401, 4096, 3)
        if certification and certification["refined"]
        else (3201, 2048, 3)
    )
    delta = (np.array(runs[final]["values"]) - baseline["values"]) / 100
    return dict(
        fixture=fixture["id"],
        grid=grid["id"],
        maturity=maturity,
        normalized_grid_error=delta.tolist(),
        status=admitted_status(reference, certification, delta),
        reference_self_check=certification,
        levels=list(runs.values()),
    )


def compare(capture, continuous, groups):
    lookup = {(row["fixture"], row["maturity"]): row for row in continuous}
    return [
        comparison(
            fixture,
            grid,
            maturity,
            lookup[(fixture["id"], maturity)],
            groups[(fixture["id"], grid["id"], maturity)],
        )
        for fixture in capture["fixtures"]
        for grid in fixture["grids"]
        for maturity in MATURITIES
    ]


def study(capture, continuous, jobs):
    groups = {}
    with ProcessPoolExecutor(max_workers=jobs) as executor:
        for name, grid_id, maturity, settings, result in executor.map(
            task, tasks(capture)
        ):
            key = (settings["nodes"], settings["steps"], settings["extent"])
            groups.setdefault((name, grid_id, maturity), {})[key] = result
    return compare(capture, continuous, groups)


def recertify(capture, continuous, previous):
    groups = {}
    for row in previous:
        groups[(row["fixture"], row["grid"], row["maturity"])] = {
            (level["nodes"], level["requested_steps"], level["extent"]): level
            for level in row["levels"]
            if level["nodes"] <= 3201 and level["requested_steps"] <= 2048
        }
    return compare(capture, continuous, groups)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--reuse-levels", type=Path)
    args = parser.parse_args()
    accepted = [
        json.loads(line)
        for line in (Path(__file__).parent / "fixtures/pde.jsonl")
        .read_text()
        .splitlines()
    ]
    capture = json.loads(args.capture.read_text())
    rows = (
        recertify(capture, accepted, json.loads(args.reuse_levels.read_text()))
        if args.reuse_levels
        else study(capture, accepted, args.jobs)
    )
    args.output.write_text(
        json.dumps(rows, indent=2, allow_nan=False) + "\n", encoding="utf-8"
    )
    print("grid PDE rows:", len(rows))
