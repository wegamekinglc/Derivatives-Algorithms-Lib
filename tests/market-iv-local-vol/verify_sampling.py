"""Independently recompute SE and audit the fixed coordinate/tier protocol."""

import argparse
import json
from pathlib import Path

import numpy as np


def require(condition, message):
    if not condition:
        raise ValueError(message)


def check_tier(tier):
    adequate = True
    for index, group in enumerate(tier["groups"]):
        batches = np.array(group["batch_means"])
        estimates = np.array(group["estimates"])
        require(
            batches.shape == (16, 80) and estimates.shape == (80, 2),
            "Batch/output shape",
        )
        require(np.all(np.isfinite(batches)), "Nonfinite batch mean")
        mean, se = batches.mean(axis=0), batches.std(axis=0, ddof=1) / 4
        require(
            np.max(np.abs(mean - estimates[:, 0])) < 1e-10,
            "Batch estimator mean mismatch",
        )
        require(
            np.max(np.abs(se - estimates[:, 1])) < 1e-12, "Batch estimator SE mismatch"
        )
        adequate = adequate and bool(np.max(se) <= (0.01 if index < 4 else 0.005))
    for index in range(3):
        paired = np.array(tier["groups"][4 + index]["batch_means"])
        expected = np.array(tier["groups"][index]["batch_means"]) - np.array(
            tier["groups"][index + 1]["batch_means"]
        )
        require(
            np.max(np.abs(paired - expected)) < 1e-10,
            "Paired batch difference mismatch",
        )
    require(adequate == tier["se_sufficient"], "SE-only stopping mismatch")


def check_coordinates(mc):
    require(
        mc["seed"] == 1024 and mc["pmax"] == 2**20 and mc["degrees_of_freedom"] == 15,
        "Frozen sampling metadata",
    )
    fine = mc["timelines"][-1]
    require(mc["dmax"] == len(fine) - 1, "Fine coordinate reservation")
    for level, times in enumerate(mc["timelines"]):
        require(times == fine[:: 2 ** (3 - level)], "Non-nested retained anchors")
        require(
            np.max(np.diff(times)) <= 1 / (48 * 2**level) * (1 + 1e-12),
            "Actual max dt exceeds requested h",
        )
    intervals = [
        (batch * 2**20 * mc["dmax"], (batch + 1) * 2**20 * mc["dmax"])
        for batch in range(16)
    ]
    require(intervals[-1][1] < 2**64, "Scalar address overflow")
    return intervals


def verify(path):
    mc = json.loads(path.read_text())
    require(mc["sampling"] == "FORMAL", "Smoke cannot enter formal sampling acceptance")
    intervals = check_coordinates(mc)
    for index, tier in enumerate(mc["tiers"]):
        require(
            tier["paths_per_batch"] == 2 ** (16 + index), "Frozen prefix tier schedule"
        )
        check_tier(tier)
        if index + 1 < len(mc["tiers"]):
            require(not tier["se_sufficient"], "Expanded after SE was sufficient")
    last = mc["tiers"][-1]
    require(
        last["se_sufficient"] or last["paths_per_batch"] == 2**20,
        "Stopped before SE sufficiency or ceiling",
    )
    return dict(
        fixture=mc["fixture"],
        status="PASS",
        normal_precision="Precise",
        bridge=False,
        seed=1024,
        dmax=mc["dmax"],
        alignment_paths=64,
        alignment_price=mc["default_alignment_price"],
        dimensions=[len(times) - 1 for times in mc["timelines"]],
        actual_max_dt=[float(np.max(np.diff(times))) for times in mc["timelines"]],
        paths_per_batch=last["paths_per_batch"],
        paths_total=16 * last["paths_per_batch"],
        scalar_intervals=intervals,
        tier_seconds=[tier["seconds"] for tier in mc["tiers"]],
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = [
        verify(args.directory / f"{fixture}-formal.json")
        for fixture in ("F1_knots", "F1_flat", "F2_linear", "F2_nonlinear")
    ]
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("Four sampling families PASS")
