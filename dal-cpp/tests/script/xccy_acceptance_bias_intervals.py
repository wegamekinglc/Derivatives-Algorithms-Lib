"""Outward libmp endpoints and exact, portable financial certificates."""

from fractions import Fraction

import mpmath as mp
from mpmath import libmp

from xccy_acceptance_interval import interval


def require(condition, message):
    if not condition:
        raise ValueError(message)


def upper(value):
    endpoint = interval(value)._mpi_[1]
    return mp.iv.make_mpf((endpoint, endpoint))


def lower(value):
    endpoint = interval(value)._mpi_[0]
    return mp.iv.make_mpf((endpoint, endpoint))


def magnitude(value):
    return upper(abs(interval(value)))


def sum_iv(values):
    return sum(values, interval(0))


def product(values):
    result = interval(1)
    for value in values:
        result *= value
    return result


def tail(value):
    lo, hi = (interval(value) / mp.iv.sqrt(2))._mpi_
    bounds = (libmp.mpf_erfc(hi, mp.iv.prec, "f"), libmp.mpf_erfc(lo, mp.iv.prec, "c"))
    return mp.iv.make_mpf(bounds) / 2


def phi(value):
    return mp.iv.exp(-(interval(value) ** 2) / 2) / mp.iv.sqrt(2 * mp.iv.pi)


def rational(endpoint):
    sign, mantissa, exponent, _ = endpoint
    require(exponent >= -100000 and exponent <= 100000, "finite certificate endpoint")
    value = Fraction(int(mantissa)) * Fraction(2) ** int(exponent)
    return -value if sign else value


def decimal_endpoint(endpoint, upward, places=35):
    value = rational(endpoint) * 10**places
    rounded = -(-value.numerator // value.denominator) if upward else value.numerator // value.denominator
    sign = "-" if rounded < 0 else ""
    digits = str(abs(rounded)).rjust(places + 1, "0")
    return sign + digits[:-places] + "." + digits[-places:]


def encode(value):
    lo, hi = interval(value)._mpi_
    return dict(
        binary_lower=[int(x) for x in lo],
        binary_upper=[int(x) for x in hi],
        decimal_lower=decimal_endpoint(lo, False),
        decimal_upper=decimal_endpoint(hi, True),
    )


def serialize(value):
    if hasattr(value, "_mpi_"):
        return encode(value)
    if isinstance(value, dict):
        return {key: serialize(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [serialize(item) for item in value]
    return value
