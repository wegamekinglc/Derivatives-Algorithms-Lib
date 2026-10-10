"""Informational complete Python boundary costs versus the accepted native core."""

import argparse
import ctypes
import json
import math
from pathlib import Path
import time

import dal


POINT = [100.0, 0.2, 0.03, 0.01, 2.0]
ROWS = [[1.0, 0.3, -0.1, 0.2, 0.5], [-2.0, -0.6, 0.2, -0.4, -1.0], [0.0, 1.0, 0.0, 0.0, 0.0]]
STEPS = [2e-4, 1e-4, 2e-4]


def analytic(point, normals):
    spot, vol, rate, div, scale = point
    value = 0.0
    gradient = [0.0] * 5
    for normal in normals:
        unit = spot ** 2 * math.exp(rate - 2 * div - vol ** 2 + 2 * vol * normal)
        payoff = scale * unit
        value += payoff / len(normals)
        for i, component in enumerate([2 * payoff / spot, 2 * payoff * (normal - vol), payoff, -2 * payoff, unit]):
            gradient[i] += component / len(normals)
    return value, gradient


def reference(directions):
    random = dal.SobolRSG_New(7, ndim=1)
    normals = [row[0] for row in dal.SobolRSG_Get_Normal(random, 35).to_rows()]
    value, gradient = analytic(POINT, normals)
    products = []
    for row, step in zip(ROWS[:directions], STEPS[:directions]):
        plus = analytic([x + step * d for x, d in zip(POINT, row)], normals)[1]
        minus = analytic([x - step * d for x, d in zip(POINT, row)], normals)[1]
        products += [(p - m) / (2 * step) for p, m in zip(plus, minus)]
    return [value, *gradient, *products]


def head_operation(directions):
    product = dal.Product_New(["SCALE", dal.Date_(2027, 1, 2)],
                              ["2", "pay PAYS SCALE * FIX(EQ[SEGMENTED_PYTHON]) ^ 2"])
    plan = dal.BlackScholesMonteCarloPlan_New(product,
        valuation=dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 1, 2)))
    settings = dal.SegmentedMonteCarloSettings_(first_path=7)
    if directions == 0:
        def operation():
            result = dal.BlackScholesMonteCarlo_Get_Risk(plan, POINT, 35, settings=settings)
            return [result.value, *result.gradient]
    else:
        request = dal.BumpOverAADRequest_(directions=dal.DoubleMatrix_(ROWS), steps=STEPS)

        def operation():
            result = dal.BlackScholesMonteCarlo_Get_Curvature(plan, POINT, 35, request, settings=settings)
            mean = result.base
            return [mean.value, *mean.gradient, *(x for row in result.hessian_products.to_rows() for x in row)]
    return operation, lambda: None


def baseline_operation(directions, bridge):
    library = ctypes.CDLL(str(Path(bridge).resolve()))
    library.PrepareSegmentedCost.restype = ctypes.c_void_p
    library.DestroySegmentedCost.argtypes = [ctypes.c_void_p]
    library.EvaluateSegmentedCost.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double),
                                             ctypes.c_int, ctypes.POINTER(ctypes.c_double)]
    library.EvaluateSegmentedCost.restype = ctypes.c_int
    context = library.PrepareSegmentedCost()
    if not context:
        raise RuntimeError("baseline preparation failed")
    point = (ctypes.c_double * 5)(*POINT)
    output = (ctypes.c_double * (6 + 5 * directions))()

    def operation():
        if library.EvaluateSegmentedCost(context, point, directions, output) != 0:
            raise RuntimeError("baseline evaluation failed")
        return list(output)
    return operation, lambda: library.DestroySegmentedCost(context)


def validate(actual, expected):
    if len(actual) != len(expected):
        raise RuntimeError("cost result shape mismatch")
    for i, (value, reference_value) in enumerate(zip(actual, expected)):
        tolerance = 1e-5 if i >= 6 else 1e-8
        if not math.isclose(value, reference_value, rel_tol=2e-8, abs_tol=tolerance):
            raise RuntimeError(f"independent analytic oracle failed at {i}: {value} versus {reference_value}")


def sample(operation, args):
    trials = []
    count = args.iterations or 1
    while True:
        checksum = 0.0
        start = time.perf_counter_ns()
        for _ in range(count):
            checksum += operation()[0]
        seconds = (time.perf_counter_ns() - start) * 1e-9
        trials.append(dict(iterations=count, seconds=seconds))
        if args.iterations or seconds >= args.minimum_seconds:
            break
        count *= 2
    return dict(side=args.side, directions=args.directions, iterations=count,
                seconds=seconds, seconds_per_call=seconds / count, checksum=checksum,
                oracle_passed=True, calibration_trials=trials if not args.iterations else [])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--side", choices=["baseline", "head"], required=True)
    parser.add_argument("--directions", choices=[0, 3], type=int, required=True)
    parser.add_argument("--bridge")
    parser.add_argument("--iterations", type=int, default=0)
    parser.add_argument("--minimum-seconds", type=float, default=0.04)
    args = parser.parse_args()
    operation, cleanup = (head_operation(args.directions) if args.side == "head"
                          else baseline_operation(args.directions, args.bridge))
    expected = reference(args.directions)
    try:
        validate(operation(), expected)
        result = sample(operation, args)
        validate(operation(), expected)
        print(json.dumps(result))
    finally:
        cleanup()


if __name__ == "__main__":
    main()
