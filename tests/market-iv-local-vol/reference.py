"""Independent F/D, Black, PCHIP and SSVI references (no DAL imports)."""

from bisect import bisect_right

import mpmath as mp
import numpy as np

TERM_DAYS = (0, 91, 182, 273, 365, 730)
FORWARDS = ("100", "101", "99", "102", "100.5", "104")
DISCOUNTS = ("1", ".995", "1.005", ".998", "1.01", ".97")
QUOTE_DAYS = (91, 182, 365, 730)
RATIOS = (".7", ".85", "1", "1.15", "1.3")


def carry(time, flat=False):
    time = mp.mpf(time)
    if flat:
        return 100 * mp.exp(mp.mpf(".02") * time), mp.exp(-mp.mpf(".03") * time)
    times = [mp.mpf(day) / 365 for day in TERM_DAYS]
    index = min(max(bisect_right(times, time) - 1, 0), len(times) - 2)
    weight = (time - times[index]) / (times[index + 1] - times[index])
    forward = mp.exp(
        mp.log(mp.mpf(FORWARDS[index]))
        + weight * mp.log(mp.mpf(FORWARDS[index + 1]) / mp.mpf(FORWARDS[index]))
    )
    discount = mp.exp(
        mp.log(mp.mpf(DISCOUNTS[index]))
        + weight * mp.log(mp.mpf(DISCOUNTS[index + 1]) / mp.mpf(DISCOUNTS[index]))
    )
    return forward, discount


def black(forward, discount, strike, variance, call=True):
    root = mp.sqrt(variance)
    sign = 1 if call else -1
    d1 = mp.log(forward / strike) / root + root / 2
    d2 = d1 - root

    def cdf(value):
        return mp.erfc(-value / mp.sqrt(2)) / 2

    return discount * sign * (forward * cdf(sign * d1) - strike * cdf(sign * d2))


def _endpoint(h0, h1, d0, d1):
    slope = ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1)
    if slope * d0 <= 0:
        return mp.mpf(0)
    return min(slope, 3 * d0)


def pchip_slopes(times, values):
    widths = [right - left for left, right in zip(times, times[1:])]
    secants = [
        (right - left) / width for left, right, width in zip(values, values[1:], widths)
    ]
    if len(times) == 2:
        return [secants[0], secants[0]]
    slopes = [_endpoint(widths[0], widths[1], secants[0], secants[1])]
    for index in range(1, len(times) - 1):
        left, right = secants[index - 1 : index + 1]
        w1, w2 = (
            2 * widths[index] + widths[index - 1],
            widths[index] + 2 * widths[index - 1],
        )
        slopes.append(
            (w1 + w2) / (w1 / left + w2 / right) if left * right > 0 else mp.mpf(0)
        )
    slopes.append(_endpoint(widths[-1], widths[-2], secants[-1], secants[-2]))
    return slopes


def theta_at(time, times, values, slopes):
    index = min(max(bisect_right(times, time) - 1, 0), len(times) - 1)
    if time == times[index]:
        return values[index], slopes[index]
    if index == len(times) - 1:
        return values[-1] + slopes[-1] * (time - times[-1]), slopes[-1]
    width = times[index + 1] - times[index]
    u = (time - times[index]) / width
    y0, y1 = values[index : index + 2]
    m0, m1 = slopes[index : index + 2]
    a = 2 * y0 - 2 * y1 + width * (m0 + m1)
    b = -3 * y0 + 3 * y1 - width * (2 * m0 + m1)
    return ((a * u + b) * u + width * m0) * u + y0, (
        3 * a * u * u + 2 * b * u
    ) / width + m0


def ssvi(theta, rho, eta, k):
    phi = eta / mp.sqrt(theta * (1 + theta))
    z = phi * k
    return theta * (1 + rho * z + mp.sqrt((z + rho) ** 2 + 1 - rho**2)) / 2


def derivatives(theta, slope, rho, eta, k):
    value = ssvi(theta, rho, eta, k)
    first = mp.diff(lambda money: ssvi(theta, rho, eta, money), k)
    second = mp.diff(lambda money: ssvi(theta, rho, eta, money), k, 2)
    time = slope * mp.diff(lambda level: ssvi(level, rho, eta, k), theta)
    density = (
        (1 - k * first / (2 * value)) ** 2
        - first**2 * (1 / value + mp.mpf(".25")) / 4
        + second / 2
    )
    return value, first, second, time, density


def local_variance(theta, slope, rho, eta, k):
    *_, time, density = derivatives(theta, slope, rho, eta, k)
    return time / density


def variance_numpy(theta, slope, rho, eta, k):
    """Vector PDE coefficient checked against arbitrary precision differentiation."""
    phi = eta / np.sqrt(theta * (1 + theta))
    z = phi * k
    root = np.sqrt(1 + 2 * rho * z + z * z)
    value = theta * (1 + rho * z + root) / 2
    dz = theta * (rho + (rho + z) / root) / 2
    first = dz * phi
    second = theta * phi * phi * (1 - rho * rho) / (2 * root**3)
    dphi = -phi * (1 / theta + 1 / (1 + theta)) / 2
    time = slope * (value / theta + dz * k * dphi)
    density = (
        (1 - k * first / (2 * value)) ** 2
        - first * first * (1 / value + 0.25) / 4
        + second / 2
    )
    result = time / density
    if not np.all(np.isfinite(result)) or np.any(result < 0):
        raise ValueError("REFERENCE_UNAVAILABLE: nonfinite or negative local variance")
    return result
