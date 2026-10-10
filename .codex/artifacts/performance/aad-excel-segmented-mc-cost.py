"""One native-public or owning-Excel segmented MC cost observation."""

import argparse
import ctypes
import json
import math
import os
from pathlib import Path
import time


POINT = [100.0, .2, .03, .01, 2.0]
DIRECTIONS = [[1, .3, -.1, .2, .5], [-2, -.6, .2, -.4, -1]]
STEPS = [2e-4, 1e-4]


def analytic(point, normals):
    spot, vol, rate, div, scale = point
    value = 0.0
    gradient = [0.0] * 5
    expiry = len(normals[0])
    for increments in normals:
        brownian = 0.0
        for time_index, normal in enumerate(increments, start=1):
            brownian += normal
            unit = spot ** 2 * math.exp(2 * (rate - div - .5 * vol ** 2) * time_index
                                       + 2 * vol * brownian - rate * expiry)
            payoff = scale * unit / len(normals)
            value += payoff
            terms = [2 * payoff / spot, 2 * (brownian - vol * time_index) * payoff,
                     (2 * time_index - expiry) * payoff, -2 * time_index * payoff, payoff / scale]
            gradient = [current + term for current, term in zip(gradient, terms)]
    return value, gradient


def reference(selected, normals):
    value, gradient = analytic(POINT, normals)
    products = []
    if selected:
        for direction, step in zip(DIRECTIONS, STEPS):
            plus = analytic([x + step * d for x, d in zip(POINT, direction)], normals)[1]
            minus = analytic([x - step * d for x, d in zip(POINT, direction)], normals)[1]
            products += [(p - m) / (2 * step) for p, m in zip(plus, minus)]
    else:
        products = [0.0] * 10
    metadata = [33, 32, 2, 1, 5, 264] if selected else [1, 32, 1, 1, 1, 0]
    return [value, *gradient, *products, *metadata]


def validate(actual, expected):
    if len(actual) != len(expected) or not all(math.isfinite(value) for value in actual):
        raise AssertionError("invalid MC observation")
    for index, (value, target) in enumerate(zip(actual, expected)):
        tolerance = 1e-5 if 6 <= index < 16 else 1e-8
        if not math.isclose(value, target, rel_tol=2e-8, abs_tol=tolerance):
            raise AssertionError(f"independent finite-path oracle failed at {index}: {value} versus {target}")


def sample(args):
    if os.environ.get("DAL_NUM_THREADS") != "1" or len(os.sched_getaffinity(0)) != 1:
        raise AssertionError("one DAL worker and pinned CPU required")
    normals = json.loads(Path(args.normals_file).read_text())[str(args.case)]
    expected = reference(args.case, normals)
    library = ctypes.CDLL(str(Path(args.bridge).resolve()))
    library.PrepareMonteCarloCost.argtypes = [ctypes.c_int]
    library.PrepareMonteCarloCost.restype = ctypes.c_void_p
    library.DestroyMonteCarloCost.argtypes = [ctypes.c_void_p]
    operation = library.EvaluateMonteCarloCost
    operation.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double)]
    operation.restype = ctypes.c_int
    context = library.PrepareMonteCarloCost(args.case)
    if not context:
        raise RuntimeError("MC preparation failed")
    point = (ctypes.c_double * 5)(*POINT)
    output = (ctypes.c_double * 22)()
    try:
        if operation(context, point, output):
            raise RuntimeError("warmup failed")
        validate(list(output), expected)
        started = time.perf_counter_ns()
        for _ in range(args.repetitions):
            if operation(context, point, output):
                raise RuntimeError("MC request failed")
        elapsed = time.perf_counter_ns() - started
        validate(list(output), expected)
        print(json.dumps(dict(case=args.case, repetitions=args.repetitions, elapsed_ns=elapsed,
                              microseconds_per_request=elapsed / args.repetitions / 1000,
                              observations=list(output), oracle_passed=True, affinity=sorted(os.sched_getaffinity(0)))))
    finally:
        library.DestroyMonteCarloCost(context)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bridge", required=True)
    parser.add_argument("--case", choices=[0, 1], type=int, required=True)
    parser.add_argument("--normals-file", required=True)
    parser.add_argument("--repetitions", type=int, required=True)
    sample(parser.parse_args())


if __name__ == "__main__":
    main()
