"""Check emitted binary doubles using exact rational polynomial extrema."""

import json
import math
import sys
from fractions import Fraction as R
from pathlib import Path


def polynomial_minimum(t0, t1, y0, y1, m0, m1):
    # Exact derivative quadratic in power form; not the production bound algorithm.
    secant = (y1 - y0) / (t1 - t0)
    a = 3 * (m0 + m1 - 2 * secant)
    b = 6 * secant - 4 * m0 - 2 * m1
    candidates = [m0, m1]
    if a > 0 and 0 < -b / (2 * a) < 1:
        u = -b / (2 * a)
        candidates.append(a * u * u + b * u + m0)
    return min(candidates)


def check_curve(row):
    times, ys, slopes, bounds = [list(map(R, row[key])) for key in ("times", "values", "slopes", "bounds")]
    if not row["certified"]:
        return 0
    for i, bound in enumerate(bounds):
        exact = polynomial_minimum(times[i], times[i + 1], ys[i], ys[i + 1], slopes[i], slopes[i + 1])
        assert 0 <= bound <= exact, (row["name"], i, float(bound), float(exact))
    assert slopes[0] >= 0 and slopes[-1] >= 0
    for t, y, dy in row["evaluations"]:
        assert math.isfinite(y) and math.isfinite(dy) and dy >= 0, (row["name"], t)
        time = R(t)
        if time in times:
            i = times.index(time)
            assert R(y) == ys[i] and R(dy) == slopes[i]
        elif time > times[-1]:
            ref = ys[-1] + slopes[-1] * (time - times[-1])
            assert abs(R(y) - ref) <= R(2 * math.ulp(float(ref)))
            assert R(dy) == slopes[-1]
        else:
            i = next(j for j in range(len(times) - 1) if time < times[j + 1])
            h = times[i + 1] - times[i]
            u = (time - times[i]) / h
            ref = ((2 * u**3 - 3 * u**2 + 1) * ys[i] + (-2 * u**3 + 3 * u**2) * ys[i + 1]
                   + h * ((u**3 - 2 * u**2 + u) * slopes[i] + (u**3 - u**2) * slopes[i + 1]))
            secant = (ys[i + 1] - ys[i]) / h
            ref_dy = (3 * (slopes[i] + slopes[i + 1] - 2 * secant) * u**2
                      + (6 * secant - 4 * slopes[i] - 2 * slopes[i + 1]) * u + slopes[i])
            assert abs(R(y) - ref) <= R(2 * math.ulp(float(ref)))
            assert abs(R(dy) - ref_dy) <= R(2 * math.ulp(float(ref_dy)))
    return len(bounds)


records = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines() if line.startswith("{")]
segments, curves, caps, prices, violations = 0, 0, 0, 0, []
for row in records:
    if row["kind"] == "platform":
        print("platform:", row)
    elif row["kind"] == "curve":
        segments += check_curve(row)
        curves += row["certified"]
        if row["name"] in ("decreasing_rejected", "ulp_below"):
            assert not row["certified"]
    elif row["kind"] == "cap":
        eta, rho, margin = map(R, (row["eta"], row["rho_limit"], row["margin"]))
        assert eta**2 * (1 + rho) <= R(row["butterfly_bound"]) <= 4 * (1 - margin)
        assert eta * (1 + rho) / 2 <= R(row["wing_bound"]) < 2
        assert row["accepted"] and not row["eta_up_accepted"] and not row["rho_up_accepted"]
        caps += 1
    elif row["kind"] == "price":
        premium, f, k, d = map(R, (row["premium"], row["forward"], row["strike"], row["discount"]))
        ref = premium / (d * f) if row["call"] else 1 + (premium - d * k) / (d * f)
        assert 0 <= ref <= 1
        if row["actual"] is not None:
            error = abs(R(row["actual"]) - ref)
            if error > R(1e-12):
                violations.append({"name": row["name"], "strike": row["strike"], "actual": row["actual"],
                                   "reference": float(ref), "error": float(error)})
        prices += 1
print(json.dumps({"certified_curves": curves, "certified_segments": segments, "cap_boxes": caps,
                  "price_cases": prices, "price_precision_violations": violations}, indent=2))
raise SystemExit(bool(violations))
