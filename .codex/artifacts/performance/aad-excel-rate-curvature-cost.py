"""One calibrated native-C++ or typed-Excel rate curvature observation."""

import argparse
import ctypes
from datetime import date
import json
import math
import os
from pathlib import Path
import time


def direction(row, column, count):
    if row == 0:
        return 1.0 if column == 0 else 0.0
    if row == 1:
        return -2.0 if column == count - 1 else 0.0
    return (1.0 if column % 2 == 0 else -1.0) / (column + 1)


def quote_gradient(quote, maturity, weight):
    return -weight * (1.0 + 0.028 * maturity) * maturity / (1.0 + quote * maturity) ** 2


def expected_values(count):
    maturities = [(date(2025 + year, 1, 2) - date(2025, 1, 2)).days / 365.0 for year in range(1, count + 1)]
    quotes = [0.0251] + [0.025] * (count - 1)
    weights = [1.0 if column % 2 == 0 else -0.5 for column in range(count)]
    value = sum(weight * ((1.0 + 0.028 * maturity) / (1.0 + quote * maturity) - 1.0)
                for weight, maturity, quote in zip(weights, maturities, quotes))
    values = [value] + [quote_gradient(quote, maturity, weight) for quote, maturity, weight in zip(quotes, maturities, weights)]
    for row, step in enumerate((0.0001, 0.0002, 0.0001)):
        for column, (quote, maturity, weight) in enumerate(zip(quotes, maturities, weights)):
            shift = step * direction(row, column, count)
            values.append((quote_gradient(quote + shift, maturity, weight) - quote_gradient(quote - shift, maturity, weight)) / (2.0 * step))
    return values + quotes + [7.0, 7.0, 7.0, float(8 * (4 + 8 * count))]


def check(values, count):
    if len(values) != 5 + 5 * count or not all(math.isfinite(value) for value in values):
        raise AssertionError("invalid financial observation")
    for index, (actual, reference) in enumerate(zip(values, expected_values(count))):
        tolerance = 1e-10 if index == 0 else 1e-8
        if abs(actual - reference) > tolerance:
            raise AssertionError(f"financial mismatch at {index}: {actual} versus {reference}")


def sample(args):
    if os.environ.get("DAL_NUM_THREADS") != "1" or len(os.sched_getaffinity(0)) != 1:
        raise AssertionError("one DAL worker and pinned CPU required")
    library = ctypes.CDLL(str(Path(args.bridge).resolve()))
    function = library.DalRateWorksheetCurvature
    function.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_double)]
    function.restype = ctypes.c_int
    output = (ctypes.c_double * (5 + 5 * args.quotes))()
    if function(args.quotes, output):
        raise RuntimeError("warmup failed")
    check(list(output), args.quotes)
    started = time.perf_counter_ns()
    for _ in range(args.repetitions):
        if function(args.quotes, output):
            raise RuntimeError("curvature request failed")
    elapsed = time.perf_counter_ns() - started
    check(list(output), args.quotes)
    print(json.dumps(dict(quotes=args.quotes, repetitions=args.repetitions, elapsed_ns=elapsed,
                          microseconds_per_request=elapsed / args.repetitions / 1000,
                          observations=list(output), affinity=sorted(os.sched_getaffinity(0)))))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bridge", required=True)
    parser.add_argument("--quotes", type=int, required=True)
    parser.add_argument("--repetitions", type=int, required=True)
    sample(parser.parse_args())


if __name__ == "__main__":
    main()
