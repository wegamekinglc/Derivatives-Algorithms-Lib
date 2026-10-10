"""Two informational complete LSMC Python boundary costs with untimed oracles."""

import argparse
import ctypes
import json
import math
from pathlib import Path
import time

import dal


POINT = [100.0, 0.2, 0.05, 0.0, 50.0]
ROW = [1.0, 0.01, 0.002, -0.003, 0.2]
STEP = 0.1


def product(constant=45.0):
    return dal.Product_New(["K", dal.Date_(2027, 4, 10), dal.Date_(2027, 10, 10)],
                           [str(constant), "EXERCISE 2 * K - spot()", "EXERCISE 2 * K - spot()"])


def head_operation(mode):
    simulation = dal.MonteCarloSettings_(enable_aad=True, compiled=True, smooth=2.0,
        lsmc_training_paths=128, lsmc_policy_risk_mode=mode)
    plan = dal.BlackScholesLsmcPlan_New(product(), simulation=simulation,
        valuation=dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 10, 10)))
    request = dal.BumpOverAADRequest_(directions=dal.DoubleMatrix_([ROW]), steps=[STEP])

    def evaluate():
        return dal.BlackScholesLsmc_Get_Curvature(plan, POINT, 35, request)

    def operation():
        result = evaluate()
        return [result.value, *result.gradient, *result.hessian_products.to_rows()[0]]
    return operation, evaluate()


def continuation(policy, spot):
    scalar = not policy.basis_powers
    mean = policy.mean if scalar else policy.normalization_means[0]
    sigma = policy.sigma if scalar else policy.normalization_sigmas[0]
    z = (spot - mean) / sigma
    powers = range(len(policy.coefficients)) if scalar else [row[0] for row in policy.basis_powers]
    return sum(coefficient * z ** power for coefficient, power in zip(policy.coefficients, powers))


def frozen_price(result, point, normals):
    times, policies, eps = result.plan.time_line, result.base_policy, result.simulation.smooth
    total = 0.0
    for gaussian in normals:
        spot, previous = point[0], 0.0
        spots = []
        for time_value, normal in zip(times, gaussian):
            dt = time_value - previous
            spot *= math.exp((point[2] - point[3] - 0.5 * point[1] ** 2) * dt + point[1] * math.sqrt(dt) * normal)
            spots.append(spot)
            previous = time_value
        value = 0.0
        for date in reversed(range(len(times))):
            if date + 1 < len(times):
                value *= math.exp(-point[2] * (times[date + 1] - times[date]))
            exercise = 2 * point[4] - spots[date]
            decision = min(1.0, max(0.0, (exercise - continuation(policies[date], spots[date])) / eps + 0.5))
            weight = decision * min(1.0, max(0.0, exercise / eps))
            value = weight * exercise + (1 - weight) * value
        total += value * math.exp(-point[2] * times[0])
    return total / len(normals)


def frozen_gradient(result, point, normals):
    gradient = []
    for coordinate, step in enumerate([1e-3, 1e-6, 1e-6, 1e-6, 1e-3]):
        plus, minus = point.copy(), point.copy()
        plus[coordinate] += step
        minus[coordinate] -= step
        gradient.append((frozen_price(result, plus, normals) - frozen_price(result, minus, normals)) / (2 * step))
    return gradient


def declared_gradient(point, simulation):
    result = dal.MonteCarlo_ValueWithSettings(product(point[4]), dal.BSModelData_New(*point[:4]), 35,
        valuation=dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 10, 10)), simulation=simulation)
    return result["PV"], [result["d_" + label] for label in ["spot", "vol", "rate", "div", "K"]]


def reference(mode, result):
    random = dal.SobolRSG_New(128, ndim=len(result.plan.time_line))
    normals = dal.SobolRSG_Get_Normal(random, 35).to_rows()
    value = frozen_price(result, POINT, normals)
    if mode == "Frozen":
        gradient = lambda point: frozen_gradient(result, point, normals)
    else:
        gradient = lambda point: declared_gradient(point, result.simulation)[1]
    plus = gradient([x + STEP * direction for x, direction in zip(POINT, ROW)])
    minus = gradient([x - STEP * direction for x, direction in zip(POINT, ROW)])
    return [value, *gradient(POINT), *((p - m) / (2 * STEP) for p, m in zip(plus, minus))]


def baseline_operation(mode, bridge):
    library = ctypes.CDLL(str(Path(bridge).resolve()))
    library.PrepareLsmcCost.argtypes = [ctypes.c_int]
    library.PrepareLsmcCost.restype = ctypes.c_void_p
    library.DestroyLsmcCost.argtypes = [ctypes.c_void_p]
    library.EvaluateLsmcCost.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double)]
    library.EvaluateLsmcCost.restype = ctypes.c_int
    context = library.PrepareLsmcCost(mode == "RetrainedBump")
    if not context:
        raise RuntimeError("baseline preparation failed")
    point = (ctypes.c_double * 5)(*POINT)
    output = (ctypes.c_double * 11)()

    def operation():
        if library.EvaluateLsmcCost(context, point, output):
            raise RuntimeError("baseline evaluation failed")
        return list(output)
    return operation, lambda: library.DestroyLsmcCost(context)


def validate(actual, expected):
    if len(actual) != len(expected):
        raise RuntimeError("cost result shape mismatch")
    for coordinate, (value, reference_value) in enumerate(zip(actual, expected)):
        tolerance = 1e-10 if coordinate == 0 else 2e-6 if coordinate < 6 else 2e-5
        if not math.isclose(value, reference_value, rel_tol=2e-8, abs_tol=tolerance):
            raise RuntimeError(f"oracle failed at {coordinate}: {value} versus {reference_value}")


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
    return dict(side=args.side, mode=args.mode, iterations=count, seconds=seconds,
                seconds_per_call=seconds / count, checksum=checksum, oracle_passed=True,
                calibration_trials=trials if not args.iterations else [])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--side", choices=["baseline", "head"], required=True)
    parser.add_argument("--mode", choices=["Frozen", "RetrainedBump"], required=True)
    parser.add_argument("--bridge")
    parser.add_argument("--iterations", type=int, default=0)
    parser.add_argument("--minimum-seconds", type=float, default=0.04)
    args = parser.parse_args()
    head, result = head_operation(args.mode)
    expected = reference(args.mode, result)
    operation, cleanup = (head, lambda: None) if args.side == "head" else baseline_operation(args.mode, args.bridge)
    try:
        validate(operation(), expected)
        measurement = sample(operation, args)
        validate(operation(), expected)
        print(json.dumps(measurement))
    finally:
        cleanup()


if __name__ == "__main__":
    main()
