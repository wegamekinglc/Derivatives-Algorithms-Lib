"""Signed exponential sums from independent dated cashflows and time kernels.

Captured production terms are never read. The accepted independent interval
Brownian model supplies means, covariances and dual Cholesky arithmetic.
"""

from dataclasses import dataclass
from functools import cached_property

import mpmath as mp

from xccy_acceptance_bias_intervals import interval, require
from xccy_acceptance_extended import qualify_schedule
from xccy_acceptance_interval import Dual, FixedCashflows, day, h


@dataclass(frozen=True)
class Affine:
    k: Dual
    ell: tuple

    def __add__(self, other):
        return Affine(self.k + other.k, tuple(a + b for a, b in zip(self.ell, other.ell, strict=True)))

    def scale(self, value):
        return Affine(self.k * value, tuple(x * value for x in self.ell))


def state_rows(previous, length, a):
    hd, hf = (h(x, length) for x in a)
    return [
        [x * mp.iv.exp(-a[0] * length) for x in previous[0]],
        [x * mp.iv.exp(-a[1] * length) for x in previous[1]],
        [x + y * hd for x, y in zip(previous[2], previous[0], strict=True)],
        [x + y * hf for x, y in zip(previous[3], previous[1], strict=True)],
        list(previous[4]),
    ]


class IndependentTerms(FixedCashflows):
    def constant(self, value=0):
        return Affine(Dual.of(value), tuple(Dual() for _ in range(self.case["dimension"])))

    @cached_property
    def affine_states(self):
        dimension = self.case["dimension"]
        zero_means = self.states([0] * dimension)
        previous = [[Dual() for _ in range(dimension)] for _ in range(5)]
        result = {0: [self.constant() for _ in range(5)]}
        for step, (length, lower, _) in enumerate(self.transitions):
            previous = state_rows(previous, length, self.a)
            for i in range(5):
                for j in range(i + 1):
                    previous[i][5 * step + j] += lower[i][j]
            date = self.grid[step + 1]
            result[date] = [Affine(zero_means[date][i], tuple(previous[i])) for i in range(5)]
        return result

    def discount_exponent(self, date):
        return self.constant(-self.phi(False, date)) + self.affine_states[date][2].scale(-1)

    def fx_exponent(self, date):
        value = self.constant(self.phi(False, date) - self.phi(True, date) - self.variance(2, 0, date) / 2)
        for slot, sign in ((2, 1), (3, -1), (4, 1)):
            value += self.affine_states[date][slot].scale(sign)
        return value

    def growth_exponent(self, period, foreign):
        fixing, start, end = (day(period[key]) for key in ("fixing", "start", "end"))
        currency, route = ("EUR", "6M") if foreign else ("USD", "3M")

        def projection(date):
            return self.logdf(currency, route, date) - self.effective(foreign, date)

        loading = h(self.a[int(foreign)], interval(end - fixing) / 365) - h(
            self.a[int(foreign)], interval(start - fixing) / 365
        )
        constant = self.phi(foreign, end) - self.phi(foreign, start) + projection(start) - projection(end)
        constant += (self.variance(int(foreign), fixing, start) - self.variance(int(foreign), fixing, end)) / 2
        return self.constant(constant) + self.affine_states[fixing][int(foreign)].scale(loading)

    def notional(self, index, domestic):
        if index == 0:
            return Dual(110), self.constant()
        return Dual(110 * interval(100.0 / 110.0)) * self.spot, self.fx_exponent(day(domestic[index]["start"]))

    def coupons(self, domestic, foreign):
        terms = []
        for i, period in enumerate(domestic):
            coefficient, exponent = self.notional(i, domestic)
            exponent += self.discount_exponent(day(period["payment"]))
            terms.extend(self.coupon_terms(period, False, -coefficient, exponent))
        foreign_n = Dual(110 * interval(100.0 / 110.0)) * self.spot
        for period in foreign:
            date = day(period["payment"])
            exponent = self.discount_exponent(date) + self.fx_exponent(date)
            terms.extend(self.coupon_terms(period, True, foreign_n, exponent))
        return terms

    def coupon_terms(self, period, foreign, coefficient, exponent):
        terms = [("B", coefficient, exponent + self.growth_exponent(period, foreign)), ("B", -coefficient, exponent)]
        if not foreign:
            terms.append(("A", coefficient * interval(period["alpha"]), exponent))
        return terms

    def principal(self, domestic):
        start, end = day(domestic[0]["start"]), day(domestic[-1]["end"])
        coefficient, last = self.notional(len(domestic) - 1, domestic)
        foreign_n = Dual(110 * interval(100.0 / 110.0)) * self.spot
        return [
            ("B", Dual(110), self.discount_exponent(start)),
            ("B", -foreign_n, self.discount_exponent(start) + self.fx_exponent(start)),
            ("B", -coefficient, self.discount_exponent(end) + last),
            ("B", foreign_n, self.discount_exponent(end) + self.fx_exponent(end)),
        ]

    def mtm(self, domestic):
        terms = []
        for i in range(1, len(domestic)):
            date = day(domestic[i]["start"])
            discount = self.discount_exponent(date)
            new_c, new_e = self.notional(i, domestic)
            old_c, old_e = self.notional(i - 1, domestic)
            terms.extend((("B", -new_c, discount + new_e), ("B", old_c, discount + old_e)))
        return terms

    def terms(self):
        domestic, foreign = qualify_schedule(self.case)
        return self.coupons(domestic, foreign) + self.principal(domestic) + self.mtm(domestic)


def native_axes(model):
    curves = [label for curve in model["curves"] for label in curve["labels"]]
    sigmas = [f"sigma:{name}:{date}" for name in ("USD", "EUR", "EUR/USD") for date in model["bucket_dates"]]
    return ["fxspot:EUR/USD"] + curves + sigmas


def admit_curves(model):
    routes = [(curve["currency"], curve["route"]) for curve in model["curves"]]
    require(
        routes == [("USD", "OIS"), ("EUR", "OIS"), ("USD", "BASIS:EUR/USD"), ("USD", "3M"), ("EUR", "6M")],
        "frozen curve routes",
    )
    for curve in model["curves"]:
        width = len(curve["labels"])
        jacobian = [[0] * width] + [[int(i == j) for j in range(width)] for i in range(width)]
        require(curve["jacobian"] == jacobian, "frozen identity native Jacobian")
        require(curve["dates"][0] == model["valuation"] and curve["logdf"][0] == 0, "curve origin")
        require(curve["values"] == curve["logdf"][1:] and len(curve["dates"]) == width + 1, "curve native coordinates")


def admit_grid(case):
    domestic, foreign = qualify_schedule(case)
    dates = {period[key] for period in domestic + foreign for key in ("fixing", "start", "end", "payment")}
    require(case["grid"] == sorted(dates | {case["model"]["valuation"]}), "independent full event time grid")
    slots = ("xd", "xf", "Id", "If", "Z")
    coordinates = [
        f"interval:{left}:{right}:innovation:{slot}"
        for left, right in zip(case["grid"], case["grid"][1:])
        for slot in slots
    ]
    require(case["coordinates"] == coordinates, "ordered five-slot coordinate identities")


def admit_case(case):
    model = case["model"]
    require(
        (case["mode"], case["scenario"], case["receive"], case["foreign_spread"], case["payment_lag"])
        == ("MARK_TO_MARKET", "staged", True, False, 2),
        "frozen financial domain",
    )
    require(case["labels"] == native_axes(model) and len(case["labels"]) == 11, "eleven ordered native axes")
    require(
        (model["a"], model["sigma"], model["spot"], model["valuation"])
        == ([0.03, 0.07], [[0.01], [0.009], [0.12]], 1.1, "2026-10-21"),
        "frozen scalar model inputs",
    )
    require(model["correlation"] == [[1, 0.3, -0.2], [0.3, 1, -0.25], [-0.2, -0.25, 1]], "frozen correlation")
    require(
        len(model["bucket_dates"]) == 1 and case["grid"][-1] <= model["bucket_dates"][0],
        "single covered volatility bucket",
    )
    require(case["dimension"] == 5 * (len(case["grid"]) - 1), "five-slot grid dimension")
    admit_curves(model)
    admit_grid(case)


def term_arguments(term):
    _, c, exponent = term
    return (
        c.value,
        exponent.k.value,
        tuple(x.value for x in exponent.ell),
        c.derivative,
        exponent.k.derivative,
        tuple(x.derivative for x in exponent.ell),
    )
