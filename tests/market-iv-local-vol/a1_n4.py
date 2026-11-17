"""Actual new-node high precision and production interpolation evidence."""

import json
from pathlib import Path

import mpmath as mp
import numpy as np
from a1_study import make_model
from qualify import curve
from reference import carry, local_variance, theta_at


def nodes(capture, directory):
    count, worst = 0, 0.0
    with mp.workdps(90), (directory / "n4-nodes.jsonl").open("w") as stream:
        for fixture in capture["fixtures"]:
            snapshot = fixture["snapshot"]
            times, values, slopes = curve(snapshot)
            for grid in fixture["grids"]:
                if grid["id"] not in ("P7", "P8"):
                    continue
                for t, time in enumerate(grid["times"]):
                    theta, slope = theta_at(mp.mpf(time), times, values, slopes)
                    forward = carry(mp.mpf(time))[0]
                    for s, spot in enumerate(grid["spots"]):
                        expected = local_variance(
                            theta,
                            slope,
                            mp.mpf(snapshot["rho"]),
                            mp.mpf(snapshot["eta"]),
                            mp.log(mp.mpf(spot) / forward),
                        )
                        actual = (
                            mp.mpf(grid["vols_spot_major"][s * len(grid["times"]) + t])
                            ** 2
                        )
                        error = float(abs(actual - expected) / max(1, abs(expected)))
                        if error > 2e-10:
                            raise ValueError("FAIL: A/1 N4 node")
                        stream.write(
                            json.dumps(
                                [
                                    fixture["id"],
                                    grid["id"],
                                    s,
                                    t,
                                    mp.nstr(expected, 90),
                                    float(actual),
                                    error,
                                ]
                            )
                            + "\n"
                        )
                        count += 1
                        worst = max(worst, error)
    return dict(nodes=count, max_scaled_variance_error=worst, status="PASS", digits=90)


def query_coordinates(grid):
    ts = [
        0.0,
        grid["times"][0] / 2,
        grid["times"][0],
        0.0005,
        0.001,
        0.004,
        (grid["times"][0] + grid["times"][1]) / 2,
        91 / 365,
        np.nextafter(91 / 365, 0),
        np.nextafter(91 / 365, 1),
        273 / 365,
        1.0,
        2.0,
        2.125,
        2.25,
        2.5,
    ]
    spots = [1.0, 10.0, 20.0, 99.0, 100.0, 101.0, 400.0, 600.0, 1000.0]
    spots += [
        float(np.sqrt(a * b)) for a, b in zip(grid["spots"][::19], grid["spots"][1::19])
    ]
    return [(float(t), float(s)) for t in ts for s in spots]


def write_probe_grid(stream, fixture, grid):
    expected, labels = [], []
    model = make_model(fixture, grid)
    queries = query_coordinates(grid)
    stream.write(f"{len(grid['spots'])} {len(grid['times'])} {len(queries)}\n")
    for name in ("spots", "times", "vols_spot_major"):
        stream.write(" ".join(map(repr, grid[name])) + "\n")
    for time, spot in queries:
        stream.write(f"{time!r} {spot!r}\n")
        expected.append(
            float(np.sqrt(model.variance(time, np.array([np.log(spot / 100)])))[0])
        )
        labels.append([fixture["id"], grid["id"], time, spot])
    return expected, labels


def probe_inputs(capture, directory):
    expected, labels = [], []
    with (directory / "probe-input.txt").open("w") as stream:
        for fixture in capture["fixtures"]:
            for grid in fixture["grids"]:
                if grid["id"] in ("P7", "P8"):
                    values, names = write_probe_grid(stream, fixture, grid)
                    expected.extend(values)
                    labels.extend(names)
    return expected, labels


def probes(capture, directory):
    expected, labels = probe_inputs(capture, directory)
    actual = (directory / "probe-actual.txt").read_text()
    values = np.array(
        [[float(v) for v in line.split()] for line in actual.splitlines()]
    )
    errors = np.abs(values - np.array(expected)[:, None])
    result = dict(
        count=len(expected),
        max_vol_error=float(np.max(errors)),
        max_direct_prepared_error=float(np.max(np.abs(values[:, 0] - values[:, 1]))),
        labels=labels,
        expected=expected,
        status="PASS" if np.max(errors) <= 1e-11 else "FAIL",
    )
    if result["status"] != "PASS":
        raise ValueError("FAIL: A/1 off-node production interpolation")
    return result


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["nodes", "inputs", "probes"])
    parser.add_argument("capture", type=Path)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    args.directory.mkdir(parents=True, exist_ok=True)
    functions = dict(nodes=nodes, inputs=probe_inputs, probes=probes)
    result = functions[args.action](
        json.loads(args.capture.read_text()), args.directory
    )
    print(json.dumps(result, allow_nan=False))
