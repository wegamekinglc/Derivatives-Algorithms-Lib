"""Informational complete Python boundary costs with equivalent raw products."""

import argparse
import json
import math
import time

import dal


def calibration(spreads):
    configuration = dal.DupireRiskInputs_(
        quote_strikes=[75.0, 105.0, 135.0], quote_maturities=[0.4, 1.2],
        quote_spreads=spreads, inclusion_spots=[60.0, 100.0, 140.0],
        max_spot_spacing=10.0, inclusion_times=[0.5, 1.0], max_time_spacing=0.5,
    )
    return dal.DupireCalibration_New(dal.BSModelData_New(100, 0.2, 0.05, 0.02), configuration)


def model(source):
    return dal.DupireModelData_New(source, "EQ[LOCAL]", "USD", "F_LOCAL", max_step=0.25)


def product(quote):
    return dal.Product_New(["QUOTE", dal.Date_(2027, 9, 12)], [repr(quote), "pay PAYS QUOTE * QUOTE"])


def risk_request():
    return dal.DupireScriptRiskRequest_(
        num_paths=17, direct_bindings=[dal.DupireQuoteBinding_(constant_ordinal=0, quote_id="quote:3")],
        valuation=dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 9, 12)),
        simulation=dal.MonteCarloSettings_(enable_aad=True, compiled=True),
    )


def configuration(count):
    matrix = dal.DoubleMatrix_(count, 6, 0.0)
    if count:
        matrix[0, 3], matrix[1, 3], matrix[2, 0] = 1.0, -2.0, 1.0
    steps = [2e-4, 1e-4, 2e-4][:count]
    source = calibration(dal.DoubleMatrix_(3, 2, 0.001))
    risk = risk_request()
    request = dal.DupireScriptCurvatureRequest_(
        risk=risk, bumps=dal.BumpOverAADRequest_(directions=matrix, steps=steps),
    )
    return source, risk, request, matrix.to_rows(), steps


def gradient(source, quote, risk):
    plan = dal.DupireScriptRiskPlan_New(product(quote), model(source), source, "equity", risk)
    result = dal.DupireScriptRiskResult_New(plan)
    return [value for row in result.quote_risk.quote_risk.total_adjoints.to_rows() for value in row]


def composed(source, risk, rows, steps):
    base = gradient(source, 0.001, risk)
    products = []
    for row, step in zip(rows, steps):
        gradients = []
        for sign in [1, -1]:
            values = [0.001 + sign * step * direction for direction in row]
            fresh = calibration(dal.DoubleMatrix_([values[i:i + 2] for i in range(0, 6, 2)]))
            gradients.append(gradient(fresh, values[3], risk))
        products.append([(plus - minus) / (2 * step) for plus, minus in zip(*gradients)])
    return base, products


def native(source, request):
    plan = dal.DupireScriptCurvaturePlan_New(product(0.001), model(source), source, "equity", request)
    result = dal.DupireScriptCurvatureResult_New(plan)
    return result.gradient, result.hessian_products.to_rows()


def validate(result, count):
    gradient_values, products = result
    expected = 2 * math.exp(-0.05)
    assert len(gradient_values) == 6 and len(products) == count
    assert abs(gradient_values[3] - 0.001 * expected) < 1e-12
    for row, multiplier in zip(products, [1, -2, 0]):
        for column, value in enumerate(row):
            reference = multiplier * expected if column == 3 else 0
            assert abs(value - reference) < 1e-9


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=["native", "composition"], required=True)
    parser.add_argument("--directions", type=int, choices=[0, 3], required=True)
    parser.add_argument("--repeats", type=int, required=True)
    arguments = parser.parse_args()
    source, risk, request, rows, steps = configuration(arguments.directions)
    operation = (lambda: native(source, request)) if arguments.mode == "native" else (lambda: composed(source, risk, rows, steps))
    validate(operation(), arguments.directions)
    started = time.perf_counter_ns()
    for _ in range(arguments.repeats):
        result = operation()
    elapsed = time.perf_counter_ns() - started
    validate(result, arguments.directions)
    print(json.dumps(dict(mode=arguments.mode, directions=arguments.directions, repeats=arguments.repeats,
                          elapsed_ns=elapsed, request_ns=elapsed / arguments.repeats,
                          gradient_evaluations=1 + 2 * arguments.directions, paths_per_evaluation=17)))


if __name__ == "__main__":
    main()
