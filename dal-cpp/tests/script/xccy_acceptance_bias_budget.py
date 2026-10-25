"""Read frozen statistics; add conditional bias without changing old outcomes."""

import hashlib
import json

from xccy_acceptance_bias_intervals import interval, magnitude, require

REPORT_SHA256 = "cca7aac898607d6b2eef7f5d477e259cbff7fd1f167399cb55da35bddbc51327"


def read_report(path):
    raw = path.read_bytes()
    require(hashlib.sha256(raw).hexdigest() == REPORT_SHA256, "original budget/report identity")
    data = json.loads(raw)
    require(
        [(r["mode"], r["factor"]) for r in data["runs"]]
        == [(mode, factor) for factor in (1, 2, 4) for mode in ("EXACT", "APPROXIMATE")],
        "six original profiles",
    )
    return data["runs"]


def augmented_row(old, name, units, bias, budget_function):
    scale = ".01" if name == "Price" else ".0001"
    parameter_scale = parameter_step(old.get("label", ""))
    require(old["scale"] == float(scale) / float(parameter_scale), "original frozen native scale")
    budget = interval(scale) / interval(parameter_scale) / 4
    result = budget_function(
        name,
        units,
        budget,
        3 * interval(old["se"]),
        magnitude(interval(old["mean"]) - interval(old["reference"])),
        interval(old["numeric_indicator"]),
        bias,
        old["precision"] and old["combined"],
    )
    result["original_flags"] = {key: old[key] for key in ("parity", "precision", "combined")}
    result["precision_pass"] = bool(old["precision"] and result["new_precision"].b <= budget.a)
    result["combined_pass"] = bool(old["combined"] and result["new_combined"].b <= budget.a)
    result["original_precision_indicator"] = interval(old["precision_indicator"])
    result["original_combined_indicator"] = interval(old["combined_indicator"])
    result["epsilon"] = interval(old["epsilon"])
    result["conditional_parity_rhs"] = (
        3 * interval(old["se"]) + interval(old["numeric_indicator"]) + interval(old["epsilon"]) + bias
    )
    result["global_qualified"] = False
    return result


def parameter_step(label):
    if not label:
        return "1"
    if label.startswith(("fxspot:", "sigma:EUR/USD:")):
        return ".01"
    return ".001"


def measure_rows(old, profile, budget_function):
    name = old["measure"]
    units = "USD" if name == "Price" else "decimal-spread"
    rows = [augmented_row(old, name, units, profile["baseline"]["D2"][name], budget_function)]
    native = {column["label"]: column for column in profile["native"]}
    for gradient in old["gradients"]:
        column = native[gradient["label"]]
        row = augmented_row(gradient, name, units, column["D2"]["d" + name], budget_function)
        row.update(
            label=gradient["label"],
            parameter_unit=column["parameter_unit"],
            units=units + "/(" + column["parameter_unit"] + ")",
            reference_indicator_qualified=False,
            reference_indicator_status="inherited unqualified binary64 eFD indicator; sum is diagnostic",
        )
        rows.append(row)
    return rows


def run_rows(run, profiles, budget_function):
    rows = []
    for i, member in enumerate(run["members"]):
        profile = next(p for p in profiles if (p["fit"], p["dimension"]) == (run["mode"], (30, 55, 80)[i]))
        for measure in member["measures"]:
            for row in measure_rows(measure, profile, budget_function):
                rows.append(
                    dict(
                        row,
                        member=member["id"],
                        dimension=profile["dimension"],
                        denominator_confidence=interval(member["denominator_confidence"]),
                    )
                )
    return dict(
        mode=run["mode"],
        factor=run["factor"],
        rows=rows,
        original_flags={
            key: run[key]
            for key in (
                "parity",
                "precision",
                "combined",
                "reservation_exhausted",
                "old_heldout_already_observed",
                "binary64_eFD_qualified",
            )
        },
        financial_bias_bound_qualified=False,
    )


def combine_report(path, profiles, budget_function):
    runs = read_report(path)
    return dict(
        original_report_sha256=REPORT_SHA256,
        scope="original SE/discrepancy/eFast or unqualified eFD + new conditional D2 systematic term; zero new paths",
        actual_joint_law_qualified=False,
        financial_bias_bound_qualified=False,
        missing_sampling_axes=["logdf:USD:OIS", "logdf:EUR:OIS", "logdf:USD:3M", "logdf:EUR:6M"],
        runs=[run_rows(run, profiles, budget_function) for run in runs],
    )
