#!/usr/bin/env python3
"""Paired informational measurements for the isolated compiled-double SoA study."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import statistics
import subprocess


def fingerprint(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cases():
    selected = []
    for workload in ("vanilla", "weekly"):
        for lanes in (8, 16, 32, 64):
            for phase in ("eval", "end_to_end"):
                selected.append((workload, lanes, phase, 16387, True))
        for phase in ("model", "pipeline"):
            selected.append((workload, 32, phase, 16387, True))
    for fallback in (True, False):
        for phase in ("eval", "end_to_end"):
            selected.append(("divergent", 32, phase, 16387, fallback))
    for paths in (1, 7, 9, 65):
        selected.append(("weekly", 32, "end_to_end", paths, True))
    return selected


def run(binary, case, engine, iterations, output):
    workload, lanes, phase, paths, fallback = case
    command = [str(binary), "--workload", workload, "--lanes", str(lanes),
               "--phase", phase, "--paths", str(paths), "--fallback", str(fallback).lower(),
               "--engine", engine, "--iterations", str(iterations)]
    completed = subprocess.run(command, text=True, capture_output=True, check=False, timeout=120)
    record = {"command": command, "returncode": completed.returncode,
              "stdout": completed.stdout, "stderr": completed.stderr}
    output.write_text(json.dumps(record, indent=2) + "\n")
    completed.check_returncode()
    result = json.loads(completed.stdout.splitlines()[-1])
    if not math.isfinite(result["seconds"]) or result["seconds"] <= 0 or not math.isfinite(result["pv"]):
        raise ValueError(f"invalid measurement: {output}")
    return result


def verify_pair(base, head):
    for key in ("workload", "phase", "paths", "lanes", "iterations", "fallback", "events", "shock_fingerprint"):
        if base[key] != head[key]:
            raise ValueError(f"identity mismatch: {key}")
    # Absolute guard only applies when the reference is exactly zero.
    tolerance = 1e-12 * abs(base["pv"]) if base["pv"] != 0 else 1e-14
    if abs(base["pv"] - head["pv"]) > tolerance:
        raise ValueError(f"PV mismatch: {base['pv']} vs {head['pv']}")


def measure(args):
    args.output.mkdir(parents=True, exist_ok=False)
    binaries = {"scalar": args.baseline.resolve(), "soa": args.head.resolve()}
    hashes = {side: fingerprint(binary) for side, binary in binaries.items()}
    manifest = {"binaries": {side: str(path) for side, path in binaries.items()},
                "sha256": hashes, "samples": 10, "rounds": 2, "reduction": "min",
                "noise_threshold_percent": 4, "DAL_NUM_THREADS": os.environ.get("DAL_NUM_THREADS"),
                "scope": "informational prototype; outside the nine-target regression gate"}
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    results = []
    for index, case in enumerate(cases()):
        directory = args.output / f"case-{index:02d}"
        directory.mkdir()
        calibration = {side: run(binary, case, side, 1, directory / f"calibration-{side}.json")
                       for side, binary in binaries.items()}
        verify_pair(calibration["scalar"], calibration["soa"])
        iterations = min(100000, max(1, math.ceil(0.05 / min(x["seconds"] for x in calibration.values()))))
        samples = []
        for round_index in range(2):
            for sample in range(10):
                order = ("scalar", "soa") if (round_index * 10 + sample) % 2 == 0 else ("soa", "scalar")
                pair = {side: run(binaries[side], case, side, iterations,
                                  directory / f"r{round_index}-s{sample:02d}-{side}.json") for side in order}
                verify_pair(pair["scalar"], pair["soa"])
                samples.append({"round": round_index, "sample": sample, "order": order, "results": pair})
        minima = [{side: min(x["results"][side]["seconds"] for x in samples if x["round"] == r)
                   for side in binaries} for r in range(2)]
        ratios = [x["scalar"] / x["soa"] for x in minima]
        deltas = [100 * (x["soa"] / x["scalar"] - 1) for x in minima]
        verdict = ("improvement" if all(x < -4 for x in deltas) else
                   "regression" if all(x > 4 for x in deltas) else "no-change")
        target = "achieved" if min(ratios) / 1.04 >= 2 else "not-achieved" if max(ratios) * 1.04 < 2 else "inconclusive"
        spread = {side: [statistics.median(x["results"][side]["seconds"] for x in samples if x["round"] == r) / minima[r][side]
                         for r in range(2)] for side in binaries}
        result = {"case": case, "iterations": iterations, "round_minima": minima,
                  "speedup": ratios, "duration_delta_percent": deltas, "verdict": verdict,
                  "target_2x": target, "median_over_min": spread, "samples": samples}
        results.append(result)
        (args.output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
        print(f"{index + 1}/28 {case}: speedup={ratios}, {verdict}, 2x={target}", flush=True)
    if hashes != {side: fingerprint(binary) for side, binary in binaries.items()}:
        raise ValueError("binary identity changed during measurement")
    lines = ["| Case | Paths | Scalar min (ms) | SoA min (ms) | Speedup R1/R2 | Delta R1/R2 (%) | 2x |",
             "|---|---|---|---|---|---|---|"]
    for result in results:
        workload, lanes, phase, paths, fallback = result["case"]
        both = {side: min(x[side] for x in result["round_minima"]) * 1000 for side in binaries}
        ratio = "/".join(f"{x:.3f}" for x in result["speedup"])
        delta = "/".join(f"{x:.1f}" for x in result["duration_delta_percent"])
        lines.append(f"| {workload}/{phase}/L{lanes}/fallback={fallback} | {paths} | {both['scalar']:.6f} | {both['soa']:.6f} | {ratio} | {delta} | {result['target_2x']} |")
    (args.output / "summary.md").write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--head", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    measure(parser.parse_args())
