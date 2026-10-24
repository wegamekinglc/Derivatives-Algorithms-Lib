"""Outward-rounded, fixed-normal cashflows; no production pricing dependencies.

The finite domain is the original MTM schedule with positive a and sigmas.
Binary64 inputs are exact rationals. Elementary Brownian kernel integrals,
unpivoted positive-definite Cholesky and first-order dual arithmetic enclose
the mathematical path value and native derivative. mpmath's interval/libmp
arithmetic is the trusted arithmetic base; this is not a stream-law proof.
"""

import bisect
import datetime as dt
from functools import cached_property, lru_cache

import mpmath as mp


def interval(value):
    if hasattr(value, "_mpi_"):
        return value
    if isinstance(value, float):
        numerator, denominator = value.as_integer_ratio()
        return mp.iv.mpf(numerator) / denominator
    return mp.iv.mpf(value)


def upper_abs(value):
    lo, hi = interval(value)._mpi_
    return max(abs(mp.make_mpf(lo)), abs(mp.make_mpf(hi)))


class Dual:
    def __init__(self, value=0, derivative=0):
        self.value, self.derivative = interval(value), interval(derivative)

    @staticmethod
    def of(other):
        return other if isinstance(other, Dual) else Dual(other)

    def __add__(self, other):
        other = self.of(other)
        return Dual(self.value + other.value, self.derivative + other.derivative)

    __radd__ = __add__

    def __neg__(self):
        return Dual(-self.value, -self.derivative)

    def __sub__(self, other):
        return self + -self.of(other)

    def __rsub__(self, other):
        return self.of(other) - self

    def __mul__(self, other):
        other = self.of(other)
        return Dual(self.value * other.value, self.derivative * other.value + self.value * other.derivative)

    __rmul__ = __mul__

    def __truediv__(self, other):
        other = self.of(other)
        return Dual(self.value / other.value, (self.derivative - self.value / other.value * other.derivative) / other.value)

    def __rtruediv__(self, other):
        return self.of(other) / self


def exp(value):
    value = Dual.of(value)
    result = mp.iv.exp(value.value)
    return Dual(result, result * value.derivative)


def sqrt(value):
    value = Dual.of(value)
    if not value.value.a > 0:
        raise ValueError("strictly positive interval Cholesky pivot")
    result = mp.iv.sqrt(value.value)
    return Dual(result, value.derivative / (2 * result))


def h(a, length):
    return (1 - mp.iv.exp(-a * length)) / a


def exponential_integral(exponent, left, right):
    if exponent == 0:
        return right - left
    return (mp.iv.exp(exponent * right) - mp.iv.exp(exponent * left)) / exponent


def kernel_terms(a, slot):
    if slot == 4:
        return [(interval(1), interval(0))]
    driver = slot % 2
    if slot < 2:
        return [(interval(1), -a[driver])]
    return [(1 / a[driver], interval(0)), (-1 / a[driver], -a[driver])]


def kernel_integral(a, i, j, left, right):
    return sum(c * d * exponential_integral(x + y, left, right) for c, x in kernel_terms(a, i) for d, y in kernel_terms(a, j))


def factor(covariance):
    lower = [[Dual() for _ in range(5)] for _ in range(5)]
    for i in range(5):
        for j in range(i + 1):
            residual = covariance[i][j] - sum(lower[i][k] * lower[j][k] for k in range(j))
            lower[i][j] = sqrt(residual) if i == j else residual / lower[j][j]
    return lower


def day(date):
    return (dt.date.fromisoformat(date) - dt.date(2026, 10, 21)).days


def sigma_values(model, label, bump):
    result = []
    for driver, name in enumerate(("USD", "EUR", "EUR/USD")):
        result.append(
            [
                Dual(x) + (bump if label == f"sigma:{name}:{date}" else 0)
                for date, x in zip(model["bucket_dates"], model["sigma"][driver], strict=True)
            ]
        )
    return result


def positive_mean_reversions(values):
    result = tuple(interval(x) for x in values)
    if any(not a.a > 0 for a in result):
        raise ValueError("certificate domain: strictly positive mean reversions")
    return result


class FixedCashflows:
    def __init__(self, case, label="", shift=0, active=False):
        self.case, self.model = case, case["model"]
        self.grid = tuple(day(date) for date in case["grid"])
        self.ends = tuple(day(date) for date in self.model["bucket_dates"])
        self.a = positive_mean_reversions(self.model["a"])
        self.r = [[interval(x) for x in row] for row in self.model["correlation"]]
        self.label = label
        self.bump = Dual(shift, int(active))
        self.sigmas = sigma_values(self.model, label, self.bump)
        self.spot = Dual(self.model["spot"]) + (self.bump if label == "fxspot:EUR/USD" else 0)

    @cached_property
    def curves(self):
        result = {}
        for curve in self.model["curves"]:
            values = [
                Dual(value) + (self.bump if label == self.label else 0)
                for label, value in zip(curve["labels"], curve["values"], strict=True)
            ]
            logdf = [sum(value * interval(w) for w, value in zip(row, values, strict=True)) for row in curve["jacobian"]]
            result[curve["currency"], curve["route"]] = (tuple(day(x) for x in curve["dates"]), logdf)
        return result

    @lru_cache(None)
    def logdf(self, currency, route, date):
        dates, values = self.curves[currency, route]
        j = bisect.bisect_left(dates, date)
        if dates[j] == date:
            return values[j]
        w = interval(date - dates[j - 1]) / (dates[j] - dates[j - 1])
        return values[j - 1] * (1 - w) + values[j] * w

    @lru_cache(None)
    def effective(self, foreign, date):
        return self.logdf("EUR", "OIS", date) - self.logdf("USD", "BASIS:EUR/USD", date) if foreign else self.logdf("USD", "OIS", date)

    @lru_cache(None)
    def variance(self, driver, start, end):
        result, previous = Dual(), 0
        for bucket, boundary in enumerate(self.ends):
            lo, hi = max(start, previous), min(end, boundary)
            if hi > lo:
                left, right = interval(end - hi) / 365, interval(end - lo) / 365
                k = kernel_integral(self.a, driver + 2, driver + 2, left, right) if driver < 2 else right - left
                result += self.sigmas[driver][bucket] * self.sigmas[driver][bucket] * k
            previous = boundary
        return result

    @lru_cache(None)
    def phi(self, foreign, date):
        return -self.effective(foreign, date) + self.variance(int(foreign), 0, date) / 2

    @cached_property
    def transitions(self):
        drivers = (0, 1, 0, 1, 2)
        result = []
        for left, right in zip(self.grid, self.grid[1:]):
            length = interval(right - left) / 365
            bucket = bisect.bisect_right(self.ends, left)
            c = [
                [
                    self.sigmas[drivers[i]][bucket]
                    * self.sigmas[drivers[j]][bucket]
                    * self.r[drivers[i]][drivers[j]]
                    * kernel_integral(self.a, i, j, interval(0), length)
                    for j in range(5)
                ]
                for i in range(5)
            ]
            q = self.sigmas[1][bucket] * self.sigmas[2][bucket] * -self.r[1][2]
            result.append((length, factor(c), q))
        return result

    def states(self, normals):
        y = [Dual() for _ in range(5)]
        result = {0: y}
        for step, (length, lower, q) in enumerate(self.transitions):
            hd, hf = (h(a, length) for a in self.a)
            mean = [
                y[0] * mp.iv.exp(-self.a[0] * length),
                y[1] * mp.iv.exp(-self.a[1] * length) + q * hf,
                y[2] + y[0] * hd,
                y[3] + y[1] * hf + q * (length - hf) / self.a[1],
                y[4],
            ]
            y = [mean[i] + sum(lower[i][j] * interval(normals[5 * step + j]) for j in range(i + 1)) for i in range(5)]
            result[self.grid[step + 1]] = y
        return result

    def discount(self, states, date):
        return exp(-self.phi(False, date) - states[date][2])

    def fx(self, states, date):
        y = states[date]
        return self.spot * exp(self.phi(False, date) - self.phi(True, date) + y[2] - y[3] - self.variance(2, 0, date) / 2 + y[4])

    def coupon(self, states, foreign, fixing, start, end):
        currency, route = ("EUR", "6M") if foreign else ("USD", "3M")
        projection = lambda date: self.logdf(currency, route, date) - self.effective(foreign, date)
        loading = h(self.a[int(foreign)], interval(end - fixing) / 365) - h(self.a[int(foreign)], interval(start - fixing) / 365)
        exponent = self.phi(foreign, end) - self.phi(foreign, start) + states[fixing][int(foreign)] * loading
        exponent += (self.variance(int(foreign), fixing, start) - self.variance(int(foreign), fixing, end)) / 2
        return exp(exponent + projection(start) - projection(end)) - 1

    def path(self, normals):
        states = self.states(normals)
        start, reset, end, delayed, fix1, fix2 = map(
            day, ("2027-05-04", "2027-08-04", "2027-11-04", "2027-11-08", "2027-04-30", "2027-08-02")
        )
        foreign_n = interval(110) * interval(100.0 / 110.0)
        later = self.fx(states, reset) * foreign_n
        ds, dr, de, dp = (self.discount(states, date) for date in (start, reset, end, delayed))
        b = 110 * self.coupon(states, False, fix1, start, reset) * dr
        b += later * self.coupon(states, False, fix2, reset, end) * de
        b -= self.coupon(states, True, fix1, start, end) * foreign_n * self.fx(states, delayed) * dp
        b += (-110 + self.fx(states, start) * foreign_n) * ds
        b += (later - self.fx(states, end) * foreign_n) * de + (later - 110) * dr
        if self.case["foreign_spread"]:
            a = self.fx(states, delayed) * -foreign_n * interval((end - start) / 360.0) * dp
            return a, b
        a = dr * 110 * interval((reset - start) / 360.0) + later * interval((end - reset) / 360.0) * de
        return -a, -b

    def values(self):
        paths = [self.path(normals) for normals in self.case["normals"]]
        a = sum(p[0] for p in paths) / len(paths)
        b = sum(p[1] for p in paths) / len(paths)
        return [a, b, b + a * interval(0.001), -b / a]
