"""One calibrated native-C++ or typed-Excel Dupire curvature observation."""

import argparse
import ctypes
import json
import math
import os
from pathlib import Path
import time


def check(values, paths):
    if len(values) != 28 or not all(math.isfinite(value) for value in values):
        raise AssertionError("invalid financial observation")
    discount = math.exp(-0.05)
    expected = [1e-6 * discount]
    expected.extend(0.002 * discount if quote == 3 else 0.0 for quote in range(6))
    for multiplier in (1.0, -2.0, 0.0):
        expected.extend(2.0 * multiplier * discount if quote == 3 else 0.0 for quote in range(6))
    expected.extend((7.0, float(paths), 720.0))
    for index, (actual, reference) in enumerate(zip(values, expected)):
        tolerance = 1e-14 if index == 0 else 1e-12 if index < 7 else 1e-10
        if abs(actual - reference) > tolerance:
            raise AssertionError(f"financial mismatch at {index}: {actual} versus {reference}")


def sample(args):
    if os.environ.get("DAL_NUM_THREADS") != "1" or len(os.sched_getaffinity(0)) != 1:
        raise AssertionError("one DAL worker and pinned CPU required")
    library = ctypes.CDLL(str(Path(args.bridge).resolve()))
    function = library.DalDupireWorksheetCurvature
    function.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_double)]
    function.restype = ctypes.c_int
    output = (ctypes.c_double * 28)()
    if function(args.paths, output):
        raise RuntimeError("warmup failed")
    check(list(output), args.paths)
    started = time.perf_counter_ns()
    for _ in range(args.repetitions):
        if function(args.paths, output):
            raise RuntimeError("curvature request failed")
    elapsed = time.perf_counter_ns() - started
    check(list(output), args.paths)
    print(json.dumps(dict(paths=args.paths, repetitions=args.repetitions, elapsed_ns=elapsed,
                          microseconds_per_request=elapsed / args.repetitions / 1000,
                          observations=list(output), affinity=sorted(os.sched_getaffinity(0)))))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bridge", required=True)
    parser.add_argument("--paths", type=int, required=True)
    parser.add_argument("--repetitions", type=int, required=True)
    sample(parser.parse_args())


if __name__ == "__main__":
    main()
