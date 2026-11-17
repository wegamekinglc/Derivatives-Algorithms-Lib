"""Canonical A/1 axes and preregistered Brownian timelines."""

import copy
import json
import math
from pathlib import Path

import mpmath as mp
from a3_protocol import DIMENSIONS, DMAX, coarse_count

SPECS = (
    (161, 41, 49, 12, 36500, ".2", "4", "2.25"),
    (241, 61, 49, 12, 36500, ".2", "4", "2.25"),
    (320, 81, 49, 12, 36500, ".2", "4", "2.25"),
    (320, 81, 97, 24, 36500, ".2", "4", "2.25"),
    (320, 81, 193, 48, 36500, ".2", "4", "2.25"),
    (320, 81, 193, 48, 73000, ".2", "4", "2.25"),
    (320, 81, 193, 48, 146000, ".2", "4", "2.25"),
    (320, 81, 193, 48, 146000, ".1", "6", "2.25"),
    (320, 81, 193, 48, 146000, ".1", "6", "2"),
)
MATURITIES = (91 / 365, 1.0, 2.0, 2.25)
POINTS = (13332, 19932, 26466, 50526, 98646, 98646, 98646, 98646, 98245)


def validate_axis(axis):
    if not axis or not all(math.isfinite(x) for x in axis):
        raise ValueError("Nonfinite or empty axis")
    for left, right in zip(axis, axis[1:]):
        parts = [left + (right - left) * j / 8 for j in range(8)] + [right]
        if any(a >= b for a, b in zip(parts, parts[1:])):
            raise ValueError("Axis cannot support three strict subdivisions")


def spot_axis(base, center, low, high):
    with mp.workdps(90):
        spots = {
            float(
                100
                * mp.exp(
                    mp.log(mp.mpf(low))
                    + (mp.log(mp.mpf(high)) - mp.log(mp.mpf(low))) * j / (base - 1)
                )
            )
            for j in range(base)
        }
        spots.update(
            float(100 * mp.exp(-mp.mpf(".02") + mp.mpf(".04") * j / (center - 1)))
            for j in range(center)
        )
    return sorted(spots)


def time_axis(uniform, early, denominator, horizon):
    with mp.workdps(90):
        tau, end = mp.mpf(1) / denominator, mp.mpf(horizon)
        times = {float(tau + (end - tau) * j / (uniform - 1)) for j in range(uniform)}
        times.update(
            float(tau * (1 / (32 * tau)) ** (mp.mpf(j) / early))
            for j in range(early + 1)
        )
        times.update(
            float(mp.mpf(day) / 365)
            for day in (91, 182, 273, 365, 730)
            if mp.mpf(day) / 365 <= end
        )
    return sorted(times)


def one_grid(index, spec):
    base, center, uniform, early, denominator, low, high, horizon = spec
    spots = spot_axis(base, center, low, high)
    times = time_axis(uniform, early, denominator, horizon)
    row = dict(id=f"P{index}", spots=spots, times=times)
    for axis in (row["spots"], row["times"]):
        validate_axis(axis)
    if len(spots) * len(times) != POINTS[index]:
        raise ValueError("Frozen point count mismatch")
    return row


def grids():
    return [one_grid(i, spec) for i, spec in enumerate(SPECS)]


def protocol(rows):
    anchors = sorted({0.0, *MATURITIES, *rows[7]["times"], *rows[8]["times"]})
    timelines = []
    for level in range(4):
        times = [0.0]
        for index, (left, right) in enumerate(zip(anchors, anchors[1:])):
            count = coarse_count(index, left, right) * 2**level
            times.extend(left + (right - left) * j / count for j in range(1, count))
            times.append(right)
        validate_axis(times)
        timelines.append(times)
    dimensions = [len(t) - 1 for t in timelines]
    if len(anchors) != 438 or dimensions != DIMENSIONS:
        raise ValueError("Frozen Brownian dimensions mismatch")
    return dict(
        anchors=anchors,
        timelines=timelines,
        dimensions=dimensions,
        seed=1024,
        precision="Precise",
        bridge=False,
        batches=16,
        pmax=2**20,
        dmax=DMAX,
        tiers=[2**k for k in range(16, 21)],
        h=[1 / 384, 1 / 768, 1 / 1536, 1 / 3072],
        checked_dimension=10408,
        families=["F2_linear", "F2_nonlinear"],
        final_grids=["P7", "P8"],
    )


def request(original):
    result = copy.deepcopy(original)
    result["schema"] = "dal.market-iv-a1-request/1"
    result["fixtures"] = [f for f in result["fixtures"] if f["id"].startswith("F2")]
    result["grids"] = grids()
    result["protocol"] = protocol(result["grids"])
    from prepare_mc import prepare

    return prepare(result)


def write_request(path):
    from generate import request as market_request

    data = (
        json.dumps(request(market_request()), indent=2, allow_nan=False) + "\n"
    ).encode()
    if path.exists() and path.read_bytes() != data:
        raise ValueError("Refusing to replace a different frozen request")
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        path.write_bytes(data)


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    write_request(parser.parse_args().output)
