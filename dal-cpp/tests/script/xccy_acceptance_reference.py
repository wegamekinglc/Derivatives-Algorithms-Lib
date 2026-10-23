"""DAL-425 independent dated cashflows; run after freezing this file's SHA256.

The financial oracle never reads production moment terms. Only wide diagnostic
replay uses those terms. mpmath 1.3.0 is the sole nonstandard dependency.
"""

import argparse
import datetime as dt
import functools
import hashlib
import json
from pathlib import Path

import mpmath as mp

POLICY = {
    "schema": "dal.xccy-independent-acceptance/1",
    "digits": [45, 65, 85],
    "quadrature_targets": ["1e-30", "1e-45", "1e-60"],
    "subdivisions": [1, 2, 4],
    "wide_digits": [50, 80],
    "deterministic_budget": ["1e-10", "1e-10", "1e-10", "1e-12"],
    "epsilon_det": ["1e-11", "1e-11", "1e-11", "1e-13"],
    "seed": 1024,
    "ranges": {"Fit": [0, 262144], "Validation": [262144, 1310720], "HeldOut": [1310720, 2359296]},
    "batches": 16,
    "paths_per_batch": {"Fit": 4096, "Validation": 16384, "HeldOut": 16384},
    "prefix_factors": [1, 2, 4],
    "price_scale": ".01",
    "spread_scale": "1e-4",
    "k": 3,
    "nu": ".25",
    "fd_relative_step": ".001",
    "gradient_epsilon": "1e-8",
}
EPOCH = dt.date(2026, 10, 21)
START, RESET, END, PAYMENT, FIX1, FIX2 = (dt.date.fromisoformat(s) for s in (
    "2027-05-04", "2027-08-04", "2027-11-04", "2027-11-08", "2027-04-30", "2027-08-02"))


def day(value):
    return (dt.date.fromisoformat(value) - EPOCH).days


def number(value):
    # Decode the exact binary64 captured input, rather than its decimal rounding.
    return mp.mpf(value) if isinstance(value, (float, int)) else mp.mpf(str(value))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def h(a, t):
    return -mp.expm1(-a * t) / a if a else t


@functools.lru_cache(None)
def kernel_integral(a_values, i, ti, j, tj, lo, hi, digits, subdivisions, closed):
    ad, af = map(number, a_values)
    times = [number(ti) / 365, number(tj) / 365]
    left, right = number(lo) / 365, number(hi) / 365

    def kernel(slot, maturity, u):
        a = ad if slot in (0, 2) else af
        if slot == 4:
            return mp.mpf(1)
        return mp.exp(-a * (maturity - u)) if slot < 2 else h(a, maturity - u)

    if not closed:
        points = [left + (right - left) * k / subdivisions for k in range(subdivisions + 1)]
        result, error = mp.quad(lambda u: kernel(i, times[0], u) * kernel(j, times[1], u), points, error=True)
        target = number(POLICY["quadrature_targets"][POLICY["digits"].index(digits)])
        require(error <= target * max(1, abs(result)), "kernel quadrature target")
        return result

    def expansion(slot, maturity):
        a = ad if slot in (0, 2) else af
        if slot == 4:
            return [(mp.mpf(1), 0, mp.mpf(0))]
        if slot < 2:
            return [(mp.exp(-a * maturity), 0, a)]
        if not a:
            return [(maturity, 0, mp.mpf(0)), (-mp.mpf(1), 1, mp.mpf(0))]
        return [(1 / a, 0, mp.mpf(0)), (-mp.exp(-a * maturity) / a, 0, a)]

    def exp_integral(exponent):
        return (right - left) * mp.exp(exponent * left) * mp.expm1(exponent * (right - left)) / (exponent * (right - left)) if exponent else right - left

    result = mp.mpf(0)
    for c, n, x in expansion(i, times[0]):
        for d, m, y in expansion(j, times[1]):
            power, exponent = n + m, x + y
            integral = ((right ** (power + 1) - left ** (power + 1)) / (power + 1) if not exponent
                        else mp.diff(exp_integral, exponent, power))
            result += c * d * integral
    return result


class BrownianCashflows:
    def __init__(self, case, digits, subdivisions=1, closed=False, label="", bump=0, mutant=""):
        self.case = case
        self.data = case["model"]
        self.valuation = day(self.data["valuation"])
        self.digits, self.subdivisions, self.closed = digits, subdivisions, closed
        self.mutant = mutant
        self.a = tuple(self.data["a"])
        self.ends = tuple(map(day, self.data["bucket_dates"]))
        self.sigma = [[number(x) for x in row] for row in self.data["sigma"]]
        self.r = [[number(x) for x in row] for row in self.data["correlation"]]
        self.curves = {}
        for curve in self.data["curves"]:
            values = [number(v) + (bump if key == label else 0) for key, v in zip(curve["labels"], curve["values"])]
            logs = tuple(mp.fsum(number(w) * v for w, v in zip(row, values)) for row in curve["jacobian"])
            self.curves[curve["currency"], curve["route"]] = tuple(map(day, curve["dates"])), logs
        self.spot = number(self.data["spot"]) + (bump if label == "fxspot:EUR/USD" else 0)
        for driver, name in enumerate(("USD", "EUR", "EUR/USD")):
            for bucket, date in enumerate(self.data["bucket_dates"]):
                if label == f"sigma:{name}:{date}":
                    self.sigma[driver][bucket] += bump

    @functools.lru_cache(None)
    def covariance(self, i, ti, j, tj, start=None):
        start = self.valuation if start is None else start
        di, dj = (0, 1, 0, 1, 2)[i], (0, 1, 0, 1, 2)[j]
        result, previous = mp.mpf(0), self.valuation
        for bucket, boundary in enumerate(self.ends):
            lo, hi = max(start, previous), min(ti, tj, boundary)
            if hi > lo:
                integral = kernel_integral(self.a, i, ti, j, tj, lo, hi, self.digits, self.subdivisions, self.closed)
                result += self.sigma[di][bucket] * self.sigma[dj][bucket] * self.r[di][dj] * integral
            previous = max(self.valuation, boundary)
        return result

    @functools.lru_cache(None)
    def mean(self, slot, date):
        if slot not in (1, 3) or self.mutant == "no-quanto":
            return mp.mpf(0)
        result, previous = mp.mpf(0), self.valuation
        for bucket, boundary in enumerate(self.ends):
            hi = min(date, boundary)
            if hi > previous:
                integral = kernel_integral(self.a, slot, date, 4, date, previous, hi, self.digits, self.subdivisions, self.closed)
                result -= self.r[1][2] * self.sigma[1][bucket] * self.sigma[2][bucket] * integral
            previous = max(self.valuation, boundary)
        return result

    def logdf(self, currency, route, date):
        dates, values = self.curves[currency, route]
        require(dates[0] <= date <= dates[-1], "independent curve coverage")
        for i in range(1, len(dates)):
            if date <= dates[i]:
                weight = mp.mpf(date - dates[i - 1]) / (dates[i] - dates[i - 1])
                return values[i - 1] * (1 - weight) + values[i] * weight
        raise ValueError("curve interpolation")

    def effective(self, foreign, date):
        if not foreign:
            return self.logdf("USD", "OIS", date)
        sign = 1 if self.mutant == "multiply-basis" else -1
        return self.logdf("EUR", "OIS", date) + sign * self.logdf("USD", "BASIS:EUR/USD", date)

    def phi(self, foreign, date):
        slot = 3 if foreign else 2
        return -self.effective(foreign, date) + self.covariance(slot, date, slot, date) / 2

    def discount(self, date):
        return -self.phi(False, date), {(2, date): -mp.mpf(1)}

    def fx(self, date):
        constant = mp.log(self.spot) + self.phi(False, date) - self.phi(True, date) - self.covariance(4, date, 4, date) / 2
        return constant, {(2, date): mp.mpf(1), (3, date): -mp.mpf(1), (4, date): mp.mpf(1)}

    def growth(self, foreign, fix, start, end):
        slot = int(foreign)
        a = number(self.a[slot])
        loading = h(a, mp.mpf(end - fix) / 365) - h(a, mp.mpf(start - fix) / 365)
        currency, route = ("EUR", "6M") if foreign else ("USD", "3M")
        q = (self.logdf(currency, route, start) - self.effective(foreign, start)
             - self.logdf(currency, route, end) + self.effective(foreign, end))
        constant = self.phi(foreign, end) - self.phi(foreign, start) + q
        constant += (self.covariance(slot + 2, start, slot + 2, start, fix) - self.covariance(slot + 2, end, slot + 2, end, fix)) / 2
        return constant, {(slot, fix): loading}

    def expectation(self, exponent):
        constant, loadings = exponent
        linear = mp.fsum(w * self.mean(slot, date) for (slot, date), w in loadings.items())
        variance = mp.fsum(w * v * self.covariance(i, ti, j, tj) for (i, ti), w in loadings.items() for (j, tj), v in loadings.items())
        return mp.exp(constant + linear + variance / 2)

    def product(self, *exponents):
        loadings = {}
        for _, row in exponents:
            for key, value in row.items():
                loadings[key] = loadings.get(key, mp.mpf(0)) + value
        return mp.fsum(x[0] for x in exponents), loadings

    def coefficient_values(self, a, b):
        return [a, b, b + number(.001) * a, -b / a if a else None]

    def financial(self):
        start, reset, end, payment, fix1, fix2 = ((date - EPOCH).days for date in (START, RESET, END, PAYMENT, FIX1, FIX2))
        if self.case["scenario"] == "no-lag":
            payment = end
        foreign_spread, receive = self.case["foreign_spread"], self.case["receive"]
        sign = (1 if foreign_spread else -1) * (1 if receive else -1)
        foreign_n = number(110.) * number(100. / 110.)
        reset_fx = ((mp.log(number(1.17)), {}) if self.case["scenario"] == "history" and reset <= self.valuation else self.fx(reset)) if end >= self.valuation else (mp.mpf(0), {})
        later = (mp.log(number(110.)), {}) if self.case["mode"] == "FIXED" else self.product((mp.log(foreign_n), {}), reset_fx)
        first = mp.log(number(110.)), {}
        a, b = mp.mpf(0), mp.mpf(0)
        for foreign, fix, begin, finish, pay, n in (
            (False, fix1, start, reset, reset, first),
            (False, fix2, reset, end, end, later),
            (True, fix1, start, end, payment, (mp.log(foreign_n), {})),
        ):
            if pay < self.valuation:
                continue
            weight = sign * (-1 if foreign else 1)
            base = self.product(n, self.discount(pay), self.fx(pay) if foreign else (mp.mpf(0), {}))
            known = .029 if foreign else (.031 if fix == fix1 else .037)
            growth = ((mp.log1p(number((finish - begin) / 360.) * number(known)), {})
                      if self.case["scenario"] == "history" and fix <= self.valuation else self.growth(foreign, fix, begin, finish))
            joint = self.expectation(self.product(base, growth))
            if self.mutant == "no-lag-covariance" and foreign:
                q = growth[1]
                wp = self.product(self.fx(pay), self.discount(pay))[1]
                wt = self.product(self.fx(finish), self.discount(finish))[1]
                cross = lambda x: mp.fsum(v * w * self.covariance(i, ti, j, tj) for (i, ti), v in q.items() for (j, tj), w in x.items())
                joint *= mp.exp(cross(wt) - cross(wp))
            base_value = self.expectation(base)
            b += weight * (joint - base_value)
            if foreign == foreign_spread:
                a += weight * number((finish - begin) / 360.) * base_value
        if start >= self.valuation:
            b += sign * (-number(110.) * self.expectation(self.discount(start))
                         + foreign_n * self.expectation(self.product(self.fx(start), self.discount(start))))
        if end >= self.valuation:
            b += sign * (self.expectation(self.product(later, self.discount(end)))
                         - foreign_n * self.expectation(self.product(self.fx(end), self.discount(end))))
        if self.case["mode"] == "MARK_TO_MARKET" and reset >= self.valuation:
            b += sign * (self.expectation(self.product(later, self.discount(reset))) - number(110.) * self.expectation(self.discount(reset)))
        return self.coefficient_values(a, b)

    def replay(self):
        a, b = mp.mpf(0), mp.mpf(0)
        for term in self.case["terms"]:
            date = day(term["payment"])
            exponent = self.discount(date)
            if term["foreign_payment"]:
                exponent = self.product(exponent, self.fx(date))
            if term["reset"]:
                exponent = self.product(exponent, self.fx(day(term["reset"])))
            if term["growth"]:
                p = term["growth"][0]
                exponent = self.product(exponent, self.growth(term["foreign_rate"], day(p["fixing"]), day(p["start"]), day(p["end"])))
            value = number(term["weight"]) * self.expectation(exponent)
            if term["annuity"]:
                a += value
            else:
                b += value
        return self.coefficient_values(a, b)


def qualify_calendar(case):
    # This synthetic fixture uses empty holiday centers and skips weekends.
    require(case["calendars"] == ["", "", "", ""], "frozen empty fixing/payment holiday centers")
    def preceding_business_days(date, count):
        while count:
            date -= dt.timedelta(days=1)
            if date.weekday() < 5:
                count -= 1
        return date

    def period(start, end, foreign):
        pay = end if not foreign or case["scenario"] == "no-lag" else end + dt.timedelta(days=4)
        return {"start": start.isoformat(), "end": end.isoformat(), "payment": pay.isoformat(),
                "fixing": preceding_business_days(start, 2).isoformat(),
                "index": "IR:EUR,LIBOR_6M_LCH" if foreign else "IR:USD,LIBOR_3M_LCH", "alpha": (end - start).days / 360}

    require(case["domestic_periods"] == [period(START, RESET, False), period(RESET, END, False)], "independent USD calendar/index table")
    require(case["foreign_periods"] == [period(START, END, True)], "independent EUR calendar/index table")
    require(FIX1 == preceding_business_days(START, 2) and FIX2 == preceding_business_days(RESET, 2), "independent fixing reference")
    require(case["dimension"] == len(case["coordinates"]) and case["dimension"] % 5 == 0, "five innovation dimensions")


def derivative_values(case, digits, subdivisions, closed, label, step):
    fn = lambda bump: (BrownianCashflows(case, digits, subdivisions, closed, label, bump).replay() if closed
                       else BrownianCashflows(case, digits, subdivisions, closed, label, bump).financial())
    if closed:
        return [mp.diff(lambda x: fn(x)[i], mp.mpf(0)) for i in range(4)]
    samples = [fn(multiplier * step) for multiplier in (2, 1, -1, -2)]
    return [(-samples[0][i] + 8 * samples[1][i] - 8 * samples[2][i] + samples[3][i]) / (12 * step) for i in range(4)]


def indicators(rows):
    return [4 * max(abs(rows[1][i] - rows[0][i]), abs(rows[2][i] - rows[1][i])) for i in range(4)]


def gate(actual, oracle, e_oracle, wide, epsilon, budget):
    e_fast = 4 * max(abs(number(actual) - wide[0]), abs(wide[0] - wide[1]))
    error = abs(number(actual) - oracle)
    return {"actual": str(actual), "oracle": mp.nstr(oracle, 88), "oracle_indicator": mp.nstr(e_oracle, 12),
            "wide": [mp.nstr(x, 88) for x in wide], "fast_indicator": mp.nstr(e_fast, 12), "error": mp.nstr(error, 12),
            "passed": bool(e_oracle <= budget / 4 and e_fast <= budget / 4 and error <= e_oracle + e_fast + epsilon <= budget)}


def run(captures, gradients):
    results = []
    for case in captures:
        qualify_calendar(case)
        oracle, wide = [], []
        for digits, panels in zip(POLICY["digits"], POLICY["subdivisions"]):
            with mp.workdps(digits):
                oracle.append(BrownianCashflows(case, digits, panels).financial())
        for digits in POLICY["wide_digits"]:
            with mp.workdps(digits):
                wide.append(BrownianCashflows(case, digits, closed=True).replay())
        with mp.workdps(95):
            errors = indicators(oracle)
            gates = [gate(case["fast"][i], oracle[-1][i], errors[i], [row[i] for row in wide], number(POLICY["epsilon_det"][i]),
                          number(POLICY["deterministic_budget"][i])) for i in range(4)]
        entry = {"id": case["id"], "values": gates, "oracle_refinements": [[mp.nstr(x, 88) for x in row] for row in oracle]}
        if gradients and case["id"] in ("flat/MARK_TO_MARKET/domestic/receive", "nonflat/MARK_TO_MARKET/domestic/receive"):
            entry["gradients"] = []
            for index, label in enumerate(case["labels"]):
                scale = mp.mpf(".01") if label.startswith(("fxspot:", "sigma:EUR/USD:")) else mp.mpf(".001")
                step = scale * mp.mpf(".001")
                fd_rows, ad_rows = [], []
                for digits, panels in zip(POLICY["digits"], POLICY["subdivisions"]):
                    with mp.workdps(digits):
                        fd_rows.append([derivative_values(case, digits, panels, False, label, step / divisor) for divisor in (1, 2, 4)])
                for digits in POLICY["wide_digits"]:
                    with mp.workdps(digits):
                        ad_rows.append(derivative_values(case, digits, 1, True, label, step))
                with mp.workdps(95):
                    fd_error = indicators(fd_rows[-1])
                    roundoff = indicators([row[-1] for row in fd_rows])
                    checks = [gate(case["fast_gradient"][i][index], fd_rows[-1][-1][i], fd_error[i] + roundoff[i],
                                   [row[i] for row in ad_rows], number(POLICY["epsilon_det"][i]) / scale,
                                   number(POLICY["deterministic_budget"][i]) / scale) for i in range(4)]
                entry["gradients"].append({"label": label, "scale": str(scale), "h": str(step), "values": checks,
                                           "fd": [[[mp.nstr(x, 88) for x in row] for row in refinement] for refinement in fd_rows]})
        results.append(entry)
        print(case["id"], "PASS" if all(x["passed"] for x in gates) else "FAIL", flush=True)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--gradients", action="store_true")
    parser.add_argument("--header", type=Path)
    args = parser.parse_args()
    captures = json.loads(args.capture.read_text())
    results = run(captures, args.gradients)
    with mp.workdps(85):
        base = next(x for x in captures if x["id"] == "flat/FIXED/domestic/receive")
        positive = BrownianCashflows(base, 85, 4).financial()
        mutants = {name: [mp.nstr(v - p, 40) for v, p in zip(BrownianCashflows(base, 85, 4, mutant=name).financial(), positive)]
                   for name in ("no-quanto", "multiply-basis", "no-lag-covariance")}
        require(all(abs(number(row[2])) > mp.mpf("1e-10") for row in mutants.values()), "negative controls fail the direct Price gate")
    output = {"policy": POLICY, "oracle_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "capture_sha256": hashlib.sha256(args.capture.read_bytes()).hexdigest(), "cases": results, "mutant_price_deltas": mutants}
    args.output.write_text(json.dumps(output, indent=2) + "\n")
    require(all(v["passed"] for case in results for v in case["values"]), "deterministic value acceptance failed; raw result retained")
    require(all(v["passed"] for case in results for gradient in case.get("gradients", []) for v in gradient["values"]),
            "deterministic gradient acceptance failed; raw result retained")
    if args.header:
        args.header.write_text(make_header(results))


def make_header(results):
    lines = ["#pragma once", "", "#include <array>", "", "namespace Dal::TestXccyAcceptance {",
             "    struct ValueReference_ {", "        const char* scenario_;", "        const char* mode_;",
             "        bool foreign_, receive_;", "        std::array<double, 4> values_;", "    };",
             "    inline constexpr std::array<ValueReference_, " + str(len(results)) + "> VALUES{{"]
    for case in results:
        scenario, mode, leg, direction = case["id"].split("/")
        values = ", ".join(format(float(v["oracle"]), ".17g") for v in case["values"])
        lines.append(f'        {{"{scenario}", "{mode}", {str(leg == "foreign").lower()}, {str(direction == "receive").lower()}, {{{{{values}}}}}}},')
    gradients = [(case, gradient) for case in results for gradient in case.get("gradients", [])]
    lines += ["    }};", "    struct GradientReference_ {", "        const char* scenario_;", "        const char* label_;",
              "        double scale_;", "        std::array<double, 4> values_;", "    };",
              "    inline constexpr std::array<GradientReference_, " + str(len(gradients)) + "> GRADIENTS{{"]
    for case, gradient in gradients:
        values = ", ".join(format(float(v["oracle"]), ".17g") for v in gradient["values"])
        lines.append(f'        {{"{case["id"].split("/")[0]}", "{gradient["label"]}", {gradient["scale"]}, {{{{{values}}}}}}},')
    return "\n".join(lines + ["    }};", "} // namespace Dal::TestXccyAcceptance", ""])


if __name__ == "__main__":
    main()
