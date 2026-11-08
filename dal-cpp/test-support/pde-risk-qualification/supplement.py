#!/usr/bin/env python3
import argparse
import concurrent.futures
import json
import math
import pathlib

from qualify import bumped, fixtures, invoke, load, write_json


def membership(regions, spot, dx):
    node = spot / dx
    low, high = math.floor(node), math.ceil(node)
    if abs(node - round(node)) < 1e-10:
        low = high = round(node)
    for region in regions:
        if region['firstNode_'] <= low and high <= region['lastNode_']:
            return 'EXERCISE'
        if region['firstNode_'] <= low <= region['lastNode_'] or region['firstNode_'] <= high <= region['lastNode_']:
            return 'BOUNDARY'
    return 'CONTINUATION'


def intersects(regions, spot, h, dx):
    for region in regions:
        for clipped, left, right in [(region['leftClipped_'], region['firstNode_'] - 1, region['firstNode_']),
                                     (region['rightClipped_'], region['lastNode_'], region['lastNode_'] + 1)]:
            if not clipped and max(spot - 2 * h, left * dx) <= min(spot + 2 * h, right * dx):
                return True
    return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--driver', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    driver, directory = args.driver.resolve(), args.output.resolve()
    requests = fixtures()
    results = {}

    def record(name, conditions, evidence):
        results[name] = dict(status='PASS' if all(conditions) else 'FAIL', evidence=evidence)

    for name in [n for n in requests if n.startswith('N2-')]:
        original = load(directory, name)
        observed = invoke(driver, directory, name + '-contacts', 'audit', dict(requests[name], audit_contacts=True))
        eligible = [row for row in observed['contact_audit'] if all(s['dividendYield_'] <= 0 for s in row['market']['carry_'])]
        record(name, [all(row['nonterminal_regions'] == 0 for row in eligible), all(row['same_price'] for row in observed['contact_audit']),
                      original['plan']['planId_'] == observed['audit_plan_id']], eligible)

    control = load(directory, 'N3-0-100')
    run = next((region for region in control['base_regions'] if region['timeIndex_'] == 0 and not region['rightClipped_']), None)
    if run is None:
        record('N10a', [False], 'No specified positive intrinsic unclipped evaluation run')
    else:
        dx = control['plan']['gridSpacing_']
        midpoint = (run['lastNode_'] + .5) * dx
        for shift in [0, -2, 2]:
            name = f'N10a-{shift}'
            request = dict(requests['N3-0-100'], spot=midpoint + shift, h=.5, audit_contacts=True)
            r = invoke(driver, directory, name, 'audit', request)
            rows = r['contact_audit']
            conditions = [r['price'] is not None, r['plan']['epsilon_'] == control['plan']['epsilon_'],
                          r['plan']['planId_'] == r['audit_plan_id'], all(row['same_price'] for row in rows)]
            memberships, intersections = [], []
            for audit, scenario in zip(rows, r['scenarios']):
                observed = membership(audit['evaluation_regions'], audit['market']['spot_'], dx)
                crossing = intersects(audit['evaluation_regions'], request['spot'], .5, dx)
                memberships.append(observed)
                intersections.append(crossing)
                conditions += [scenario['evaluationMembership_'] == observed,
                               scenario['evaluationBoundaryIntersectsSpotStencil_'] == crossing]
            for risk in r['risks']:
                indices = {entry['scenarioIndex_'] for entry in risk['fineStencil_'] + risk['coarseStencil_']} | {0}
                if risk['riskId_'] in ['Delta', 'Gamma']:
                    boundary = any(intersections[i] for i in indices)
                    if boundary:
                        conditions.append(risk['status_'] == 'UNAVAILABLE_NEAR_EXERCISE_BOUNDARY')
                    else:
                        stable = risk['normalizedStepDifference_'] <= risk['stabilityTolerance_']
                        expected = 'AVAILABLE_EXERCISE_REGION' if all(memberships[i] == 'EXERCISE' for i in indices) else 'AVAILABLE'
                        conditions.append(risk['status_'] == (expected if stable else 'UNAVAILABLE_STEP_UNSTABLE'))
                elif any(memberships[i] == 'BOUNDARY' or memberships[i] != memberships[0] for i in indices):
                    conditions.append(risk['status_'] == 'UNAVAILABLE_EXERCISE_SWITCH')
            if shift == 0:
                conditions.append(all(row['status_'] == 'UNAVAILABLE_NEAR_EXERCISE_BOUNDARY' for row in r['risks'][:2]))
            record(name, conditions, dict(midpoint=midpoint, query=request['spot'], memberships=memberships, intersections=intersections, risks=r['risks']))

    for knot in [137, 182]:
        for bump in [1e-4, 5e-5]:
            name = f'N12-{knot}-{bump}'
            r = invoke(driver, directory, name + '-parallel', 'audit', dict(requests[name], audit_parallel=True))
            comparisons = []
            for parallel in r['parallel_audit']:
                family = 'Rate[' if parallel['rate'] else 'DividendYield['
                total = sum(row['value_'] for row in r['risks'] if row['riskId_'].startswith(family))
                comparisons.append(abs(total - parallel['partial']) / 100)
            record(name, [all(value <= .003 for value in comparisons), r['plan']['planId_'] == r['audit_plan_id']],
                   dict(differences=comparisons, parallel=r['parallel_audit']))

    def reference(knot):
        base = requests[f'N4-0-100-{knot}']
        levels = []
        for level in range(3):
            estimates = []
            for family in [1, 2]:
                values = []
                for bump in [5e-5, 2.5e-5, 1.25e-5]:
                    sides = []
                    for side in [-1, 1]:
                        request = dict(bumped(base, (family, 'parallel'), side * bump), level=level)
                        raw = invoke(driver, directory, f'N12ref-{knot}-{level}-{family}-{bump}-{side}', 'log', request)
                        if 'error' in raw:
                            raise RuntimeError(raw['error'])
                        sides.append(raw['price'])
                    values.append((sides[1] - sides[0]) / (200 * bump))
                estimates.append(values)
            levels.append(estimates)
        bump_diff = [abs(family[-1] - family[-2]) for family in levels[-1]]
        level_diff = [abs(levels[-1][family][-1] - levels[-2][family][-1]) for family in range(2)]
        return knot, levels, bump_diff, level_diff

    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
        for knot, levels, bump_diff, level_diff in executor.map(reference, [137, 182]):
            record(f'N12ref-{knot}', [all(value < 7.5e-4 for value in bump_diff + level_diff)],
                   dict(levels=levels, bump_difference=bump_diff, refinement_difference=level_diff))
    write_json(directory / 'supplement.json', dict(status='PASS' if all(row['status'] == 'PASS' for row in results.values()) else 'FAIL', checks=results))
    print(json.dumps({name: row['status'] for name, row in results.items()}, indent=2))


if __name__ == '__main__':
    main()
