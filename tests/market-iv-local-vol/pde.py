"""Independent log-spot backward CN with two initial half-step implicit solves."""

from dataclasses import dataclass

import numpy as np
from scipy.linalg import solve_banded

from reference import DISCOUNTS, FORWARDS, TERM_DAYS, theta_at, variance_numpy


@dataclass(frozen=True)
class Coefficients:
    times: tuple
    theta: tuple
    slopes: tuple
    rho: float = 0
    eta: float = 0
    flat: bool = False

    def logs(self, time):
        if self.flat:
            return 0.02 * time, -0.03 * time, 0.02, 0.03
        times = np.array(TERM_DAYS) / 365
        index = np.clip(
            np.searchsorted(times, time, side="right") - 1, 0, len(times) - 2
        )
        forwards = np.log(np.array(FORWARDS, dtype=float) / 100)
        discounts = np.log(np.array(DISCOUNTS, dtype=float))
        dl = (forwards[index + 1] - forwards[index]) / (times[index + 1] - times[index])
        dh = (discounts[index + 1] - discounts[index]) / (
            times[index + 1] - times[index]
        )
        return (
            forwards[index] + dl * (time - times[index]),
            discounts[index] + dh * (time - times[index]),
            dl,
            -dh,
        )

    def variance(self, time, log_spots):
        theta, slope = theta_at(time, self.times, self.theta, self.slopes)
        return variance_numpy(
            theta, slope, self.rho, self.eta, log_spots - self.logs(time)[0]
        )


@dataclass(frozen=True)
class GridCoefficients:
    continuous: Coefficients
    log_spots: np.ndarray
    times: np.ndarray
    vols: np.ndarray

    def logs(self, time):
        return self.continuous.logs(time)

    def variance(self, time, log_spots):
        column = np.clip(
            np.searchsorted(self.times, time, side="right") - 1, 0, len(self.times) - 2
        )
        weight = np.clip(
            (time - self.times[column]) / (self.times[column + 1] - self.times[column]),
            0,
            1,
        )
        vols = self.vols[:, column] * (1 - weight) + self.vols[:, column + 1] * weight
        return np.interp(log_spots, self.log_spots, vols) ** 2


def timeline(maturity, steps, extra=()):
    anchors = sorted(
        {
            0.0,
            maturity,
            *(day / 365 for day in TERM_DAYS if 0 < day / 365 < maturity),
            *(time for time in extra if 0 < time < maturity),
        }
    )
    result = [0.0]
    for left, right in zip(anchors, anchors[1:]):
        count = int(np.ceil((right - left) * steps / maturity))
        result.extend(np.linspace(left, right, count + 1)[1:])
    return np.asarray(result)


def _boundary(coefficients, time, maturity, spots, strikes):
    logf, logd, *_ = coefficients.logs(time)
    endf, endd, *_ = coefficients.logs(maturity)
    discount = np.exp(endd - logd)
    forwards = spots[:, None] * np.exp(endf - logf)
    calls = discount * np.maximum(forwards - strikes[None, :], 0)
    puts = discount * np.maximum(strikes[None, :] - forwards, 0)
    return np.column_stack(
        (calls, puts, np.full(len(spots), discount), discount * forwards[:, 0])
    )


def _operator(coefficients, time, interior, dx):
    variance = coefficients.variance(time, interior)
    _, _, carry_slope, rate = coefficients.logs(time)
    drift = carry_slope - variance / 2
    lower = variance / (2 * dx * dx) - drift / (2 * dx)
    diagonal = -variance / (dx * dx) - rate
    upper = variance / (2 * dx * dx) + drift / (2 * dx)
    return lower, diagonal, upper


def _step(values, coefficients, lo, hi, weight, x, strikes, maturity):
    dt = hi - lo
    lower, diagonal, upper = _operator(
        coefficients, (lo + hi) / 2, x[1:-1], x[1] - x[0]
    )
    rhs = values[1:-1] + (1 - weight) * dt * (
        lower[:, None] * values[:-2]
        + diagonal[:, None] * values[1:-1]
        + upper[:, None] * values[2:]
    )
    boundaries = _boundary(
        coefficients, lo, maturity, 100 * np.exp(x[[0, -1]]), strikes
    )
    rhs[0] += weight * dt * lower[0] * boundaries[0]
    rhs[-1] += weight * dt * upper[-1] * boundaries[1]
    bands = np.zeros((3, len(x) - 2))
    bands[0, 1:] = -weight * dt * upper[:-1]
    bands[1] = 1 - weight * dt * diagonal
    bands[2, :-1] = -weight * dt * lower[1:]
    result = np.empty_like(values)
    result[[0, -1]] = boundaries
    solution = solve_banded((1, 1), bands, rhs, check_finite=False)
    result[1:-1] = solution
    residual = bands[1, :, None] * solution - rhs
    residual[1:] += bands[2, :-1, None] * solution[:-1]
    residual[:-1] += bands[0, 1:, None] * solution[1:]
    relative_residual = float(np.max(np.abs(residual)) / max(1, np.max(np.abs(rhs))))
    if not np.all(np.isfinite(result)):
        raise ValueError("REFERENCE_UNAVAILABLE: nonfinite PDE step")
    return result, relative_residual


def solve(coefficients, maturity, strikes, nodes=3201, steps=2048, extent=3):
    x = np.linspace(-extent, extent, nodes)
    strikes = np.asarray(strikes, dtype=float)
    spots = 100 * np.exp(x)
    values = np.column_stack(
        (
            np.maximum(spots[:, None] - strikes, 0),
            np.maximum(strikes - spots[:, None], 0),
            np.ones(nodes),
            spots,
        )
    )
    times = timeline(maturity, steps, coefficients.times)
    residuals = []
    for index in range(len(times) - 1, 0, -1):
        lo, hi = times[index - 1 : index + 1]
        if index == len(times) - 1:
            mid = (lo + hi) / 2
            values, residual = _step(
                values, coefficients, mid, hi, 1, x, strikes, maturity
            )
            residuals.append(residual)
            values, residual = _step(
                values, coefficients, lo, mid, 1, x, strikes, maturity
            )
        else:
            values, residual = _step(
                values, coefficients, lo, hi, 0.5, x, strikes, maturity
            )
        residuals.append(residual)
    center = [
        float(np.interp(0, x, values[:, column])) for column in range(values.shape[1])
    ]
    return dict(
        values=center,
        nodes=nodes,
        requested_steps=steps,
        actual_steps=len(times) - 1,
        extent=extent,
        max_dt=float(np.max(np.diff(times))),
        max_relative_linear_residual=max(residuals),
        scheme="CN_MIDPOINT_2_HALF_IE/1",
    )
