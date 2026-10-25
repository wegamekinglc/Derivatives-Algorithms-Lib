"""Independent paired-batch XCCY value and native-gradient acceptance.

Retain failed numerical acceptance with --strict exit 1. Fit/HeldOut are never
fed to ValidateXccyMc or relabelled as Validation by this tool.
"""

import argparse
import hashlib
import json
import math
import statistics
from pathlib import Path


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sample_se(values):
    return statistics.stdev(values) / math.sqrt(len(values))


def compare(mean, se, reference, indicator, scale, epsilon):
    discrepancy = abs(mean - reference)
    precision = 3 * se + indicator
    return {
        "mean": mean,
        "se": se,
        "reference": reference,
        "numeric_indicator": indicator,
        "scale": scale,
        "parity": discrepancy <= precision + epsilon,
        "precision": precision <= 0.25 * scale,
        "combined": discrepancy + precision <= 0.25 * scale,
        "precision_indicator": precision,
        "combined_indicator": discrepancy + precision,
        "epsilon": epsilon,
    }


def row_vector(batch):
    row = [batch["b"], batch["a"]]
    for db, da in zip(batch["db"], batch["da"]):
        row.extend([db, da])
    return row


def projected_se(rows, weights):
    return sample_se([math.fsum(w * x for w, x in zip(weights, row)) for row in rows])


def reference_indicator(reference):
    return float(reference["oracle_indicator"]) + float(reference["fast_indicator"])


def legacy_fingerprint(data):
    encoded = json.dumps(data, sort_keys=True, separators=(",", ":"), allow_nan=False).encode()
    return hashlib.sha256(encoded).hexdigest()


def require_width(values, width, diagnostic):
    require(isinstance(values, list) and len(values) == width, diagnostic)


def check_axes(data, labels):
    require(data["labels"] == labels and len(set(labels)) == len(labels), "selected gradient axis labels")
    count = len(labels)
    require(count > 0, "selected gradient axes nonempty")
    for key in ("gradient", "gradient_se"):
        require_width(data.get(key), count, "gradient output axis: " + key)
    for row in [data["coefficients"]] + data["batches"]:
        for key in ("db", "da"):
            require_width(row.get(key), count, "batch coefficient axis: " + key)
    require_width(data["covariance"], 2 + 2 * count, "complete covariance axis count")
    for row in data["covariance"]:
        require_width(row, 2 + 2 * count, "complete covariance row axis")


def check_context(data, context):
    for key in ("source", "model", "mapping", "dimension"):
        require(data.get(key) == context[key], "reference context mismatch: " + key)
    if "instrument" in data:
        require(data["instrument"] in context["instrument_aliases"], "reference context mismatch: instrument binding")
    if all(key in data for key in ("instrument", "binding", "measure")):
        require(data["binding"] == context["binding"], "reference context mismatch: binding")
        require(data["measure"] == context["measure"], "reference context mismatch: measure")
    else:
        require(legacy_fingerprint(data) in context["legacy_records"], "qualified legacy source/binding/measure provenance required")


def qualified_direct_gate(value, budget, epsilon):
    parts = [float(value[key]) for key in ("oracle_indicator", "fast_indicator", "error")]
    require(value["passed"] is True, "direct gate not passed")
    require(all(math.isfinite(x) and x >= 0 for x in parts), "finite nonnegative direct gate indicators")
    require(max(parts[:2]) <= budget / 4, "direct gate quarter indicators")
    require(sum(parts) <= budget + epsilon, "direct gate combined indicator")
    require(math.isfinite(float(value["oracle"])), "finite direct reference")


def qualify_direct_values(values, scale, diagnostic):
    require_width(values, 4, diagnostic + ": A/B/Price/par axes")
    try:
        for column, value in enumerate(values):
            budget = (1e-12 if column == 3 else 1e-10) / scale
            qualified_direct_gate(value, budget, budget / 10)
    except (KeyError, TypeError, ValueError) as error:
        raise ValueError(diagnostic + ": " + str(error)) from error


def qualify_gradients(reference, labels):
    rows = reference.get("gradients", [])
    indexed = {row["label"]: row for row in rows}
    require(len(indexed) == len(rows), "gradient references must have unique labels")
    for label in labels:
        require(label in indexed, "selected gradient reference missing: " + label)
        row = indexed[label]
        scale = 0.01 if label.startswith(("fxspot:", "sigma:EUR/USD:")) else 0.001
        require(float(row["scale"]) == scale, "direct gradient frozen native scale")
        require(math.isclose(float(row["h"]), 0.001 * scale, rel_tol=1e-14, abs_tol=0.0), "direct gradient frozen FD step")
        qualify_direct_values(row["values"], scale, "direct gradient qualification: " + label)


def qualify_reference(data, reference):
    context = reference.get("context")
    require(isinstance(context, dict), "qualified reference context required")
    check_axes(data, context["selected_labels"])
    check_context(data, context)
    qualify_direct_values(reference["values"], 1.0, "direct value qualification")
    qualify_gradients(reference, data["labels"])


def unique_cases(rows, diagnostic):
    result = {row["id"]: row for row in rows}
    require(len(result) == len(rows), diagnostic + " unique case IDs")
    return result


def match_fixture(context, fixture):
    for key in ("source", "model", "mapping", "dimension"):
        require(context[key] == fixture[key], "frozen reference fixture mismatch: " + key)
    require(context["fixture_instrument"] == fixture["instrument"], "frozen reference fixture instrument/binding")
    require(context["native_labels"] == fixture["labels"], "frozen reference native axes")
    require(context["measure"] == "Price", "paired Price reference measure")


def load_qualified_oracle(oracle_path, capture_path, contexts_path, legacy_path=None, selected_cases=None):
    oracle = json.loads(oracle_path.read_text())
    require(oracle.get("capture_sha256") == hashlib.sha256(capture_path.read_bytes()).hexdigest(), "oracle frozen reference capture SHA256")
    fixtures = unique_cases(json.loads(capture_path.read_text()), "reference capture")
    contexts = unique_cases(json.loads(contexts_path.read_text()), "reference contexts")
    rows = unique_cases(oracle["cases"], "oracle")
    selected = set(rows) & set(contexts) if selected_cases is None else set(selected_cases)
    require(bool(selected) and selected <= set(rows) & set(fixtures) & set(contexts), "complete selected reference cases")
    legacy = json.loads(legacy_path.read_text())["records"] if legacy_path else {}
    for case_id in selected:
        context = contexts[case_id]
        match_fixture(context, fixtures[case_id])
        rows[case_id]["context"] = dict(context, legacy_records=legacy.get(case_id, []))
    return oracle


def add_reference_arguments(parser):
    parser.add_argument("--reference-capture", required=True, type=Path)
    parser.add_argument("--contexts", required=True, type=Path)
    parser.add_argument("--legacy-records", type=Path)


def check_reservation(data, expected_dimension=30):
    phase, factor, dimension = data["phase"], data["factor"], data["dimension"]
    require(factor in (1, 2, 4) and dimension == expected_dimension, "frozen prefix/mapping dimension")
    first, limit = {
        "Fit": (0, 262144),
        "Validation": (262144, 1310720),
        "HeldOut": (1310720, 2359296),
    }[phase]
    count = (4096 if phase == "Fit" else 16384) * factor
    require(len(data["batches"]) == len(data["ranges"]) == 16, "sixteen equal batches")
    for batch, actual in enumerate(data["ranges"]):
        begin, end = first + batch * count, first + (batch + 1) * count
        require(
            actual == [begin, end, begin * dimension, end * dimension] and end <= limit,
            "complete phase scalar coordinates",
        )
    require(data["consumed"] == 16 * count * dimension, "actual consumed scalar coordinates")
    require(
        data["validation"] == (phase == "Validation") and data["held_out"] == (phase == "HeldOut"),
        "actual phase flags",
    )


def row_matches(actual, expected):
    return len(actual) == len(expected) and all(math.isclose(x, y, rel_tol=2e-11, abs_tol=1e-11) for x, y in zip(actual, expected))


def check_covariance(data, rows, means):
    require(len(data["covariance"]) == len(means), "complete paired covariance row count")
    covariance = [[statistics.covariance([row[i] for row in rows], [row[j] for row in rows]) for j in range(len(means))] for i in range(len(means))]
    require(
        all(row_matches(actual, expected) for actual, expected in zip(data["covariance"], covariance)),
        "full paired coefficient covariance",
    )


def coefficient_summary(data, reference, annuity_sign=-1):
    rows = [row_vector(batch) for batch in data["batches"]]
    means = [statistics.mean(column) for column in zip(*rows)]
    b, a = means[:2]
    check_covariance(data, rows, means)
    price_weights = [1, 0.001] + [0] * (len(means) - 2)
    price, price_se = b + 0.001 * a, projected_se(rows, price_weights)
    require(
        math.isclose(price, data["mean"], abs_tol=1e-12) and math.isclose(price_se, data["se"], rel_tol=1e-11),
        "independent Price SE",
    )
    annuity_se = sample_se([row[1] for row in rows])
    confidence = abs(a) - 3 * annuity_se
    require(annuity_sign * a > 0 and confidence > 1e-12, "signed denominator confidence")
    par = -b / a
    par_weights = [-1 / a, b / (a * a)] + [0] * (len(means) - 2)
    par_se = projected_se(rows, par_weights)
    output = {
        "phase": data["phase"],
        "factor": data["factor"],
        "reservation": data["reservation"],
        "mapping": data["mapping"],
        "paths": sum(r[1] - r[0] for r in data["ranges"]),
        "ranges": data["ranges"],
        "annuity": a,
        "annuity_se": annuity_se,
        "denominator_confidence": confidence,
        "delta_method": True,
        "price": compare(
            price,
            price_se,
            float(reference["values"][2]["oracle"]),
            reference_indicator(reference["values"][2]),
            0.01,
            1e-10,
        ),
        "par": compare(
            par,
            par_se,
            float(reference["values"][3]["oracle"]),
            reference_indicator(reference["values"][3]),
            1e-4,
            1e-12,
        ),
        "gradients": [],
        "timing_conclusion": "inconclusive; no isolated paired baseline",
        "prepare_seconds": data["prepare_seconds"],
        "evaluate_seconds": data["evaluate_seconds"],
    }
    return output, rows, means


def gradient_summary(data, reference, rows, means, j, label):
    b, a = means[:2]
    ref = next(g for g in reference["gradients"] if g["label"] == label)
    scale = float(ref["scale"])
    db, da = means[2 + 2 * j : 4 + 2 * j]
    weights = [0] * len(means)
    weights[2 + 2 * j], weights[3 + 2 * j] = 1, 0.001
    price_g, price_g_se = db + 0.001 * da, projected_se(rows, weights)
    require(
        math.isclose(price_g, data["gradient"][j], abs_tol=1e-10) and math.isclose(price_g_se, data["gradient_se"][j], rel_tol=1e-10, abs_tol=1e-11),
        "batch gradient SE independent of Price dispersion",
    )
    par_g = -db / a + b * da / (a * a)
    weights = [da / (a * a), db / (a * a) - 2 * b * da / (a**3)] + [0] * (len(means) - 2)
    weights[2 + 2 * j], weights[3 + 2 * j] = -1 / a, b / (a * a)
    par_g_se = projected_se(rows, weights)
    return {
        "label": label,
        "price": compare(
            price_g,
            price_g_se,
            float(ref["values"][2]["oracle"]),
            reference_indicator(ref["values"][2]),
            0.01 / scale,
            1e-8,
        ),
        "par": compare(
            par_g,
            par_g_se,
            float(ref["values"][3]["oracle"]),
            reference_indicator(ref["values"][3]),
            1e-4 / scale,
            1e-8,
        ),
    }


def analyze(data, oracle):
    check_reservation(data)
    reference = next(row for row in oracle["cases"] if row["id"] == "flat/MARK_TO_MARKET/domestic/receive")
    qualify_reference(data, reference)
    output, rows, means = coefficient_summary(data, reference)
    output["gradients"] = [gradient_summary(data, reference, rows, means, j, label) for j, label in enumerate(data["labels"])]
    output["numerics"] = complete_acceptance(output)
    output["budget_exhausted"] = not output["numerics"]
    return output


def complete_acceptance(output):
    checks = [output["price"], output["par"]] + [g[m] for g in output["gradients"] for m in ("price", "par")]
    return all(c["parity"] and c["precision"] and c["combined"] for c in checks)


def wrong_se_control():
    prices = [10 + (j % 2) * 0.00001 for j in range(16)]
    gradients = [(-1) ** j * 2 for j in range(16)]
    actual = compare(statistics.mean(gradients), sample_se(gradients), 0, 0, 1, 1e-8)
    mutant = compare(statistics.mean(gradients), sample_se(prices), 0, 0, 1, 1e-8)
    require(
        not actual["precision"] and mutant["precision"],
        "prequalified wrong-gradient-SE mutant must flip precision",
    )
    return {"correct": actual, "wrong_price_se": mutant}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("oracle", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("captures", nargs="+", type=Path)
    parser.add_argument("--strict", action="store_true")
    add_reference_arguments(parser)
    args = parser.parse_args()
    oracle = load_qualified_oracle(args.oracle, args.reference_capture, args.contexts, args.legacy_records, ["flat/MARK_TO_MARKET/domestic/receive"])
    reports = [analyze(json.loads(path.read_text()), oracle) for path in args.captures]
    args.output.write_text(json.dumps({"runs": reports, "wrong_se_control": wrong_se_control()}, indent=2) + "\n")
    for row in reports:
        print(
            row["phase"],
            row["factor"],
            "Price",
            row["price"],
            "par",
            row["par"],
            "numerics",
            row["numerics"],
        )
    if args.strict:
        require(
            all(row["numerics"] for row in reports),
            "frozen statistical acceptance failed; estimates retained",
        )


if __name__ == "__main__":
    main()
