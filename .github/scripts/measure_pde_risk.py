#!/usr/bin/env python3
"""Frozen S5 P1/P2 sampling; separate from the formal benchmark allowlist."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import subprocess


CASES = ("Bermudan17", "American17", "TwoCarry25", "Boundary17")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-build", type=Path, required=True)
    parser.add_argument("--head-build", type=Path, required=True)
    parser.add_argument("--base-sha", required=True)
    parser.add_argument("--head-sha", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cases", nargs="+", choices=CASES, default=CASES)
    parser.add_argument("--group", choices=("all", "P1", "P2"), default="all")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    raw = args.output / "raw"
    raw.mkdir(exist_ok=True)
    environment = dict(os.environ, DAL_NUM_THREADS="4")
    invocations = []

    def run(label, command):
        result = subprocess.run([str(x) for x in command], env=environment, text=True, capture_output=True)
        (raw / f"{label}.stdout").write_text(result.stdout)
        (raw / f"{label}.stderr").write_text(result.stderr)
        invocations.append({"label": label, "command": [str(x) for x in command], "returncode": result.returncode})
        (args.output / "commands.json").write_text(json.dumps(invocations, indent=2) + "\n")
        if result.returncode:
            raise RuntimeError(f"{label} exited {result.returncode}; see retained raw output")
        return result.stdout

    binaries = {
        "base_price": args.base_build / "dal-cpp/benchmarks/pde_perf/pde_perf",
        "head_price": args.head_build / "dal-cpp/benchmarks/pde_perf/pde_perf",
        "head_risk": args.head_build / "dal-cpp/benchmarks/pde_perf/pde_perf",
    }
    identities = {key: {"path": str(path.resolve()), "sha256": digest(path)} for key, path in binaries.items()}
    report = {"base_sha": args.base_sha, "head_sha": args.head_sha, "threads": 4,
              "sampling": "2 rounds, 10 interleaved process samples/side, alternating first side, per-round minimum",
              "binaries_before": identities, "P1": {}, "P2": {}, "calibration": {}, "preflight": {}}

    def save():
        (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")

    save()
    if args.group in ("all", "P1"):
        pattern = re.compile(r"^(Bermudan|American|Daily|NearZero)\s+([\d.]+) (ms|us|ns)", re.MULTILINE)
        samples = []
        counters = {}
        for round_id in range(2):
            sides = {"base": [], "head": []}
            for sample in range(10):
                for side in (("base", "head") if (round_id + sample) % 2 == 0 else ("head", "base")):
                    output = run(f"P1-r{round_id}-{sample}-{side}", [binaries[f"{side}_price"], "--early-exercise"])
                    values = {name: float(value) * {"ms": 1e6, "us": 1e3, "ns": 1}[unit]
                              for name, value, unit in pattern.findall(output)}
                    if set(values) != {"Bermudan", "American", "Daily", "NearZero"}:
                        raise RuntimeError("incomplete P1 sample")
                    rows = [line for line in output.splitlines() if " price=" in line]
                    if len(rows) != 4:
                        raise RuntimeError("missing P1 counters")
                    if counters and rows != counters["rows"]:
                        raise RuntimeError("P1 price/work/capacity identity changed")
                    counters = {"rows": rows}
                    sides[side].append(values)
            samples.append(sides)
            report["P1"]["samples"] = samples
            report["P1"]["counters"] = counters
            save()
        for name in ("Bermudan", "American", "Daily", "NearZero"):
            minima = [{"base_ns": min(s["base"][i][name] for i in range(10)),
                       "head_ns": min(s["head"][i][name] for i in range(10))} for s in samples]
            deltas = [100 * (m["head_ns"] / m["base_ns"] - 1) for m in minima]
            report["P1"][name] = {"minima": minima, "deltas_percent": deltas,
                                    "verdict": "FAIL" if all(d > 4 for d in deltas) else "PASS"}
        save()

    if args.group in ("all", "P2"):
        for case in args.cases:
            report["preflight"][case] = json.loads(run(f"P2-{case}-identity", [binaries["head_risk"], "--risk", case, "verify", 1]))
        save()
        for case in args.cases:
            calibration = {side: json.loads(run(f"P2-{case}-calibrate-{side}", [binaries["head_risk"], "--risk", case, side, 1]))
                           for side in ("reference", "risk")}
            repeats = max(1, math.ceil(200_000_000 / min(row["elapsed_ns"] for row in calibration.values())))
            if repeats > 10000:
                raise RuntimeError("calibrated repetitions exceed driver cap")
            report["calibration"][case] = {"repeats": repeats, "diagnostic": calibration}
            samples = []
            for round_id in range(2):
                sides = {"reference": [], "risk": []}
                for sample in range(10):
                    for side in (("reference", "risk") if (round_id + sample) % 2 == 0 else ("risk", "reference")):
                        row = json.loads(run(f"P2-{case}-r{round_id}-{sample}-{side}", [binaries["head_risk"], "--risk", case, side, repeats]))
                        if row["repeats"] != repeats or row["ns_per_call"] <= 0:
                            raise RuntimeError("invalid P2 timing sample")
                        sides[side].append(row["ns_per_call"])
                samples.append(sides)
                report["P2"][case] = {"repeats": repeats, "samples": samples}
                save()
            minima = [{"reference_ns": min(s["reference"]), "risk_ns": min(s["risk"])} for s in samples]
            deltas = [100 * (m["risk_ns"] / m["reference_ns"] - 1) for m in minima]
            report["P2"][case].update(minima=minima, overhead_percent=deltas,
                                       verdict="PASS" if all(d <= 15 for d in deltas) else "FAIL")
            save()
    report["binaries_after"] = {key: digest(path) for key, path in binaries.items()}
    if any(row["sha256"] != report["binaries_after"][key] for key, row in identities.items()):
        report["identity_drift"] = True
    failures = [name for group in ("P1", "P2") for name, row in report[group].items()
                if isinstance(row, dict) and row.get("verdict") == "FAIL"]
    report["verdict"] = "FAIL" if failures or report.get("identity_drift") else "PASS"
    save()
    print(json.dumps({"verdict": report["verdict"], "failures": failures, "output": str(args.output)}))
    return int(report["verdict"] != "PASS")


if __name__ == "__main__":
    raise SystemExit(main())
