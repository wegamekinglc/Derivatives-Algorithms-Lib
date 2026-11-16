"""Certify independent references and report blocked product qualifications honestly."""

import argparse
from concurrent.futures import ProcessPoolExecutor
import hashlib
import json
from pathlib import Path

import mpmath as mp
import numpy as np

from pde import Coefficients, solve
from reference import (
    RATIOS,
    black,
    carry,
    derivatives,
    pchip_slopes,
    ssvi,
    theta_at,
    variance_numpy,
)

MATURITIES = (91 / 365, 1.0, 2.0, 2.25)
PDE_LIMIT = 2.5e-5


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def curve(snapshot):
    times = list(map(mp.mpf, snapshot["times"]))
    values = list(map(mp.mpf, snapshot["theta"]))
    return times, values, pchip_slopes(times, values)


def fitted_price(snapshot, time, strike, flat, call=True):
    theta, _ = theta_at(time, *curve(snapshot))
    forward, discount = carry(time, flat)
    variance = ssvi(
        theta,
        mp.mpf(snapshot["rho"]),
        mp.mpf(snapshot["eta"]),
        mp.log(strike / forward),
    )
    return black(forward, discount, strike, variance, call)


def fit_rows(fixture, capture):
    rows = []
    for quote in fixture["quotes"]:
        time = mp.mpf(quote["days"]) / 365
        expected = mp.mpf(quote["generating_price"])
        actual = fitted_price(
            capture["snapshot"],
            time,
            mp.mpf(quote["strike"]),
            fixture["flat"],
            quote["option"] == "CALL",
        )
        error = abs(actual - expected) / 100
        rows.append(
            dict(
                id=quote["id"],
                estimate=mp.nstr(actual, 90),
                oracle=mp.nstr(expected, 90),
                normalized_error=float(error),
                status=("PASS" if error <= 1e-6 else "FAIL")
                if fixture["strict"]
                else "REPORTED_NO_F2_FIT_THRESHOLD",
            )
        )
    return rows


def derivative_rows(fixture, capture):
    snapshot = capture["snapshot"]
    times, values, slopes = curve(snapshot)
    slope_error = max(abs(a - mp.mpf(b)) for a, b in zip(slopes, snapshot["slopes"]))
    rows = []
    for point in capture["probes"]:
        time, strike = mp.mpf(point["time"]), mp.mpf(point["strike"])
        theta, slope = theta_at(time, times, values, slopes)
        forward, _ = carry(time, fixture["flat"])
        k = mp.log(strike / forward)
        rho, eta = mp.mpf(snapshot["rho"]), mp.mpf(snapshot["eta"])
        w, wk, wkk, wt, density = derivatives(theta, slope, rho, eta, k)
        local = wt / density
        vector = variance_numpy(
            float(theta), float(slope), float(rho), float(eta), np.array([float(k)])
        )[0]
        error = (
            None
            if point["local_variance"] is None
            else float(abs(mp.mpf(point["local_variance"]) - local))
        )
        log_pdf = -((-k / mp.sqrt(w) + mp.sqrt(w) / 2) ** 2) / 2 - mp.log(2 * mp.pi) / 2
        vector_error = float(abs(mp.mpf(vector) - local))
        rows.append(
            dict(
                time=float(time),
                strike=float(strike),
                production_status=point["dupire_status"],
                reference={
                    key: mp.nstr(value, 90)
                    for key, value in zip(
                        (
                            "variance",
                            "variance_k",
                            "variance_kk",
                            "variance_t",
                            "density",
                            "local_variance",
                            "log_black_pdf",
                        ),
                        (w, wk, wkk, wt, density, local, log_pdf),
                    )
                },
                local_variance_error=error,
                vector_coefficient_error=vector_error,
                reference_status="PASS"
                if vector_error <= 2e-10 * max(1, abs(local))
                else "REFERENCE_UNAVAILABLE",
                status="PASS"
                if error is not None and error <= 2e-10 * max(1, abs(local))
                else "FAIL",
            )
        )
    return dict(slope_max_error=float(slope_error), rows=rows)


def coefficients(snapshot, flat):
    with mp.workdps(90):
        times, values, slopes = curve(snapshot)
        return Coefficients(
            tuple(map(float, times)),
            tuple(map(float, values)),
            tuple(map(float, slopes)),
            snapshot["rho"],
            snapshot["eta"],
            flat,
        )


def _study_task(task):
    name, coefficient, maturity, strikes, configuration = task
    result = solve(coefficient, maturity, strikes, **configuration)
    return name, maturity, configuration, result


def configurations():
    return (
        [dict(nodes=nodes, steps=2048, extent=3) for nodes in (801, 1601, 3201)]
        + [dict(nodes=3201, steps=steps, extent=3) for steps in (512, 1024)]
        + [dict(nodes=3201, steps=2048, extent=4)]
    )


def _difference(left, right):
    return (np.abs(np.array(left["values"]) - right["values"]) / 100).tolist()


def _study_checks(runs, discount, forward, refined=False):
    nodes, steps, previous_nodes, previous_steps = (
        (6401, 4096, 3201, 2048) if refined else (3201, 2048, 1601, 1024)
    )
    final = runs[(nodes, steps, 3)]
    differences = dict(
        space=_difference(final, runs[(previous_nodes, steps, 3)]),
        time=_difference(final, runs[(nodes, previous_steps, 3)]),
        domain=_difference(final, runs[(nodes, steps, 4)]),
    )
    differences["mass"] = [abs(final["values"][-2] / discount - 1)]
    differences["forward"] = [abs(final["values"][-1] / discount - forward) / 100]
    return differences


def _black_check(runs, maturity, discount, forward, refined=False):
    with mp.workdps(90):
        oracle = [
            float(
                black(
                    mp.mpf(forward),
                    mp.mpf(discount),
                    mp.mpf(float(ratio) * forward),
                    mp.mpf(".04") * maturity,
                    call,
                )
            )
            for call in (True, False)
            for ratio in RATIOS
        ]
    nodes, steps = (6401, 4096) if refined else (3201, 2048)
    return (
        np.abs(np.array(runs[(nodes, steps, 3)]["values"][:10]) - oracle) / 100
    ).tolist()


def _accepted(differences):
    return bool(differences) and all(
        len(values) > 0
        and np.all(np.isfinite(values))
        and min(values) >= 0
        and max(values) <= PDE_LIMIT
        for values in differences.values()
    )


def _refine(name, coefficient, maturity, runs):
    forward = float(carry(maturity, coefficient.flat)[0])
    for nodes, steps, extent in (
        (6401, 4096, 3),
        (3201, 4096, 3),
        (6401, 2048, 3),
        (6401, 4096, 4),
    ):
        _, _, configuration, result = _study_task(
            (
                name,
                coefficient,
                maturity,
                [float(ratio) * forward for ratio in RATIOS],
                dict(nodes=nodes, steps=steps, extent=extent),
            )
        )
        runs[
            (configuration["nodes"], configuration["steps"], configuration["extent"])
        ] = result


def _finish_study(name, coefficient, maturity, runs):
    forward, discount = map(float, carry(maturity, coefficient.flat))
    differences = _study_checks(runs, discount, forward)
    if name.startswith("Black_"):
        differences["black"] = _black_check(runs, maturity, discount, forward)
    refined = not _accepted(differences)
    if refined:
        _refine(name, coefficient, maturity, runs)
        differences = _study_checks(runs, discount, forward, True)
        if name.startswith("Black_"):
            differences["black"] = _black_check(runs, maturity, discount, forward, True)
    return dict(
        fixture=name,
        maturity=maturity,
        status="PASS" if _accepted(differences) else "REFERENCE_UNAVAILABLE",
        refined=refined,
        normalized_differences=differences,
        levels=list(runs.values()),
    )


def pde_studies(captured, requested, jobs):
    sources = [
        (row["id"], coefficients(row["snapshot"], fixture["flat"]))
        for row, fixture in zip(captured, requested)
        if row["fit_success"]
    ]
    # An exact flat-vol model checks the solver independently of a DAL fit.
    sources += [
        ("Black_knots", Coefficients((0.0, 2.0), (0.0, 0.08), (0.04, 0.04))),
        ("Black_flat", Coefficients((0.0, 2.0), (0.0, 0.08), (0.04, 0.04), flat=True)),
    ]
    tasks = []
    for name, coefficient in sources:
        for maturity in MATURITIES:
            forward = float(carry(maturity, coefficient.flat)[0])
            for configuration in configurations():
                tasks.append(
                    (
                        name,
                        coefficient,
                        maturity,
                        [float(ratio) * forward for ratio in RATIOS],
                        configuration,
                    )
                )
    groups = {}
    with ProcessPoolExecutor(max_workers=jobs) as executor:
        for name, maturity, configuration, result in executor.map(_study_task, tasks):
            key = (
                configuration["nodes"],
                configuration["steps"],
                configuration["extent"],
            )
            groups.setdefault((name, maturity), {})[key] = result
    return [
        _finish_study(name, coefficient, maturity, groups[(name, maturity)])
        for name, coefficient in sources
        for maturity in MATURITIES
    ]


def mc_rows(fixture, observed, studies):
    result = []
    for time in MATURITIES:
        forward, discount = map(float, carry(time, fixture["flat"]))
        study = next(
            row
            for row in studies
            if row["fixture"] == fixture["id"] and row["maturity"] == time
        )
        final = (6401, 4096, 3) if study["refined"] else (3201, 2048, 3)
        oracle_values = next(
            level["values"]
            for level in study["levels"]
            if (level["nodes"], level["requested_steps"], level["extent"]) == final
        )
        reason = (
            "Frozen production grid rejected before model construction"
            if not all(grid["success"] for grid in observed["grids"])
            else "Formal statistical runner not implemented; "
            "this delivery is a blocked reference checkpoint"
        )
        products = [
            (side, ratio, index + offset)
            for side, offset in (("call", 0), ("put", 5))
            for index, ratio in enumerate(RATIOS)
        ]
        products += [("discount", None, 10), ("discounted_forward", None, 11)]
        for side, ratio, index in products:
            result.append(
                dict(
                    fixture=fixture["id"],
                    maturity=time,
                    product=side,
                    strike=None if ratio is None else float(ratio) * forward,
                    estimate=None,
                    se=None,
                    oracle=oracle_values[index],
                    oracle_status=study["status"],
                    fit_error="see fit_rows",
                    grid_error=None,
                    paired_time_error=None,
                    status="NOT_RUN",
                    reason=reason,
                    seed=1024,
                    stream_ids=list(range(16)),
                )
            )
        for ratio in RATIOS:
            result.append(
                dict(
                    fixture=fixture["id"],
                    maturity=time,
                    product="parity",
                    strike=float(ratio) * forward,
                    estimate=None,
                    se=None,
                    oracle=discount * forward * (1 - float(ratio)),
                    status="NOT_RUN",
                    reason=reason,
                    seed=1024,
                    stream_ids=list(range(16)),
                )
            )
        for product, oracle in (
            ("forward_mean", forward),
            ("fixed_payment", discount),
            ("delayed_payment", float(carry(2.25, fixture["flat"])[1])),
        ):
            result.append(
                dict(
                    fixture=fixture["id"],
                    maturity=time,
                    product=product,
                    strike=None,
                    estimate=None,
                    se=None,
                    oracle=oracle,
                    status="NOT_RUN",
                    reason=reason,
                    seed=1024,
                    stream_ids=list(range(16)),
                )
            )
    return result


def qualify(request_path, capture_path, jobs):
    request = json.loads(request_path.read_text(encoding="utf-8"))
    capture = json.loads(capture_path.read_text(encoding="utf-8"))
    if [fixture["id"] for fixture in request["fixtures"]] != [
        row["id"] for row in capture["fixtures"]
    ]:
        raise ValueError("Incomplete or mismatched capture fixtures")
    studies = pde_studies(capture["fixtures"], request["fixtures"], jobs)
    rows = []
    with mp.workdps(90):
        for fixture, observed in zip(request["fixtures"], capture["fixtures"]):
            if fixture["id"] != observed["id"]:
                raise ValueError("Mismatched capture fixture")
            if not observed["fit_success"]:
                rows.append(
                    dict(
                        id=fixture["id"], status="FAIL", reason=observed["fit_failure"]
                    )
                )
                continue
            rows.append(
                dict(
                    id=fixture["id"],
                    fit_rows=fit_rows(fixture, observed),
                    high_precision=derivative_rows(fixture, observed),
                    grids=[
                        {
                            key: value
                            for key, value in grid.items()
                            if key not in ("vols_spot_major", "times", "spots")
                        }
                        for grid in observed["grids"]
                    ],
                    mc_rows=mc_rows(fixture, observed, studies),
                )
            )
    return dict(
        schema="dal.market-iv-independent-qualification/1",
        status="BLOCKED",
        decimal_digits=90,
        request_sha256=digest(request_path),
        capture_sha256=digest(capture_path),
        pde_studies=studies,
        fixtures=rows,
        remaining=(
            "MRG32 production-path/price alignment and formal N3/N5 statistical runs "
            "require valid production grids; not implemented or claimed here"
        ),
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("request", type=Path)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    report = qualify(args.request, args.capture, args.jobs)
    args.output.write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8"
    )
    print(
        json.dumps(
            dict(
                status=report["status"],
                pde_studies=len(report["pde_studies"]),
                fixtures=len(report["fixtures"]),
            )
        )
    )
    raise SystemExit(2)
