import hashlib
import json
import math

from fixtures import BASE
from qualify import bumped, differences, normalize_reference, required_vector, within

PHYSICAL = ('evaluation', 'days', 'spot', 'vol', 'call', 'american', 'carry', 'exercise')


def physical(request):
    result = {key: request.get(key, BASE[key]) for key in PHYSICAL}
    result['spot'] = float(result['spot'])
    result['vol'] = float(result['vol'])
    result['days'] = float(result['days'])
    result['carry'] = [[float(v) for v in row] for row in result['carry']]
    result['exercise'] = [float(v) for v in result['exercise']]
    return result


def fingerprint(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def binding(request):
    return fingerprint(dict(physical=physical(request), scales=scales(request)))


def scales(request):
    h, vol, carry = required_vector(request)
    return [[h * scale, vol / 2 * scale, carry / 2 * scale] for scale in [2, 1, .5]]


def axes(request):
    return ['spot', 'vol'] + [[family, i] for i in range(len(request['carry'])) for family in [1, 2]]


def offsets(request):
    result = [('base', 0.)]
    for spot, vol, carry in scales(request):
        sizes = [spot, vol] + [carry] * (2 * len(request['carry']))
        for axis, size in zip(axes(request), sizes):
            result += [(axis, -size), (axis, size)]
    return result


def price_spec(base, axis, offset, mode, level):
    request = physical(base)
    if axis != 'base':
        request = bumped(request, axis, offset)
    selector = dict(tree_steps=level) if mode == 'tree' else dict(level=level)
    return dict(mode=mode, request=dict(request, **selector), axis=axis, offset=float(offset))


def price_key(spec, identity):
    return fingerprint(dict(spec=spec, execution=identity))


def node_charge(spec):
    if spec['mode'] != 'tree':
        return 0
    n = spec['request']['tree_steps']
    return (n + 1) * (n + 2) // 2


def estimates(request, price):
    result = []
    for spot, vol, carry in scales(request):
        base = price('base', 0.)
        low, high = price('spot', -spot), price('spot', spot)
        values = [base, (high - low) / (2 * spot), (high - 2 * base + low) / spot ** 2]
        sizes = [vol] + [carry] * (2 * len(request['carry']))
        for axis, size in zip(axes(request)[1:], sizes):
            values.append((price(axis, size) - price(axis, -size)) / (2 * size))
        result.append(normalize_reference(values))
    return result


def finite_values(values):
    return all(value is not None and math.isfinite(value) for value in values)


def self_qualification(levels, mode):
    last = levels[-1]
    mesh = differences(last['mean'][2], levels[-2]['mean'][2])
    bump = differences(last['mean'][2], last['mean'][1])
    parity = []
    if mode == 'tree':
        parity = [differences(last['members'][0][s], last['members'][1][s]) for s in range(3)]
    passed = all(within(diff, .25, True) for diff in [mesh, bump] + parity)
    return dict(status='PASS' if passed else 'INCOMPLETE', levels=levels,
                mesh_difference=mesh, bump_difference=bump, parity_differences=parity,
                normalized=last['mean'][2])


def mean_members(members):
    return [[sum(member[s][i] for member in members) / len(members)
             for i in range(len(members[0][s]))] for s in range(3)]
