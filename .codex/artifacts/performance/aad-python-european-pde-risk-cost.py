"""One fresh-process calibrated European PDE observation."""

import argparse
import ctypes
import json
import math
import os
from pathlib import Path
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


def native_operation(args):
    library = ctypes.CDLL(str(Path(args.bridge).resolve()))
    function = library.DalEuropeanOwningRisk if args.implementation == "owning" else library.DalEuropeanNativeRisk
    function.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_double)]
    function.restype = ctypes.c_int
    output = (ctypes.c_double * 10)()
    def operation():
        if function(args.nodes, args.intervals, output):
            raise RuntimeError("native PDE bridge failed")
        return output
    return operation, lambda value: list(value)


def python_operation(args):
    sys.path.insert(0, args.module_root)
    import dal
    settings = dal.EuropeanPdeSettings_(grid_points=args.nodes, ordinary_steps=args.intervals)
    def operation():
        return dal.EuropeanPdeRiskResult_New(0.05, 0.20, 110.0, settings=settings)
    def observe(result):
        return [*result.prices, *(value for row in result.jacobian.to_rows() for value in row),
                max(value for row in result.forward_backward_errors.to_rows() for value in row),
                max(value for row in result.transpose_backward_errors.to_rows() for value in row)]
    return operation, observe


def check(values, args):
    reference = REFERENCES[(args.nodes, args.intervals)]
    if not all(math.isfinite(value) for value in values):
        raise AssertionError("nonfinite financial observation")
    for index, (actual, expected) in enumerate(zip(values, reference)):
        if abs(actual - expected) > (1e-9 if index < 2 else 1e-8):
            raise AssertionError(f"financial mismatch at {index}: {actual}, {expected}")


def check_errors(values):
    if any(error < 0 or error > 1e-12 for error in values[8:]):
        raise AssertionError("physical solve error policy failed")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--implementation", choices=["native", "owning", "python"], required=True)
    parser.add_argument("--bridge")
    parser.add_argument("--module-root")
    parser.add_argument("--nodes", type=int, required=True)
    parser.add_argument("--intervals", type=int, required=True)
    parser.add_argument("--repetitions", type=int, required=True)
    args = parser.parse_args()
    if os.environ.get("DAL_NUM_THREADS") != "1" or len(os.sched_getaffinity(0)) != 1:
        raise AssertionError("one DAL thread and one pinned CPU required")
    operation, observe = python_operation(args) if args.implementation == "python" else native_operation(args)
    initial = observe(operation())
    check(initial, args)
    check_errors(initial)
    started = time.perf_counter_ns()
    for _ in range(args.repetitions):
        result = operation()
    elapsed = time.perf_counter_ns() - started
    values = observe(result)
    check(values, args)
    check_errors(values)
    print(json.dumps(dict(implementation=args.implementation, nodes=args.nodes, intervals=args.intervals,
                         repetitions=args.repetitions, elapsed_ns=elapsed,
                         microseconds_per_request=elapsed / args.repetitions / 1000,
                         observations=values, affinity=sorted(os.sched_getaffinity(0)))))


if __name__ == "__main__":
    main()
