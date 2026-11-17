"""A/1 current view, independent of immutable historical qualification rows."""

import copy
import json
from collections import Counter
from pathlib import Path

from a1_verify import verify
from formal_report import gate, paired_gate, qualified_status
from reference import carry


def grid_outputs(record, time):
    values = record["final_values"]
    discount = float(carry(time)[1])
    delayed = float(carry(2.25)[1]) / discount
    return (
        values[:12]
        + [values[i] - values[i + 5] for i in range(5)]
        + [values[-1] / discount, values[-2], values[-2] * delayed]
    )


def base_row(source, grid, name):
    row = copy.deepcopy(source)
    row.update(
        grid=grid,
        evidence_namespace="A/3",
        estimate=None,
        se=None,
        paired_difference=None,
        paired_se=None,
        normalized_error=None,
        stepping_plus_sampling_normalized=None,
        grid_pde=None,
        grid_error_normalized=None,
        source_sampling=None,
        paths_per_batch=None,
        status="NOT_RUN",
        price_gate="NOT_RUN",
        paired_gate="NOT_RUN",
        grid_gate="NOT_RUN",
        grid_reference_status="NOT_RUN",
        dmax=8736,
        stream_family=0 if name == "F2_linear" else 1,
        final_h=1 / 3072,
        pair_h=[1 / 1536, 1 / 3072],
    )
    return row


def add_grid(row, record, index):
    if record:
        row["grid_gate"] = record["comparison"]["status"]
        row["grid_reference_status"] = record["status"]
        grid_value = grid_outputs(record, row["maturity"])[index % 20]
        row.update(
            grid_pde=grid_value,
            grid_error_normalized=(grid_value - row["oracle"]) / 100,
            decomposition_certified=record["status"] == "PASS",
        )


def add_measurement(row, mc, g, index):
    if mc and mc["sampling"] == "FORMAL":
        tier = mc["tiers"][-1]
        estimate, se = tier["groups"][g * 4 + 3]["estimates"][index]
        value, pair_se = tier["groups"][10 + g * 3]["estimates"][index]
        deterministic = row["product"] in (
            "discount",
            "fixed_payment",
            "delayed_payment",
        )
        price = gate(estimate, se, row["oracle"], False, deterministic, "PASS")
        pair = paired_gate(value, pair_se)
        admission = dict(
            status=row["grid_gate"],
            reference_self_check=dict(status=row["grid_reference_status"]),
        )
        row.update(
            estimate=estimate,
            se=se,
            paired_difference=value,
            paired_se=pair_se,
            normalized_error=(estimate - row["oracle"]) / 100,
            source_sampling="FORMAL",
            paths_per_batch=tier["paths_per_batch"],
            price_gate=price,
            paired_gate=pair,
            status=qualified_status(price, admission, pair, deterministic, False),
        )
        if not tier["se_sufficient"]:
            row["status"] = "INCOMPLETE"
        if row["grid_pde"] is not None:
            row["stepping_plus_sampling_normalized"] = (
                estimate - row["grid_pde"]
            ) / 100


def family_rows(directory, history, studies, name):
    mc_path = directory / (name + "-mc.json")
    mc = json.loads(mc_path.read_text()) if mc_path.exists() else None
    if mc and mc["sampling"] == "FORMAL":
        verify(mc)
    original = [r for r in history["rows"] if r["fixture"] == name]
    result = []
    for g, grid in enumerate(("P7", "P8")):
        for index, source in enumerate(original):
            row = base_row(source, grid, name)
            add_grid(row, studies.get((name, grid, row["maturity"])), index)
            add_measurement(row, mc, g, index)
            result.append(row)
    return result


def rows(directory, history, studies):
    retained = [
        dict(r, evidence_namespace="original-F1-reused")
        for r in history["rows"]
        if r["fixture"].startswith("F1")
    ]
    added = [
        row
        for name in ("F2_linear", "F2_nonlinear")
        for row in family_rows(directory, history, studies, name)
    ]
    if len(retained) != 160 or len(added) != 320:
        raise ValueError("Current qualification shape mismatch")
    return retained + added


def certificates_pass(directory):
    names = (
        "stage1-admission.json",
        "mc-admission.json",
        "n4-summary.json",
        "interpolation-probes.json",
    )
    if not all((directory / name).exists() for name in names):
        return False
    return all(
        json.loads((directory / name).read_text())["status"] == "PASS" for name in names
    )


def studies_pass(studies):
    return len(studies) == 16 and all(
        r["status"] == "PASS" and r["comparison"]["status"] == "PASS"
        for r in studies.values()
    )


def report(directory, history, studies, stop_reason):
    result = rows(directory, history, studies)
    counts = dict(Counter(r["status"] for r in result))
    complete = certificates_pass(directory) and studies_pass(studies)
    return dict(
        schema="dal.market-iv-a1-qualification/1",
        rows=result,
        counts=counts,
        history_counts=history["counts"],
        history_untouched=True,
        accepted=stop_reason is None
        and complete
        and len(result) == 480
        and counts.get("PASS", 0) == 480
        and len(counts) == 1,
        stop_reason=stop_reason,
    )


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="Verified, extracted A/3 evidence")
    args = parser.parse_args()
    studies = json.loads((args.directory / "grid-studies.json").read_text())
    lookup = {(r["fixture"], r["grid"], r["maturity"]): r for r in studies}
    history = json.loads((args.directory / "history.json").read_text())
    print(json.dumps(report(args.directory, history, lookup, None), allow_nan=False))
