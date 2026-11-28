# Generate dupirereference.hpp using mpmath 1.3.0 at 48 decimal digits.

from functools import lru_cache

import mpmath as mp


mp.mp.dps = 48
D = mp.mpf

SPOT, STRIKE, RATE, DIVIDEND = D(100), D(105), D(".05"), D(".02")
QUOTES = tuple(map(D, (".208", ".212", ".200", ".204", ".206", ".209")))
QUOTE_STRIKES, QUOTE_TIMES = tuple(map(D, (75, 105, 135))), (D(".4"), D("1.2"))
LOCAL_STRIKES, LOCAL_TIMES = tuple(map(D, range(60, 141, 10))), (D(".5"), D(1))
QUOTE_STEPS = tuple(map(D, (".0008", ".0004", ".0002")))
DIMENSION = len(QUOTES)


def unit(column):
    return tuple(D(i == column) for i in range(DIMENSION))


DIRECTIONS = (unit(3), unit(2), tuple(map(D, (".5", "-.25", "-.5", "1", ".25", "-.5"))))
DX, DT = D(".02"), D(1) / 64
GRID = tuple(-D(".8") + i * DX for i in range(81))
STOCKS = tuple(SPOT * mp.exp(x) for x in GRID)
LOCAL_LOG_SPOTS = tuple(mp.log(k / SPOT) for k in LOCAL_STRIKES)
ROOT_TWO = mp.sqrt(2)


def bracket(value, grid):
    if value <= grid[0]:
        return 0, 0, D(0)
    if value >= grid[-1]:
        return len(grid) - 1, len(grid) - 1, D(0)
    right = next(i for i, knot in enumerate(grid) if knot > value)
    fraction = (value - grid[right - 1]) / (grid[right] - grid[right - 1])
    return right - 1, right, fraction


def blend(left, right, fraction):
    return (1 - fraction) * left + fraction * right


SPACE_BLEND = tuple(bracket(x, LOCAL_LOG_SPOTS) for x in GRID[1:-1])
TIME_BLEND = tuple(bracket(1 - (step + D(".5")) * DT, LOCAL_TIMES) for step in range(64))
TERMINAL = tuple(max(s - STRIKE, D(0)) for s in STOCKS[1:-1])
RIGHT = tuple(STOCKS[-1] * mp.exp(-DIVIDEND * (step + 1) * DT) - STRIKE * mp.exp(-RATE * (step + 1) * DT)
              for step in range(64))


def normal_cdf(x):
    return mp.erfc(-x / ROOT_TWO) / 2


@lru_cache(maxsize=None)
def call_geometry(strike, time):
    forward = SPOT * mp.exp((RATE - DIVIDEND) * time)
    return (bracket(strike, QUOTE_STRIKES), bracket(time, QUOTE_TIMES), mp.log(forward / strike),
            mp.sqrt(time), SPOT * mp.exp(-DIVIDEND * time), strike * mp.exp(-RATE * time))


def call(strike, time, quotes):
    (kl, kr, k), (tl, tr, t), moneyness, root_time, discounted_spot, discounted_strike = call_geometry(strike, time)
    vol = blend(blend(quotes[2 * kl + tl], quotes[2 * kl + tr], t),
                blend(quotes[2 * kr + tl], quotes[2 * kr + tr], t), k)
    stddev = vol * root_time
    d1 = moneyness / stddev + stddev / 2
    return discounted_spot * normal_cdf(d1) - discounted_strike * normal_cdf(d1 - stddev)


def calibrate(quotes):
    workspace = []
    for maturity_index, time in enumerate(LOCAL_TIMES):
        low, high = ((1, 7), (0, 8))[maturity_index]
        column = []
        for strike in LOCAL_STRIKES[low:high + 1]:
            h, k = strike / 10000, time / 10000
            mid = call(strike, time, quotes)
            left, right = call(strike - h, time, quotes), call(strike + h, time, quotes)
            curvature = (right - mid + left - mid) / h**2
            space = (right - left) / (2 * h)
            elapsed = (call(strike, time + k, quotes) - call(strike, time - k, quotes)) / (2 * k)
            numerator = elapsed + DIVIDEND * mid + (RATE - DIVIDEND) * strike * space
            column.append(mp.sqrt(2 * numerator / (strike**2 * curvature)))
        workspace.append([column[0]] * low + column + [column[-1]] * (len(LOCAL_STRIKES) - 1 - high))
    return workspace


@lru_cache(maxsize=None)
def price(quotes):
    local = calibrate(quotes)
    space = [[blend(strikes[lo], strikes[hi], weight) for lo, hi, weight in SPACE_BLEND] for strikes in local]
    old = list(TERMINAL)
    width = len(old)
    alpha, beta = DT / DX**2, DT / (2 * DX)
    for step, (lo, hi, weight) in enumerate(TIME_BLEND):
        lower_scaled, rhs = [D(0)] * width, [D(0)] * width
        # Eliminate from the right and substitute to the right.
        for i in range(width - 1, -1, -1):
            vol = blend(space[lo][i], space[hi][i], weight)
            diffusion = vol**2 / 2
            advection = RATE - DIVIDEND - diffusion
            a = beta * advection - alpha * diffusion
            b = 1 + RATE * DT + 2 * alpha * diffusion
            c = -beta * advection - alpha * diffusion
            if i == width - 1:
                pivot = b
                value = old[i] - c * RIGHT[step]
            else:
                pivot = b - c * lower_scaled[i + 1]
                value = old[i] - c * rhs[i + 1]
            lower_scaled[i] = a / pivot
            rhs[i] = value / pivot
        for i in range(1, width):
            rhs[i] -= lower_scaled[i] * rhs[i - 1]
        old = rhs
    return old[width // 2]


def bumped(direction, step, other=None, other_step=D(0)):
    if other is None:
        return tuple(q + step * v for q, v in zip(QUOTES, direction))
    return tuple(q + step * v + other_step * w for q, v, w in zip(QUOTES, direction, other))


def vega(column, h):
    direction = unit(column)
    return (price(bumped(direction, h)) - price(bumped(direction, -h))) / (2 * h)


def curvature(row, column, h):
    direction, coordinate = DIRECTIONS[row], unit(column)
    if (row, column) == (0, 3):
        return (price(bumped(direction, h)) + price(bumped(direction, -h)) - 2 * price(QUOTES)) / h**2
    pp = price(bumped(direction, h, coordinate, h))
    pm = price(bumped(direction, h, coordinate, -h))
    mp_ = price(bumped(direction, -h, coordinate, h))
    mm = price(bumped(direction, -h, coordinate, -h))
    return (pp - pm - mp_ + mm) / (4 * h**2)


def richardson(coarse, fine):
    return (4 * fine - coarse) / 3


def printable(value):
    return mp.nstr(value, 18, min_fixed=0, max_fixed=0)


def initializer(values):
    return "{" + ", ".join(map(printable, values)) + "}"


def main():
    pv = price(QUOTES)
    base_vols = calibrate(QUOTES)
    vega_values = [richardson(vega(i, QUOTE_STEPS[1]), vega(i, QUOTE_STEPS[2])) for i in range(DIMENSION)]
    products = [[], [], []]
    disagreement = D(0)
    for row, out in enumerate(products):
        for column in range(DIMENSION):
            coarse, medium, fine = (curvature(row, column, h) for h in QUOTE_STEPS)
            extrapolated = richardson(medium, fine)
            disagreement = max(disagreement, abs(richardson(coarse, medium) - extrapolated))
            out.append(extrapolated)

    print("#pragma once\n\n#include <array>\n\nnamespace TapePerfDupire::Reference {")
    print("    // Generated by dupirereference.py with 48-digit arithmetic.")
    print("    inline constexpr double PRICE = " + printable(pv) + ";")
    print("    inline constexpr std::array<double, 6> VEGA" + initializer(vega_values) + ";")
    print("    inline constexpr std::array<std::array<double, 6>, 3> PRODUCTS{{")
    for row in products:
        print("        " + initializer(row) + ",")
    print("    }};")
    print("    inline constexpr double REFINEMENT = " + printable(disagreement) + ";")
    print("    inline constexpr std::array<std::array<double, 9>, 2> BASE_VOLS{{")
    for column in base_vols:
        print("        " + initializer(column) + ",")
    print("    }};")
    print("    struct Scenario_ { int direction_; double signedStep_; double price_; };")
    print("    inline constexpr std::array<Scenario_, 20> SCENARIOS{{")
    scenarios = [(row, sign * h) for h in QUOTE_STEPS for row in range(3) for sign in (-1, 1)]
    scenarios += [(0, -D(".0001")), (0, D(".0001"))]
    for row, h in scenarios:
        print(f"        {{{row}, {printable(h)}, {printable(price(bumped(DIRECTIONS[row], h)))} }},")
    print("    }};")
    print("} // namespace TapePerfDupire::Reference")


if __name__ == "__main__":
    main()
