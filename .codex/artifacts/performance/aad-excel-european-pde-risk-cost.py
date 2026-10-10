"""Calibrated, paired public-C++ versus typed-Excel PDE boundary costs."""

import argparse
import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import time


REFERENCES = {
    (9, 8): [4.153690693968586, 10.770781220697858,
             40.40684028445804, 27.8766807705311, -0.0907395605282994,
             -64.1496803210784, 27.87666960963085, 0.8605082862555484],
    (61, 120): [5.1900030895691085, 11.805380104352425,
                35.05018913872579, 38.01512316376091, -0.315791003495686,
                -69.58469778820111, 38.01512316376076, 0.6354385028889284],
}


def check(values, nodes, intervals):
    if len(values) != 10 or not all(math.isfinite(value) for value in values):
        raise AssertionError("invalid financial observation")
    for index, expected in enumerate(REFERENCES[(nodes, intervals)]):
        if abs(values[index] - expected) > (1e-9 if index < 2 else 1e-8):
            raise AssertionError(f"financial mismatch at {index}")
    if any(error < 0.0 or error > 1e-12 for error in values[8:]):
        raise AssertionError("solve error policy failed")


def sample(args):
    if os.environ.get("DAL_NUM_THREADS") != "1" or len(os.sched_getaffinity(0)) != 1:
        raise AssertionError("one DAL thread and pinned CPU required")
    library = ctypes.CDLL(str(Path(args.bridge).resolve()))
    function = library.DalEuropeanWorksheetRisk
    function.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_double)]
    function.restype = ctypes.c_int
    output = (ctypes.c_double * 10)()
    if function(args.nodes, args.intervals, output):
        raise RuntimeError("warmup failed")
    check(list(output), args.nodes, args.intervals)
    started = time.perf_counter_ns()
    for _ in range(args.repetitions):
        if function(args.nodes, args.intervals, output):
            raise RuntimeError("financial request failed")
    elapsed = time.perf_counter_ns() - started
    check(list(output), args.nodes, args.intervals)
    print(json.dumps(dict(nodes=args.nodes, intervals=args.intervals,
                          repetitions=args.repetitions, elapsed_ns=elapsed,
                          microseconds_per_request=elapsed / args.repetitions / 1000,
                          observations=list(output), affinity=sorted(os.sched_getaffinity(0)))))


def collect(args):
    evidence = Path(args.evidence).resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    cpu = min(os.sched_getaffinity(0))
    environment = dict(os.environ, DAL_NUM_THREADS="1")
    bridges = {side: str(Path(getattr(args, side)).resolve()) for side in ("base", "head")}

    def observe(side, nodes, intervals, repetitions, label):
        command = ["taskset", "-c", str(cpu), sys.executable, str(Path(__file__).resolve()),
                   "--sample", "--bridge", bridges[side], "--nodes", str(nodes),
                   "--intervals", str(intervals), "--repetitions", str(repetitions)]
        result = subprocess.run(command, env=environment, capture_output=True, text=True)
        capture = dict(command=command, exit_code=result.returncode,
                       stdout=result.stdout, stderr=result.stderr)
        (evidence / (label + ".json")).write_text(json.dumps(capture, indent=2) + "\n")
        if result.returncode:
            raise RuntimeError(capture)
        lines = result.stdout.strip().splitlines()
        observation = json.loads(lines[-1])
        observation.update(side=side, capture=label + ".json")
        return observation

    raw, cases = [], []
    for nodes, intervals in REFERENCES:
        repetitions = {}
        for side in bridges:
            trial = observe(side, nodes, intervals, 10, f"calibration-{nodes}-{intervals}-{side}")
            repetitions[side] = max(1, math.ceil(100000000 * 10 / trial["elapsed_ns"]))
        rounds = []
        for round_index in range(2):
            paired = []
            for pair in range(10):
                order = ("base", "head") if (round_index + pair) % 2 == 0 else ("head", "base")
                for side in order:
                    label = f"sample-{nodes}-{intervals}-{round_index}-{pair}-{side}"
                    row = observe(side, nodes, intervals, repetitions[side], label)
                    if row["elapsed_ns"] < 25000000:
                        raise AssertionError(f"uncalibrated observation: {row}")
                    row.update(round=round_index + 1, pair=pair + 1)
                    paired.append(row)
                    raw.append(row)
            minima = {side: min(row["microseconds_per_request"] for row in paired if row["side"] == side)
                      for side in bridges}
            rounds.append(dict(round=round_index + 1, minima_us=minima,
                               delta_percent=100 * (minima["head"] / minima["base"] - 1)))
        cases.append(dict(nodes=nodes, intervals=intervals, rounds=rounds,
                          verdict="informational: unequal ownership and labeled spill work"))
    report = dict(observation_count=len(raw), measured_seconds=sum(row["elapsed_ns"] for row in raw) / 1e9,
                  minimum_observation_ns=min(row["elapsed_ns"] for row in raw),
                  cpu=cpu, dal_threads=1, rounds=2, pairs_per_round=10,
                  binaries={side: dict(path=path, sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest())
                            for side, path in bridges.items()}, cases=cases,
                  limitations="Portable typed calls; excludes XLL export, Excel host, COM and UI costs.")
    assert len(raw) == 80
    (evidence / "raw-samples.json").write_text(json.dumps(raw, indent=2) + "\n")
    (evidence / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sample", action="store_true")
    parser.add_argument("--bridge")
    parser.add_argument("--nodes", type=int)
    parser.add_argument("--intervals", type=int)
    parser.add_argument("--repetitions", type=int)
    parser.add_argument("--base")
    parser.add_argument("--head")
    parser.add_argument("--evidence")
    args = parser.parse_args()
    sample(args) if args.sample else collect(args)


if __name__ == "__main__":
    main()
