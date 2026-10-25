"""Exact finite-law admission controls for synthetic mixed-moment obligations."""

import hashlib
import json
from dataclasses import dataclass
from fractions import Fraction
from math import prod


def require(condition, message):
    if not condition:
        raise ValueError(message)


def bounded_rational(value):
    require(isinstance(value, Fraction), "exact rational input")
    require(
        max(abs(value.numerator).bit_length(), value.denominator.bit_length()) <= 128, "rational cost limit"
    )


def digest(value):
    raw = json.dumps(value, sort_keys=True, separators=(",", ":"), default=str).encode()
    return hashlib.sha256(raw).hexdigest()


@dataclass(frozen=True)
class TinyLaw:
    coordinates: tuple
    atoms: tuple

    def validate(self):
        require(0 < len(self.coordinates) <= 4, "synthetic dimension cost limit")
        require(all(isinstance(x, str) and x for x in self.coordinates), "named coordinates")
        require(len(set(self.coordinates)) == len(self.coordinates), "unique ordered coordinates")
        require(0 < len(self.atoms) <= 64, "synthetic atom cost limit")
        for point, weight in self.atoms:
            require(len(point) == len(self.coordinates), "atom dimension")
            require(all(type(x) is int and abs(x) <= 8 for x in point), "integer atom cost limit")
            bounded_rational(weight)
            require(weight > 0, "positive weights")
        require(len({point for point, _ in self.atoms}) == len(self.atoms), "unique atoms")
        require(sum(weight for _, weight in self.atoms) == 1, "normalized weights")

    def identity(self):
        self.validate()
        return digest(("synthetic-exact-discrete/1", self.coordinates, self.atoms))


@dataclass(frozen=True)
class MomentCondition:
    bases: tuple
    direction: tuple
    lower: Fraction
    upper: Fraction

    def validate(self, dimension):
        require(len(self.bases) == dimension, "tilt dimension")
        require(len(self.direction) in (0, dimension), "direction dimension")
        for value in (*self.bases, *self.direction, self.lower, self.upper):
            bounded_rational(value)
        require(all(base > 0 for base in self.bases), "positive exponential bases")
        require(self.lower <= self.upper, "ordered moment envelope")

    def identity(self):
        return (self.bases, self.direction, self.lower, self.upper)


@dataclass(frozen=True)
class MomentRequest:
    law_identity: str
    coordinates: tuple
    conditions: tuple

    def identity(self):
        require(0 < len(self.conditions) <= 64, "nonempty complete moment coverage and cost limit")
        for condition in self.conditions:
            condition.validate(len(self.coordinates))
        return digest((self.law_identity, self.coordinates, [row.identity() for row in self.conditions]))


@dataclass(frozen=True)
class MomentCertificate:
    law_identity: str
    request_identity: str
    moments: tuple


def exact_moment(law, condition):
    law.validate()
    condition.validate(len(law.coordinates))
    result = Fraction(0)
    for point, weight in law.atoms:
        value = prod((base**x for base, x in zip(condition.bases, point, strict=True)), start=Fraction(1))
        if condition.direction:
            value *= sum(g * x for g, x in zip(condition.direction, point, strict=True))
        result += weight * value
    return result


def admit_tiny_law(law, request, certificate):
    require(isinstance(law, TinyLaw), "explicit synthetic law required")
    require(law.coordinates == request.coordinates, "ordered coordinate identity")
    identity = law.identity()
    require(request.law_identity == certificate.law_identity == identity, "law identity")
    require(certificate.request_identity == request.identity(), "request identity")
    require(len(certificate.moments) == len(request.conditions), "complete moment coverage")
    for condition, claimed in zip(request.conditions, certificate.moments, strict=True):
        require(isinstance(claimed, Fraction), "exact certified moment")
        require(claimed == exact_moment(law, condition), "exact mixed-moment replay")
        require(condition.lower <= claimed <= condition.upper, "required moment envelope")
    return dict(scope="synthetic-exact-discrete/1", local_moments_qualified=True, moments=certificate.moments)
