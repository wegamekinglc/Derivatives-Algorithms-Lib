"""A/1 current view, independent of immutable historical qualification rows."""

import copy
import json
from collections import Counter

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


def rows(directory, history, studies):
    retained = [
        dict(r, evidence_namespace="original-F1-reused")
        for r in history["rows"]
        if r["fixture"].startswith("F1")
    ]
    added = []
    for name in ("F2_linear", "F2_nonlinear"):
        mc_path = directory / (name + "-mc.json")
        mc = json.loads(mc_path.read_text()) if mc_path.exists() else None
        original = [r for r in history["rows"] if r["fixture"] == name]
        for g, grid in enumerate(("P7", "P8")):
            for i, source in enumerate(original):
                row = copy.deepcopy(source)
                row.update(
                    grid=grid,
                    evidence_namespace="A/1",
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
                    dmax=8728,
                    stream_family=0 if name == "F2_linear" else 1,
                    final_h=1 / 3072,
                    pair_h=[1 / 1536, 1 / 3072],
                )
                record = studies.get((name, grid, row["maturity"]))
                if record:
                    row["grid_gate"] = record["comparison"]["status"]
                    row["grid_reference_status"] = record["status"]
                    grid_value = grid_outputs(record, row["maturity"])[i % 20]
                    row.update(
                        grid_pde=grid_value,
                        grid_error_normalized=(grid_value - row["oracle"]) / 100,
                        decomposition_certified=record["status"] == "PASS",
                    )
                if mc and mc["sampling"] == "FORMAL":
                    tier = mc["tiers"][-1]
                    estimate, se = tier["groups"][g * 4 + 3]["estimates"][i]
                    value, pair_se = tier["groups"][10 + g * 3]["estimates"][i]
                    deterministic = row["product"] in (
                        "discount",
                        "fixed_payment",
                        "delayed_payment",
                    )
                    price = gate(
                        estimate, se, row["oracle"], False, deterministic, "PASS"
                    )
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
                        status=qualified_status(
                            price, admission, pair, deterministic, False
                        ),
                    )
                    if row["grid_pde"] is not None:
                        row["stepping_plus_sampling_normalized"] = (
                            estimate - row["grid_pde"]
                        ) / 100
                added.append(row)
    if len(retained) != 160 or len(added) != 320:
        raise ValueError("Current qualification shape mismatch")
    return retained + added


def report(directory, history, studies, stop_reason):
    result = rows(directory, history, studies)
    counts = dict(Counter(r["status"] for r in result))
    certificates = (
        "stage1-admission.json",
        "mc-admission.json",
        "n4-summary.json",
        "interpolation-probes.json",
    )
    complete = all((directory / name).exists() for name in certificates)
    if complete:
        complete = all(
            json.loads((directory / name).read_text())["status"] == "PASS"
            for name in certificates
        )
    complete = (
        complete
        and len(studies) == 16
        and all(
            r["status"] == "PASS" and r["comparison"]["status"] == "PASS"
            for r in studies.values()
        )
    )
    return dict(
        schema="dal.market-iv-a1-qualification/1",
        rows=result,
        counts=counts,
        history_counts=history["counts"],
        history_untouched=True,
        accepted=stop_reason is None
        and complete
        and len(result) == 480
        and counts == {"PASS": 480},
        stop_reason=stop_reason,
    )
