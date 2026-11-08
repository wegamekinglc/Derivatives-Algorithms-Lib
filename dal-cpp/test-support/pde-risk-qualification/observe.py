#!/usr/bin/env python3
import argparse
import gzip
import itertools
import json
import os
import pathlib
import subprocess

from qualify import digest, write_json

INCLUDE = '\n#include <cstdlib>\n#include <fstream>\n#include <iomanip>\n'
HOOK = r'''
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
'''


def solve(matrix, rhs):
    a = [row[:] + [value] for row, value in zip(matrix, rhs)]
    n = len(a)
    for i in range(n):
        pivot = max(range(i, n), key=lambda j: abs(a[j][i]))
        a[i], a[pivot] = a[pivot], a[i]
        for j in range(i + 1, n):
            factor = a[j][i] / a[i][i]
            a[j] = [x - factor * y for x, y in zip(a[j], a[i])]
    result = [0.] * n
    for i in reversed(range(n)):
        result[i] = (a[i][-1] - sum(a[i][j] * result[j] for j in range(i + 1, n))) / a[i][i]
    return result


def audit(layer, diagnostics, epsilon):
    n = 5
    dt, vol = layer['dt'], layer['vol']
    obstacle = [max(100 - 400 * i / 6, 0.) for i in range(1, 6)]
    matrix, rhs = [], []
    for j in range(n):
        i = j + 1
        diffusion, drift = .5 * vol * vol * i * i, .05 * i
        low = diffusion + (max(-drift, 0) if diffusion < .5 * abs(drift) else -.5 * drift)
        high = diffusion + (max(drift, 0) if diffusion < .5 * abs(drift) else .5 * drift)
        diagonal = 1 + dt * (low + high + .05)
        row = [0.] * n
        row[j] = 1.
        if j > 0:
            row[j - 1] = -dt * low / diagonal
        if j + 1 < n:
            row[j + 1] = -dt * high / diagonal
        matrix.append(row)
        rhs.append((layer['source'][i] + (dt * low * 100 if j == 0 else 0)) / diagonal)

    def residual(value):
        gap = [v - p for v, p in zip(value, obstacle)]
        dual = [sum(a * v for a, v in zip(row, value)) - b for row, b in zip(matrix, rhs)]
        return [max([0.] + [-g for g in gap]), max([0.] + [-d for d in dual]), max(abs(min(g, d)) for g, d in zip(gap, dual))]

    feasible = []
    for mask in itertools.product([False, True], repeat=n):
        active = [[float(i == j) for j in range(n)] if mask[i] else matrix[i] for i in range(n)]
        target = [obstacle[i] if mask[i] else rhs[i] for i in range(n)]
        candidate = solve(active, target)
        if max(residual(candidate)) < 1e-10:
            feasible.append(candidate)
    value = layer['current'][1:-1]
    original = residual(value)
    change = max(abs(a - b) for a, b in zip(layer['correction'][1:-1], layer['previous_correction'][1:-1]))
    error = max(abs(v - r) for v, r in zip(value, feasible[0])) if feasible else None
    linear = 0.
    for i in range(n):
        penalty = 1e8 if layer['previous_correction'][i + 1] < 0 else 0.
        terms = [matrix[i][j] * value[j] for j in range(n)]
        terms[i] += penalty * value[i]
        target = rhs[i] + penalty * obstacle[i]
        denominator = abs(target) + sum(abs(v) for v in terms)
        linear = max(linear, abs(sum(terms) - target) / denominator if denominator else 0.)
    recorded = [diagnostics[k] for k in ['obstacleViolation_', 'dualViolation_', 'complementarityResidual_']]
    passed = (layer['success'] and feasible and error <= epsilon + 1e-8 and max(original + [change]) <= epsilon
              and linear <= 1e-12 and max(abs(a - b) for a, b in zip(original, recorded)) <= 1e-12
              and abs(change - diagnostics['iterationChange_']) <= 1e-12)
    return dict(vol=vol, time=layer['time'], matrix=matrix, rhs=rhs, feasible=feasible, current=value,
                enumerated_error=error, residuals=original, recorded_residuals=recorded,
                iteration_change=change, linear_backward_error=linear, status='PASS' if passed else 'FAIL')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', required=True, type=pathlib.Path)
    parser.add_argument('--evidence', required=True, type=pathlib.Path)
    args = parser.parse_args()
    root = pathlib.Path.cwd()
    source = root / 'dal-cpp/dal/math/pde/earlyexercisepde.cpp'
    build, evidence = args.build.resolve(), args.evidence.resolve()
    text = source.read_text()
    anchor = '                CompleteStep(&step);'
    assert text.count(anchor) == 1
    instrumented = INCLUDE + text.replace(anchor, HOOK + anchor)
    assert instrumented.removeprefix(INCLUDE).replace(HOOK, '') == text
    copy = build / 'dal502-observed.cpp'
    copy.write_text(instrumented)
    flags = ['g++', '-std=c++17', '-O3', '-DNDEBUG', '-ffp-contract=fast', '-I', str(root / 'dal-cpp'),
             '-I', str(root / 'dal-cpp/externals/rapidjson/include')]
    object_file = build / 'dal502-observed.o'
    executable = build / 'dal502-observed-driver'
    subprocess.run(flags + ['-c', str(copy), '-o', str(object_file)], check=True)
    subprocess.run(flags + [str(root / 'dal-cpp/test-support/pde-risk-qualification/driver.cpp'), str(object_file),
                            str(build / 'dal-cpp/libdal_cpp.a'), '-pthread', '-o', str(executable)], check=True)
    trace = evidence / 'N9a-layers.jsonl'
    trace.write_text('')
    run = subprocess.run([str(executable), 'product', str(evidence / 'N9a.input.json')],
                         env=dict(os.environ, DAL502_LAYER_FILE=str(trace)), capture_output=True, text=True, check=True)
    observed = json.loads(run.stdout)
    with gzip.open(evidence / 'N9a.json.gz', 'rt') as stream:
        original = json.load(stream)
    original.pop('elapsed_seconds')
    original.pop('returncode')
    identical = original == observed
    layers = [json.loads(line) for line in trace.read_text().splitlines()]
    diagnostics = {d['timeIndex_']: d for d in original['base_steps']}
    # Step diagnostics are retained for base only; scenario maxima still bound each observed bump.
    rows = [audit(layer, diagnostics[layer['time']], original['plan']['epsilon_']) for layer in layers if layer['vol'] == .2]
    bump_rows = []
    for layer in layers:
        if layer['vol'] == .2:
            continue
        recomputed = audit(layer, dict.fromkeys(['obstacleViolation_', 'dualViolation_', 'complementarityResidual_', 'iterationChange_'], 0.),
                           original['plan']['epsilon_'])
        recomputed['status'] = 'PASS' if (layer['success'] and recomputed['feasible'] and
            recomputed['enumerated_error'] <= original['plan']['epsilon_'] + 1e-8 and
            max(recomputed['residuals'] + [recomputed['iteration_change']]) <= original['plan']['epsilon_'] and
            recomputed['linear_backward_error'] <= 1e-12) else 'FAIL'
        bump_rows.append(recomputed)
    result = dict(status='PASS' if identical and len(layers) == 40 and all(r['status'] == 'PASS' for r in rows + bump_rows) else 'FAIL',
                  identical_uninstrumented_result=identical, observed_layers=len(layers), source_sha256=digest(source),
                  observed_source_sha256=digest(copy), executable_sha256=digest(executable), rows=rows + bump_rows)
    write_json(evidence / 'n9a-audit.json', result)
    print(result['status'], 'layers', len(layers), 'ordinary result identical', identical)


if __name__ == '__main__':
    main()
