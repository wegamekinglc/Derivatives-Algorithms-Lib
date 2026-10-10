"""One fresh-process, calibrated public-C++ or typed-Excel PDE observation."""

import argparse
import ctypes
import json
import math
import os
from pathlib import Path
import time


REFERENCES = {
    (9, 8): [4.153690693968586, 10.770781220697858,
             40.40684028445804, 27.8766807705311, -0.0907395605282994,
             -64.1496803210784, 27.87666960963085, 0.8605082862555484],
    (61, 120): [5.1900030895691085, 11.805380104352425,
                35.05018913872579, 38.01512316376091, -0.315791003495686,
                -69.58469778820111, 38.01512316376076, 0.6354385028889284],
}


def check_finite(values):
    if len(values) != 10 or not all(math.isfinite(value) for value in values):
        raise AssertionError("invalid financial observation")


def check_risks(values, reference):
    for index, expected in enumerate(reference):
        tolerance = 1e-9 if index < 2 else 1e-8
        if abs(values[index] - expected) > tolerance:
            raise AssertionError(f"financial mismatch at {index}")


def check(values, nodes, intervals):
    check_finite(values)
    check_risks(values, REFERENCES[(nodes, intervals)])
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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sample", action="store_true")
    parser.add_argument("--bridge", required=True)
    parser.add_argument("--nodes", type=int, required=True)
    parser.add_argument("--intervals", type=int, required=True)
    parser.add_argument("--repetitions", type=int, required=True)
    args = parser.parse_args()
    sample(args)


if __name__ == "__main__":
    main()
