"""Independent original staged basket ledger, covariance and numerical audit."""

import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import statistics

from xccy_acceptance_extended import gradient_gate, qualify_schedule
from xccy_acceptance_statistics import (
    compare, projected_se, qualify_direct_values, qualify_gradients, qualified_direct_gate, require, require_width, row_matches, sample_se,
)

DIMS = (30, 55, 80)
OFFSETS = (0, 70778880, 200540160, 389283840)
PHASES = ((0, 262144), (262144, 1310720), (1310720, 2359296))
SLOTS = ("xd", "xf", "Id", "If", "Z")
MEMBER_CONTEXT_FIELDS = ("id", "source", "binding", "context", "mapping", "instrument", "measure")


def same(actual, expected, diagnostic):
    require(actual == expected, diagnostic)


def square(matrix, width, diagnostic):
    require(len(matrix) == width and all(len(row) == width for row in matrix), diagnostic)
    require(all(math.isfinite(x) for row in matrix for x in row), diagnostic + " finite")


def fingerprint(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False).encode()).hexdigest()


def context(capture, fit):
    return {"model": fit["cases"][0]["model"]["identity"], "archive": fingerprint(capture["model_archive"]),
            "members": [{key: m[key] for key in MEMBER_CONTEXT_FIELDS}
                        for m in capture["allocation"]["members"]]}


def finite_matrix(matrix, height, width, diagnostic):
    require_width(matrix, height, diagnostic + " row count")
    for row in matrix:
        require_width(row, width, diagnostic + " row width")
        require(all(math.isfinite(x) for x in row), diagnostic + " finite entries")


def native_curve_rows(curve, offset, width):
    finite_matrix(curve["jacobian"], len(curve["dates"]), len(curve["labels"]), "retained native chain curve")
    return [[0.] * offset + row + [0.] * (width - offset - len(row)) for row in curve["jacobian"][1:]]


def native_labels(model):
    domestic, foreign = (curve["currency"] for curve in model["curves"][:2])
    spot = "fxspot:" + foreign + "/" + domestic
    sigmas = ["sigma:" + name + ":" + date for name in (domestic, foreign, foreign + "/" + domestic)
              for date in model["bucket_dates"]]
    return [spot] + [label for curve in model["curves"] for label in curve["labels"]] + sigmas


def retained_native_chain(case):
    model = case["model"]
    labels = native_labels(model)
    same(case["labels"], labels, "retained native chain coordinate axes")
    width = len(labels)
    chain, offset = [[1.] + [0.] * (width - 1)], 1
    for curve in model["curves"]:
        chain.extend(native_curve_rows(curve, offset, width))
        offset += len(curve["labels"])
    chain.extend([[float(i == j) for j in range(width)] for i in range(offset, width)])
    return chain


def qualify_direct(direct, case):
    same(direct["labels"], case["labels"], "full native map axes")
    width = len(case["labels"])
    finite_matrix(direct.get("fast_gradient"), 4, width, "complete direct A/B/Price/par derivatives")
    finite_matrix(case["fast_gradient"], 4, width, "complete retained A/B/Price/par derivatives")
    require(row_matches(direct["fast"], case["fast"]), "original deterministic values")
    for row, expected in zip(direct["fast_gradient"], case["fast_gradient"]):
        require(row_matches(row, expected), "original full native direct derivatives")
    chain = retained_native_chain(case)
    finite_matrix(direct.get("native_chain"), len(chain), width, "complete native chain")
    same(direct["native_chain"], chain, "native chain retained coordinate correspondence")


def qualify_member(i, member, case, direct):
    qualify_schedule(case)
    for key in ("source", "instrument", "mapping", "grid", "coordinates"):
        same(member[key], case[key], "original staged geometry: " + key)
    same((member["D"], member["block"]), (DIMS[i], list(OFFSETS[i:i + 2])), "original dimensions/derived blocks")
    same(member["id"], "independent-staged-" + str(i), "original member order")
    same(member["measure"], "Price", "source Price view")
    qualify_direct(direct, case)
    for j, (begin, end) in enumerate(PHASES):
        same(member["phases"][j], [begin, end, OFFSETS[i] + begin * DIMS[i], OFFSETS[i] + end * DIMS[i]], "all nine reserved intervals")
    coordinates = ["interval:" + left + ":" + right + ":innovation:" + slot
                   for left, right in zip(case["grid"], case["grid"][1:]) for slot in SLOTS]
    same(member["coordinates"], coordinates, "five-slot dated coordinates")


def qualify_preflight(capture, original):
    fit = next(f for f in original["fits"] if f["mode"] == capture["mode"])
    same(capture["parameters"], fit["parameters"], "original immutable fitted parameters")
    a = capture["allocation"]
    same((a["layout"], a["stream"], a["seed"], a["precision"], a["seek"]),
         ("dal.xccy-gaussian-basket-coordinate-ranges/1", "dal.mrg32-normal-paths/1", 1024, "Precise", "absolute-scalar/1"), "fixed allocation rules")
    same((a["T"], a["end"], a["phase"]), (2359296, OFFSETS[-1], "HeldOut"), "frozen reservation")
    same(len(a["members"]), 3, "all original members")
    same(len(capture["direct"]), 3, "complete direct reference rows")
    for i, (member, case, direct) in enumerate(zip(a["members"], fit["cases"], capture["direct"])):
        qualify_member(i, member, case, direct)
    same(OFFSETS[1] % DIMS[1], 40, "nonaligned scalar start")
    return fit


def check_axes(s, original):
    fit = next(f for f in original["fits"] if f["cases"][0]["model"]["identity"] == s["model"])
    labels = [label for label in fit["cases"][0]["labels"]
              if label.startswith(("fxspot:", "logdf:USD:BASIS:", "sigma:"))]
    same(s["labels"], labels, "complete selected native labels in original order")
    units = ["USD/EUR"] + ["Dimensionless"] * 3 + ["year^-3/2"] * 2 + ["year^-1/2"]
    same(s["units"], units, "native parameter units")
    coefficient, output = [], []
    for m in s["members"]:
        coefficient += [[m["id"], "B", "USD"], [m["id"], "A", "USD/decimal-spread"]]
        output.append([m["id"], "Price", "Price:USD"])
        for label, unit in zip(labels, units):
            coefficient += [[m["id"], "dB/" + label, "USD/(" + unit + ")"],
                            [m["id"], "dA/" + label, "USD/decimal-spread/(" + unit + ")"]]
            output.append([m["id"], "dPrice/" + label, "Price:USD/(" + unit + ")"])
    same(s["coefficient_axes"], coefficient, "complete coefficient axis identity and units")
    same(s["output_axes"], output, "complete output axis identity and units")


def check_member_rows(i, m, a, width):
    same((m["id"], m["state"], m["paths"], m["batches"]),
         (a["members"][i]["id"], "Complete", a["batches"] * a["n"], a["batches"]), "member completion")
    same(m["consumed"], m["paths"] * DIMS[i], "member actual consumption")
    same((m["estimate"]["validation"], m["estimate"]["held_out"]), (False, True), "actual HeldOut flags")
    require(len(m["rows"]) == a["batches"] and all(len(row) == width for row in m["rows"]), "complete finite rows")
    require(all(math.isfinite(x) for row in m["rows"] for x in row), "finite coefficient derivatives")
    expected = [[1310720 + b * a["n"], 1310720 + (b + 1) * a["n"],
                 OFFSETS[i] + (1310720 + b * a["n"]) * DIMS[i],
                 OFFSETS[i] + (1310720 + (b + 1) * a["n"]) * DIMS[i]] for b in range(a["batches"])]
    same(m["actual_ranges"], expected, "actual absolute scalar endpoints")
    same(a["members"][i]["ranges"], expected, "planned absolute scalar endpoints")


def joint_rows(s, count):
    return [sum([m["rows"][b] for m in s["members"]], []) for b in range(count)]


def qualify_result(data):
    s, a = data["result"], data["result"]["allocation"]
    factor = data["factor"]
    if factor in (1, 2, 4):
        same((a["batches"], a["n"]), (16, 16384 * factor), "predeclared initial/refined budget")
    else:
        require(factor == 0 and "crn" in data, "explicit technical diagnostic request")
        same((a["batches"], a["n"]), (2, 3), "technical diagnostic size")
    same(a, data["allocation"], "prepared/executed allocation")
    same((s["complete"], s["candidate_geometry_validated"]), (True, True), "complete real candidate")
    same(len(s["members"]), 3, "all completed members")
    require(bool(s["native_map"]) and bool(s["plan"]), "actual native map and plan identities")
    same(len(s["labels"]), 7, "all seven selected native axes")
    same(len(s["units"]), 7, "ordered parameter units")
    same(s["consumed"], a["batches"] * a["n"] * sum(DIMS), "total actual consumption")
    width = 2 + 2 * len(s["labels"])
    for i, m in enumerate(s["members"]):
        check_member_rows(i, m, a, width)
    rows = joint_rows(s, a["batches"])
    same(s["rows"], rows, "paired concatenation in original member/batch order")
    return rows, width


def covariance(rows):
    columns = list(zip(*rows))
    means = [statistics.mean(c) for c in columns]
    c = [[math.fsum((x - means[i]) * (y - means[j]) for x, y in zip(left, right)) / (len(rows) - 1)
          for j, right in enumerate(columns)] for i, left in enumerate(columns)]
    return means, c


def check_member_covariance(i, member, q, means, c, width):
    e = member["estimate"]
    require(all(len(e[key]) == q for key in ("da", "db", "gradient", "gradient_se")), "complete member summary axes")
    summary = [e["b"], e["a"]] + sum([[b, a] for b, a in zip(e["db"], e["da"])], [])
    require(row_matches(summary, means[i * width:(i + 1) * width]), "coefficient summary from all B rows")
    square(e["covariance"], width, "member covariance")
    for j, actual in enumerate(e["covariance"]):
        require(row_matches(actual, c[i * width + j][i * width:(i + 1) * width]), "member diagonal block")


def check_covariances(s, rows, width):
    means, c = covariance(rows)
    require(row_matches(s["means"], means), "joint coefficient means")
    for key in ("C_batch", "C_mean"):
        square(s[key], len(means), key)
        for actual, expected in zip(s[key], c):
            divisor = 1 if key == "C_batch" else len(rows)
            require(row_matches(actual, [x / divisor for x in expected]), "entire " + key + " including cross-member blocks")
    for i, member in enumerate(s["members"]):
        check_member_covariance(i, member, len(s["labels"]), means, c, width)
    return means, c


def jacobian(means, q, par=False):
    b, a = means[:2]
    width = len(means)
    j = [[0.] * width for _ in range(q + 1)]
    j[0][:2] = [-1 / a, b / a**2] if par else [1., .001]
    for k in range(q):
        db, da = means[2 + 2 * k:4 + 2 * k]
        j[k + 1][2 + 2 * k:4 + 2 * k] = j[0][:2]
        if par:
            j[k + 1][:2] = [da / a**2, db / a**2 - 2 * b * da / a**3]
    return j


def check_projection(s, rows, means, width):
    q = len(s["labels"])
    j = []
    for i in range(3):
        local = jacobian(means[i * width:(i + 1) * width], q)
        j.extend([[0.] * (i * width) + row + [0.] * ((2 - i) * width) for row in local])
    same(s["J"], j, "full independent Price/native-gradient Jacobian")
    influence = [[math.fsum(w * (x - mean) for w, x, mean in zip(weight, row, means)) for weight in j] for row in rows]
    _, projected = covariance(influence)
    square(s["C_output"], len(j), "full projected covariance")
    for actual, expected in zip(s["C_output"], projected):
        require(row_matches(actual, [x / len(rows) for x in expected]), "full projected covariance with cross-member terms")


def par_projection(s, rows, means, width):
    j = []
    for i in range(3):
        local = jacobian(means[i * width:(i + 1) * width], len(s["labels"]), True)
        j.extend([[0.] * (i * width) + row + [0.] * ((2 - i) * width) for row in local])
    influence = [[math.fsum(w * (x - mean) for w, x, mean in zip(weight, row, means)) for weight in j] for row in rows]
    _, c = covariance(influence)
    return {"J": j, "C_mean_output": [[x / len(rows) for x in row] for row in c]}


def matmul(left, right, modulus):
    return [[sum(a * b for a, b in zip(row, column)) % modulus for column in zip(*right)] for row in left]


def rng_state(seed, coordinate, modulus, last):
    step = [[0, 1, 0], [0, 0, 1], last]
    result = [[seed], [seed], [seed]]
    while coordinate:
        if coordinate % 2:
            result = matmul(step, result, modulus)
        step = matmul(step, step, modulus)
        coordinate //= 2
    return [x[0] for x in result]


def uniform_trace(seed, coordinate, count):
    m1, m2 = 4294967087, 4294944443
    x = rng_state(seed, coordinate, m1, [m1 - 810728, 1403580, 0])
    y = rng_state(seed + 1, coordinate, m2, [m2 - 1370589, 0, 527612])
    output = []
    for _ in range(count):
        x = x[1:] + [(1403580 * x[1] - 810728 * x[0]) % m1]
        y = y[1:] + [(527612 * y[2] - 1370589 * y[0]) % m2]
        difference = x[2] - y[2] if x[2] > y[2] else x[2] + m1 - y[2]
        output.append(difference / 4294967088)
    return output


def defined_normal(probability):
    if .02425 <= probability <= 1 - .02425:
        return statistics.NormalDist().inv_cdf(probability)
    p = probability if probability < .02425 else 1 - probability
    r = math.sqrt(-2 * math.log(p))
    numerator = (((((-7.784894002430293e-3 * r - .3223964580411365) * r - 2.400758277161838) * r - 2.549732539343734) * r + 4.374664141464968) * r + 2.938163982698783)
    denominator = ((((7.784695709041462e-3 * r + .3224671290700398) * r + 2.445134137142996) * r + 3.754408661907416) * r + 1)
    z = numerator / denominator
    z = z if probability < .02425 else -z
    error = .5 * math.erfc(-z / math.sqrt(2)) - probability
    return z - error * 2.5066282746310002 * math.exp(min(8., .5 * z * z))


def normal_trace(seed, coordinate, count):
    return [defined_normal(p) for p in uniform_trace(seed, coordinate, count)]


def ideal_quantile_diagnostics(s):
    failures = []
    for member in s["members"]:
        for r, actual in zip(member["actual_ranges"], member["first_normals"]):
            uniforms = uniform_trace(1024, r[2], len(actual))
            for slot, (p, observed) in enumerate(zip(uniforms, actual)):
                ideal = statistics.NormalDist().inv_cdf(p)
                if abs(observed - ideal) > 5e-13:
                    failures.append({"coordinate": r[2] + slot, "observed": observed, "ideal": ideal, "error": observed - ideal})
    return {"ideal_5e_13_pass": not failures, "failures": failures, "financial_bias_bound_qualified": False}


def check_traces(s):
    checked = 0
    for allocation, member in zip(s["allocation"]["members"], s["members"]):
        same(len(member["first_normals"]), s["allocation"]["batches"], "all batch-start traces")
        for r, actual in zip(member["actual_ranges"], member["first_normals"]):
            expected = normal_trace(1024, r[2], allocation["D"])
            require(len(actual) == len(expected) and all(abs(a - b) <= 5e-13 for a, b in zip(actual, expected)), "independent bigint MRG32 absolute scalar trace")
            checked += len(actual)
    return checked


def audit(data, original):
    fit = qualify_preflight(data, original)
    same(data["result"]["model"], fit["cases"][0]["model"]["identity"], "actual fitted model identity")
    check_axes(data["result"], original)
    rows, width = qualify_result(data)
    s = data["result"]
    means, c = check_covariances(s, rows, width)
    check_projection(s, rows, means, width)
    return {"mode": data["mode"], "factor": data["factor"], "width": len(means), "B": len(rows),
            "consumed": s["consumed"], "cross_member_nonzero": sum(c[i][j] != 0 for i in range(width) for j in range(width, 3 * width)),
            "covariance_verified": True, "normal_coordinates_checked": check_traces(s),
            "derived_joint_par_projection": par_projection(s, rows, means, width),
            "ideal_quantile_diagnostic": ideal_quantile_diagnostics(s)}


def check_crn_coordinates(base, control):
    for sign in ("plus", "minus"):
        bumped = control[sign]
        same(bumped["allocation"], base["allocation"], "CRN immutable allocation")
        for member, actual in zip(base["members"], bumped["members"]):
            same(member["first_normals"], actual["first_normals"], "actual CRN draws")
            same(member["actual_ranges"], actual["actual_ranges"], "actual CRN endpoints")


def crn_par_comparison(member, plus, minus, h, index):
    b, a, db, da = (member["estimate"][key] for key in ("b", "a", "db", "da"))
    db, da = db[index], da[index]
    bp, ap = (statistics.mean(column) for column in zip(*plus))
    bm, am = (statistics.mean(column) for column in zip(*minus))
    require(all(abs(a_) - 3 * sample_se([r[1] for r in rows_]) > 1e-12 for a_, rows_ in ((ap, plus), (am, minus))), "CRN denominator confidence")
    par_difference = (-bp / ap + bm / am) / (2 * h) - (-db / a + b * da / a**2)
    paired = [[r[0], r[1], r[2 + 2 * index], r[3 + 2 * index]] + p + n for r, p, n in zip(member["rows"], plus, minus)]
    weights = [-da / a**2, -db / a**2 + 2 * b * da / a**3, 1 / a, -b / a**2,
               -1 / ap / (2 * h), bp / ap**2 / (2 * h), 1 / am / (2 * h), -bm / am**2 / (2 * h)]
    return {"paired_par_FD_minus_AAD": par_difference, "paired_par_difference_se": projected_se(paired, weights)}


def crn_comparison(member, plus, minus, label, h, index):
    differences = [(p[0] - n[0] + .001 * (p[1] - n[1])) / (2 * h) - (r[2 + 2 * index] + .001 * r[3 + 2 * index])
                   for p, n, r in zip(plus, minus, member["rows"])]
    result = {"label": label, "h": h, "member": member["id"], "paired_FD_minus_AAD": statistics.mean(differences),
              "paired_difference_se": sample_se(differences), "eFD_qualified": False}
    result.update(crn_par_comparison(member, plus, minus, h, index))
    return result


def crn_controls(data, original):
    checked = audit(data, original)
    base, records = data["result"], []
    for control in data["crn"]:
        label, h = control["label"], control["h"]
        index = base["labels"].index(label)
        check_crn_coordinates(base, control)
        for i, member in enumerate(base["members"]):
            plus = control["plus"]["members"][i]["rows"]
            minus = control["minus"]["members"][i]["rows"]
            records.append(crn_comparison(member, plus, minus, label, h, index))
    return {"audit": checked, "paired_crn": records, "binary64_eFD_qualified": False}


def paired_measure(rows, means, width, q, par):
    lx, ly = jacobian(means[:width], q, par), jacobian(means[width:], q, par)
    value = (lambda v: -v[0] / v[1]) if par else (lambda v: v[0] + .001 * v[1])
    return {"measure": "par" if par else "Price", "difference": value(means[:width]) - value(means[width:]),
            "paired_se": projected_se(rows, lx[0] + [-w for w in ly[0]]),
            "gradient_paired_se": [projected_se(rows, a + [-w for w in b]) for a, b in zip(lx[1:], ly[1:])]}


def paired_member(left, right, x, y):
    same(x["actual_ranges"], y["actual_ranges"], "mode paired absolute ranges")
    same(x["first_normals"], y["first_normals"], "mode actual common random numbers")
    rows = [a + b for a, b in zip(x["rows"], y["rows"])]
    means = [statistics.mean(c) for c in zip(*rows)]
    width, q = len(x["rows"][0]), len(left["result"]["labels"])
    summaries = [paired_measure(rows, means, width, q, par) for par in (False, True)]
    return {"factor": left["factor"], "member": x["id"], "left_mode": left["mode"], "right_mode": right["mode"], "comparisons": summaries}


def paired_models(data):
    output = []
    for factor in (1, 2, 4):
        pair = [d for d in data if d["factor"] == factor]
        if len(pair) != 2:
            continue
        left, right = sorted(pair, key=lambda d: d["mode"])
        for x, y in zip(left["result"]["members"], right["result"]["members"]):
            output.append(paired_member(left, right, x, y))
    return output


def qualify_oracle(preflights, original, prior_oracle):
    output = []
    for capture in preflights:
        fit = qualify_preflight(capture, original)
        old = next(f for f in prior_oracle["fits"] if f["mode"] == fit["mode"])
        cases = []
        for case, direct, prior in zip(fit["cases"], capture["direct"], old["cases"]):
            current = dict(case, fast=direct["fast"], fast_gradient=direct["fast_gradient"])
            gradients = copy.deepcopy(prior["gradients"])
            for label in ("fxspot:EUR/USD", "sigma:USD:2029-10-20", "sigma:EUR:2029-10-20", "sigma:EUR/USD:2029-10-20"):
                gradients.append(gradient_gate(current, case["labels"].index(label), label, 4))
            cases.append(dict(prior, gradients=gradients))
        output.append({"mode": fit["mode"], "context": context(capture, fit), "cases": cases})
    return {"fits": output, "original_fingerprint": fingerprint(original)}


def qualified_fit(data, oracle):
    s = data["result"]
    fit = next(f for f in oracle["fits"] if f["mode"] == data["mode"])
    same((len(fit["cases"]), len(fit["context"]["members"])), (3, 3), "complete oracle member references")
    same(fit["context"]["model"], s["model"], "qualified oracle model")
    same(fit["context"]["archive"], fingerprint(data["model_archive"]), "qualified immutable model archive")
    for member, expected in zip(data["allocation"]["members"], fit["context"]["members"]):
        require(all(key in expected for key in MEMBER_CONTEXT_FIELDS), "complete oracle member context")
        require(all(key in member for key in MEMBER_CONTEXT_FIELDS), "complete prepared member context")
        same({key: member[key] for key in MEMBER_CONTEXT_FIELDS}, {key: expected[key] for key in MEMBER_CONTEXT_FIELDS},
             "qualified source/full binding/context/measure/mapping")
    return fit


def gradient_report(m, rows, means, gradient, k, weight, measure):
    par, output_column, scale, epsilon = measure
    b, a = means[:2]
    gref, gscale = gradient["values"][output_column], float(gradient["scale"])
    qualified_direct_gate(gref, (1e-12 if par else 1e-10) / gscale, epsilon / gscale)
    db, da = means[2 + 2 * k:4 + 2 * k]
    gvalue = -db / a + b * da / a**2 if par else db + .001 * da
    se = projected_se(rows, weight)
    if not par:
        require(math.isclose(gvalue, m["estimate"]["gradient"][k], abs_tol=1e-10), "raw native gradient")
        require(math.isclose(se, m["estimate"]["gradient_se"][k], rel_tol=1e-10, abs_tol=1e-11), "independent raw gradient SE")
    gate = compare(gvalue, se, float(gref["oracle"]), float(gref["oracle_indicator"]) + float(gref["fast_indicator"]), scale / gscale, 1e-8)
    return dict(gate, label=gradient["label"])


def measure_report(m, reference, labels, rows, means, measure):
    par, output_column, scale, epsilon = measure
    b, a = means[:2]
    weights = jacobian(means, len(labels), par)
    value = -b / a if par else b + .001 * a
    ref = reference["values"][output_column]
    qualified_direct_gate(ref, 1e-12 if par else 1e-10, epsilon)
    indicator = float(ref["oracle_indicator"]) + float(ref["fast_indicator"])
    result = compare(value, projected_se(rows, weights[0]), float(ref["oracle"]), indicator, scale, epsilon)
    if not par:
        require(math.isclose(value, m["estimate"]["mean"], abs_tol=1e-12), "independent member Price")
        require(math.isclose(result["se"], m["estimate"]["se"], rel_tol=1e-10), "independent member Price SE")
    result["measure"] = "par" if par else "Price"
    result["gradients"] = []
    for k, label in enumerate(labels):
        gradient = next(g for g in reference["gradients"] if g["label"] == label)
        result["gradients"].append(gradient_report(m, rows, means, gradient, k, weights[k + 1], measure))
    return result


def member_report(m, reference, labels):
    qualify_direct_values(reference["values"], 1., "all direct A/B/Price/par gates")
    qualify_gradients(reference, labels)
    rows = m["rows"]
    means = [statistics.mean(c) for c in zip(*rows)]
    _, a = means[:2]
    confidence = abs(a) - 3 * sample_se([r[1] for r in rows])
    require(a < 0 and confidence > 1e-12, "signed annuity denominator qualification")
    gates = [measure_report(m, reference, labels, rows, means, measure)
             for measure in ((False, 2, .01, 1e-10), (True, 3, 1e-4, 1e-12))]
    return {"id": m["id"], "denominator_confidence": confidence, "measures": gates, "par_jacobian": jacobian(means, len(labels), True)}


def all_member_gates(reports):
    return [g for r in reports for m in r["measures"] for g in [m] + m["gradients"]]


def numerical(data, oracle):
    fit = qualified_fit(data, oracle)
    s = data["result"]
    reports = [member_report(m, reference, s["labels"]) for m, reference in zip(s["members"], fit["cases"])]
    flat = all_member_gates(reports)
    return {"mode": data["mode"], "factor": data["factor"], "members": reports,
            "parity": all(g["parity"] for g in flat), "precision": all(g["precision"] for g in flat),
            "combined": all(g["combined"] for g in flat), "reservation_exhausted": data["factor"] == 4,
            "old_heldout_already_observed": True, "binary64_eFD_qualified": False}


def audit_output(data, original, oracle):
    same(oracle["original_fingerprint"], fingerprint(original), "qualified original reference capture")
    return {"audits": [audit(row, original) for row in data], "runs": [numerical(row, oracle) for row in data],
            "paired_model_differences": paired_models(data)}


def dispatch(args, original, data):
    if args.mode == "preflight":
        return [qualify_preflight(row, original)["mode"] for row in data]
    if args.mode == "oracle":
        return qualify_oracle(data, original, json.loads(args.oracle.read_text()))
    if args.mode == "controls":
        return [crn_controls(row, original) for row in data]
    return audit_output(data, original, json.loads(args.oracle.read_text()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("preflight", "oracle", "audit", "controls"))
    parser.add_argument("original", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("captures", nargs="+", type=Path)
    parser.add_argument("--oracle", type=Path)
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args()
    original = json.loads(args.original.read_text())
    data = [json.loads(path.read_text()) for path in args.captures]
    output = dispatch(args, original, data)
    args.output.write_text(json.dumps(output, indent=2, allow_nan=False) + "\n")
    if args.strict and not all(r["parity"] and r["precision"] and r["combined"] for r in output["runs"]):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
