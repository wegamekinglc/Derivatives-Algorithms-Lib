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

from check_benchmark_regressions import benchmark_binary


CASES = ("Bermudan17", "American17", "TwoCarry25", "Boundary17")
PRICE_CASES = ("Bermudan", "American", "Daily", "NearZero")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def timing_row(output):
    return json.loads(output.splitlines()[-1])


def valid_repeats(repeats):
    return isinstance(repeats, int) and not isinstance(repeats, bool) and 1 <= repeats <= 10000


def validated_arguments(arguments):
    if arguments == ("--early-exercise",):
        return list(arguments)
    if len(arguments) != 4 or arguments[0] != "--risk":
        raise ValueError("unsupported PDE benchmark arguments")
    _, case, mode, repeats = arguments
    if case not in CASES or mode not in ("verify", "reference", "risk"):
        raise ValueError("unsupported PDE risk case/mode")
    if not valid_repeats(repeats):
        raise ValueError("repeat count outside [1,10000]")
    return ["--risk", case, mode, str(repeats)]


class Recorder:
    def __init__(self, args):
        self.output = args.output
        self.output.mkdir(parents=True, exist_ok=True)
        self.raw = self.output / "raw"
        self.raw.mkdir(exist_ok=True)
        self.environment = dict(os.environ, DAL_NUM_THREADS="4")
        self.invocations = []
        self.binaries = {
            "base_price": benchmark_binary(args.base_build, "pde_perf"),
            "head_price": benchmark_binary(args.head_build, "pde_perf"),
            "head_risk": benchmark_binary(args.head_build, "pde_perf"),
        }

    def run(self, label, key, arguments):
        if re.fullmatch(r"[A-Za-z0-9-]+", label) is None:
            raise ValueError("invalid raw output label")
        command = [str(self.binaries[key]), *validated_arguments(arguments)]
        # Only contained, executable pde_perf paths and fixed validated arguments; no shell parses them.
        result = subprocess.run(  # nosemgrep
            command,  # nosemgrep
            env=self.environment, text=True, capture_output=True, shell=False, check=False,
        )
        (self.raw / f"{label}.stdout").write_text(result.stdout)
        (self.raw / f"{label}.stderr").write_text(result.stderr)
        self.invocations.append({"label": label, "command": command, "returncode": result.returncode})
        (self.output / "commands.json").write_text(json.dumps(self.invocations, indent=2) + "\n")
        if result.returncode:
            raise RuntimeError(f"{label} exited {result.returncode}; see retained raw output")
        return result.stdout

    def save(self, report):
        (self.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-build", type=Path, required=True)
    parser.add_argument("--head-build", type=Path, required=True)
    parser.add_argument("--base-sha", required=True)
    parser.add_argument("--head-sha", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cases", nargs="+", choices=CASES, default=CASES)
    parser.add_argument("--group", choices=("all", "P1", "P2"), default="all")
    return parser.parse_args()


def initial_report(args, recorder):
    identities = {key: {"path": str(path), "sha256": digest(path)} for key, path in recorder.binaries.items()}
    return {
        "base_sha": args.base_sha, "head_sha": args.head_sha, "threads": 4,
        "sampling": "2 rounds, 10 interleaved process samples/side, alternating first side, per-round minimum",
        "binaries_before": identities, "P1": {}, "P2": {}, "calibration": {}, "preflight": {},
    }


def paired_round(round_id, sides, read):
    samples = {side: [] for side in sides}
    for sample in range(10):
        order = sides if (round_id + sample) % 2 == 0 else tuple(reversed(sides))
        for side in order:
            samples[side].append(read(side, sample))
    return samples


def p1_sample(recorder, report, round_id, side, sample):
    output = recorder.run(f"P1-r{round_id}-{sample}-{side}", f"{side}_price", ("--early-exercise",))
    pattern = re.compile(r"^(Bermudan|American|Daily|NearZero)\s+([\d.]+) (ms|us|ns)", re.MULTILINE)
    values = {name: float(value) * {"ms": 1e6, "us": 1e3, "ns": 1}[unit]
              for name, value, unit in pattern.findall(output)}
    if set(values) != set(PRICE_CASES):
        raise RuntimeError("incomplete P1 sample")
    rows = [line for line in output.splitlines() if " price=" in line]
    if len(rows) != 4:
        raise RuntimeError("missing P1 counters")
    counters = report["P1"].get("counters")
    if counters and rows != counters["rows"]:
        raise RuntimeError("P1 price/work/capacity identity changed")
    report["P1"]["counters"] = {"rows": rows}
    return values


def p1_result(samples, name):
    minima = [{"base_ns": min(s["base"][i][name] for i in range(10)),
               "head_ns": min(s["head"][i][name] for i in range(10))} for s in samples]
    deltas = [100 * (m["head_ns"] / m["base_ns"] - 1) for m in minima]
    return {"minima": minima, "deltas_percent": deltas,
            "verdict": "FAIL" if all(d > 4 for d in deltas) else "PASS"}


def measure_p1(recorder, report):
    samples = []
    for round_id in range(2):
        samples.append(paired_round(round_id, ("base", "head"),
            lambda side, sample: p1_sample(recorder, report, round_id, side, sample)))
        report["P1"]["samples"] = samples
        recorder.save(report)
    for name in PRICE_CASES:
        report["P1"][name] = p1_result(samples, name)
    recorder.save(report)


def risk_row(recorder, label, case, mode, repeats):
    return timing_row(recorder.run(label, "head_risk", ("--risk", case, mode, repeats)))


def calibrate(recorder, report, case):
    calibration = {side: risk_row(recorder, f"P2-{case}-calibrate-{side}", case, side, 1)
                   for side in ("reference", "risk")}
    repeats = max(1, math.ceil(200_000_000 / min(row["elapsed_ns"] for row in calibration.values())))
    if repeats > 10000:
        raise RuntimeError("calibrated repetitions exceed driver cap")
    report["calibration"][case] = {"repeats": repeats, "diagnostic": calibration}
    return repeats


def p2_sample(recorder, case, repeats, round_id, side, sample):
    row = risk_row(recorder, f"P2-{case}-r{round_id}-{sample}-{side}", case, side, repeats)
    if row["repeats"] != repeats or row["ns_per_call"] <= 0:
        raise RuntimeError("invalid P2 timing sample")
    return row["ns_per_call"]


def p2_result(samples):
    minima = [{"reference_ns": min(s["reference"]), "risk_ns": min(s["risk"])} for s in samples]
    deltas = [100 * (m["risk_ns"] / m["reference_ns"] - 1) for m in minima]
    return {"minima": minima, "overhead_percent": deltas,
            "verdict": "PASS" if all(d <= 15 for d in deltas) else "FAIL"}


def measure_case(recorder, report, case):
    repeats = calibrate(recorder, report, case)
    samples = []
    for round_id in range(2):
        samples.append(paired_round(round_id, ("reference", "risk"),
            lambda side, sample: p2_sample(recorder, case, repeats, round_id, side, sample)))
        report["P2"][case] = {"repeats": repeats, "samples": samples}
        recorder.save(report)
    report["P2"][case].update(p2_result(samples))
    recorder.save(report)


def measure_p2(recorder, report, cases):
    for case in cases:
        report["preflight"][case] = risk_row(recorder, f"P2-{case}-identity", case, "verify", 1)
    recorder.save(report)
    for case in cases:
        measure_case(recorder, report, case)


def failed_cases(report):
    return [name for group in ("P1", "P2") for name, row in report[group].items()
            if isinstance(row, dict) and row.get("verdict") == "FAIL"]


def finish(recorder, report):
    report["binaries_after"] = {key: digest(path) for key, path in recorder.binaries.items()}
    if any(row["sha256"] != report["binaries_after"][key] for key, row in report["binaries_before"].items()):
        report["identity_drift"] = True
    failures = failed_cases(report)
    report["verdict"] = "FAIL" if failures or report.get("identity_drift") else "PASS"
    recorder.save(report)
    print(json.dumps({"verdict": report["verdict"], "failures": failures, "output": str(recorder.output)}))
    return int(report["verdict"] != "PASS")


def main():
    args = arguments()
    recorder = Recorder(args)
    report = initial_report(args, recorder)
    recorder.save(report)
    if args.group in ("all", "P1"):
        measure_p1(recorder, report)
    if args.group in ("all", "P2"):
        measure_p2(recorder, report, args.cases)
    return finish(recorder, report)


if __name__ == "__main__":
    raise SystemExit(main())
