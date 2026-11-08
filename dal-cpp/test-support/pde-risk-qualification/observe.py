#!/usr/bin/env python3
import argparse
import gzip
import itertools
import json
import pathlib

from qualify import digest, write_json

INCLUDE = "\n#include <cstdlib>\n#include <fstream>\n#include <iomanip>\n"
HOOK = r"""
                if (const char* file = std::getenv("DAL502_LAYER_FILE")) {
                    std::ofstream trace(file, std::ios::app);
                    trace << std::setprecision(17) << "{\"vol\":" << r_.market_.volatility_
                          << ",\"time\":" << step.timeIndex_ << ",\"dt\":" << step.dt_
                          << ",\"success\":" << (IsSuccess() ? "true" : "false");
                    const auto array = [&](const char* key, const Vector_<double>& v, double scale) {
                        trace << ",\"" << key << "\":[";
                        for (size_t i = 0; i < v.size(); ++i) {
                            if (i) trace << ',';
                            trace << v[i] / scale;
                        }
                        trace << ']';
                    };
                    array("source", *out_.values_, 1.0);
                    array("current", a_.current_, 1.0);
                    array("previous_correction", a_.previous_, a_.scale_);
                    array("correction", a_.iterate_, a_.scale_);
                    trace << "}\n";
                }
"""


def solve(matrix, rhs):
    a = [row[:] + [value] for row, value in zip(matrix, rhs)]
    n = len(a)
    for i in range(n):
        pivot = max(range(i, n), key=lambda j: abs(a[j][i]))
        a[i], a[pivot] = a[pivot], a[i]
        for j in range(i + 1, n):
            factor = a[j][i] / a[i][i]
            a[j] = [x - factor * y for x, y in zip(a[j], a[i])]
    result = [0.0] * n
    for i in reversed(range(n)):
        result[i] = (a[i][-1] - sum(a[i][j] * result[j] for j in range(i + 1, n))) / a[
            i
        ][i]
    return result


def row(layer, j):
    i = j + 1
    diffusion, drift = 0.5 * layer["vol"] * layer["vol"] * i * i, 0.05 * i
    low = diffusion + (max(-drift, 0) if diffusion < 0.5 * abs(drift) else -0.5 * drift)
    high = diffusion + (max(drift, 0) if diffusion < 0.5 * abs(drift) else 0.5 * drift)
    diagonal = 1 + layer["dt"] * (low + high + 0.05)
    result = [0.0] * 5
    result[j] = 1.0
    if j > 0:
        result[j - 1] = -layer["dt"] * low / diagonal
    if j + 1 < 5:
        result[j + 1] = -layer["dt"] * high / diagonal
    rhs = (layer["source"][i] + (layer["dt"] * low * 100 if j == 0 else 0)) / diagonal
    return result, rhs


def system(layer):
    rows = [row(layer, j) for j in range(5)]
    return [a for a, b in rows], [b for a, b in rows]


def residual(matrix, rhs, obstacle, value):
    gap = [v - p for v, p in zip(value, obstacle)]
    dual = [sum(a * v for a, v in zip(row, value)) - b for row, b in zip(matrix, rhs)]
    return [
        max([0.0] + [-g for g in gap]),
        max([0.0] + [-d for d in dual]),
        max(abs(min(g, d)) for g, d in zip(gap, dual)),
    ]


def enumerate_lcp(matrix, rhs, obstacle):
    feasible = []
    for mask in itertools.product([False, True], repeat=5):
        active = [
            [float(i == j) for j in range(5)] if mask[i] else matrix[i]
            for i in range(5)
        ]
        target = [obstacle[i] if mask[i] else rhs[i] for i in range(5)]
        candidate = solve(active, target)
        if max(residual(matrix, rhs, obstacle, candidate)) < 1e-10:
            feasible.append(candidate)
    return feasible


def linear_error(layer, matrix, rhs, obstacle, value):
    linear = 0.0
    for i in range(5):
        penalty = 1e8 if layer["previous_correction"][i + 1] < 0 else 0.0
        terms = [matrix[i][j] * value[j] for j in range(5)]
        terms[i] += penalty * value[i]
        target = rhs[i] + penalty * obstacle[i]
        denominator = abs(target) + sum(abs(v) for v in terms)
        linear = max(
            linear, abs(sum(terms) - target) / denominator if denominator else 0.0
        )
    return linear


def admitted(layer, feasible, error, original, change, linear, epsilon):
    return all(
        [
            layer["success"],
            bool(feasible),
            error is not None,
            error is not None and error <= epsilon + 1e-8,
            max(original + [change]) <= epsilon,
            linear <= 1e-12,
        ]
    )


def layer_details(layer):
    matrix, rhs = system(layer)
    obstacle = [max(100 - 400 * i / 6, 0.0) for i in range(1, 6)]
    feasible = enumerate_lcp(matrix, rhs, obstacle)
    value = layer["current"][1:-1]
    original = residual(matrix, rhs, obstacle, value)
    change = max(
        abs(a - b)
        for a, b in zip(layer["correction"][1:-1], layer["previous_correction"][1:-1])
    )
    error = max(abs(v - r) for v, r in zip(value, feasible[0])) if feasible else None
    linear = linear_error(layer, matrix, rhs, obstacle, value)
    return matrix, rhs, feasible, value, original, change, error, linear


def diagnostic_match(original, recorded, change, diagnostics):
    return (
        max(abs(a - b) for a, b in zip(original, recorded)) <= 1e-12
        and abs(change - diagnostics["iterationChange_"]) <= 1e-12
    )


def audit(layer, diagnostics, epsilon):
    matrix, rhs, feasible, value, original, change, error, linear = layer_details(layer)
    recorded = [
        diagnostics[k]
        for k in ["obstacleViolation_", "dualViolation_", "complementarityResidual_"]
    ]
    matched = diagnostic_match(original, recorded, change, diagnostics)
    passed = (
        admitted(layer, feasible, error, original, change, linear, epsilon) and matched
    )
    return dict(
        vol=layer["vol"],
        time=layer["time"],
        matrix=matrix,
        rhs=rhs,
        feasible=feasible,
        current=value,
        enumerated_error=error,
        residuals=original,
        recorded_residuals=recorded,
        iteration_change=change,
        linear_backward_error=linear,
        status="PASS" if passed else "FAIL",
    )


def prepare(build):
    source = pathlib.Path("dal-cpp/dal/math/pde/earlyexercisepde.cpp")
    text = source.read_text()
    anchor = "                CompleteStep(&step);"
    if text.count(anchor) != 1:
        raise RuntimeError("observation insertion point is not unique")
    instrumented = INCLUDE + text.replace(anchor, HOOK + anchor)
    if instrumented.removeprefix(INCLUDE).replace(HOOK, "") != text:
        raise RuntimeError("observation does not restore original source")
    (build / "observed.cpp").write_text(instrumented)


def audit_layer(layer, diagnostics, epsilon):
    if layer["vol"] == 0.2:
        return audit(layer, diagnostics[layer["time"]], epsilon)
    dummy = dict.fromkeys(
        [
            "obstacleViolation_",
            "dualViolation_",
            "complementarityResidual_",
            "iterationChange_",
        ],
        0.0,
    )
    result = audit(layer, dummy, epsilon)
    good = admitted(
        layer,
        result["feasible"],
        result["enumerated_error"],
        result["residuals"],
        result["iteration_change"],
        result["linear_backward_error"],
        epsilon,
    )
    result["status"] = "PASS" if good else "FAIL"
    return result


def collect(build, evidence):
    trace = evidence / "N9a-layers.jsonl"
    with gzip.open(evidence / "N9a-observed.json.gz", "rt") as stream:
        observed = json.load(stream)
    observed.pop("elapsed_seconds", None)
    observed.pop("returncode", None)
    with gzip.open(evidence / "N9a.json.gz", "rt") as stream:
        original = json.load(stream)
    original.pop("elapsed_seconds", None)
    original.pop("returncode", None)
    identical = original == observed
    layers = [json.loads(line) for line in trace.read_text().splitlines()]
    diagnostics = {d["timeIndex_"]: d for d in original["base_steps"]}
    rows = [
        audit_layer(layer, diagnostics, original["plan"]["epsilon_"])
        for layer in layers
    ]
    passed = (
        identical and len(layers) == 40 and all(r["status"] == "PASS" for r in rows)
    )
    result = dict(
        status="PASS" if passed else "FAIL",
        identical_uninstrumented_result=identical,
        observed_layers=len(layers),
        source_sha256=digest(pathlib.Path("dal-cpp/dal/math/pde/earlyexercisepde.cpp")),
        observed_source_sha256=digest(build / "observed.cpp"),
        executable_sha256=digest(build / "dal502_observed"),
        rows=rows,
    )
    write_json(evidence / "n9a-audit.json", result)
    print(
        result["status"], "layers", len(layers), "ordinary result identical", identical
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("phase", choices=["prepare", "collect"])
    parser.add_argument("--build", required=True, type=pathlib.Path)
    parser.add_argument("--evidence", required=True, type=pathlib.Path)
    args = parser.parse_args()
    if args.phase == "prepare":
        prepare(args.build.resolve())
    else:
        collect(args.build.resolve(), args.evidence.resolve())


if __name__ == "__main__":
    main()
