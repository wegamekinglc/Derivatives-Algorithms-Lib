"""Independent Crank–Nicolson rollback for discrete-exercise put references."""

import math

import numpy as np


def tridiagonal_factors(lower, diagonal, upper):
    inverse = np.empty_like(diagonal)
    factors = np.empty_like(lower)
    inverse[0] = 1.0 / diagonal[0]
    for index in range(1, len(diagonal)):
        factors[index - 1] = lower[index - 1] * inverse[index - 1]
        inverse[index] = 1.0 / (diagonal[index] - factors[index - 1] * upper[index - 1])
    return factors, inverse


def solve_tridiagonal(factors, inverse, upper, rhs):
    values = rhs.copy()
    for index in range(1, len(values)):
        values[index] -= factors[index - 1] * values[index - 1]
    values[-1] *= inverse[-1]
    for index in range(len(values) - 2, -1, -1):
        values[index] = (values[index] - upper[index] * values[index + 1]) * inverse[
            index
        ]
    return values


def put_surface(
    exercise_times,
    grid_points,
    interval_steps,
    vol,
    rate,
    strike=100.0,
    dividend=0.0,
):
    grid = np.linspace(0.0, 4.0 * strike, grid_points)
    width = grid[1] - grid[0]
    inner = grid[1:-1]
    lower = 0.5 * vol**2 * inner**2 / width**2 - (rate - dividend) * inner / (
        2.0 * width
    )
    center = -(vol**2) * inner**2 / width**2 - rate
    upper = 0.5 * vol**2 * inner**2 / width**2 + (rate - dividend) * inner / (
        2.0 * width
    )
    payoff = np.maximum(strike - grid, 0.0)
    values = payoff.copy()
    boundaries = (0.0, *exercise_times)

    for interval in range(len(exercise_times) - 1, -1, -1):
        left, right = boundaries[interval : interval + 2]
        step = (right - left) / interval_steps[interval]
        lhs_lower = -0.5 * step * lower[1:]
        lhs_center = 1.0 - 0.5 * step * center
        lhs_upper = -0.5 * step * upper[:-1]
        factors, inverse = tridiagonal_factors(lhs_lower, lhs_center, lhs_upper)
        for step_index in range(interval_steps[interval]):
            time_new = right - step_index * step
            time_old = time_new - step
            boundary_new = strike * math.exp(-rate * (right - time_new))
            boundary_old = strike * math.exp(-rate * (right - time_old))
            rhs = (1.0 + 0.5 * step * center) * values[1:-1]
            rhs[1:] += 0.5 * step * lower[1:] * values[1:-2]
            rhs[:-1] += 0.5 * step * upper[:-1] * values[2:-1]
            rhs[0] += 0.5 * step * lower[0] * (boundary_new + boundary_old)
            values[1:-1] = solve_tridiagonal(factors, inverse, lhs_upper, rhs)
            values[0] = boundary_old
            values[-1] = 0.0
        if left > 0.0:
            values = np.maximum(values, payoff)

    return grid, values
