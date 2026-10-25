"""Signed exponential-polynomial enclosures with a shared annuity anchor."""

from dataclasses import dataclass
from fractions import Fraction
from functools import lru_cache

import mpmath as mp

from xccy_acceptance_bias_intervals import interval, lower, magnitude, require, sum_iv, upper


@dataclass(frozen=True)
class PolynomialTerm:
    k: object
    ell: tuple
    constant: object
    linear: tuple

    def __post_init__(self):
        require(len(self.ell) == len(self.linear), "complete common coordinates")

    def spatial(self, j):
        return PolynomialTerm(
            self.k,
            self.ell,
            self.constant * self.ell[j] + self.linear[j],
            tuple(x * self.ell[j] for x in self.linear),
        )


def affine_box(constant, linear, radius):
    width = interval(radius) * sum_iv(map(magnitude, linear))
    return interval(constant) + mp.iv.mpf([-width.b, width.b])


def intersection(*values):
    lo = max(x.a for x in values)
    hi = min(x.b for x in values)
    require(lo <= hi, "inconsistent outward enclosures")
    return mp.iv.mpf([lo, hi])


def exponent_difference(term, anchor, radius):
    return exponent_factors(term, anchor, radius)[0]


def exponent_factors(term, anchor, radius):
    k, ell = anchor
    return cached_exponent(
        interval(term.k)._mpi_,
        tuple(interval(x)._mpi_ for x in term.ell),
        interval(k)._mpi_,
        tuple(interval(x)._mpi_ for x in ell),
        interval(radius)._mpi_,
        mp.iv.prec,
    )


@lru_cache(maxsize=4096)
def cached_exponent(k, ell, anchor_k, anchor_ell, radius, precision):
    require(mp.iv.prec == precision, "cached interval precision")
    make = mp.iv.make_mpf
    box = affine_box(
        make(k) - make(anchor_k),
        tuple(make(x) - make(y) for x, y in zip(ell, anchor_ell, strict=True)),
        make(radius),
    )
    return box, mp.iv.exp(box), mp.iv.expm1(box)


def unpaired(term, anchor, radius):
    return exponent_factors(term, anchor, radius)[1] * affine_box(term.constant, term.linear, radius)


def pair_enclosure(first, second, anchor, radius):
    ordinary = unpaired(first, anchor, radius) + unpaired(second, anchor, radius)
    return intersection(
        ordinary, factored_pair(first, second, anchor, radius), factored_pair(second, first, anchor, radius)
    )


def factored_pair(first, second, anchor, radius):
    combined = affine_box(
        first.constant + second.constant,
        tuple(x + y for x, y in zip(first.linear, second.linear, strict=True)),
        radius,
    )
    polynomial = affine_box(first.constant, first.linear, radius)
    difference = exponent_factors(first, (second.k, second.ell), radius)[2]
    return exponent_factors(second, anchor, radius)[1] * (combined + polynomial * difference)


def signed_enclosure(terms, anchor, radius):
    pairs = [pair_enclosure(terms[i], terms[i + 1], anchor, radius) for i in range(0, len(terms) - 1, 2)]
    last = [unpaired(terms[-1], anchor, radius)] if len(terms) % 2 else []
    return sum_iv(pairs + last)


def spatial_bound(terms, anchor, radius):
    return upper(
        sum_iv(
            magnitude(signed_enclosure([term.spatial(j) for term in terms], anchor, radius))
            for j in range(len(anchor[1]))
        )
    )


def annuity_anchor(terms):
    require(bool(terms), "nonempty annuity")
    require(all(t.constant.a > 0 and all(x == 0 for x in t.linear) for t in terms), "positive annuity terms")
    n = len(terms)
    k = mp.iv.ln(n) + sum_iv(mp.iv.ln(t.constant) + t.k for t in terms) / n
    ell = tuple(sum_iv(t.ell[j] for t in terms) / n for j in range(len(terms[0].ell)))
    return k, ell


def logarithmic_natives(values, natives):
    return [
        (native.constant / value.constant, tuple(x / value.constant for x in native.linear))
        for value, native in zip(values, natives, strict=True)
    ]


def annuity_controls(values, natives, radius):
    require(len(values) == len(natives), "complete annuity native terms")
    anchor = annuity_anchor(values)
    logarithmic = logarithmic_natives(values, natives)
    u = max((magnitude(affine_box(d, g, radius)) for d, g in logarithmic), key=lambda x: x.b)
    loadings, mixed = [], []
    for j in range(len(anchor[1])):
        loadings.append(max((magnitude(t.ell[j]) for t in values), key=lambda x: x.b))
        candidates = [
            magnitude(affine_box(t.ell[j] * d + g[j], tuple(t.ell[j] * x for x in g), radius))
            for t, (d, g) in zip(values, logarithmic, strict=True)
        ]
        mixed.append(max(candidates, key=lambda x: x.b))
    minimum = lower(sum_iv(t.constant * mp.iv.exp(affine_box(t.k, t.ell, radius)) for t in values))
    require(minimum.a > 0, "strict positive denominator on entire homotopy")
    return dict(anchor=anchor, minimum=minimum, L=upper(sum_iv(loadings)), U=upper(u), W=upper(sum_iv(mixed)))


def quotient_bounds(relative, denominator, error):
    r, rt, v, vt = (relative[key] for key in ("R", "Rt", "V", "Vt"))
    loading, u, w = (denominator[key] for key in ("L", "U", "W"))
    require(denominator["minimum"].a > 0 and interval(error).a >= 0, "positive denominator/error")
    pieces = dict(
        Bt=vt,
        native_B_times_logD=rt * loading,
        moving_B_times_native_D=v * u,
        B_times_mixed_D=r * w,
        double_denominator_movement=2 * r * u * loading,
    )
    return dict(
        value=upper(interval(error) * (v + r * loading)),
        native=upper(interval(error) * sum_iv(pieces.values())),
        native_pieces=pieces,
    )


def coefficient_mean_ratio(a, b, da, db, weights, perturbed_weights):
    require(len(a) == len(b) == len(da) == len(db) == len(weights), "complete common sample coordinates")
    weights = tuple(map(Fraction, weights))
    require(weights == tuple(map(Fraction, perturbed_weights)), "identical mean weights")
    require(all(w >= 0 for w in weights) and sum(weights) == 1, "nonnegative normalized weights")
    w = [interval(x.numerator) / x.denominator for x in weights]
    am, bm, dam, dbm = (
        sum_iv(interval(x) * y for x, y in zip(row, w, strict=True)) for row in (a, b, da, db)
    )
    require(am.a > 0, "positive coefficient mean denominator")
    return bm / am, dbm / am - bm * dam / am**2
