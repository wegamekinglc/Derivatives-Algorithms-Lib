"""Finite-input production binary64 FD/eFD certificate; no MC acceptance.

Run with the private capture's `qualification` JSON. Every endpoint is a real
production evaluation. The independent interval path answer and its dual
derivative enclose the exact same finite normal inputs. The triangle bound
accounts for the entire endpoint pipeline (including input/snapshot rounding),
final subtraction/division and exact-stencil truncation separately.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path

import mpmath as mp

from xccy_acceptance_interval import FixedCashflows, interval, upper_abs

OUTPUTS = ("A", "B", "Price", "par")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def require_axes(actual, expected):
    require(actual == expected and len(actual) == len(set(actual)), "complete ordered native axes")


def certify_stencil(up, down, observed, exact_up, exact_down, derivative, h):
    denominator = 2 * interval(h)
    exact_fd = (exact_up - exact_down) / denominator
    pipeline = upper_abs((interval(upper_abs(interval(up) - exact_up)) + interval(upper_abs(interval(down) - exact_down))) / denominator)
    subtraction = upper_abs(interval(observed) - (interval(up) - interval(down)) / denominator)
    truncation = upper_abs(exact_fd - derivative)
    total = upper_abs(interval(pipeline) + interval(subtraction) + interval(truncation))
    return dict(pipeline=pipeline, subtraction=subtraction, truncation=truncation, total=total)


def native_labels(model):
    labels = ["fxspot:EUR/USD"]
    for curve in model["curves"]:
        labels.extend(curve["labels"])
    for name in ("USD", "EUR", "EUR/USD"):
        labels.extend(f"sigma:{name}:{date}" for date in model["bucket_dates"])
    return labels


def expected_chain(model, labels):
    rows = [[int(j == 0) for j in range(len(labels))]]
    column = 1
    for curve in model["curves"]:
        require(len(curve["jacobian"]) == len(curve["dates"]), "snapshot row dimensions")
        for weights in curve["jacobian"][1:]:
            require(len(weights) == len(curve["labels"]), "snapshot column dimensions")
            rows.append([0] * column + weights + [0] * (len(labels) - column - len(weights)))
        column += len(curve["labels"])
    for i in range(column, len(labels)):
        rows.append([int(j == i) for j in range(len(labels))])
    return rows


def selected_labels(labels, nonflat):
    if not nonflat:
        return labels
    return [x for x in labels if x != "fxspot:EUR/USD" and not x.startswith("logdf:USD:BASIS:")]


def admit_grid(case, nonflat):
    grid = ["2026-10-21", "2027-04-30", "2027-05-04", "2027-08-02", "2027-08-04", "2027-11-04", "2027-11-08"]
    if nonflat:
        grid += ["2027-04-19", "2027-10-21"]
    require(case["grid"] == sorted(grid), "independent frozen grid")
    dimension = 5 * (len(case["grid"]) - 1)
    require(all(len(row) == dimension and all(math.isfinite(x) for x in row) for row in case["normals"]), "full finite normals")
    batches, paths = (2, 3) if nonflat else (4, 8)
    expected_ranges = [
        [262144 + i * paths, 262144 + (i + 1) * paths, (262144 + i * paths) * dimension, (262144 + (i + 1) * paths) * dimension]
        for i in range(batches)
    ]
    require(case["ranges"] == expected_ranges, "frozen complete Validation reservation")


def admit_steps(case):
    for row in case["rows"]:
        scale = 0.01 if row["label"].startswith(("fxspot:", "sigma:EUR/USD:")) else 0.001
        require(row["scale"] == scale and len(row["steps"]) == 3 and len(row["aad"]) == 4, "frozen scales/stencil/outputs")
        for divisor, step in zip((1, 2, 4), row["steps"], strict=True):
            require(step["h"] == 0.001 * scale / divisor and len(step["fd"]) == 4, "unchanged h/h2/h4")
            for sign, key in ((1, "up"), (-1, "down")):
                require(len(step[key]["values"]) == 4, "complete endpoint")
                admit_bump(case["model"], step[key]["model"], row["label"], sign * step["h"])


def admit(case):
    model = case["model"]
    require_axes(case["native_labels"], native_labels(model))
    labels = case["native_labels"]
    nonflat = case["id"] == "nonflat"
    require(case["id"] in ("flat", "nonflat"), "frozen qualification fixture")
    require_axes([row["label"] for row in case["rows"]], selected_labels(labels, nonflat))
    require(len(labels) == (30 if nonflat else 9), "frozen full native dimension")
    require(len(case["normals"]) == (6 if nonflat else 32), "frozen technical paths")
    require(case["foreign_spread"] == nonflat, "frozen spread leg")
    require(case["chain"] == expected_chain(model, labels), "complete snapshot chain content")
    require(case["base"]["model"] == model, "base immutable model")
    require(len(case["base"]["values"]) == 4 and len(case["base"]["batches"]) == (2 if nonflat else 4), "complete base coefficients")
    admit_grid(case, nonflat)
    admit_steps(case)


def admit_curve(original, actual, label, shift):
    for key in ("currency", "route", "dates", "labels", "jacobian"):
        require(actual[key] == original[key], "immutable curve snapshot structure")
    values = [v + (shift if axis == label else 0) for v, axis in zip(original["values"], original["labels"], strict=True)]
    require(actual["values"] == values, "complete native snapshot perturbation")
    require(
        actual["logdf"] == [sum(w * v for w, v in zip(row, values, strict=True)) for row in original["jacobian"]],
        "full snapshot Jacobian bump",
    )


def admit_bump(base, bumped, label, shift):
    require(base["a"] == bumped["a"] and base["correlation"] == bumped["correlation"], "frozen dynamics")
    require(base["bucket_dates"] == bumped["bucket_dates"], "frozen buckets")
    require(bumped["spot"] == base["spot"] + (shift if label == "fxspot:EUR/USD" else 0), "complete spot bump")
    require(len(base["curves"]) == len(bumped["curves"]), "complete bump curves")
    for original, actual in zip(base["curves"], bumped["curves"], strict=True):
        admit_curve(original, actual, label, shift)
    for driver, name in enumerate(("USD", "EUR", "EUR/USD")):
        expected = [
            value + (shift if label == f"sigma:{name}:{date}" else 0)
            for value, date in zip(base["sigma"][driver], base["bucket_dates"], strict=True)
        ]
        require(bumped["sigma"][driver] == expected, "complete native sigma bump")


def serialized_interval(value):
    lo, hi = value._mpi_
    return dict(bounds=[mp.nstr(mp.make_mpf(x), 110) for x in (lo, hi)], binary_endpoints=[[int(y) for y in x] for x in (lo, hi)])


def check_row(case, row):
    label = row["label"]
    exact = FixedCashflows(case, label, active=True).values()
    derivatives = [x.derivative for x in exact]
    for actual, expected in zip(row["aad"], derivatives, strict=True):
        require(upper_abs(interval(actual) - expected) <= mp.mpf("1e-8"), "independent fixed-normal AAD derivative")
    results = []
    for step in row["steps"]:
        up = FixedCashflows(case, label, step["h"]).values()
        down = FixedCashflows(case, label, -step["h"]).values()
        outputs = []
        for i, name in enumerate(OUTPUTS):
            bound = certify_stencil(
                step["up"]["values"][i], step["down"]["values"][i], step["fd"][i], up[i].value, down[i].value, derivatives[i], step["h"]
            )
            scale = mp.mpf(".0001") if name == "par" else mp.mpf(".01")
            cap = mp.mpf(".01") * scale / mp.mpf(row["scale"])
            require(bound["total"] <= cap, "finite-input eFD budget")
            require(abs(mp.mpf(step["fd"][i]) - mp.mpf(row["aad"][i])) <= bound["total"] + mp.mpf("1e-8"), "FD with complete eFD")
            outputs.append(
                dict(
                    output=name,
                    derivative=serialized_interval(derivatives[i]),
                    exact_up=serialized_interval(up[i].value),
                    exact_down=serialized_interval(down[i].value),
                    cap=mp.nstr(cap, 100),
                    total_binary=[int(x) for x in bound["total"]._mpf_],
                    **{key: mp.nstr(value, 110) for key, value in bound.items()},
                )
            )
        results.append(dict(h=step["h"], outputs=outputs))
    return dict(label=label, steps=results)


def qualify(case, digits):
    admit(case)
    mp.iv.dps = digits
    base = FixedCashflows(case).values()
    for actual, expected in zip(case["base"]["values"], base, strict=True):
        require(upper_abs(interval(actual) - expected.value) <= mp.mpf("1e-10"), "independent fixed-normal base coefficients")
    rows = []
    for row in case["rows"]:
        rows.append(check_row(case, row))
        print(f"{digits} digits: {case['id']} {row['label']}", flush=True)
    return dict(id=case["id"], digits=digits, base=[serialized_interval(x.value) for x in base], rows=rows)


def refinement(coarse, fine):
    for a, b in zip(coarse["rows"], fine["rows"], strict=True):
        require(a["label"] == b["label"], "refinement identity")
        for sa, sb in zip(a["steps"], b["steps"], strict=True):
            for oa, ob in zip(sa["outputs"], sb["outputs"], strict=True):
                for key in ("derivative", "exact_up", "exact_down"):
                    la, ha = [mp.make_mpf(tuple(x)) for x in oa[key]["binary_endpoints"]]
                    lb, hb = [mp.make_mpf(tuple(x)) for x in ob[key]["binary_endpoints"]]
                    require(max(la, lb) <= min(ha, hb), "independent interval refinement overlap")
                    require(ha - la <= mp.mpf("1e-40") and hb - lb <= mp.mpf("1e-40"), "narrow certified enclosures")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = json.loads(args.capture.read_text())
    require([case["id"] for case in data] == ["flat", "nonflat"], "complete frozen case list")
    mp.mp.dps = 140
    results = []
    for case in data:
        coarse, fine = (qualify(case, digits) for digits in (70, 100))
        refinement(coarse, fine)
        results.append(dict(coarse=coarse, fine=fine))
    args.output.write_text(
        json.dumps(
            dict(
                capture_sha256=hashlib.sha256(args.capture.read_bytes()).hexdigest(),
                financial_bias_bound_qualified=False,
                mc_numerics_qualified=False,
                domain="finite captured normals and native stencils only",
                results=results,
            ),
            indent=2,
        )
        + "\n"
    )
    print("PASS: finite-input eFD only; all original hard-floor/statistical/tail RED remain")


if __name__ == "__main__":
    main()
