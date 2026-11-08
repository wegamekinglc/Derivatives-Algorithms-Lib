#!/usr/bin/env python3
import argparse
import concurrent.futures
import copy
import gzip
import hashlib
import json
import math
import pathlib
import subprocess
import time

HARD = [1e-4, 2e-3, 2e-2, 3e-3]
BASE = dict(evaluation=[2026, 10, 18], days=365, spot=100.0, vol=.2,
            call=False, american=False, carry=[[365, .05, .03]], exercise=[365])


def fixtures():
    result = {}

    def add(name, **overrides):
        result[name] = dict(copy.deepcopy(BASE), **overrides)

    for call in [False, True]:
        for spot in [80, 100, 120]:
            for q in [0, .03]:
                add(f'N1-{int(call)}-{spot}-{q}', call=call, spot=spot, carry=[[365, .05, q]])
    add('N1-default', call=True, spot=120, carry=[[365, .05, 0]], nodes=401, steps=500,
        default_upper=True, default_caps=True)
    for spot in [80, 100, 120]:
        add(f'N2-{spot}', call=True, spot=spot, carry=[[365, .05, 0]], american=True, exercise=[])
    add('N2-piecewise', call=True, american=True, exercise=[], carry=[[137, .02, 0], [365, .06, 0]])
    for call in [False, True]:
        for spot in [100, 120]:
            add(f'N3-{int(call)}-{spot}', call=call, spot=spot, american=True, exercise=[],
                carry=[[365, .05, .08 if call else 0]])
        for spot in [100, 120]:
            for knot in [137, 182]:
                add(f'N4-{int(call)}-{spot}-{knot}', call=call, spot=spot, exercise=[182, 365],
                    carry=[[knot, .02, .01], [365, .06, .03]])
    for swap in [False, True]:
        rate = .2 if swap else -.2
        for style in ['bermudan', 'american', 'terminal']:
            add(f'N5a-{int(swap)}-{style}', spot=80, vol=0,
                carry=[[137, rate, rate], [365, -rate, -rate]],
                american=style == 'american', exercise=[] if style == 'american' else [365] if style == 'terminal' else [137, 365])
    for year, maturity_days in [(2028, 2), (2027, 2)]:
        for call in [False, True]:
            for spot in [100, 100.0125]:
                add(f'N5b-{year}-{int(call)}-{spot}', evaluation=[year, 2, 28], days=maturity_days,
                    call=call, spot=spot, nodes=16001, carry=[[2, .05, .03]], exercise=[2])
    for american in [False, True]:
        add(f'N5c-{int(american)}', spot=50, american=american, exercise=[] if american else [0, 182, 365], carry=[[365, .05, 0]])
    controls = ['N1-0-100-0.03', 'N3-0-100', 'N3-1-100', 'N4-0-100-137']
    for i, name in enumerate(controls):
        base = result[name]
        for nodes in [401, 801, 1601]:
            add(f'N6-{i}-space-{nodes}', **dict(base, nodes=nodes, steps=8000))
        for steps in [1000, 2000, 4000]:
            add(f'N6-{i}-time-{steps}', **dict(base, nodes=1601, steps=steps, h=.5))
        for upper, nodes in [(400, 1601), (600, 2401), (800, 3201)]:
            add(f'N7-{i}-domain-{upper}', **dict(base, nodes=nodes, upper=upper, h=.5))
        for penalty in [1e7, 1e8, 1e9]:
            add(f'N7-{i}-penalty-{penalty}', **dict(base, penalty=penalty))
        for nodes in [801, 1601]:
            for cells in [4, 2]:
                add(f'N7-{i}-spot-{nodes}-{cells}', **dict(base, nodes=nodes, h=cells * 400 / (nodes - 1)))
        for scale in [1, .5, .25]:
            add(f'N7-{i}-parameter-{scale}', **dict(base, vol_bump=.001 * scale, carry_bump=.0001 * scale))
    for i, name in enumerate(controls[:2]):
        add(f'N8-{i}', **dict(result[name], theta=.5, nodes=401, steps=5000))
    add('N9a', american=True, exercise=[], nodes=7, steps=8, carry=[[365, .05, 0]], delta=False, gamma=False, carry_risk=False)
    for penalty, iterations in [(1, 100), (1e8, 100), (1e8, 1)]:
        add(f'N9b-{penalty}-{iterations}', american=True, exercise=[], spot=80, vol=0,
            carry=[[137, .1, .1], [365, -.01, -.01]], nodes=41, steps=500, penalty=penalty, iterations=iterations)
    add('N9c', nodes=401, steps=500, linear=1e-30)
    for work in [1326, 1325, 390, 78, 77]:
        add(f'N9d-{work}', nodes=41, steps=2, rannacher=0, work=work)
    for vol in [0, 1e-8]:
        add(f'N10b-{vol}', vol=vol)
    for spot in [.25, 399.75]:
        add(f'N10c-{spot}', spot=spot, h=.5)
    for call in [False, True]:
        for i, (spot, vol, rate, q) in enumerate([(20, .2, .05, 0), (300, .2, .05, 0), (100, 1., .05, .03),
                                                (100, .2, -.05, .03), (100, .2, .20, -.10)]):
            add(f'N10d-{int(call)}-{i}', call=call, spot=spot, vol=vol, carry=[[365, rate, q]],
                upper=800 if vol == 1. else 400, nodes=3201 if vol == 1. else 1601)
    add('N10-upwind', vol=math.sqrt(.05), nodes=401, carry=[[365, .05, 0]])
    for spot in [100, 100.125]:
        add(f'N11-reject-{spot}', spot=spot, h=.125)
        add(f'N11-admit-{spot}', spot=spot, h=.5)
    for half in [False, True]:
        for nodes in [401, 801, 1601]:
            add(f'N11-space-{int(half)}-{nodes}', nodes=nodes, spot=100 + (200 / (nodes - 1) if half else 0))
    for knot in [137, 182]:
        for bump in [1e-4, 5e-5]:
            add(f'N12-{knot}-{bump}', **dict(result[f'N4-0-100-{knot}'], carry_bump=bump))
    return result


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def invoke(driver, directory, name, mode, request):
    source = directory / (name + '.input.json')
    write_json(source, request)
    started = time.monotonic()
    run = subprocess.run([str(driver), mode, str(source)], capture_output=True, text=True)
    try:
        output = json.loads(run.stdout)
    except json.JSONDecodeError:
        output = dict(error=f'driver exit={run.returncode}', stdout=run.stdout, stderr=run.stderr)
    output['elapsed_seconds'] = time.monotonic() - started
    output['returncode'] = run.returncode
    target = directory / (name + '.json.gz')
    temporary = target.with_suffix('.partial')
    with gzip.open(temporary, 'wt') as stream:
        json.dump(output, stream, allow_nan=False)
    temporary.replace(target)
    return output


def load(directory, name):
    with gzip.open(directory / (name + '.json.gz'), 'rt') as stream:
        return json.load(stream)


def normalized(output):
    return [None if output.get('price') is None else output['price'] / 100] + [
        None if r['value_'] is None else r['value_'] * (100 if r['riskId_'] == 'Gamma' else 1 if r['riskId_'] == 'Delta' else .01)
        for r in output.get('risks', [])]


def normalize_reference(values):
    return [v * (.01 if i == 0 or i >= 3 else 100 if i == 2 else 1) for i, v in enumerate(values)]


def limits(count):
    return HARD + [3e-3] * (count - 4)


def differences(a, b):
    if len(a) != len(b):
        return None
    return [None if x is None or y is None else abs(x - y) for x, y in zip(a, b)]


def within(diff, fraction=1, strict=False):
    return diff is not None and all(x is not None and (x < fraction * t if strict else x <= fraction * t)
                                    for x, t in zip(diff, limits(len(diff))))


def scalar_price(request):
    t = request.get('days', 365) / 365
    start = 0
    rate = dividend = 0
    for end, r, q in request['carry']:
        rate += r * (end - start) / 365
        dividend += q * (end - start) / 365
        start = end
    spot, vol = request.get('spot', 100), request.get('vol', .2)
    sd = vol * math.sqrt(t)
    d2 = (math.log(spot / 100) + rate - dividend - .5 * sd * sd) / sd
    d1 = d2 + sd
    sign = 1 if request.get('call', False) else -1
    cdf = lambda x: (1 + math.erf(x / math.sqrt(2))) / 2
    return sign * (spot * math.exp(-dividend) * cdf(sign * d1) - 100 * math.exp(-rate) * cdf(sign * d2))


def bumped(request, axis, offset):
    r = copy.deepcopy(request)
    if axis in ['spot', 'vol']:
        r[axis] = r.get(axis, 100 if axis == 'spot' else .2) + offset
    else:
        family, index = axis
        if index == 'parallel':
            for segment in r['carry']:
                segment[family] += offset
        else:
            r['carry'][index][family] += offset
    return r


def derivatives(price, request, spot_bump, parameter_bump, carry_bump):
    base = price(request)
    result = [base]
    low, high = price(bumped(request, 'spot', -spot_bump)), price(bumped(request, 'spot', spot_bump))
    result += [(high - low) / (2 * spot_bump), (high - 2 * base + low) / spot_bump ** 2]
    for axis, bump in [('vol', parameter_bump)] + [(family_index, carry_bump) for i in range(len(request['carry'])) for family_index in [(1, i), (2, i)]]:
        result.append((price(bumped(request, axis, bump)) - price(bumped(request, axis, -bump))) / (2 * bump))
    return result


def analytic_reference(driver, directory, name, request):
    output = invoke(driver, directory, name, 'analytic', request)
    values = output['values']
    independent = derivatives(scalar_price, request, .001, 1e-5, 1e-5)
    difference = differences(normalize_reference(values), normalize_reference(independent))
    return dict(values=values, normalized=normalize_reference(values), formula_difference=difference,
                status='PASS' if within(difference, .25, True) else 'INCOMPLETE')


def numerical_reference(driver, directory, name, request, mode):
    levels = []
    for level in range(3):
        members = []
        for parity in range(2 if mode == 'tree' else 1):
            counter = 0
            cache = {}

            def price(r):
                nonlocal counter
                key = json.dumps(r, sort_keys=True)
                if key not in cache:
                    settings = dict(r, tree_steps=[4096, 8192, 16384][level] + parity, level=level)
                    raw = invoke(driver, directory, f'{name}-L{level}-P{parity}-{counter}', mode, settings)
                    if 'error' in raw:
                        raise RuntimeError(raw['error'])
                    cache[key] = raw['price']
                    counter += 1
                return cache[key]

            scales = []
            dx = request.get('upper', 400) / (request.get('nodes', 1601) - 1)
            h = request.get('h', 2 * dx)
            for scale in [2, 1, .5]:
                scales.append(normalize_reference(derivatives(price, request, h * scale,
                    request.get('vol_bump', .001) / 2 * scale, request.get('carry_bump', .0001) / 2 * scale)))
            members.append(scales)
        mean = [[sum(member[s][i] for member in members) / len(members) for i in range(len(members[0][s]))] for s in range(3)]
        levels.append(dict(members=members, mean=mean))
        print(name, mode, 'level', level, 'captured', flush=True)
    last = levels[-1]
    mesh = differences(last['mean'][2], levels[-2]['mean'][2])
    bump = differences(last['mean'][2], last['mean'][1])
    parity = [differences(last['members'][0][scale], last['members'][1][scale]) for scale in range(3)] if mode == 'tree' else []
    status = 'PASS' if all(within(diff, .25, True) for diff in [mesh, bump] + parity) else 'INCOMPLETE'
    return dict(status=status, levels=levels, mesh_difference=mesh, bump_difference=bump,
                parity_differences=parity, normalized=last['mean'][2])


def analyze(directory):
    requests = fixtures()
    refs_path = directory / 'references.json'
    refs = json.loads(refs_path.read_text()) if refs_path.exists() else {}
    rows = {}
    for name, request in requests.items():
        path = directory / (name + '.json.gz')
        if not path.exists():
            rows[name] = dict(status='INCOMPLETE', reason='not executed')
            continue
        output = load(directory, name)
        row = dict(status='INCOMPLETE', normalized=normalized(output), aggregate=output.get('status'), error=output.get('error'),
                   elapsed_seconds=output['elapsed_seconds'])
        if 'risks' in output:
            row.update(risks=output['risks'], scenarios=output['scenarios'], execution=output['execution'],
                       plan_id=output['plan']['planId_'], actual_steps=output['plan']['plannedSteps_'])
        numerical = name.startswith(('N1-', 'N2-', 'N3-', 'N4-', 'N5b-', 'N6-', 'N7-', 'N8-', 'N10d-', 'N11-admit', 'N11-space', 'N12-'))
        if name == 'N1-default':
            row['status'] = 'PASS' if output.get('plan', {}).get('upper_') == 480 else 'FAIL'
        elif name.startswith('N11-reject'):
            row['status'] = 'PASS' if 'InvalidEarlyExercisePdeRiskRequest' in output.get('error', '') else 'FAIL'
        elif numerical:
            refname = reference_name(name)
            reference = refs.get(refname)
            row['reference'] = refname
            if output.get('status') != 'COMPLETE':
                row.update(status='FAIL', reason='mandatory smooth risk unavailable or base/preflight failed')
            elif reference is None or reference['status'] != 'PASS':
                row.update(status='INCOMPLETE', reason='independent reference not qualified')
            else:
                diff = differences(row['normalized'], reference['normalized'])
                row.update(difference=diff, status='PASS' if within(diff) else 'FAIL')
        elif 'error' in output:
            row.update(status='FAIL', reason='unexpected preflight/error')
        rows[name] = row
    write_json(directory / 'summary.json', rows)
    counts = {}
    for name, row in rows.items():
        group = name.split('-')[0]
        counts.setdefault(group, {})
        counts[group][row['status']] = counts[group].get(row['status'], 0) + 1
    print(json.dumps(counts, indent=2), flush=True)


def reference_name(name):
    if name.startswith(('N1-', 'N2-', 'N3-', 'N4-', 'N5b-', 'N10d-', 'N11-')):
        return name
    controls = ['N1-0-100-0.03', 'N3-0-100', 'N3-1-100', 'N4-0-100-137']
    if name.startswith(('N6-', 'N7-')):
        return controls[int(name.split('-')[1])]
    if name.startswith('N8-'):
        return controls[int(name.split('-')[1])]
    if name.startswith('N12-'):
        return 'N4-0-100-' + name.split('-')[1]
    return name


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('phase', choices=['product', 'reference', 'report'])
    parser.add_argument('--driver', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--select', default='')
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args()
    driver = args.driver.resolve()
    directory = args.output.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    requests = {name: r for name, r in fixtures().items() if any(name.startswith(prefix) for prefix in args.select.split(','))}
    if args.phase == 'product':
        write_json(directory / 'fixtures.json', fixtures())

        def run(item):
            name, request = item
            output = invoke(driver, directory, name, 'product', request)
            print(name, output.get('status', output.get('error')), f"{output['elapsed_seconds']:.2f}s", flush=True)

        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
            list(executor.map(run, requests.items()))
    elif args.phase == 'reference':
        path = directory / 'references.json'
        refs = json.loads(path.read_text()) if path.exists() else {}
        selected = [(name, request) for name, request in requests.items()
                    if reference_name(name) == name and name not in refs and request.get('vol', .2) != 0
                    and not name.startswith(('N9', 'N10b', 'N10c', 'N11-reject'))]

        def calculate(item):
            name, request = item
            mode = 'tree' if name.startswith('N3-') else 'log' if name.startswith('N4-') else 'analytic'
            try:
                result = analytic_reference(driver, directory, name + '-ref', request) if mode == 'analytic' else numerical_reference(driver, directory, name + '-ref', request, mode)
            except RuntimeError as error:
                result = dict(status='INCOMPLETE', error=str(error))
            return name, mode, result

        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
            for name, mode, result in executor.map(calculate, selected):
                refs[name] = result
                write_json(path, refs)
                print(name, mode, result['status'], flush=True)
    else:
        analyze(directory)


if __name__ == '__main__':
    main()
