"""Missing financial controls, using deterministic probes and Gaussian algebra.

The production source is affine in its joint-normal innovations before exp.
Zero/basis probes extract those affine loadings; a mixed probe checks extraction.
No random paths are sampled. Independent scalar Brownian integrals establish
Qf direction and conditional-forward values; no production transition, lowerer
or factorization supplies the expected financial answer.
"""

import argparse
import hashlib
import json
from pathlib import Path

import mpmath as mp

from xccy_acceptance_reference import BrownianCashflows, day, h, kernel_integral, number, require

SCENARIOS = ("flat", "nonflat", "rho-df", "rho-dx", "rho-fx", "negative-basis")
TOLERANCE = "1e-10"


def at(probe, date):
    rows = [row for row in probe["states"] if day(row["date"]) == date]
    require(len(rows) == 1, "exact financial probe date")
    return rows[0]


def density(row):
    return mp.log(number(row["spot"])) + mp.log(number(row["nf"])) - mp.log(number(row["nd"]))


def gaussian_mean(probes, function):
    base = function(probes[0])
    loadings = [function(row) - base for row in probes[1:]]
    return mp.exp(base + mp.fsum(x * x for x in loadings) / 2), base, loadings


def admit_unconditional(case, dimension):
    probes = case["probes"]
    require(len(probes) == dimension + 2, "full zero/basis/mixed probe set")
    require(probes[0]["normals"] == [0] * dimension, "zero probe")
    for i, row in enumerate(probes[1:-1]):
        require(row["normals"] == [int(i == j) for j in range(dimension)], "ordered exact basis probes")
    mixed = [(1 if i % 2 == 0 else -1) * (i + 1) / 10 for i in range(dimension)]
    require(probes[-1]["normals"] == mixed, "fixed mixed probe")


def admit_state_grid(case, dimension):
    probes = case["probes"]
    for row in probes + case["conditional"]:
        require([x["date"] for x in row["states"]] == case["grid"], "complete state grid")
        require(len(row["normals"]) == dimension, "full normal dimension")
        require(all(len(x["values"]) == 5 for x in row["states"]), "five state slots")


def admit_conditional(case, dimension):
    first = case["grid"].index("2027-10-21") * 5
    require(case["future_first_slot"] == first, "conditional future coordinate boundary")
    expected = [(0.2 if i % 2 == 0 else -0.3) if i < first else 0 for i in range(dimension)]
    conditional = case["conditional"]
    require(len(conditional) == dimension - first + 1 and conditional[0]["normals"] == expected, "conditional base probe")
    for i, row in enumerate(conditional[1:]):
        normal = list(expected)
        normal[first + i] = 1
        require(row["normals"] == normal, "conditional future basis probes")


def admit_probes(case):
    dimension = 5 * (len(case["grid"]) - 1)
    admit_unconditional(case, dimension)
    admit_state_grid(case, dimension)
    admit_conditional(case, dimension)


def reciprocal_controls(case):
    errors = []
    for probe in case["probes"] + case["conditional"]:
        for row in probe["states"]:
            error = abs(number(row["spot"]) * number(row["reciprocal"]) - 1)
            require(error < mp.mpf("1e-12"), "pathwise reciprocal")
            errors.append(error)
    return max(errors)


def tilted_measure(env, slot, date):
    if slot in (1, 3):
        return mp.mpf(0)
    if slot == 4:
        return env.covariance(4, date, 4, date)
    total, previous = mp.mpf(0), env.valuation
    for bucket, boundary in enumerate(env.ends):
        hi = min(date, boundary)
        if hi > previous:
            total += (
                env.r[0][2]
                * env.sigma[0][bucket]
                * env.sigma[2][bucket]
                * kernel_integral(env.a, slot, date, 4, date, previous, hi, env.digits, 1, True)
            )
        previous = boundary
    return total


def qf_control(env, date):
    errors = []
    for slot in range(5):
        tilted = env.mean(slot, date) + env.covariance(slot, date, 4, date)
        expected = tilted_measure(env, slot, date)
        errors.append(abs(tilted - expected))
    require(max(errors) < mp.mpf("1e-60"), "independent Qf Girsanov direction")
    wrong = abs(env.mean(1, date) - env.covariance(1, date, 4, date))
    require(wrong > mp.mpf("1e-12"), "opposite Qf density/drift rejected")
    return dict(error=mp.nstr(max(errors), 90), wrong_direction_error=mp.nstr(wrong, 90))


def production_qf(probes, env, date):
    base = at(probes[0], date)
    density_loadings = [density(at(probe, date)) - density(base) for probe in probes[1:]]
    errors = []
    for slot in range(5):
        mean = number(base["values"][slot])
        loadings = [number(at(probe, date)["values"][slot]) - mean for probe in probes[1:]]
        tilted = mean + mp.fsum(w * v for w, v in zip(loadings, density_loadings, strict=True))
        errors.append(abs(tilted - tilted_measure(env, slot, date)))
    require(max(errors) <= mp.mpf(TOLERANCE), "production states under independent Qf density tilt")
    return mp.nstr(max(errors), 90)


def conditional_quanto(env, start, end):
    total, previous = mp.mpf(0), env.valuation
    for bucket, boundary in enumerate(env.ends):
        lo, hi = max(previous, start), min(boundary, end)
        if hi > lo:
            total -= (
                env.r[1][2]
                * env.sigma[1][bucket]
                * env.sigma[2][bucket]
                * kernel_integral(env.a, 3, end, 4, end, lo, hi, env.digits, 1, True)
            )
        previous = boundary
    return total


def independent_forward(env, state, start=365, end=730):
    xd, xf, id_, if_, z = map(number, state["values"])
    length = mp.mpf(end - start) / 365
    mean_if = if_ + h(number(env.a[1]), length) * xf + conditional_quanto(env, start, end)
    variance = env.covariance(3, end, 3, end, start) + env.covariance(4, end, 4, end, start) - 2 * env.covariance(3, end, 4, end, start)
    weighted = env.spot * mp.exp(
        env.phi(False, start) - env.phi(True, end) + id_ - mean_if + z - env.covariance(4, end, 4, end) / 2 + variance / 2
    )
    pd = mp.exp(
        -(env.phi(False, end) - env.phi(False, start)) - h(number(env.a[0]), length) * xd + env.covariance(2, end, 2, end, start) / 2
    )
    return weighted / pd


def conditional_control(case, env):
    probes = case["conditional"]
    state = at(probes[0], 365)
    function = lambda probe: (
        mp.log(number(at(probe, 730)["spot"])) - mp.log(number(at(probe, 730)["nd"])) + mp.log(number(at(probe, 365)["nd"]))
    )
    weighted, _, _ = gaussian_mean(probes, function)
    forward = weighted / number(state["pd"])
    independent = independent_forward(env, state)
    actual = number(state["forward"])
    require(
        abs(forward - independent) <= mp.mpf(TOLERANCE) and abs(actual - independent) <= mp.mpf(TOLERANCE),
        "conditional domestic-T-forward expectation",
    )
    ordinary, _, _ = gaussian_mean(probes, lambda probe: mp.log(number(at(probe, 730)["spot"])))
    require(abs(ordinary - actual) > mp.mpf("1e-8"), "ordinary Qd FX mean is not forward")
    inverse, _, _ = gaussian_mean(probes, lambda probe: mp.log(number(at(probe, 730)["reciprocal"])))
    require(abs(inverse - 1 / ordinary) > mp.mpf("1e-8"), "mean reciprocal is not reciprocal mean")
    return dict(
        actual=mp.nstr(actual, 90),
        independent=mp.nstr(independent, 90),
        gaussian_probe=mp.nstr(forward, 90),
        ordinary_qd_mean=mp.nstr(ordinary, 90),
        reciprocal_mean_gap=mp.nstr(abs(inverse - 1 / ordinary), 90),
    )


def case_control(case):
    admit_probes(case)
    env = BrownianCashflows({"model": case["model"]}, 85, 1, closed=True)
    probes, mixed = case["probes"][:-1], case["probes"][-1]
    rows = []
    for date in (365, 730):
        value, base, loadings = gaussian_mean(probes, lambda probe: density(at(probe, date)))
        prediction = base + mp.fsum(number(z) * loading for z, loading in zip(mixed["normals"], loadings, strict=True))
        require(abs(prediction - density(at(mixed, date))) <= mp.mpf("1e-12"), "density log-affine mixed probe")
        require(abs(value - env.spot) <= mp.mpf(TOLERANCE), "discounted density martingale")
        rows.append(
            dict(
                days=date,
                density_mean=mp.nstr(value, 90),
                expected_spot=mp.nstr(env.spot, 90),
                qf=qf_control(env, date),
                production_qf_max_error=production_qf(probes, env, date),
            )
        )
    return dict(
        scenario=case["scenario"],
        density=rows,
        reciprocal_max_error=mp.nstr(reciprocal_controls(case), 90),
        conditional=conditional_control(case, env),
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = json.loads(args.capture.read_text())
    require([case["scenario"] for case in data] == list(SCENARIOS), "complete financial scenario set")
    with mp.workdps(100):
        rows = [case_control(case) for case in data]
    args.output.write_text(
        json.dumps(
            dict(
                capture_sha256=hashlib.sha256(args.capture.read_bytes()).hexdigest(), rows=rows, random_paths_sampled=0, mc_acceptance=False
            ),
            indent=2,
        )
        + "\n"
    )
    print("PASS: density/Qf/conditional-forward/pathwise-reciprocal; zero random paths")


if __name__ == "__main__":
    main()
