"""Self-certification and actual-grid comparisons under A/1."""

from dataclasses import dataclass

import mpmath as mp
import numpy as np
from a1_pde import admitted, configurations, solve
from grid_study import CertifiedGrid
from pde import Coefficients
from qualify import coefficients
from reference import RATIOS, black, carry


@dataclass(frozen=True)
class Clocked:
    model: object
    times: tuple
    flat: bool = False

    def variance(self, time, spots):
        return self.model.variance(time, spots)

    def logs(self, time):
        return self.model.logs(time)


def canonical_logs(spots):
    with mp.workdps(90):
        return np.array([float(mp.log(mp.mpf(s) / 100)) for s in spots])


def case_strikes(time, flat=False):
    with mp.workdps(90):
        forward = carry(mp.mpf(time), flat)[0]
        return [float(forward * mp.mpf(ratio)) for ratio in RATIOS]


def make_model(fixture, grid):
    result = CertifiedGrid(
        coefficients(fixture["snapshot"], fixture["id"] == "F1_flat"),
        canonical_logs(grid["spots"]),
        np.array(grid["times"]),
        np.array(grid["vols_spot_major"]).reshape(
            len(grid["spots"]), len(grid["times"])
        ),
    )
    return Clocked(result, tuple(grid["times"]), fixture["id"] == "F1_flat")


def black_model(grid, flat):
    result = Coefficients((0.0, 2.0), (0.0, 0.08), (0.04, 0.04), flat=flat)
    return Clocked(result, tuple(grid["times"]), flat)


def task(arguments):
    model, time, strikes, knots, config = arguments
    level, steps, expanded = config
    return solve(model, time, strikes, knots, level, steps, expanded)


def difference(a, b):
    return (np.abs(np.array(a["values"]) - b["values"]) / 100).tolist()


def checks(runs, model, time, strikes, refined=False, black_limit=False):
    level, steps, old_steps = (3, 4096, 2048) if refined else (2, 2048, 1024)
    final = runs[(level, steps, False)]
    forward, discount = map(float, carry(time, model.flat))
    result = dict(
        space=difference(final, runs[(level - 1, steps, False)]),
        time=difference(final, runs[(level, old_steps, False)]),
        domain=difference(final, runs[(level, steps, True)]),
        mass=[abs(final["values"][-2] / discount - 1)],
        forward=[abs(final["values"][-1] / discount - forward) / 100],
    )
    if black_limit:
        with mp.workdps(90):
            f, d = carry(mp.mpf(time), model.flat)
            expected = [
                float(black(f, d, mp.mpf(k), mp.mpf(".04") * time, call))
                for call in (True, False)
                for k in strikes
            ]
        result["black"] = (
            np.abs(np.array(final["values"][:10]) - expected) / 100
        ).tolist()
    return result


def certify(name, model, time, knots, execute, black_limit=False):
    strikes = case_strikes(time, model.flat)
    runs = {}
    for fallback in (False, True):
        settings = configurations(fallback)
        values = execute([(model, time, strikes, knots, c) for c in settings], name)
        runs.update(zip(settings, values))
        residuals = checks(runs, model, time, strikes, fallback, black_limit)
        if admitted(residuals):
            break
    status = "PASS" if admitted(residuals) else "REFERENCE_UNAVAILABLE"
    final = runs[(3, 4096, False) if fallback else (2, 2048, False)]
    return dict(
        id=name,
        maturity=time,
        status=status,
        refined=fallback,
        normalized_differences=residuals,
        levels=list(runs.values()),
        final_values=final["values"],
        strikes=strikes,
    )


def grid_comparison(study, continuous):
    if study["status"] != "PASS" or continuous["status"] != "PASS":
        return dict(status="REFERENCE_UNAVAILABLE", normalized_errors=None)
    old = next(
        r["values"]
        for r in continuous["levels"]
        if (r["nodes"], r["requested_steps"], r["extent"]) == (3201, 2048, 3)
    )
    delta = (np.array(study["final_values"]) - old) / 100
    return dict(
        status="PASS" if np.max(np.abs(delta)) <= 2.5e-4 else "FAIL",
        normalized_errors=delta.tolist(),
        continuous_values=old,
    )
