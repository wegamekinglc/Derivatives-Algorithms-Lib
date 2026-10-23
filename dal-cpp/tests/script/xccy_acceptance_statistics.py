"""Independent paired-batch XCCY value and native-gradient acceptance.

Retain failed numerical acceptance with --strict exit 1. Fit/HeldOut are never
fed to ValidateXccyMc or relabelled as Validation by this tool.
"""

import argparse
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
    return {"mean": mean, "se": se, "reference": reference, "numeric_indicator": indicator, "scale": scale,
            "parity": discrepancy <= precision + epsilon, "precision": precision <= .25 * scale,
            "combined": discrepancy + precision <= .25 * scale, "precision_indicator": precision,
            "combined_indicator": discrepancy + precision, "epsilon": epsilon}


def row_vector(batch):
    row = [batch["b"], batch["a"]]
    for db, da in zip(batch["db"], batch["da"]):
        row.extend([db, da])
    return row


def projected_se(rows, weights):
    return sample_se([math.fsum(w * x for w, x in zip(weights, row)) for row in rows])


def reference_indicator(reference):
    return float(reference["oracle_indicator"]) + float(reference["fast_indicator"])


def analyze(data, oracle):
    phase, factor, dimension = data["phase"], data["factor"], data["dimension"]
    require(factor in (1, 2, 4) and dimension == 30, "frozen prefix/mapping dimension")
    first, limit = {"Fit": (0, 262144), "Validation": (262144, 1310720), "HeldOut": (1310720, 2359296)}[phase]
    count = (4096 if phase == "Fit" else 16384) * factor
    require(len(data["batches"]) == len(data["ranges"]) == 16, "sixteen equal batches")
    for batch, actual in enumerate(data["ranges"]):
        begin, end = first + batch * count, first + (batch + 1) * count
        require(actual == [begin, end, begin * dimension, end * dimension] and end <= limit, "complete phase scalar coordinates")
    require(data["consumed"] == 16 * count * dimension, "actual consumed scalar coordinates")
    require(data["validation"] == (phase == "Validation") and data["held_out"] == (phase == "HeldOut"), "actual phase flags")
    reference = next(row for row in oracle["cases"] if row["id"] == "flat/MARK_TO_MARKET/domestic/receive")
    require(all(v["passed"] for v in reference["values"]), "direct deterministic gate must precede sampling")
    rows = [row_vector(batch) for batch in data["batches"]]
    means = [statistics.mean(column) for column in zip(*rows)]
    b, a = means[:2]
    covariance = [[statistics.covariance([row[i] for row in rows], [row[j] for row in rows]) for j in range(len(means))]
                  for i in range(len(means))]
    require(all(math.isclose(x, y, rel_tol=2e-11, abs_tol=1e-11)
                for actual, expected in zip(data["covariance"], covariance) for x, y in zip(actual, expected)), "full paired coefficient covariance")
    price_weights = [1, .001] + [0] * (len(means) - 2)
    price, price_se = b + .001 * a, projected_se(rows, price_weights)
    require(math.isclose(price, data["mean"], abs_tol=1e-12) and math.isclose(price_se, data["se"], rel_tol=1e-11), "independent Price SE")
    annuity_se = sample_se([row[1] for row in rows])
    confidence = abs(a) - 3 * annuity_se
    require(a < 0 and confidence > 1e-12, "signed denominator confidence")
    par = -b / a
    par_weights = [-1 / a, b / (a * a)] + [0] * (len(means) - 2)
    par_se = projected_se(rows, par_weights)
    output = {"phase": phase, "factor": factor, "reservation": data["reservation"], "mapping": data["mapping"],
              "paths": 16 * count, "ranges": data["ranges"], "annuity": a, "annuity_se": annuity_se,
              "denominator_confidence": confidence, "delta_method": True,
              "price": compare(price, price_se, float(reference["values"][2]["oracle"]), reference_indicator(reference["values"][2]), .01, 1e-10),
              "par": compare(par, par_se, float(reference["values"][3]["oracle"]), reference_indicator(reference["values"][3]), 1e-4, 1e-12),
              "gradients": [], "timing_conclusion": "inconclusive; no isolated paired baseline",
              "prepare_seconds": data["prepare_seconds"], "evaluate_seconds": data["evaluate_seconds"]}
    for j, label in enumerate(data["labels"]):
        ref = next(g for g in reference["gradients"] if g["label"] == label)
        scale = float(ref["scale"])
        db, da = means[2 + 2 * j:4 + 2 * j]
        weights = [0] * len(means)
        weights[2 + 2 * j], weights[3 + 2 * j] = 1, .001
        price_g, price_g_se = db + .001 * da, projected_se(rows, weights)
        require(math.isclose(price_g, data["gradient"][j], abs_tol=1e-10) and
                math.isclose(price_g_se, data["gradient_se"][j], rel_tol=1e-10, abs_tol=1e-11), "batch gradient SE independent of Price dispersion")
        par_g = -db / a + b * da / (a * a)
        weights = [da / (a * a), db / (a * a) - 2 * b * da / (a ** 3)] + [0] * (len(means) - 2)
        weights[2 + 2 * j], weights[3 + 2 * j] = -1 / a, b / (a * a)
        par_g_se = projected_se(rows, weights)
        output["gradients"].append({"label": label,
            "price": compare(price_g, price_g_se, float(ref["values"][2]["oracle"]), reference_indicator(ref["values"][2]), .01 / scale, 1e-8),
            "par": compare(par_g, par_g_se, float(ref["values"][3]["oracle"]), reference_indicator(ref["values"][3]), 1e-4 / scale, 1e-8)})
    checks = [output["price"], output["par"]] + [g[m] for g in output["gradients"] for m in ("price", "par")]
    output["numerics"] = all(c["parity"] and c["precision"] and c["combined"] for c in checks)
    output["budget_exhausted"] = not output["numerics"]
    return output


def wrong_se_control():
    prices = [10 + (j % 2) * .00001 for j in range(16)]
    gradients = [(-1) ** j * 2 for j in range(16)]
    actual = compare(statistics.mean(gradients), sample_se(gradients), 0, 0, 1, 1e-8)
    mutant = compare(statistics.mean(gradients), sample_se(prices), 0, 0, 1, 1e-8)
    require(not actual["precision"] and mutant["precision"], "prequalified wrong-gradient-SE mutant must flip precision")
    return {"correct": actual, "wrong_price_se": mutant}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("oracle", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("captures", nargs="+", type=Path)
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args()
    oracle = json.loads(args.oracle.read_text())
    reports = [analyze(json.loads(path.read_text()), oracle) for path in args.captures]
    args.output.write_text(json.dumps({"runs": reports, "wrong_se_control": wrong_se_control()}, indent=2) + "\n")
    for row in reports:
        print(row["phase"], row["factor"], "Price", row["price"], "par", row["par"], "numerics", row["numerics"])
    if args.strict:
        require(all(row["numerics"] for row in reports), "frozen statistical acceptance failed; estimates retained")


if __name__ == "__main__":
    main()
