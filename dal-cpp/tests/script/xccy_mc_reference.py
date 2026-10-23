"""Independent fixed-path cashflows and native FD references; requires mpmath."""

import argparse
import bisect
import datetime as dt
import functools
import json
from pathlib import Path

import mpmath as mp


def number(value):
    return mp.mpf(str(value))


class Cashflows:
    def __init__(self, case):
        self.case = case
        self.grid = tuple(number(day) / 365 for day in case["grid"])
        self.ends = tuple(number(day) / 365 for day in case["bucket_days"])
        self.a = tuple(map(number, case["a"]))
        self.r = mp.matrix(case["correlation"])
        self.base_sigma = tuple(tuple(map(number, row)) for row in case["sigma"])
        self.normals = tuple(tuple(map(number, row)) for row in case["normals"])

    def h(self, driver, t):
        a = self.a[driver]
        return -mp.expm1(-a * t) / a if a else t

    @functools.lru_cache(None)
    def kernel_integral(self, i, j, length):
        def kernel(slot, u):
            if slot < 2:
                return mp.exp(-self.a[slot] * u)
            return self.h(slot - 2, u) if slot < 4 else mp.mpf(1)

        return mp.quad(lambda u: kernel(i, u) * kernel(j, u), [0, length / 2, length])

    @functools.lru_cache(None)
    def variance_parts(self, driver, start, end):
        left = mp.mpf(0)
        parts = []
        for boundary in self.ends:
            lo, hi = max(left, start), min(boundary, end)
            if hi <= lo:
                parts.append(mp.mpf(0))
            elif driver == 2:
                parts.append(hi - lo)
            else:
                parts.append(mp.quad(lambda u: self.h(driver, end - u) ** 2, [lo, (lo + hi) / 2, hi]))
            left = boundary
        return tuple(parts)

    def variance(self, sigma, driver, start, end):
        return mp.fsum(v * s * s for v, s in zip(self.variance_parts(driver, start, end), sigma[driver]))

    @functools.lru_cache(None)
    def states(self, sigma):
        drivers = (0, 1, 0, 1, 2)
        transitions = []
        for left, right in zip(self.grid, self.grid[1:]):
            length = right - left
            bucket = bisect.bisect_right(self.ends, left)
            covariance = mp.matrix(5)
            for i in range(5):
                for j in range(5):
                    di, dj = drivers[i], drivers[j]
                    covariance[i, j] = sigma[di][bucket] * sigma[dj][bucket] * self.r[di, dj] * self.kernel_integral(i, j, length)
            lower = mp.cholesky(covariance)
            q = -self.r[1, 2] * sigma[1][bucket] * sigma[2][bucket]
            transitions.append((length, lower, q))
        paths = []
        for normals in self.normals:
            y = mp.matrix(5, 1)
            samples = {self.grid[0]: tuple(y)}
            for step, (length, lower, q) in enumerate(transitions):
                hd, hf = self.h(0, length), self.h(1, length)
                mean = mp.matrix([
                    mp.exp(-self.a[0] * length) * y[0],
                    mp.exp(-self.a[1] * length) * y[1] + q * hf,
                    y[2] + hd * y[0],
                    y[3] + hf * y[1] + q * (length - hf) / self.a[1],
                    y[4],
                ])
                y = mean + lower * mp.matrix(normals[step * 5:step * 5 + 5])
                samples[self.grid[step + 1]] = tuple(y)
            paths.append(samples)
        return tuple(paths)

    def price(self, label="", shift=mp.mpf(0)):
        curves = {}
        for curve in self.case["curves"]:
            values = [number(v) + (shift if name == label else 0) for name, v in zip(curve["labels"], curve["values"])]
            logdf = [mp.fsum(number(w) * v for w, v in zip(row, values)) for row in curve["jacobian"]]
            curves[curve["currency"], curve["route"]] = (tuple(number(day) / 365 for day in curve["days"]), tuple(logdf))
        sigma = [list(row) for row in self.base_sigma]
        for driver, name in enumerate(("USD", "EUR", "EUR/USD")):
            for bucket, date in enumerate(self.case["bucket_dates"]):
                if label == f"sigma:{name}:{date}":
                    sigma[driver][bucket] += shift
        sigma = tuple(map(tuple, sigma))
        spot = number(self.case["spot"]) + (shift if label == "fxspot:EUR/USD" else 0)

        def logdf(currency, route, t):
            dates, values = curves[currency, route]
            upper = bisect.bisect_left(dates, t)
            if dates[upper] == t:
                return values[upper]
            weight = (t - dates[upper - 1]) / (dates[upper] - dates[upper - 1])
            return values[upper - 1] * (1 - weight) + values[upper] * weight

        @functools.lru_cache(None)
        def effective(foreign, t):
            return logdf("EUR", "OIS", t) - logdf("USD", "BASIS:EUR/USD", t) if foreign else logdf("USD", "OIS", t)

        @functools.lru_cache(None)
        def phi(foreign, t):
            return -effective(foreign, t) + self.variance(sigma, int(foreign), mp.mpf(0), t) / 2

        epoch = dt.date(2026, 10, 21)

        def time(year, month, day):
            return mp.mpf((dt.date(year, month, day) - epoch).days) / 365

        start, reset, end, delayed = (time(2027, 5, 4), time(2027, 8, 4), time(2027, 11, 4), time(2027, 11, 8))
        fix1, fix2 = time(2027, 4, 30), time(2027, 8, 2)
        foreign_spread = self.case["foreign_spread"]
        sign = 1 if foreign_spread else -1
        k = number(.001)
        foreign_n = 110 * number(100. / 110.)
        prices = []
        for path in self.states(sigma):
            def discount(t):
                return mp.exp(-phi(False, t) - path[t][2])

            def fx(t):
                x = path[t]
                return spot * mp.exp(phi(False, t) - phi(True, t) + x[2] - x[3] - self.variance(sigma, 2, mp.mpf(0), t) / 2 + x[4])

            def coupon(foreign, fixing, first, last, notional):
                currency, route = ("EUR", "6M") if foreign else ("USD", "3M")
                shift_q = lambda t: logdf(currency, route, t) - effective(foreign, t)
                loading = self.h(int(foreign), last - fixing) - self.h(int(foreign), first - fixing)
                exponent = phi(foreign, last) - phi(foreign, first) + loading * path[fixing][int(foreign)]
                exponent += (self.variance(sigma, int(foreign), fixing, first) - self.variance(sigma, int(foreign), fixing, last)) / 2
                exponent += shift_q(first) - shift_q(last)
                alpha = number(float((last - first) * 365 / 360))
                return notional * (mp.expm1(exponent) + (k * alpha if foreign == foreign_spread else 0))

            later = foreign_n * fx(reset)
            amount = coupon(False, fix1, start, reset, 110) * discount(reset)
            amount += coupon(False, fix2, reset, end, later) * discount(end)
            amount -= coupon(True, fix1, start, end, foreign_n) * fx(delayed) * discount(delayed)
            amount += (-110 + foreign_n * fx(start)) * discount(start)
            amount += (later - foreign_n * fx(end)) * discount(end)
            amount += (later - 110) * discount(reset)
            prices.append(sign * amount)
        return mp.fsum(prices) / len(prices)


def evaluate(case, digits):
    with mp.workdps(digits):
        oracle = Cashflows(case)
        rows = []
        for label in case["labels"]:
            scale = mp.mpf(".01") if label.startswith(("fxspot:", "sigma:EUR/USD:")) else mp.mpf(".001")
            differences = []
            for h in (scale / 1000, scale / 2000):
                differences.append((8 * (oracle.price(label, h) - oracle.price(label, -h)) -
                                    (oracle.price(label, 2 * h) - oracle.price(label, -2 * h))) / (12 * h))
            rows.append([mp.nstr(value, digits) for value in differences])
        return {"price": mp.nstr(oracle.price(), digits), "fd": rows}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("inputs", type=Path)
    parser.add_argument("header", type=Path)
    parser.add_argument("audit", type=Path)
    args = parser.parse_args()
    cases = json.loads(args.inputs.read_text())["cases"]
    results = []
    lines = ["#pragma once", "", "#include <array>", "", "namespace Dal::TestXccyMc {",
             "    struct NativeReference_ { const char* label_; double full_, half_; };"]
    for case in cases:
        coarse, fine = evaluate(case, 50), evaluate(case, 80)
        with mp.workdps(90):
            indicator = 4 * max(abs(mp.mpf(a) - mp.mpf(b)) for ca, cb in zip(coarse["fd"], fine["fd"]) for a, b in zip(ca, cb))
            assert indicator < mp.mpf("1e-30")
            assert abs(mp.mpf(fine["price"]) - number(case["observed_price"])) < mp.mpf("1e-10")
            for observed, row in zip(case["observed_aad"], fine["fd"]):
                assert max(abs(number(observed) - mp.mpf(value)) for value in row) < mp.mpf("1e-8")
        name = case["name"].upper()
        lines.append(f"    inline constexpr double {name}_PRICE = {float(fine['price']):.17g};")
        lines.append(f"    inline constexpr std::array<NativeReference_, {len(case['labels'])}> {name}_NATIVE{{{{")
        lines.extend(f'        {{"{label}", {float(row[0]):.17g}, {float(row[1]):.17g}}},' for label, row in zip(case["labels"], fine["fd"]))
        lines.append("    }};")
        results.append({"name": case["name"], "coarse_digits": 50, "fine_digits": 80,
                        "gradient_precision_indicator": str(indicator), "coarse": coarse, "fine": fine})
    lines.append("} // namespace Dal::TestXccyMc")
    args.header.write_text("\n".join(lines) + "\n")
    args.audit.write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
