"""Knot-fitted independent nonuniform CN with matched-domain self-checks."""

import mpmath as mp
import numpy as np
from pde import _boundary, timeline
from scipy.linalg import solve_banded


def mandatory_anchors(grid_knots, strike_knots):
    anchors = np.array(sorted({-3.0, 3.0, 0.0, *grid_knots, *strike_knots}))
    if (
        np.any(np.diff(anchors) <= 0)
        or anchors[0] != -3
        or anchors[-1] != 3
        or len(anchors) > 601
    ):
        raise ValueError("REFERENCE_UNAVAILABLE: invalid mandatory mesh")
    return anchors


def allocated_extras(anchors, exact):
    shares = [(601 - len(anchors)) * (b - a) / 6 for a, b in zip(exact, exact[1:])]
    extra = [int(mp.floor(s)) for s in shares]
    order = sorted(
        range(len(extra)), key=lambda j: (-(shares[j] - extra[j]), anchors[j])
    )
    for j in order[: 601 - len(anchors) - sum(extra)]:
        extra[j] += 1
    return extra


def initial_mesh(anchors):
    with mp.workdps(90):
        exact = list(map(mp.mpf, anchors))
        extra = allocated_extras(anchors, exact)
        central = [anchors[0]]
        for j, count in enumerate(extra):
            central.extend(
                float(exact[j] + (exact[j + 1] - exact[j]) * k / (count + 1))
                for k in range(1, count + 1)
            )
            central.append(anchors[j + 1])
    return np.array(central)


def mesh(grid_knots, strike_knots, level=0, expanded=False):
    x = initial_mesh(mandatory_anchors(grid_knots, strike_knots))
    if expanded:
        x = np.concatenate(
            (
                np.linspace(-4.0, -3.0, 101)[:-1],
                x,
                np.linspace(3.0, 4.0, 101)[1:],
            )
        )
    for _ in range(level):
        x = np.sort(np.concatenate((x, (x[:-1] + x[1:]) / 2)))
    if np.any(np.diff(x) <= 0):
        raise ValueError("REFERENCE_UNAVAILABLE: collapsed mesh")
    return x


def derivative_weights(x):
    hm, hp = np.diff(x)[:-1], np.diff(x)[1:]
    first = (-hp / (hm * (hm + hp)), (hp - hm) / (hm * hp), hm / (hp * (hm + hp)))
    second = (2 / (hm * (hm + hp)), -2 / (hm * hp), 2 / (hp * (hm + hp)))
    return np.array(first), np.array(second)


def operator(model, time, interior, weights):
    variance = model.variance(time, interior)
    _, _, growth, rate = model.logs(time)
    first, second = weights
    result = 0.5 * variance * second + (growth - 0.5 * variance) * first
    result[1] -= rate
    return result


def step(values, model, lo, hi, weight, x, strikes, maturity, weights):
    dt = hi - lo
    lower, diagonal, upper = operator(model, (lo + hi) / 2, x[1:-1], weights)
    rhs = values[1:-1] + (1 - weight) * dt * (
        lower[:, None] * values[:-2]
        + diagonal[:, None] * values[1:-1]
        + upper[:, None] * values[2:]
    )
    boundaries = _boundary(model, lo, maturity, 100 * np.exp(x[[0, -1]]), strikes)
    rhs[0] += weight * dt * lower[0] * boundaries[0]
    rhs[-1] += weight * dt * upper[-1] * boundaries[1]
    bands = np.zeros((3, len(x) - 2))
    bands[0, 1:] = -weight * dt * upper[:-1]
    bands[1] = 1 - weight * dt * diagonal
    bands[2, :-1] = -weight * dt * lower[1:]
    solution = solve_banded((1, 1), bands, rhs, check_finite=False)
    result = np.empty_like(values)
    result[[0, -1]], result[1:-1] = boundaries, solution
    residual = bands[1, :, None] * solution - rhs
    residual[1:] += bands[2, :-1, None] * solution[:-1]
    residual[:-1] += bands[0, 1:, None] * solution[1:]
    if not np.all(np.isfinite(result)):
        raise ValueError("REFERENCE_UNAVAILABLE: nonfinite step")
    return result, float(np.max(np.abs(residual)) / max(1.0, np.max(np.abs(rhs))))


def solve(model, maturity, strikes, grid_knots, level=2, steps=2048, expanded=False):
    strikes = np.array(strikes)
    with mp.workdps(90):
        strike_knots = [float(mp.log(mp.mpf(s) / 100)) for s in strikes]
    x = mesh(grid_knots, strike_knots, level, expanded)
    values = _boundary(model, maturity, maturity, 100 * np.exp(x), strikes)
    times = timeline(maturity, steps, model.times)
    if len(times) - 1 > 4342 or steps > 4096 or len(x) > 6401:
        raise ValueError("INCOMPLETE: reference cap")
    weights = derivative_weights(x)
    residual = 0.0
    for index in range(len(times) - 1, 0, -1):
        lo, hi = times[index - 1 : index + 1]
        intervals = (
            ((lo, hi, 0.5),)
            if index != len(times) - 1
            else ((float((lo + hi) / 2), hi, 1.0), (lo, float((lo + hi) / 2), 1.0))
        )
        for left, right, factor in intervals:
            values, error = step(
                values, model, left, right, factor, x, strikes, maturity, weights
            )
            residual = max(residual, error)
    center = int(np.searchsorted(x, 0.0))
    return dict(
        values=values[center].tolist(),
        nodes=len(x),
        requested_steps=steps,
        actual_steps=len(times) - 1,
        linear_solves=len(times),
        max_dt=float(np.max(np.diff(times))),
        max_relative_linear_residual=residual,
        mesh=x.tolist(),
        timeline=times.tolist(),
        level=level,
        expanded=expanded,
        scheme="A1_NONUNIFORM_CN_MIDPOINT_2_HALF_IE/1",
    )


def configurations(fallback=False):
    if fallback:
        return [(3, 4096, False), (2, 4096, False), (3, 2048, False), (3, 4096, True)]
    return (
        [(j, 2048, False) for j in range(3)]
        + [(2, j, False) for j in (512, 1024)]
        + [(2, 2048, True)]
    )


def admitted(checks):
    return {"space", "time", "domain", "mass", "forward"} <= checks.keys() and all(
        len(v) and np.all(np.isfinite(v)) and min(v) >= 0 and max(v) <= 2.5e-5
        for v in checks.values()
    )
